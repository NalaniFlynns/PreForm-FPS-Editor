#ifndef FPSDOCUMENT_H
#define FPSDOCUMENT_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

// 一份 .fps 打印设置文件的内存模型
class FpsDocument
{
public:
    FpsDocument() = default;

    bool load(const QString &path, QString *error);
    bool save(const QString &path, QString *error);
    bool save(QString *error) { return save(m_path, error); }

    bool isOpen() const { return !m_root.isEmpty(); }
    QString path() const { return m_path; }
    QString fileName() const;
    bool isDirty() const { return m_dirty; }
    void setDirty(bool d) { m_dirty = d; }

    QJsonObject root() const { return m_root; }
    QJsonObject metadata() const { return m_root.value(QStringLiteral("metadata")).toObject(); }
    QJsonObject baseMaterialIdentifier() const {
        return metadata().value(QStringLiteral("base_material_identifier")).toObject();
    }
    QJsonValue publicFields() const { return m_root.value(QStringLiteral("public_fields")); }
    QString underlyingMaterial() const { return m_root.value(QStringLiteral("underlying_material")).toString(); }

    // 按路径读/写（路径元素：QString=对象键，int=数组下标）
    QJsonValue valueAt(const QVector<QJsonValue> &path, bool *ok) const;
    bool setValueAt(const QVector<QJsonValue> &path, const QJsonValue &v, QString *error);
    void replaceRoot(const QJsonObject &root) { m_root = root; m_dirty = true; }

    static QStringList validate(const QJsonObject &root);

    struct Summary {
        QString machineId, machineName, materialCode, name;
        QJsonValue thickness;
        int schemaVersion = -1;
        int source = -1;
        bool hasPublicFields = false;
        int cipherChars = 0;
        int cipherBytes = 0;
        int algorithmProfiles = 0;
    };
    Summary summary() const;

    void makeCustom();

    static QString machineDisplayName(const QString &id);
    static QString typeName(const QJsonValue &v);
    static QString formatScalar(const QJsonValue &v);
    static bool parseScalar(const QString &text, const QJsonValue &like, QJsonValue *out, QString *error);

private:
    QJsonObject m_root;
    QString m_path;
    bool m_dirty = false;
};

#endif // FPSDOCUMENT_H
