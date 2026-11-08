#ifndef FPSEXTRACTOR_H
#define FPSEXTRACTOR_H

#include <QString>
#include <QStringList>
#include <QVector>

// 从 PreForm.exe 中提取内嵌的 .fps 打印设置（明文 JSON 对象），并按内容自动去重。
struct ExtractedFps {
    QByteArray raw;          // 原始 JSON 字节
    QString machine;         // machine_type_id
    QString material;        // versioned_material_code
    double thickness = 0.0;  // layer_thickness_mm
    QString name;            // name
    QString sha256;          // 原始字节 sha256，用于去重
    qint64 fileOffset = -1;  // 在 exe 中的偏移
};

class FpsExtractor
{
public:
    // 自动定位 PreForm.exe（Program Files、注册表、运行中的进程、常见路径）
    static QStringList candidatePreFormPaths();
    static QString autoLocate(QString *howFound = nullptr);

    // 从指定 exe 提取所有内嵌 fps；解析失败/去重规则内部处理
    static bool extract(const QString &exePath, QVector<ExtractedFps> *out, QString *error);

    // 把提取结果写入目录（文件名 machine__material__thickmm__name.fps）
    static bool saveAll(const QVector<ExtractedFps> &items, const QString &outDir,
                        QStringList *writtenFiles, QString *error);

    static QString shortHash(const QByteArray &data);
};

#endif // FPSEXTRACTOR_H
