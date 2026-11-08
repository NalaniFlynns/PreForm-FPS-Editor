#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>
#include <QElapsedTimer>
#include "fpsdocument.h"
#include "mainwindow.h"
#include "physicslibrary.h"
#include "fpsextractor.h"

static int selfTest(const QString &srcPath, const QString &outPath)
{
    QTextStream out(stdout);
    int fails = 0;

    auto check = [&](bool ok, const QString &what) {
        out << (ok ? "[ OK ] " : "[FAIL] ") << what << "\n";
        if (!ok) ++fails;
    };

    FpsDocument doc;
    QString err;
    check(doc.load(srcPath, &err), QStringLiteral("load: ") + srcPath);
    if (!err.isEmpty()) out << "        err=" << err << "\n";
    if (!doc.isOpen()) { out.flush(); return 1; }

    const auto s0 = doc.summary();
    out << "        machine=" << s0.machineId << " material=" << s0.materialCode
        << " thickness=" << FpsDocument::formatScalar(s0.thickness)
        << " cipherChars=" << s0.cipherChars << "\n";
    check(s0.schemaVersion == 3, QStringLiteral("schema_version == 3"));
    check(s0.cipherChars > 1000, QStringLiteral("underlying_material present"));

    check(FpsDocument::validate(doc.root()).isEmpty(), QStringLiteral("validate(original) empty"));

    // ---- 编辑：source -> 1，name 追加标记，layer_thickness 不变
    QVector<QJsonValue> pSrc{QJsonValue(QStringLiteral("metadata")), QJsonValue(QStringLiteral("source"))};
    QVector<QJsonValue> pName{QJsonValue(QStringLiteral("metadata")), QJsonValue(QStringLiteral("name"))};
    check(doc.setValueAt(pSrc, QJsonValue(1), &err), QStringLiteral("set metadata.source=1"));
    check(doc.setValueAt(pName, QJsonValue(QStringLiteral("SELFTEST")), &err),
          QStringLiteral("set metadata.name=SELFTEST"));

    bool ok = false;
    check(doc.valueAt(pSrc, &ok).toInt() == 1 && ok, QStringLiteral("readback source==1"));
    check(doc.isDirty(), QStringLiteral("dirty flag set"));

    check(doc.save(outPath, &err), QStringLiteral("save: ") + outPath);
    if (!err.isEmpty()) out << "        err=" << err << "\n";

    // ---- 重新加载校验
    FpsDocument re;
    check(re.load(outPath, &err), QStringLiteral("reload saved file"));
    const auto s1 = re.summary();
    check(s1.source == 1, QStringLiteral("saved source == 1"));
    check(s1.name == QStringLiteral("SELFTEST"), QStringLiteral("saved name == SELFTEST"));
    check(s1.cipherChars == s0.cipherChars, QStringLiteral("cipher untouched (same length)"));
    check(s1.materialCode == s0.materialCode && s1.machineId == s0.machineId,
          QStringLiteral("material/machine preserved"));
    check(FpsDocument::validate(re.root()).isEmpty(), QStringLiteral("validate(saved) empty"));

    // ---- 非法值必须被拒
    QVector<QJsonValue> pSchema{QJsonValue(QStringLiteral("metadata")), QJsonValue(QStringLiteral("schema_version"))};
    FpsDocument bad;
    bad.load(srcPath, &err);
    bad.setValueAt(pSchema, QJsonValue(7), &err);
    check(!FpsDocument::validate(bad.root()).isEmpty(),
          QStringLiteral("validate rejects schema_version=7"));

    // ---- 数字格式化
    check(FpsDocument::formatScalar(QJsonValue(0.03)) == QStringLiteral("0.03"),
          QStringLiteral("formatScalar(0.03) == \"0.03\""));
    check(FpsDocument::formatScalar(QJsonValue(0.1)) == QStringLiteral("0.1"),
          QStringLiteral("formatScalar(0.1) == \"0.1\""));
    check(FpsDocument::formatScalar(QJsonValue(3)) == QStringLiteral("3"),
          QStringLiteral("formatScalar(3) == \"3\""));

    out << "\n" << (fails == 0 ? "ALL PASSED" : QStringLiteral("%1 FAILED").arg(fails)) << "\n";
    out.flush();
    return fails == 0 ? 0 : 2;
}

static int physSelfTest()
{
    QTextStream out(stdout);
    int fails = 0;
    auto check = [&](bool ok, const QString &what) {
        out << (ok ? "[ OK ] " : "[FAIL] ") << what << "\n";
        if (!ok) ++fails;
    };
    PhysicsLibrary lib;
    QString err;
    const bool ok = lib.load(&err);
    check(ok, QStringLiteral("PhysicsLibrary::load ") + (err.isEmpty() ? QString() : err));
    if (!ok) { out.flush(); return 1; }

    auto recs = lib.all();
    check(recs.size() == 108, QStringLiteral("总记录 108 条 (实际 %1)").arg(recs.size()));
    check(lib.materials(QStringLiteral("FORM-4-0")).size() == 52, QStringLiteral("F4 材料 52 种"));
    check(lib.materials(QStringLiteral("CLRK-1-2")).size() == 10, QStringLiteral("ST1 材料 10 种"));

    // 三锚点验证
    ConvertedRecord c;
    bool a1 = lib.findConverted(QStringLiteral("FLFL8002"), 0.1, &c);
    check(a1 && std::abs(c.st1E - 71.0) < 1e-6 && std::abs(c.st1TNorm - 11.8333) < 1e-3,
          QStringLiteral("Flex80A 转换 E=71 t=11.833"));
    bool a2 = lib.findConverted(QStringLiteral("FLTO1502"), 0.1, &c);
    check(a2 && std::abs(c.st1E - 42.0) < 1e-6 && std::abs(c.st1TNorm - 6.6667) < 1e-3,
          QStringLiteral("Tough1500 转换 E=42 t=6.667"));
    bool a3 = lib.findConverted(QStringLiteral("FLTCLV01"), 0.05, &c);
    check(a3 && std::abs(c.st1E - 16.0) < 1e-6 && std::abs(c.st1TNorm - 3.2) < 1e-3,
          QStringLiteral("ClearCast 转换 E=16 t=3.2"));

    // 原始物理记录查询
    PhysicsRecord r;
    bool q = lib.find(QStringLiteral("FORM-4-0"), QStringLiteral("FLTO1502"), 0.1, &r);
    check(q && std::abs(r.iNorm - 13.0) < 1e-9 && std::abs(r.eNorm - 53.0) < 1e-9,
          QStringLiteral("F4 Tough1500 原始 I=13 E=53"));

    out << "\n" << (fails == 0 ? "PHYS ALL PASSED" : QStringLiteral("%1 FAILED").arg(fails)) << "\n";
    out.flush();
    return fails == 0 ? 0 : 2;
}

static int extractSelfTest(const QString &exePath)
{
    QTextStream out(stdout);
    int fails = 0;
    auto check = [&](bool ok, const QString &what) {
        out << (ok ? "[ OK ] " : "[FAIL] ") << what << "\n";
        if (!ok) ++fails;
    };
    out << "自动定位: " << FpsExtractor::autoLocate() << "\n";
    check(!exePath.isEmpty(), QStringLiteral("提供 PreForm.exe 路径"));
    QVector<ExtractedFps> items;
    QString err;
    QElapsedTimer t; t.start();
    const bool ok = FpsExtractor::extract(exePath, &items, &err);
    check(ok, QStringLiteral("extract ") + (err.isEmpty()?QString():err));
    if (!ok) { out.flush(); return 1; }
    out << "        耗时 " << t.elapsed() << " ms, 提取 " << items.size() << " 份（已按内容去重）\n";
    check(items.size() >= 220, QStringLiteral("至少 220 份内置材料 (实际 %1)").arg(items.size()));
    QMap<QString,int> byMach;
    for (const auto &it : items) byMach[it.machine]++;
    QString line;
    for (auto it = byMach.begin(); it != byMach.end(); ++it)
        line += QStringLiteral("%1:%2 ").arg(it.key()).arg(it.value());
    out << "        机型分布: " << line << "\n";
    check(byMach.contains(QStringLiteral("FORM-4-0")), QStringLiteral("含 Form 4 材料"));
    check(byMach.contains(QStringLiteral("CLRK-1-2")), QStringLiteral("含 Form Studio 1 材料"));
    // 抽验一份内容可被 FpsDocument 解析
    int parsedOk = 0;
    for (const auto &it : items) {
        QJsonParseError pe{}; auto d=QJsonDocument::fromJson(it.raw,&pe);
        if (pe.error==QJsonParseError::NoError && d.isObject() && d.object().contains("underlying_material")) ++parsedOk;
    }
    check(parsedOk==items.size(), QStringLiteral("全部可解析且含 underlying_material (%1/%2)").arg(parsedOk).arg(items.size()));
    out << "\n" << (fails==0?"EXTRACT ALL PASSED":QStringLiteral("%1 FAILED").arg(fails)) << "\n";
    out.flush();
    return fails==0?0:2;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("PreFormFpsEditor"));

    QStringList args = app.arguments();
    if (args.contains(QStringLiteral("--physselftest"))) {
        return physSelfTest();
    }
    if (args.contains(QStringLiteral("--extractselftest"))) {
        const int i = args.indexOf(QStringLiteral("--extractselftest"));
        QString exe = (i + 1 < args.size() && !args.at(i + 1).startsWith(QStringLiteral("--"))) ? args.at(i + 1) : QString();
        if (exe.isEmpty()) exe = FpsExtractor::autoLocate();
        return extractSelfTest(exe);
    }
    if (args.contains(QStringLiteral("--selftest"))) {
        const int i = args.indexOf(QStringLiteral("--selftest"));
        const QString src = (i + 1 < args.size()) ? args.at(i + 1) : QString();
        const QString dst = (i + 2 < args.size()) ? args.at(i + 2) : QString();
        if (src.isEmpty()) { QTextStream(stdout) << "usage: --selftest <src.fps> [out.fps]\n"; return 1; }
        const QString outPath = dst.isEmpty() ? QDir::temp().filePath(QStringLiteral("fps_selftest_out.fps")) : dst;
        return selfTest(src, outPath);
    }

    MainWindow w;
    if (args.size() > 2)
        w.setLibraryDir(args.at(2));
    if (args.size() > 1 && !args.at(1).startsWith(QStringLiteral("--")))
        w.openFile(args.at(1));
    w.show();
    return app.exec();
}
