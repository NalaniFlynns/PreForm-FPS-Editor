#include "physicslibrary.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QCoreApplication>
#include <QStringList>
#include <algorithm>

namespace {
// 简易 CSV 行解析（支持引号包裹、双引号转义）
QStringList parseCsvLine(const QString &line)
{
    QStringList out;
    QString cur;
    bool inQuotes = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (inQuotes) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                    cur.append(QLatin1Char('"')); ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                cur.append(c);
            }
        } else {
            if (c == QLatin1Char('"')) inQuotes = true;
            else if (c == QLatin1Char(',')) { out << cur; cur.clear(); }
            else cur.append(c);
        }
    }
    out << cur;
    return out;
}

QJsonArray parseJsonArrayField(const QString &s)
{
    QJsonParseError pe{};
    const QJsonDocument d = QJsonDocument::fromJson(s.toUtf8(), &pe);
    if (pe.error == QJsonParseError::NoError && d.isArray())
        return d.array();
    return QJsonArray();
}

double toD(const QString &s) { bool ok=false; double v=s.toDouble(&ok); return ok?v:0.0; }
} // namespace

PhysicsLibrary::PhysicsLibrary(const QString &dir) : m_dir(dir.isEmpty() ? defaultDir() : dir) {}

QString PhysicsLibrary::defaultDir()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList cands = {
        appDir + QStringLiteral("/physics-library"),
        appDir + QStringLiteral("/../../outputs/PreFormFpsEditor/physics-library"),
        appDir + QStringLiteral("/../../../outputs/PreFormFpsEditor/physics-library"),
        QStringLiteral("C:/Users/Nalani/Documents/Codex/2026-10-15/new-chat/outputs/PreFormFpsEditor/physics-library"),
        appDir + QStringLiteral("/../../../outputs/conversion"),
    };
    for (const QString &c : cands) {
        if (QFile::exists(c + QStringLiteral("/decoded_physics_complete.csv")))
            return QDir(c).absolutePath();
    }
    return appDir + QStringLiteral("/physics-library");
}

bool PhysicsLibrary::isAvailable() const
{
    return QFile::exists(QDir(m_dir).filePath(QStringLiteral("decoded_physics_complete.csv")));
}

bool PhysicsLibrary::load(QString *error)
{
    m_records.clear();
    m_converted.clear();

    // ---- decoded_physics_complete.csv ----
    QFile f(QDir(m_dir).filePath(QStringLiteral("decoded_physics_complete.csv")));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("无法打开 decoded_physics_complete.csv：%1").arg(f.errorString());
        return false;
    }
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    const QString headerLine = ts.readLine();
    const QStringList header = parseCsvLine(headerLine);
    QHash<QString,int> col;
    for (int i = 0; i < header.size(); ++i) col.insert(header.at(i).trimmed(), i);
    auto at = [&](const QStringList &row, const QString &name) -> QString {
        const int i = col.value(name, -1);
        return (i >= 0 && i < row.size()) ? row.at(i) : QString();
    };
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        if (line.trimmed().isEmpty()) continue;
        const QStringList row = parseCsvLine(line);
        PhysicsRecord r;
        r.machine = at(row, "machine");
        r.material = at(row, "material");
        r.thickness = toD(at(row, "layer_thickness_mm"));
        r.iNorm = toD(at(row, "I_norm_mWcm2"));
        r.eNorm = toD(at(row, "E_norm_mJcm2"));
        r.tNormal = toD(at(row, "t_normal_exp_s"));
        r.iEarly = toD(at(row, "I_early_mWcm2"));
        r.eEarlyFirst = toD(at(row, "E_early_first_mJcm2"));
        r.tEarly = toD(at(row, "t_early_exp_s"));
        r.tStart = toD(at(row, "T_start_C"));
        r.tPrint = toD(at(row, "T_print_C"));
        r.tMin = toD(at(row, "T_min_C"));
        r.film = at(row, "film");
        r.eTrans = parseJsonArrayField(at(row, "E_trans"));
        r.nETrans = r.eTrans.size();
        r.tWait = parseJsonArrayField(at(row, "t_wait"));
        r.aBp = parseJsonArrayField(at(row, "A_bp"));
        r.areaWait = parseJsonArrayField(at(row, "area_wait"));
        r.zStage = parseJsonArrayField(at(row, "Z_stage"));
        r.v1 = parseJsonArrayField(at(row, "v1"));
        r.v2 = parseJsonArrayField(at(row, "v2"));
        r.v3 = parseJsonArrayField(at(row, "v3"));
        r.pixX = toD(at(row, "pix_x_mm"));
        r.pixY = toD(at(row, "pix_y_mm"));
        m_records.push_back(r);
    }
    f.close();

    // ---- f4_to_st1_converted_physics.csv ----
    QFile cf(QDir(m_dir).filePath(QStringLiteral("f4_to_st1_converted_physics.csv")));
    if (cf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream cts(&cf);
        cts.setEncoding(QStringConverter::Utf8);
        const QStringList ch = parseCsvLine(cts.readLine());
        QHash<QString,int> cc;
        for (int i = 0; i < ch.size(); ++i) cc.insert(ch.at(i).trimmed(), i);
        auto cat = [&](const QStringList &row, const QString &name) -> QString {
            const int i = cc.value(name, -1);
            return (i >= 0 && i < row.size()) ? row.at(i) : QString();
        };
        while (!cts.atEnd()) {
            const QString line = cts.readLine();
            if (line.trimmed().isEmpty()) continue;
            const QStringList row = parseCsvLine(line);
            ConvertedRecord c;
            c.material = cat(row, "material");
            c.thickness = toD(cat(row, "layer_thickness_mm"));
            c.f4I = toD(cat(row, "f4_I_norm"));
            c.st1I = toD(cat(row, "st1_I_norm"));
            c.f4E = toD(cat(row, "f4_E_norm"));
            c.st1E = toD(cat(row, "st1_E_norm"));
            c.kE = toD(cat(row, "k_E"));
            c.kESource = cat(row, "k_E_source");
            c.f4TNorm = toD(cat(row, "f4_t_normal_s"));
            c.st1TNorm = toD(cat(row, "st1_t_normal_s"));
            c.f4EEarly = toD(cat(row, "f4_E_early_first"));
            c.st1EEarly = toD(cat(row, "st1_E_early_first(keep)"));
            c.f4TEarly = toD(cat(row, "f4_t_early_s"));
            c.st1TEarly = toD(cat(row, "st1_t_early_s"));
            c.tStart = toD(cat(row, "T_start"));
            c.tPrint = toD(cat(row, "T_print"));
            c.tMin = toD(cat(row, "T_min"));
            c.f4Film = cat(row, "f4_film");
            m_converted.push_back(c);
        }
        cf.close();
    }
    return !m_records.isEmpty();
}

QVector<PhysicsRecord> PhysicsLibrary::byMachine(const QString &machine) const
{
    QVector<PhysicsRecord> out;
    for (const auto &r : m_records) if (r.machine == machine) out.push_back(r);
    return out;
}

QVector<PhysicsRecord> PhysicsLibrary::byMaterial(const QString &machine, const QString &material) const
{
    QVector<PhysicsRecord> out;
    for (const auto &r : m_records)
        if (r.machine == machine && r.material == material) out.push_back(r);
    std::sort(out.begin(), out.end(), [](const PhysicsRecord &a, const PhysicsRecord &b){
        return a.thickness < b.thickness;
    });
    return out;
}

bool PhysicsLibrary::find(const QString &machine, const QString &material, double thickness,
                          PhysicsRecord *out) const
{
    for (const auto &r : m_records) {
        if (r.machine == machine && r.material == material
            && std::abs(r.thickness - thickness) < 1e-6) {
            if (out) *out = r;
            return true;
        }
    }
    return false;
}

bool PhysicsLibrary::findConverted(const QString &material, double thickness, ConvertedRecord *out) const
{
    for (const auto &c : m_converted) {
        if (c.material == material && std::abs(c.thickness - thickness) < 1e-6) {
            if (out) *out = c;
            return true;
        }
    }
    return false;
}

QStringList PhysicsLibrary::materials(const QString &machine) const
{
    QStringList out;
    for (const auto &r : m_records)
        if (r.machine == machine && !out.contains(r.material)) out << r.material;
    out.sort();
    return out;
}

QString PhysicsLibrary::machineDisplayName(const QString &id)
{
    if (id == QStringLiteral("FORM-4-0")) return QStringLiteral("Form 4");
    if (id == QStringLiteral("CLRK-1-2")) return QStringLiteral("Form Studio 1");
    if (id == QStringLiteral("FRML-4-0")) return QStringLiteral("Form 4L");
    return id;
}
