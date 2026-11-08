#include "knoblibrary.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <cmath>

KnobLibrary::KnobLibrary(const QString &dir) : m_dir(dir.isEmpty() ? defaultDir() : dir) {}

QString KnobLibrary::defaultDir()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList cands = {
        appDir + QStringLiteral("/knob-library"),
        appDir + QStringLiteral("/../../outputs/knob-library"),
        appDir + QStringLiteral("/../../../outputs/knob-library"),
        QStringLiteral("C:/Users/Nalani/Documents/Codex/2026-10-15/new-chat/outputs/knob-library"),
    };
    for (const QString &c : cands)
        if (QFile::exists(c + QStringLiteral("/index.json")))
            return QDir(c).absolutePath();
    return appDir + QStringLiteral("/knob-library");
}

bool KnobLibrary::isAvailable() const
{
    return QFile::exists(QDir(m_dir).filePath(QStringLiteral("index.json")));
}

QVector<KnobLibrary::Entry> KnobLibrary::entries() const
{
    QVector<Entry> out;
    QFile f(QDir(m_dir).filePath(QStringLiteral("index.json")));
    if (!f.open(QIODevice::ReadOnly)) return out;
    const QJsonObject idx = QJsonDocument::fromJson(f.readAll()).object();
    f.close();
    for (const QJsonValue &v : idx.value(QStringLiteral("entries")).toArray()) {
        const QJsonObject o = v.toObject();
        Entry e;
        e.machine = o.value(QStringLiteral("machine_type")).toString();
        e.material = o.value(QStringLiteral("material_code")).toString();
        e.thickness = o.value(QStringLiteral("layer_thickness_mm")).toDouble();
        e.schema = o.value(QStringLiteral("schema")).toInt();
        e.params = o.value(QStringLiteral("params")).toInt();
        e.named = o.value(QStringLiteral("named")).toInt();
        out.push_back(e);
    }
    return out;
}

bool KnobLibrary::find(const QString &machineType, const QString &materialCode,
                       double layerThicknessMm, QVector<KnobParam> *out,
                       int *schemaVersion) const
{
    if (out) out->clear();
    const QString file = QDir(m_dir).filePath(
        QStringLiteral("%1__%2__%3mm.json").arg(machineType, materialCode)
            .arg(QString::number(layerThicknessMm, 'g', 10)));
    QFile f(file);
    if (!f.exists()) return false;
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    f.close();
    if (root.isEmpty()) return false;
    if (schemaVersion) *schemaVersion = root.value(QStringLiteral("knobs_schema_version")).toInt();
    for (const QJsonValue &cv : root.value(QStringLiteral("categories")).toArray()) {
        const QJsonObject c = cv.toObject();
        const QString ck = c.value(QStringLiteral("category_key")).toString();
        const QString cn = c.value(QStringLiteral("category_name")).toString();
        for (const QJsonValue &pv : c.value(QStringLiteral("params")).toArray()) {
            const QJsonObject p = pv.toObject();
            KnobParam kp;
            kp.categoryKey = ck;
            kp.categoryName = cn;
            kp.key = p.value(QStringLiteral("key")).toString();
            kp.name = p.value(QStringLiteral("name")).toString();
            kp.value = p.value(QStringLiteral("value"));
            if (out) out->push_back(kp);
        }
    }
    return true;
}
