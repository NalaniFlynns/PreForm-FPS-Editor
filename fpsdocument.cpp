#include "fpsdocument.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUuid>
#include <QDateTime>
#include <QRegularExpression>
#include <QByteArray>
#include <cmath>

static const int kSchemaVersion = 3;

QString FpsDocument::fileName() const
{
    if (m_path.isEmpty()) return QStringLiteral("（未命名）");
    return QFileInfo(m_path).fileName();
}

bool FpsDocument::load(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("无法打开文件：%1").arg(f.errorString());
        return false;
    }
    const QByteArray raw = f.readAll();
    f.close();

    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &pe);
    if (pe.error != QJsonParseError::NoError) {
        if (error) *error = QStringLiteral("JSON 解析失败（偏移 %1）：%2").arg(pe.offset).arg(pe.errorString());
        return false;
    }
    if (!doc.isObject()) {
        if (error) *error = QStringLiteral("根节点不是 JSON 对象");
        return false;
    }
    m_root = doc.object();
    m_path = path;
    m_dirty = false;
    return true;
}

bool FpsDocument::save(const QString &path, QString *error)
{
    if (m_root.isEmpty()) {
        if (error) *error = QStringLiteral("没有可保存的内容");
        return false;
    }
    const QStringList problems = validate(m_root);
    if (!problems.isEmpty()) {
        if (error) *error = QStringLiteral("结构校验未通过：\n• %1").arg(problems.join(QStringLiteral("\n• ")));
        return false;
    }

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("无法写入：%1").arg(out.errorString());
        return false;
    }
    const QByteArray json = QJsonDocument(m_root).toJson(QJsonDocument::Indented);
    out.write(json);
    if (!out.commit()) {
        if (error) *error = QStringLiteral("提交写入失败：%1").arg(out.errorString());
        return false;
    }
    m_path = path;
    m_dirty = false;
    return true;
}

QStringList FpsDocument::validate(const QJsonObject &root)
{
    QStringList e;
    if (root.isEmpty()) { e << QStringLiteral("根对象为空"); return e; }

    if (!root.contains("metadata") || !root.value("metadata").isObject())
        e << QStringLiteral("缺少 metadata 对象");

    const QJsonObject md = root.value("metadata").toObject();

    if (!md.contains("schema_version"))
        e << QStringLiteral("metadata.schema_version 缺失");
    else if (!md.value("schema_version").isDouble())
        e << QStringLiteral("metadata.schema_version 必须是整数");
    else if (md.value("schema_version").toInt() != kSchemaVersion)
        e << QStringLiteral("metadata.schema_version 必须是 %1（当前 %2）")
             .arg(kSchemaVersion).arg(md.value("schema_version").toInt());

    for (const QString &k : {QStringLiteral("id"), QStringLiteral("version"),
                             QStringLiteral("name"), QStringLiteral("source")}) {
        if (!md.contains(k))
            e << QStringLiteral("metadata.%1 缺失").arg(k);
    }
    if (md.contains("version") && !md.value("version").isDouble())
        e << QStringLiteral("metadata.version 必须是整数");
    if (md.contains("source") && !md.value("source").isDouble())
        e << QStringLiteral("metadata.source 必须是整数（0=Formlabs, 1=自定义）");

    if (!md.contains("base_material_identifier") || !md.value("base_material_identifier").isObject()) {
        e << QStringLiteral("metadata.base_material_identifier 缺失或不是对象");
    } else {
        const QJsonObject bmi = md.value("base_material_identifier").toObject();
        for (const QString &k : {QStringLiteral("machine_type_id"),
                                 QStringLiteral("versioned_material_code"),
                                 QStringLiteral("layer_thickness_mm")}) {
            if (!bmi.contains(k))
                e << QStringLiteral("metadata.base_material_identifier.%1 缺失").arg(k);
        }
    }

    if (root.contains("public_fields") && !root.value("public_fields").isObject()
        && !root.value("public_fields").isNull()) {
        e << QStringLiteral("public_fields 必须是对象或 null");
    }

    const QJsonValue um = root.value("underlying_material");
    if (!um.isString() || um.toString().size() < 100)
        e << QStringLiteral("underlying_material 必须是较长的加密字符串（当前 %1 字符）")
             .arg(um.isString() ? um.toString().size() : 0);

    if (root.contains("algorithm_profiles") && !root.value("algorithm_profiles").isArray())
        e << QStringLiteral("algorithm_profiles 必须是数组");

    return e;
}

QString FpsDocument::machineDisplayName(const QString &id)
{
    static const QHash<QString, QString> m = {
        {QStringLiteral("FORM-4-0"), QStringLiteral("Form 4")},
        {QStringLiteral("FRML-4-0"), QStringLiteral("Form 4L")},
        {QStringLiteral("CLRK-1-2"), QStringLiteral("Form Studio 1")},
        {QStringLiteral("FS30-1-0"), QStringLiteral("Form 3 / 3B")},
        {QStringLiteral("PILK-1-0"), QStringLiteral("Form 3L")},
        {QStringLiteral("DGJR-1-0"), QStringLiteral("Form 3 (Daguerre Jr)")},
        {QStringLiteral("DGSR-1-0"), QStringLiteral("Form 3L (Daguerre Sr)")},
        {QStringLiteral("FUSX-1-0"), QStringLiteral("Fuse X1 (SLS)")},
    };
    return m.value(id, id);
}

FpsDocument::Summary FpsDocument::summary() const
{
    Summary s;
    const QJsonObject md = metadata();
    const QJsonObject bmi = baseMaterialIdentifier();
    s.machineId = bmi.value("machine_type_id").toString();
    s.machineName = machineDisplayName(s.machineId);
    s.materialCode = bmi.value("versioned_material_code").toString();
    s.thickness = bmi.value("layer_thickness_mm");
    s.name = md.value("name").toString();
    s.schemaVersion = md.value("schema_version").toInt(-1);
    s.source = md.value("source").toInt(-1);
    s.hasPublicFields = !root().value("public_fields").isNull()
                        && root().contains("public_fields");
    s.cipherChars = underlyingMaterial().size();
    s.cipherBytes = QByteArray::fromBase64(underlyingMaterial().toLatin1()).size();
    s.algorithmProfiles = root().value("algorithm_profiles").toArray().size();
    return s;
}

void FpsDocument::makeCustom()
{
    QJsonObject md = metadata();
    md.insert("source", 1);
    QString name = md.value("name").toString();
    if (name.isEmpty()) name = QStringLiteral("Default");
    if (!name.contains(QStringLiteral("(Custom)")))
        name += QStringLiteral(" (Custom)");
    md.insert("name", name);
    md.insert("id", QStringLiteral("{%1}").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces)));
    const QString now = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzz"));
    md.insert("last_modified", now);
    if (!md.contains("created")) md.insert("created", now);
    m_root.insert("metadata", md);
    m_dirty = true;
}

QString FpsDocument::typeName(const QJsonValue &v)
{
    switch (v.type()) {
    case QJsonValue::Bool:   return QStringLiteral("布尔");
    case QJsonValue::Double: {
        const double d = v.toDouble();
        if (std::floor(d) == d && std::abs(d) < 1e15) return QStringLiteral("整数");
        return QStringLiteral("小数");
    }
    case QJsonValue::String: return QStringLiteral("字符串");
    case QJsonValue::Object: return QStringLiteral("对象");
    case QJsonValue::Array:  return QStringLiteral("数组");
    case QJsonValue::Null:   return QStringLiteral("null");
    default: return QStringLiteral("未知");
    }
}

QString FpsDocument::formatScalar(const QJsonValue &v)
{
    switch (v.type()) {
    case QJsonValue::Bool:   return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    case QJsonValue::Null:   return QStringLiteral("null");
    case QJsonValue::Double: {
        const double d = v.toDouble();
        if (std::floor(d) == d && std::abs(d) < 1e15)
            return QString::number(static_cast<qlonglong>(d));
        // 最短可往返表示：从低精度试起，直到解析回来完全相等
        for (int prec = 1; prec <= 17; ++prec) {
            const QString s = QString::number(d, 'g', prec);
            bool ok = false;
            if (s.toDouble(&ok) == d && ok)
                return s;
        }
        return QString::number(d, 'g', 17);
    }
    case QJsonValue::String: return v.toString();
    case QJsonValue::Object: return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact)).left(200);
    case QJsonValue::Array:  return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact)).left(200);
    default: return QString();
    }
}

bool FpsDocument::parseScalar(const QString &text, const QJsonValue &like, QJsonValue *out, QString *error)
{
    const QString t = text.trimmed();
    switch (like.type()) {
    case QJsonValue::Bool: {
        const QString low = t.toLower();
        if (low == "true" || low == "1" || low == "yes" || low == "y") { *out = QJsonValue(true); return true; }
        if (low == "false" || low == "0" || low == "no" || low == "n") { *out = QJsonValue(false); return true; }
        if (error) *error = QStringLiteral("需要布尔值 true / false");
        return false;
    }
    case QJsonValue::Double: {
        bool ok = false;
        double d = 0.0;
        if (t.startsWith("0x", Qt::CaseInsensitive)) d = static_cast<double>(t.mid(2).toLongLong(&ok, 16));
        else d = t.toDouble(&ok);
        if (!ok) { if (error) *error = QStringLiteral("需要数字"); return false; }
        *out = QJsonValue(d);
        return true;
    }
    case QJsonValue::String:
        *out = QJsonValue(t);
        return true;
    case QJsonValue::Null: {
        if (t.isEmpty() || t == "null") { *out = QJsonValue(QJsonValue::Null); return true; }
        QJsonParseError pe{};
        const QJsonDocument d = QJsonDocument::fromJson(t.toUtf8(), &pe);
        if (pe.error == QJsonParseError::NoError && d.isObject()) { *out = QJsonValue(d.object()); return true; }
        *out = QJsonValue(t);
        return true;
    }
    default:
        *out = QJsonValue(t);
        return true;
    }
}

// 追加到 fpsdocument.cpp 的路径读写实现
QJsonValue FpsDocument::valueAt(const QVector<QJsonValue> &path, bool *ok) const
{
    if (ok) *ok = false;
    QJsonValue cur = QJsonValue(m_root);
    for (const QJsonValue &p : path) {
        if (cur.isObject()) {
            if (!p.isString()) return QJsonValue();
            const QJsonObject o = cur.toObject();
            if (!o.contains(p.toString())) return QJsonValue();
            cur = o.value(p.toString());
        } else if (cur.isArray()) {
            const QJsonArray a = cur.toArray();
            const int i = p.toInt(-1);
            if (i < 0 || i >= a.size()) return QJsonValue();
            cur = a.at(i);
        } else {
            return QJsonValue();
        }
    }
    if (ok) *ok = true;
    return cur;
}

bool FpsDocument::setValueAt(const QVector<QJsonValue> &path, const QJsonValue &v, QString *error)
{
    if (path.isEmpty()) { if (error) *error = QStringLiteral("空路径"); return false; }

    std::function<QJsonValue(const QJsonValue &, int)> rebuild =
        [&](const QJsonValue &node, int idx) -> QJsonValue {
            if (idx == path.size()) return v;
            const QJsonValue key = path[idx];
            if (node.isObject()) {
                QJsonObject o = node.toObject();
                const QString k = key.toString();
                o.insert(k, rebuild(o.value(k), idx + 1));
                return QJsonValue(o);
            }
            if (node.isArray()) {
                QJsonArray a = node.toArray();
                const int i = key.toInt(-1);
                if (i < 0 || i >= a.size()) return node;
                a.replace(i, rebuild(a.at(i), idx + 1));
                return QJsonValue(a);
            }
            return node;
        };

    const QJsonValue nr = rebuild(QJsonValue(m_root), 0);
    if (!nr.isObject()) { if (error) *error = QStringLiteral("结果不是对象"); return false; }
    m_root = nr.toObject();
    m_dirty = true;
    return true;
}
