#ifndef KNOBLIBRARY_H
#define KNOBLIBRARY_H

#include <QString>
#include <QVector>
#include <QJsonObject>

// 一份材料的解密参数（来自 outputs/knob-library）
struct KnobParam {
    QString categoryKey;
    QString categoryName;
    QString key;
    QString name;      // 可能为空（该键的真名未能解出）
    QJsonValue value;
};

class KnobLibrary
{
public:
    explicit KnobLibrary(const QString &dir = QString());

    void setDir(const QString &dir) { m_dir = dir; }
    QString dir() const { return m_dir; }
    bool isAvailable() const;

    // 按 机型/材料码/层厚 查找；找到返回 true
    bool find(const QString &machineType, const QString &materialCode,
              double layerThicknessMm, QVector<KnobParam> *out,
              int *schemaVersion = nullptr) const;

    // 列出素材库中所有材料（用于浏览）
    struct Entry { QString machine, material; double thickness; int schema; int params; int named; };
    QVector<Entry> entries() const;

    static QString defaultDir();

private:
    QString m_dir;
};

#endif // KNOBLIBRARY_H
