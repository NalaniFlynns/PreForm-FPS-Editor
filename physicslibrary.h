#ifndef PHYSICSLIBRARY_H
#define PHYSICSLIBRARY_H

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

// 一条物理参数记录（来自 decoded_physics_complete.csv，真实解密值）
struct PhysicsRecord {
    QString machine;
    QString material;
    double thickness = 0.0;
    double iNorm = 0.0;      // 标准层光强 mW/cm^2
    double eNorm = 0.0;      // 标准层曝光能量 mJ/cm^2
    double tNormal = 0.0;    // 主曝光秒数 = E/I
    double iEarly = 0.0;
    double eEarlyFirst = 0.0;
    double tEarly = 0.0;
    double tStart = 0.0;
    double tPrint = 0.0;
    double tMin = 0.0;
    QString film;
    int nETrans = 0;
    QJsonArray eTrans, tWait, aBp, areaWait, zStage, v1, v2, v3;
    double pixX = 0.0, pixY = 0.0;
};

// 转换结果（F4 -> ST1 目标物理参数）
struct ConvertedRecord {
    QString material;
    double thickness = 0.0;
    double f4I = 0.0, st1I = 0.0;
    double f4E = 0.0, st1E = 0.0;
    double kE = 0.0;
    QString kESource;
    double f4TNorm = 0.0, st1TNorm = 0.0;
    double f4EEarly = 0.0, st1EEarly = 0.0;
    double f4TEarly = 0.0, st1TEarly = 0.0;
    double tStart = 0.0, tPrint = 0.0, tMin = 0.0;
    QString f4Film;
};

// 物理参数库：加载 outputs/conversion 的解码结果，提供按机型/材料查询与 F4->ST1 转换
class PhysicsLibrary
{
public:
    explicit PhysicsLibrary(const QString &dir = QString());

    void setDir(const QString &dir) { m_dir = dir; }
    QString dir() const { return m_dir; }
    bool isAvailable() const;
    bool load(QString *error = nullptr);

    // 查询
    QVector<PhysicsRecord> all() const { return m_records; }
    QVector<PhysicsRecord> byMachine(const QString &machine) const;
    QVector<PhysicsRecord> byMaterial(const QString &machine, const QString &material) const;
    bool find(const QString &machine, const QString &material, double thickness, PhysicsRecord *out) const;

    QVector<ConvertedRecord> converted() const { return m_converted; }
    bool findConverted(const QString &material, double thickness, ConvertedRecord *out) const;

    // 分类展示用的材料清单（machine -> sorted materials）
    QStringList materials(const QString &machine) const;

    static QString defaultDir();
    static QString machineDisplayName(const QString &id);

private:
    QString m_dir;
    QVector<PhysicsRecord> m_records;
    QVector<ConvertedRecord> m_converted;
};

#endif // PHYSICSLIBRARY_H
