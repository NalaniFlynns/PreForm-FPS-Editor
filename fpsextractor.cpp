#include "fpsextractor.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QCoreApplication>

namespace {
const QByteArray kMarker = QByteArrayLiteral("{\"algorithm_profiles\"");

// 从 start（指向 '{'）开始找到匹配的 '}'，返回其后的位置；-1 表示失败。
qint64 findBalancedObject(const QByteArray &data, qint64 start)
{
    int depth = 0;
    qint64 i = start;
    const qint64 n = data.size();
    bool inStr = false, esc = false;
    while (i < n) {
        const char c = data.at(i);
        if (inStr) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') inStr = false;
        } else {
            if (c == '"') inStr = true;
            else if (c == '{') ++depth;
            else if (c == '}') {
                --depth;
                if (depth == 0) return i + 1;
            }
        }
        ++i;
    }
    return -1;
}

bool looksLikePreFormExe(const QString &path)
{
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) return false;
    if (fi.size() < 5 * 1024 * 1024) return false; // PreForm.exe 约 200MB+
    return fi.fileName().compare(QStringLiteral("PreForm.exe"), Qt::CaseInsensitive) == 0;
}
} // namespace

QStringList FpsExtractor::candidatePreFormPaths()
{
    QStringList out;
    auto add = [&](const QString &p) {
        if (!p.isEmpty() && !out.contains(p)) out << QDir::cleanPath(p);
    };

    // 1) 运行中的 PreForm 进程（最准确）
    QProcess ps;
    ps.start(QStringLiteral("wmic"),
             {QStringLiteral("process"), QStringLiteral("where"),
              QStringLiteral("name='PreForm.exe'"), QStringLiteral("get"),
              QStringLiteral("ExecutablePath"), QStringLiteral("/value")});
    if (ps.waitForFinished(2500)) {
        const QString text = QString::fromLocal8Bit(ps.readAllStandardOutput());
        static const QRegularExpression re(QStringLiteral("ExecutablePath=(.+)"));
        auto it = re.globalMatch(text);
        while (it.hasNext()) {
            QString p = it.next().captured(1).trimmed().replace('\\', '/');
            if (looksLikePreFormExe(p)) add(p);
        }
    }

    // 2) 常见安装根目录
    const QStringList roots = {
        QStringLiteral("C:/Program Files/Formlabs"),
        QStringLiteral("C:/Program Files (x86)/Formlabs"),
        QStringLiteral("D:/Program Files/Formlabs"),
        QStringLiteral("C:/Program Files/Formlabs/PreForm-beta"),
    };
    for (const QString &root : roots) {
        QDir rd(root);
        if (!rd.exists()) continue;
        // 递归两层查找 PreForm.exe
        for (const QFileInfo &sub : rd.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString direct = sub.absoluteFilePath() + QStringLiteral("/PreForm.exe");
            if (looksLikePreFormExe(direct)) add(direct);
            for (const QFileInfo &sub2 : QDir(sub.absoluteFilePath()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                const QString p2 = sub2.absoluteFilePath() + QStringLiteral("/PreForm.exe");
                if (looksLikePreFormExe(p2)) add(p2);
            }
        }
    }

    // 3) 注册表卸载信息
    for (const QString &hive : {QStringLiteral("HKEY_LOCAL_MACHINE"), QStringLiteral("HKEY_CURRENT_USER")}) {
        QSettings reg(hive + QStringLiteral("\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall"),
                      QSettings::NativeFormat);
        for (const QString &grp : reg.childGroups()) {
            reg.beginGroup(grp);
            const QString disp = reg.value(QStringLiteral("DisplayName")).toString();
            const QString loc = reg.value(QStringLiteral("InstallLocation")).toString();
            if (disp.contains(QStringLiteral("PreForm"), Qt::CaseInsensitive) && !loc.isEmpty()) {
                QDir ld(loc);
                if (ld.exists()) {
                    const QString direct = ld.absolutePath() + QStringLiteral("/PreForm.exe");
                    if (looksLikePreFormExe(direct)) add(direct);
                    for (const QFileInfo &sub : ld.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                        const QString p = sub.absoluteFilePath() + QStringLiteral("/PreForm.exe");
                        if (looksLikePreFormExe(p)) add(p);
                    }
                }
            }
            reg.endGroup();
        }
    }

    // 4) 相对编辑器位置的兜底
    const QString appDir = QCoreApplication::applicationDirPath();
    for (const QString &c : {appDir + QStringLiteral("/PreForm.exe")}) add(c);

    return out;
}

QString FpsExtractor::autoLocate(QString *howFound)
{
    const QStringList cands = candidatePreFormPaths();
    if (cands.isEmpty()) {
        if (howFound) *howFound = QStringLiteral("未找到 PreForm.exe");
        return QString();
    }
    if (howFound) *howFound = QStringLiteral("自动定位到：%1").arg(cands.first());
    return cands.first();
}

bool FpsExtractor::extract(const QString &exePath, QVector<ExtractedFps> *out, QString *error)
{
    if (out) out->clear();
    QFile f(exePath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("无法打开 %1：%2").arg(exePath, f.errorString());
        return false;
    }
    const QByteArray data = f.readAll();
    f.close();
    if (data.size() < 1024) {
        if (error) *error = QStringLiteral("文件过小，不是有效的 PreForm.exe");
        return false;
    }

    QHash<QString, int> byHash;      // sha256 -> index in out
    QHash<QString, int> byIdentity;  // machine|material|thick|name -> index
    int parsed = 0, dupHash = 0, dupIdentity = 0, bad = 0;

    qint64 idx = 0;
    while (true) {
        idx = data.indexOf(kMarker, idx);
        if (idx < 0) break;
        const qint64 end = findBalancedObject(data, idx);
        if (end < 0) { ++bad; idx += 1; continue; }
        const QByteArray raw = data.mid(idx, end - idx);
        QJsonParseError pe{};
        const QJsonDocument doc = QJsonDocument::fromJson(raw, &pe);
        if (pe.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject j = doc.object();
            const QJsonObject md = j.value(QStringLiteral("metadata")).toObject();
            const QJsonObject bmi = md.value(QStringLiteral("base_material_identifier")).toObject();
            // 只接受真正的完整 FPS（含 underlying_material 加密块）
            const bool hasCipher = j.value(QStringLiteral("underlying_material")).isString();
            ExtractedFps item;
            item.raw = raw;
            item.machine = bmi.value(QStringLiteral("machine_type_id")).toString();
            item.material = bmi.value(QStringLiteral("versioned_material_code")).toString();
            item.thickness = bmi.value(QStringLiteral("layer_thickness_mm")).toDouble();
            item.name = md.value(QStringLiteral("name")).toString();
            item.sha256 = QString::fromLatin1(
                QCryptographicHash::hash(raw, QCryptographicHash::Sha256).toHex());
            item.fileOffset = idx;
            ++parsed;

            const QString identity = QStringLiteral("%1|%2|%3|%4")
                .arg(item.machine, item.material).arg(item.thickness).arg(item.name);

            if (!hasCipher) { ++bad; }
            else if (byHash.contains(item.sha256)) { ++dupHash; }
            else if (byIdentity.contains(identity)) { ++dupIdentity; }
            else {
                byHash.insert(item.sha256, out->size());
                byIdentity.insert(identity, out->size());
                out->push_back(item);
            }
        } else {
            ++bad;
        }
        idx = end;
    }

    if (out->isEmpty()) {
        if (error) *error = QStringLiteral("未在 %1 中找到内嵌 fps（解析 %2，失败 %3）")
                                .arg(QFileInfo(exePath).fileName()).arg(parsed).arg(bad);
        return false;
    }
    return true;
}

QString FpsExtractor::shortHash(const QByteArray &data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()).left(12);
}

bool FpsExtractor::saveAll(const QVector<ExtractedFps> &items, const QString &outDir,
                           QStringList *writtenFiles, QString *error)
{
    QDir d(outDir);
    if (!d.exists() && !d.mkpath(outDir)) {
        if (error) *error = QStringLiteral("无法创建目录：%1").arg(outDir);
        return false;
    }
    int n = 0;
    for (const ExtractedFps &it : items) {
        QString name = it.name.isEmpty() ? QStringLiteral("Default") : it.name;
        name.replace('/', '_').replace('\\', '_').replace(':', '_');
        QString mat = it.material.isEmpty() ? QStringLiteral("UNKNOWN") : it.material;
        QString mach = it.machine.isEmpty() ? QStringLiteral("UNKNOWN") : it.machine;
        QString fn = QStringLiteral("%1__%2__%3mm__%4.fps").arg(mach, mat).arg(it.thickness).arg(name);
        const QString path = d.filePath(fn);
        QFile of(path);
        if (!of.open(QIODevice::WriteOnly)) {
            if (error) *error = QStringLiteral("写入失败：%1").arg(path);
            return false;
        }
        of.write(it.raw);
        of.close();
        if (writtenFiles) *writtenFiles << path;
        ++n;
    }
    return n > 0;
}
