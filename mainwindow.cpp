#include "mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QCheckBox>
#include <QScrollArea>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QSettings>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <cmath>
#include <QFont>
#include <QColor>
#include <QBrush>
#include <QTreeWidgetItemIterator>
#include <QPlainTextEdit>
#include <QHash>
#include <algorithm>
#include <functional>
#include <QFileDialog>
#include <QTextStream>
#include <QHeaderView>

namespace {
// 默认素材库：<exe>/fps-library 或 ../../outputs/fps-library
QString defaultLibraryDir()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList cands = {
        appDir + QStringLiteral("/fps-library"),
        appDir + QStringLiteral("/../../outputs/fps-library"),
        appDir + QStringLiteral("/../../../outputs/fps-library"),
        QStringLiteral("C:/Users/Nalani/Documents/Codex/2026-10-15/new-chat/outputs/fps-library"),
    };
    for (const QString &c : cands) {
        if (QFile::exists(c + QStringLiteral("/index.json")))
            return QDir(c).absolutePath();
    }
    return appDir + QStringLiteral("/fps-library");
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_libraryDir = defaultLibraryDir();
    m_knobLib.setDir(KnobLibrary::defaultDir());
    m_physLib.setDir(PhysicsLibrary::defaultDir());
    QString physErr;
    m_physLib.load(&physErr);
    buildUi();
    buildMenus();
    refreshLibrary();
}

void MainWindow::setLibraryDir(const QString &dir)
{
    m_libraryDir = dir;
    refreshLibrary();
}

// ------------------------------------------------------------------ UI
void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("PreForm FPS 打印设置编辑器"));
    resize(1360, 860);

    auto *central = new QWidget(this);
    auto *rootLay = new QVBoxLayout(central);
    rootLay->setContentsMargins(10, 8, 10, 8);

    // 顶部：文件名 + 状态
    auto *topBar = new QHBoxLayout;
    m_fileLabel = new QLabel(QStringLiteral("（未打开文件）"), this);
    QFont f = m_fileLabel->font();
    f.setPointSize(13);
    f.setBold(true);
    m_fileLabel->setFont(f);
    m_stateLabel = new QLabel(QString(), this);
    m_stateLabel->setStyleSheet(QStringLiteral("color:#666;"));
    topBar->addWidget(m_fileLabel);
    topBar->addStretch();
    topBar->addWidget(m_stateLabel);
    rootLay->addLayout(topBar);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    // ---------------- 左侧：素材库 ----------------
    auto *leftBox = new QGroupBox(QStringLiteral("素材库（PreForm 内嵌真实配置）"), this);
    auto *leftLay = new QVBoxLayout(leftBox);

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(QStringLiteral("筛选：机型 / 材料码 / 层厚…"));
    leftLay->addWidget(m_filter);

    m_library = new QTableWidget(this);
    m_library->setColumnCount(4);
    m_library->setHorizontalHeaderLabels({QStringLiteral("机型"), QStringLiteral("材料码"),
                                          QStringLiteral("层厚"), QStringLiteral("名称")});
    m_library->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_library->horizontalHeader()->setStretchLastSection(true);
    m_library->setColumnWidth(0, 120);
    m_library->setColumnWidth(1, 110);
    m_library->setColumnWidth(2, 66);
    m_library->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_library->setSelectionMode(QAbstractItemView::SingleSelection);
    m_library->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_library->verticalHeader()->setVisible(false);
    leftLay->addWidget(m_library, 1);

    auto *libBtnRow = new QHBoxLayout;
    auto *openLibBtn = new QPushButton(QStringLiteral("从素材库打开"), this);
    auto *extractBtn = new QPushButton(QStringLiteral("自动提取 PreForm…"), this);
    auto *locateBtn = new QPushButton(QStringLiteral("定位 PreForm"), this);
    m_libInfo = new QLabel(this);
    m_libInfo->setStyleSheet(QStringLiteral("color:#666;"));
    libBtnRow->addWidget(openLibBtn);
    libBtnRow->addWidget(extractBtn);
    libBtnRow->addWidget(locateBtn);
    libBtnRow->addWidget(m_libInfo, 1);
    leftLay->addLayout(libBtnRow);
    m_extractInfo = new QLabel(QStringLiteral("可从已安装的 PreForm.exe 内嵌资源中自动提取全部 .fps（自动去重）"), this);
    m_extractInfo->setWordWrap(true);
    m_extractInfo->setStyleSheet(QStringLiteral("color:#666;"));
    leftLay->addWidget(m_extractInfo);

    splitter->addWidget(leftBox);

    // ---------------- 右侧：编辑器 ----------------
    auto *rightTabs = new QTabWidget(this);
    auto *rightSplit = new QSplitter(Qt::Vertical, this);

    auto *treeBox = new QGroupBox(QStringLiteral("配置结构"), this);
    auto *treeLay = new QVBoxLayout(treeBox);
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({QStringLiteral("键"), QStringLiteral("值")});
    m_tree->setColumnWidth(0, 340);
    m_tree->setAlternatingRowColors(true);
    treeLay->addWidget(m_tree, 1);
    rightSplit->addWidget(treeBox);

    auto *editBox = new QGroupBox(QStringLiteral("编辑"), this);
    auto *editLay = new QVBoxLayout(editBox);

    auto *form = new QFormLayout;
    m_pathLabel = new QLabel(QStringLiteral("—"), this);
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pathLabel->setWordWrap(true);
    m_typeLabel = new QLabel(QStringLiteral("—"), this);
    form->addRow(QStringLiteral("路径："), m_pathLabel);
    form->addRow(QStringLiteral("类型："), m_typeLabel);
    editLay->addLayout(form);

    auto *valueRow = new QHBoxLayout;
    valueRow->addWidget(new QLabel(QStringLiteral("新值："), this));
    m_valueEdit = new QLineEdit(this);
    valueRow->addWidget(m_valueEdit, 1);
    m_applyBtn = new QPushButton(QStringLiteral("应用修改"), this);
    m_revertBtn = new QPushButton(QStringLiteral("恢复原值"), this);
    valueRow->addWidget(m_applyBtn);
    valueRow->addWidget(m_revertBtn);
    editLay->addLayout(valueRow);

    m_hintLabel = new QLabel(QString(), this);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setStyleSheet(QStringLiteral("color:#a05000;"));
    editLay->addWidget(m_hintLabel);
    editLay->addStretch();

    rightSplit->addWidget(editBox);
    rightSplit->setStretchFactor(0, 3);
    rightSplit->setStretchFactor(1, 2);
    rightTabs->addTab(rightSplit, QStringLiteral("配置结构 / 编辑"));

    // ---------------- 材料参数（解密后的默认值） ----------------
    auto *knobBox = new QWidget(this);
    auto *knobLay = new QVBoxLayout(knobBox);
    auto *knobBar = new QHBoxLayout;
    m_knobInfo = new QLabel(QStringLiteral("打开一份 .fps 后自动匹配解密参数"), this);
    m_knobInfo->setStyleSheet(QStringLiteral("color:#666;"));
    auto *knobRefreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    auto *knobCsvBtn = new QPushButton(QStringLiteral("导出 CSV…"), this);
    auto *knobApplyBtn = new QPushButton(QStringLiteral("写入 public_fields"), this);
    m_knobHint = new QLabel(QStringLiteral("提示：双击“默认值”列可编辑任意参数（含未解真名的键），再点“写入 public_fields”"), this);
    m_knobHint->setStyleSheet(QStringLiteral("color:#a05000;"));
    knobBar->addWidget(m_knobInfo, 1);
    knobBar->addWidget(m_knobHint, 1);
    knobBar->addWidget(knobRefreshBtn);
    knobBar->addWidget(knobCsvBtn);
    knobBar->addWidget(knobApplyBtn);
    knobLay->addLayout(knobBar);
    m_knobTable = new QTableWidget(this);
    m_knobTable->setColumnCount(5);
    m_knobTable->setHorizontalHeaderLabels({QStringLiteral("分类"), QStringLiteral("分类名"),
                                            QStringLiteral("参数键"), QStringLiteral("参数名"),
                                            QStringLiteral("默认值")});
    m_knobTable->horizontalHeader()->setStretchLastSection(true);
    m_knobTable->setColumnWidth(0, 150);
    m_knobTable->setColumnWidth(1, 120);
    m_knobTable->setColumnWidth(2, 230);
    m_knobTable->setColumnWidth(3, 230);
    m_knobTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    m_knobTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_knobTable->verticalHeader()->setVisible(false);
    m_knobTable->setAlternatingRowColors(true);
    knobLay->addWidget(m_knobTable, 1);
    rightTabs->addTab(knobBox, QStringLiteral("材料参数（解密默认值）"));

    // ---------------- 材料对比（两材料逐参数对比） ----------------
    auto *cmpPage = new QWidget(this);
    buildCompareTab(cmpPage);
    rightTabs->addTab(cmpPage, QStringLiteral("材料对比"));

    // ---------------- F4→ST1 参数转换 ----------------
    auto *cvtPage = new QWidget(this);
    buildConvertTab(cvtPage);
    rightTabs->addTab(cvtPage, QStringLiteral("F4→ST1 转换"));

    splitter->addWidget(rightTabs);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    rootLay->addWidget(splitter, 1);
    setCentralWidget(central);
    statusBar()->showMessage(QStringLiteral("就绪"));

    // 信号
    connect(m_filter, &QLineEdit::textChanged, this, &MainWindow::onLibraryFilterChanged);
    connect(m_library, &QTableWidget::cellDoubleClicked, this, [this](int, int) { onOpenFromLibrary(); });
    connect(m_library, &QTableWidget::itemSelectionChanged, this, [this]() {
        const int row = m_library->currentRow();
        if (row >= 0 && row < m_libraryFiles.size()) {
            auto *it0 = m_library->item(row, 0);
            auto *it1 = m_library->item(row, 1);
            auto *it2 = m_library->item(row, 2);
            if (it0 && it1 && it2)
                m_libInfo->setText(QStringLiteral("%1 · %2 · %3mm")
                                   .arg(it0->text(), it1->text(), it2->text()));
        }
    });
    connect(openLibBtn, &QPushButton::clicked, this, &MainWindow::onOpenFromLibrary);
    connect(extractBtn, &QPushButton::clicked, this, &MainWindow::onExtractFromPreForm);
    connect(locateBtn, &QPushButton::clicked, this, &MainWindow::onLocatePreForm);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onTreeSelectionChanged);
    connect(m_applyBtn, &QPushButton::clicked, this, &MainWindow::onApplyValue);
    connect(m_revertBtn, &QPushButton::clicked, this, &MainWindow::onRevertValue);
    connect(m_valueEdit, &QLineEdit::returnPressed, this, &MainWindow::onApplyValue);
    if (knobRefreshBtn) connect(knobRefreshBtn, &QPushButton::clicked, this, &MainWindow::onKnobRefresh);
    if (knobCsvBtn) connect(knobCsvBtn, &QPushButton::clicked, this, &MainWindow::onKnobExportCsv);
    if (knobApplyBtn) connect(knobApplyBtn, &QPushButton::clicked, this, &MainWindow::onKnobApplyToPublicFields);
    connect(m_knobTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onKnobTableDoubleClicked);

    m_valueEdit->setEnabled(false);
    m_applyBtn->setEnabled(false);
    m_revertBtn->setEnabled(false);
}

void MainWindow::buildMenus()
{
    QMenu *fm = menuBar()->addMenu(QStringLiteral("文件"));
    fm->addAction(QStringLiteral("打开 .fps…"), QKeySequence::Open, this, &MainWindow::onOpen);
    fm->addAction(QStringLiteral("保存"), QKeySequence::Save, this, &MainWindow::onSave);
    fm->addAction(QStringLiteral("另存为…"), QKeySequence(QStringLiteral("Ctrl+Shift+S")), this, &MainWindow::onSaveAs);
    fm->addSeparator();
    fm->addAction(QStringLiteral("自动提取 PreForm 内嵌 FPS…"), this, &MainWindow::onExtractFromPreForm);
    fm->addAction(QStringLiteral("定位 PreForm.exe…"), this, &MainWindow::onLocatePreForm);
    fm->addSeparator();
    fm->addAction(QStringLiteral("退出"), QKeySequence::Quit, this, &QWidget::close);

    QMenu *em = menuBar()->addMenu(QStringLiteral("编辑"));
    em->addAction(QStringLiteral("复制为自定义 (source=1)"), this, &MainWindow::onMakeCustom);
    em->addAction(QStringLiteral("校验当前文件"), this, &MainWindow::onValidate);
    em->addSeparator();
    em->addAction(QStringLiteral("查看原始 JSON"), this, &MainWindow::onViewJson);
    em->addAction(QStringLiteral("查看加密封包信息"), this, &MainWindow::onCipherInfo);

    QMenu *hm = menuBar()->addMenu(QStringLiteral("帮助"));
    hm->addAction(QStringLiteral("关于 / 格式说明"), this, &MainWindow::onAbout);
}

// ------------------------------------------------------------------ 素材库
void MainWindow::refreshLibrary()
{
    m_library->setRowCount(0);
    m_libraryFiles.clear();

    const QString indexPath = m_libraryDir + QStringLiteral("/index.json");
    QFile f(indexPath);
    if (!f.open(QIODevice::ReadOnly)) {
        m_libInfo->setText(QStringLiteral("未找到素材库：%1").arg(indexPath));
        statusBar()->showMessage(QStringLiteral("素材库缺失，可仅用「打开 .fps」"));
        return;
    }
    const QJsonObject idx = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    const QJsonArray profiles = idx.value(QStringLiteral("profiles")).toArray();
    struct Row { QString file, machine, material, thickness, name; double th; };
    QVector<Row> rows;
    for (const QJsonValue &v : profiles) {
        const QJsonObject o = v.toObject();
        if (!o.value(QStringLiteral("is_sla")).toBool()) continue;
        Row r;
        r.file = o.value(QStringLiteral("file")).toString();
        r.machine = o.value(QStringLiteral("machine_name")).toString();
        r.material = o.value(QStringLiteral("material_code")).toString();
        r.thickness = FpsDocument::formatScalar(o.value(QStringLiteral("layer_thickness_mm")));
        r.name = o.value(QStringLiteral("name")).toString();
        r.th = o.value(QStringLiteral("layer_thickness_mm")).toDouble();
        rows.push_back(r);
    }
    std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
        if (a.machine != b.machine) return a.machine < b.machine;
        if (a.material != b.material) return a.material < b.material;
        return a.th < b.th;
    });

    m_library->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        m_libraryFiles << rows[i].file;
        m_library->setItem(i, 0, new QTableWidgetItem(rows[i].machine));
        m_library->setItem(i, 1, new QTableWidgetItem(rows[i].material));
        m_library->setItem(i, 2, new QTableWidgetItem(rows[i].thickness));
        m_library->setItem(i, 3, new QTableWidgetItem(rows[i].name));
    }
    m_libInfo->setText(QStringLiteral("%1 份 SLA 配置 / 共 %2 份")
                       .arg(rows.size()).arg(idx.value(QStringLiteral("total")).toInt()));
    statusBar()->showMessage(QStringLiteral("素材库：%1").arg(m_libraryDir));
}

void MainWindow::onLibraryFilterChanged(const QString &text)
{
    const QString t = text.trimmed();
    for (int i = 0; i < m_library->rowCount(); ++i) {
        bool match = t.isEmpty();
        for (int c = 0; c < 4 && !match; ++c) {
            auto *it = m_library->item(i, c);
            if (it && it->text().contains(t, Qt::CaseInsensitive)) match = true;
        }
        m_library->setRowHidden(i, !match);
    }
}

void MainWindow::onLibraryActivated() { onOpenFromLibrary(); }

void MainWindow::onOpenFromLibrary()
{
    const int row = m_library->currentRow();
    if (row < 0 || row >= m_libraryFiles.size()) {
        QMessageBox::information(this, QStringLiteral("素材库"),
                                 QStringLiteral("请先在左侧素材库选一份配置。"));
        return;
    }
    const QString path = QDir(m_libraryDir).filePath(m_libraryFiles[row]);
    if (!QFile::exists(path)) {
        QMessageBox::warning(this, QStringLiteral("素材库"),
                             QStringLiteral("文件不存在：\n%1").arg(path));
        return;
    }
    openFile(path);
}

// ------------------------------------------------------------------ 打开/保存
void MainWindow::onOpen()
{
    const QString p = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开 .fps 打印设置"), QString(),
        QStringLiteral("Formlabs Print Settings (*.fps);;所有文件 (*.*)"));
    if (!p.isEmpty()) openFile(p);
}

void MainWindow::openFile(const QString &path)
{
    QString err;
    if (!m_doc.load(path, &err)) {
        QMessageBox::critical(this, QStringLiteral("打开失败"), err);
        return;
    }
    m_hasSelection = false;
    m_selectedPath.clear();
    refreshTree();
    updateHeader();
    refreshKnobParams();
    statusBar()->showMessage(QStringLiteral("已打开：%1").arg(path), 5000);
}

void MainWindow::onSave()
{
    if (!m_doc.isOpen()) return;
    if (m_doc.path().isEmpty()) { onSaveAs(); return; }
    QString err;
    if (!m_doc.save(&err)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), err);
        return;
    }
    updateHeader();
    statusBar()->showMessage(QStringLiteral("已保存：%1").arg(m_doc.path()), 5000);
}

void MainWindow::onSaveAs()
{
    if (!m_doc.isOpen()) return;
    const QString suggested = m_doc.fileName().isEmpty()
        ? QStringLiteral("print_setting.fps")
        : QFileInfo(m_doc.path()).fileName();
    QString p = QFileDialog::getSaveFileName(
        this, QStringLiteral("另存为 .fps"), suggested,
        QStringLiteral("Formlabs Print Settings (*.fps)"));
    if (p.isEmpty()) return;
    if (!p.endsWith(QStringLiteral(".fps"), Qt::CaseInsensitive))
        p += QStringLiteral(".fps");
    QString err;
    if (!m_doc.save(p, &err)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), err);
        return;
    }
    updateHeader();
    statusBar()->showMessage(QStringLiteral("已保存：%1").arg(p), 5000);
}

// ------------------------------------------------------------------ 头部
void MainWindow::updateHeader()
{
    if (!m_doc.isOpen()) {
        m_fileLabel->setText(QStringLiteral("（未打开文件）"));
        m_stateLabel->clear();
        setWindowTitle(QStringLiteral("PreForm FPS 打印设置编辑器"));
        return;
    }
    const auto s = m_doc.summary();
    m_fileLabel->setText(m_doc.fileName());
    m_stateLabel->setText(QStringLiteral("%1 · %2 · %3mm   |   schema v%4   source=%5   %6")
                          .arg(s.machineName, s.materialCode,
                               FpsDocument::formatScalar(s.thickness))
                          .arg(s.schemaVersion)
                          .arg(s.source)
                          .arg(m_doc.isDirty() ? QStringLiteral("已修改 ●") : QStringLiteral("未修改")));
    setWindowTitle(QStringLiteral("%1 — %2").arg(QStringLiteral("PreForm FPS 打印设置编辑器"), m_doc.fileName()));
}

void MainWindow::markDirty()
{
    m_doc.setDirty(true);
    updateHeader();
}

// ------------------------------------------------------------------ 树
static QString pathToString(const QVector<QJsonValue> &path)
{
    QStringList parts;
    for (const QJsonValue &v : path)
        parts << (v.isString() ? v.toString() : QString::number(v.toInt()));
    return parts.join(QStringLiteral(" / "));
}

QTreeWidgetItem *MainWindow::addValueItem(QTreeWidgetItem *parent, const QString &key,
                                          const QJsonValue &value,
                                          const QVector<QJsonValue> &path)
{
    auto *item = new QTreeWidgetItem(parent);
    item->setText(0, key);
    if (value.isObject() || value.isArray()) {
        const int n = value.isObject() ? value.toObject().size() : value.toArray().size();
        item->setText(1, value.isObject()
                             ? QStringLiteral("{%1 项}").arg(n)
                             : QStringLiteral("[%1 项]").arg(n));
        item->setData(0, Qt::UserRole, QVariant());   // 容器不可直接编辑
    } else {
        item->setText(1, FpsDocument::formatScalar(value));
        // 存路径
        QJsonArray arr;
        for (const QJsonValue &p : path) arr.append(p);
        item->setData(0, Qt::UserRole, QVariant::fromValue(arr));
    }
    item->setData(1, Qt::UserRole + 1, pathToString(path));
    return item;
}

void MainWindow::addChild(QTreeWidgetItem *parent, const QVector<QJsonValue> &path,
                          const QJsonValue &value)
{
    const QJsonValue last = path.last();
    const QString key = last.isString() ? last.toString() : QString::number(last.toInt());
    auto *item = addValueItem(parent, key, value, path);
    if (value.isObject()) {
        const QJsonObject o = value.toObject();
        for (auto it = o.begin(); it != o.end(); ++it) {
            QVector<QJsonValue> child = path;
            child.append(QJsonValue(it.key()));
            addChild(item, child, it.value());
        }
    } else if (value.isArray()) {
        const QJsonArray a = value.toArray();
        for (int i = 0; i < a.size(); ++i) {
            QVector<QJsonValue> child = path;
            child.append(QJsonValue(i));
            addChild(item, child, a.at(i));
        }
    }
}

void MainWindow::refreshTree()
{
    m_tree->clear();
    if (!m_doc.isOpen()) return;

    const QJsonObject rootObj = m_doc.root();

    // metadata
    auto *mdItem = new QTreeWidgetItem(m_tree);
    mdItem->setText(0, QStringLiteral("metadata"));
    mdItem->setText(1, QStringLiteral("{%1 项}").arg(rootObj.value("metadata").toObject().size()));
    mdItem->setData(1, Qt::UserRole + 1, QStringLiteral("metadata"));
    mdItem->setExpanded(true);
    const QJsonObject md = rootObj.value("metadata").toObject();
    for (auto it = md.begin(); it != md.end(); ++it) {
        QVector<QJsonValue> path{QJsonValue(QStringLiteral("metadata")), QJsonValue(it.key())};
        addChild(mdItem, path, it.value());
    }

    // public_fields
    if (!rootObj.contains("public_fields") || rootObj.value("public_fields").isNull()) {
        auto *pfItem = new QTreeWidgetItem(m_tree);
        pfItem->setText(0, QStringLiteral("public_fields"));
        pfItem->setText(1, QStringLiteral("<null> —— 完全继承材料默认值"));
        pfItem->setForeground(1, QBrush(QColor(120, 120, 120)));
        pfItem->setData(1, Qt::UserRole + 1, QStringLiteral("public_fields"));
    } else {
        auto *pfItem = new QTreeWidgetItem(m_tree);
        pfItem->setText(0, QStringLiteral("public_fields"));
        pfItem->setText(1, QStringLiteral("{%1 项}").arg(rootObj.value("public_fields").toObject().size()));
        pfItem->setData(1, Qt::UserRole + 1, QStringLiteral("public_fields"));
        pfItem->setExpanded(true);
        const QJsonObject pf = rootObj.value("public_fields").toObject();
        for (auto it = pf.begin(); it != pf.end(); ++it) {
            QVector<QJsonValue> path{QJsonValue(QStringLiteral("public_fields")), QJsonValue(it.key())};
            addChild(pfItem, path, it.value());
        }
    }

    // algorithm_profiles
    const QJsonArray ap = rootObj.value("algorithm_profiles").toArray();
    auto *apItem = new QTreeWidgetItem(m_tree);
    apItem->setText(0, QStringLiteral("algorithm_profiles"));
    apItem->setText(1, QStringLiteral("[%1 项]").arg(ap.size()));
    apItem->setData(1, Qt::UserRole + 1, QStringLiteral("algorithm_profiles"));
    for (int i = 0; i < ap.size(); ++i) {
        QVector<QJsonValue> path{QJsonValue(QStringLiteral("algorithm_profiles")), QJsonValue(i)};
        addChild(apItem, path, ap.at(i));
    }

    // underlying_material
    auto *umItem = new QTreeWidgetItem(m_tree);
    umItem->setText(0, QStringLiteral("underlying_material"));
    const auto s = m_doc.summary();
    umItem->setText(1, QStringLiteral("%1 字符 → %2 字节密文（加密，只读）")
                       .arg(s.cipherChars).arg(s.cipherBytes));
    umItem->setForeground(1, QBrush(QColor(120, 120, 120)));
    umItem->setData(1, Qt::UserRole + 1, QStringLiteral("underlying_material"));
}

void MainWindow::onTreeSelectionChanged()
{
    auto *item = m_tree->currentItem();
    m_valueEdit->setEnabled(false);
    m_applyBtn->setEnabled(false);
    m_revertBtn->setEnabled(false);

    if (!item) {
        m_hasSelection = false;
        m_pathLabel->setText(QStringLiteral("—"));
        m_typeLabel->setText(QStringLiteral("—"));
        m_hintLabel->clear();
        m_valueEdit->clear();
        return;
    }

    const QVariant v = item->data(0, Qt::UserRole);
    const QString display = item->data(1, Qt::UserRole + 1).toString();
    m_pathLabel->setText(display);

    if (!v.isValid() || item->text(0) == QStringLiteral("underlying_material")) {
        m_hasSelection = false;
        m_typeLabel->setText(QStringLiteral("不可编辑"));
        m_hintLabel->setText(QStringLiteral("underlying_material 是加密后的材料参数集，必须以 PreForm 导出的原始值为准。"));
        m_valueEdit->clear();
        return;
    }

    const QJsonArray arr = v.value<QJsonArray>();
    QVector<QJsonValue> path;
    for (const QJsonValue &p : arr) path.append(p);

    bool ok = false;
    const QJsonValue cur = resolvePath(path, &ok);
    if (!ok) { m_hasSelection = false; return; }

    m_selectedPath = path;
    m_hasSelection = true;
    m_typeLabel->setText(FpsDocument::typeName(cur));
    m_valueEdit->setText(FpsDocument::formatScalar(cur));
    m_hintLabel->setText(hintFor(path, cur));
    m_valueEdit->setEnabled(true);
    m_applyBtn->setEnabled(true);
    m_revertBtn->setEnabled(true);
}

QJsonValue MainWindow::resolvePath(const QVector<QJsonValue> &path, bool *ok) const
{
    return m_doc.valueAt(path, ok);
}

bool MainWindow::setPathValue(const QVector<QJsonValue> &path, const QJsonValue &value, QString *error)
{
    return m_doc.setValueAt(path, value, error);
}

QString MainWindow::hintFor(const QVector<QJsonValue> &path, const QJsonValue &value) const
{
    const QString key = path.isEmpty() ? QString()
                                       : (path.last().isString() ? path.last().toString()
                                                                 : QString::number(path.last().toInt()));
    static const QHash<QString, QString> hints = {
        {QStringLiteral("schema_version"),
         QStringLiteral("固定为 3。小于 3 判为过旧、大于 3 判为过新，PreForm 都会拒绝加载。")},
        {QStringLiteral("id"),
         QStringLiteral("形如 {GUID} 的字符串；同机型下不要重复。")},
        {QStringLiteral("version"), QStringLiteral("整数，通常为 0；在副本上可递增。")},
        {QStringLiteral("source"), QStringLiteral("整数：0 = Formlabs 官方，1 = 自定义（用户设置）。")},
        {QStringLiteral("name"), QStringLiteral("显示在 Job Setup 里的打印设置名称。")},
        {QStringLiteral("machine_type_id"),
         QStringLiteral("机型 ID，例如 FORM-4-0 / FRML-4-0 / CLRK-1-2 / FS30-1-0 / PILK-1-0。")},
        {QStringLiteral("versioned_material_code"), QStringLiteral("材料码，例如 FLGPWH05 / FLOPEN01。")},
        {QStringLiteral("layer_thickness_mm"), QStringLiteral("层厚（mm），必须是该材料支持的值。")},
        {QStringLiteral("required_capabilities"), QStringLiteral("机型能力约束数组，通常为空。")},
        {QStringLiteral("requires_revalidation"), QStringLiteral("布尔；跨机型修改后建议置 true 以触发复检。")},
        {QStringLiteral("description"), QStringLiteral("打印设置说明文字。")},
        {QStringLiteral("created"), QStringLiteral("创建时间，格式 yyyy-MM-ddTHH:mm:ss.zzz。")},
        {QStringLiteral("last_modified"), QStringLiteral("最后修改时间，格式同上。")},
    };
    if (hints.contains(key)) return hints.value(key);

    if (!path.isEmpty() && path.first().isString()
        && path.first().toString() == QStringLiteral("public_fields"))
        return QStringLiteral("public_fields 覆盖材料默认参数；为空表示完全继承 underlying_material。");

    if (value.isBool()) return QStringLiteral("布尔值：true / false");
    if (value.isDouble()) return QStringLiteral("数值：直接输入数字");
    return QString();
}

void MainWindow::onApplyValue()
{
    if (!m_hasSelection) return;

    const QJsonValue cur = m_doc.valueAt(m_selectedPath, nullptr);
    QJsonValue nv;
    QString err;
    if (!FpsDocument::parseScalar(m_valueEdit->text(), cur, &nv, &err)) {
        QMessageBox::warning(this, QStringLiteral("输入格式错误"), err);
        return;
    }
    if (!setPathValue(m_selectedPath, nv, &err)) {
        QMessageBox::critical(this, QStringLiteral("修改失败"), err);
        return;
    }
    const QVector<QJsonValue> keep = m_selectedPath;
    markDirty();
    refreshTree();
    reselect(keep);
    statusBar()->showMessage(QStringLiteral("已修改 —— Ctrl+S 保存"), 5000);
}

void MainWindow::onRevertValue()
{
    if (!m_hasSelection || m_doc.path().isEmpty()) return;

    FpsDocument orig;
    QString err;
    if (!orig.load(m_doc.path(), &err)) {
        QMessageBox::warning(this, QStringLiteral("恢复失败"), err);
        return;
    }
    bool ok = false;
    const QJsonValue old = orig.valueAt(m_selectedPath, &ok);
    if (!ok) {
        QMessageBox::information(this, QStringLiteral("恢复原值"),
                                 QStringLiteral("该节点在已保存的文件中不存在。"));
        return;
    }
    if (!setPathValue(m_selectedPath, old, &err)) {
        QMessageBox::critical(this, QStringLiteral("恢复失败"), err);
        return;
    }
    const QVector<QJsonValue> keep = m_selectedPath;
    refreshTree();
    reselect(keep);
    m_doc.setDirty(true);
    updateHeader();
}

void MainWindow::reselect(const QVector<QJsonValue> &path)
{
    if (path.isEmpty()) return;
    const QString target = pathToString(path);
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        QTreeWidgetItem *item = *it;
        if (item->data(1, Qt::UserRole + 1).toString() == target) {
            m_tree->setCurrentItem(item);
            m_tree->scrollToItem(item);
            return;
        }
        ++it;
    }
}

void MainWindow::onMakeCustom()
{
    if (!m_doc.isOpen()) return;
    m_doc.makeCustom();
    refreshTree();
    updateHeader();
    statusBar()->showMessage(QStringLiteral("已转为自定义设置（source=1 / 新 id / 名称加 Custom）"), 6000);
}

void MainWindow::onValidate()
{
    if (!m_doc.isOpen()) return;
    const QStringList errs = FpsDocument::validate(m_doc.root());
    if (errs.isEmpty())
        QMessageBox::information(this, QStringLiteral("校验通过"),
                                 QStringLiteral("结构校验通过，可以导入 PreForm 使用。"));
    else
        QMessageBox::warning(this, QStringLiteral("校验未通过"),
                             QStringLiteral("发现 %1 个问题：\n\n• %2")
                                 .arg(errs.size()).arg(errs.join(QStringLiteral("\n• "))));
}

void MainWindow::onViewJson()
{
    if (!m_doc.isOpen()) return;

    auto *w = new QWidget(this, Qt::Window);
    w->setWindowTitle(QStringLiteral("原始 JSON — %1").arg(m_doc.fileName()));
    w->resize(920, 720);
    auto *lay = new QVBoxLayout(w);
    auto *txt = new QPlainTextEdit(w);
    txt->setFont(QFont(QStringLiteral("Consolas"), 9));
    txt->setLineWrapMode(QPlainTextEdit::NoWrap);

    QJsonObject copy = m_doc.root();
    const QString um = copy.value(QStringLiteral("underlying_material")).toString();
    if (um.size() > 300)
        copy.insert(QStringLiteral("underlying_material"),
                    um.left(120) + QStringLiteral("...(已截断，共 %1 字符)").arg(um.size()));
    txt->setPlainText(QString::fromUtf8(QJsonDocument(copy).toJson(QJsonDocument::Indented)));
    txt->setReadOnly(true);
    lay->addWidget(txt);
    w->show();
}

void MainWindow::onCipherInfo()
{
    if (!m_doc.isOpen()) return;

    const QString um = m_doc.underlyingMaterial();
    const QByteArray raw = QByteArray::fromBase64(um.toLatin1());
    if (raw.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("加密封包"),
                                 QStringLiteral("无法按 Base64 解码 underlying_material。"));
        return;
    }

    int counts[256] = {0};
    for (unsigned char c : raw) counts[c]++;
    const double n = double(raw.size());
    double ent = 0.0;
    for (int i = 0; i < 256; ++i)
        if (counts[i] > 0) { const double p = counts[i] / n; ent -= p * std::log2(p); }

    QMessageBox::information(this, QStringLiteral("underlying_material 加密封包"),
        QStringLiteral("Base64 字符数 : %1\n解码后字节数 : %2\nlen %% 16      : %3\n"
                       "字节熵        : %4 bit/byte（>7.99 表示强加密）\n首 16 字节    : %5\n末 16 字节    : %6\n\n"
                       "结论：这是 PreForm 用内部密钥加密的 knob 参数集，\n"
                       "无法在外部离线解密，必须由 PreForm 自己读写。")
            .arg(um.size()).arg(raw.size()).arg(raw.size() % 16)
            .arg(ent, 0, 'f', 4)
            .arg(QString::fromLatin1(raw.left(16).toHex(' ')))
            .arg(QString::fromLatin1(raw.right(16).toHex(' '))));
}



// ------------------------------------------------------------------ 材料参数
void MainWindow::refreshKnobParams()
{
    if (!m_knobTable) return;
    m_knobTable->setRowCount(0);
    m_knobParams.clear();
    m_knobSchema = -1;

    if (!m_doc.isOpen()) {
        if (m_knobInfo) m_knobInfo->setText(QStringLiteral("打开一份 .fps 后自动匹配解密参数"));
        return;
    }
    if (!m_knobLib.isAvailable()) {
        if (m_knobInfo) m_knobInfo->setText(QStringLiteral("未找到参数库：%1").arg(m_knobLib.dir()));
        return;
    }
    const auto s = m_doc.summary();
    const double thk = s.thickness.toDouble();
    const bool ok = m_knobLib.find(s.machineId, s.materialCode, thk, &m_knobParams, &m_knobSchema);
    if (!ok) {
        if (m_knobInfo) m_knobInfo->setText(
            QStringLiteral("参数库中没有 %1 / %2 / %3mm 的解密数据")
                .arg(s.machineId, s.materialCode, FpsDocument::formatScalar(s.thickness)));
        return;
    }
    int named = 0;
    m_knobTable->setRowCount(m_knobParams.size());
    for (int i = 0; i < m_knobParams.size(); ++i) {
        const KnobParam &p = m_knobParams[i];
        if (!p.name.isEmpty()) ++named;
        m_knobTable->setItem(i, 0, new QTableWidgetItem(p.categoryName.isEmpty() ? p.categoryKey.left(16) : p.categoryName));
        m_knobTable->setItem(i, 1, new QTableWidgetItem(p.categoryName));
        m_knobTable->setItem(i, 2, new QTableWidgetItem(p.key));
        m_knobTable->setItem(i, 3, new QTableWidgetItem(p.name));
        m_knobTable->setItem(i, 4, new QTableWidgetItem(FpsDocument::formatScalar(p.value)));
        if (p.name.isEmpty())
            m_knobTable->item(i, 3)->setForeground(QBrush(QColor(150, 150, 150)));
    }
    if (m_knobInfo)
        m_knobInfo->setText(QStringLiteral("%1 / %2 / %3mm —— 共 %4 个参数，已解出真名 %5 个（schema v%6）")
            .arg(s.machineId, s.materialCode, FpsDocument::formatScalar(s.thickness))
            .arg(m_knobParams.size()).arg(named).arg(m_knobSchema));
}

void MainWindow::onKnobRefresh() { refreshKnobParams(); }

void MainWindow::onKnobExportCsv()
{
    if (m_knobParams.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("导出 CSV"),
                                 QStringLiteral("当前没有可导出的材料参数（先打开一份 .fps）。"));
        return;
    }
    const auto s = m_doc.summary();
    const QString suggested = QStringLiteral("%1__%2__%3mm.csv")
        .arg(s.machineId, s.materialCode, FpsDocument::formatScalar(s.thickness));
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出材料参数 CSV"),
                                                 suggested, QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)) path += QStringLiteral(".csv");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, QStringLiteral("导出失败"), f.errorString());
        return;
    }
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts.setGenerateByteOrderMark(true);
    auto esc = [](const QString &x) {
        QString v = x; v.replace('"', QStringLiteral("\"\""));
        if (v.contains(',') || v.contains('"') || v.contains('\n')) v = QStringLiteral("\"%1\"").arg(v);
        return v;
    };
    ts << "machine_type,material_code,layer_thickness_mm,schema_version,category_key,category_name,knob_key,knob_name,value\n";
    for (const KnobParam &p : m_knobParams) {
        ts << esc(s.machineId) << ',' << esc(s.materialCode) << ','
           << esc(FpsDocument::formatScalar(s.thickness)) << ',' << m_knobSchema << ','
           << esc(p.categoryKey) << ',' << esc(p.categoryName) << ','
           << esc(p.key) << ',' << esc(p.name) << ','
           << esc(FpsDocument::formatScalar(p.value)) << '\n';
    }
    f.close();
    statusBar()->showMessage(QStringLiteral("已导出 CSV：%1").arg(path), 6000);
}

void MainWindow::onAbout()
{
    QMessageBox::information(this, QStringLiteral("关于 / 格式说明"),
        QStringLiteral("PreForm FPS 打印设置编辑器（C++ / Qt 6）\n\n"
                       "支持读取 / 编辑 / 保存 PreForm 3.65 的 .fps（JSON 文本）。\n"
                       "素材库：PreForm.exe 内嵌的 224 份真实配置（223 份 SLA）。\n\n"
                       "结构要点\n"
                       "  metadata.schema_version 必须 = 3（整数）\n"
                       "  metadata.source: 0=官方, 1=自定义\n"
                       "  metadata.base_material_identifier 内含机型/材料/层厚\n"
                       "  public_fields 覆盖参数；null 表示完全继承材料默认值\n"
                       "  underlying_material 为加密 knob 数据，只读\n"));
}

// ==================================================================
// 材料对比 / F4→ST1 转换 / 未知值编辑 等新增功能
// ==================================================================
static QString jarr(const QJsonArray &a)
{
    return QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact));
}

QString MainWindow::physRowSummary(const PhysicsRecord &r)
{
    return QStringLiteral("%1 · %2 · %3mm  (I=%4, E=%5, T=%6/%7/%8)")
        .arg(PhysicsLibrary::machineDisplayName(r.machine), r.material)
        .arg(r.thickness).arg(r.iNorm).arg(r.eNorm)
        .arg(r.tStart).arg(r.tPrint).arg(r.tMin);
}

QString MainWindow::escapedCsv(const QString &s)
{
    QString v = s;
    v.replace('"', QStringLiteral("\"\""));
    if (v.contains(',') || v.contains('"') || v.contains('\n'))
        v = QStringLiteral("\"%1\"").arg(v);
    return v;
}

bool MainWindow::writeTextFile(const QString &path, const QString &text, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = f.errorString();
        return false;
    }
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts.setGenerateByteOrderMark(true);
    ts << text;
    f.close();
    return true;
}

// ---------------- 材料对比 ----------------
void MainWindow::buildCompareTab(QWidget *page)
{
    auto *lay = new QVBoxLayout(page);

    auto *selRow = new QHBoxLayout;
    m_cmpLeftMachine = new QComboBox(page);
    m_cmpLeftMachine->addItem(QStringLiteral("Form 4"), QStringLiteral("FORM-4-0"));
    m_cmpLeftMachine->addItem(QStringLiteral("Form Studio 1"), QStringLiteral("CLRK-1-2"));
    m_cmpLeftMaterial = new QComboBox(page);
    m_cmpLeftThk = new QComboBox(page);

    m_cmpRightMachine = new QComboBox(page);
    m_cmpRightMachine->addItem(QStringLiteral("Form 4"), QStringLiteral("FORM-4-0"));
    m_cmpRightMachine->addItem(QStringLiteral("Form Studio 1"), QStringLiteral("CLRK-1-2"));
    m_cmpRightMachine->setCurrentIndex(1);
    m_cmpRightMaterial = new QComboBox(page);
    m_cmpRightThk = new QComboBox(page);

    selRow->addWidget(new QLabel(QStringLiteral("左侧："), page));
    selRow->addWidget(m_cmpLeftMachine);
    selRow->addWidget(m_cmpLeftMaterial);
    selRow->addWidget(m_cmpLeftThk);
    selRow->addSpacing(20);
    selRow->addWidget(new QLabel(QStringLiteral("右侧："), page));
    selRow->addWidget(m_cmpRightMachine);
    selRow->addWidget(m_cmpRightMaterial);
    selRow->addWidget(m_cmpRightThk);
    selRow->addStretch();
    auto *runBtn = new QPushButton(QStringLiteral("对比"), page);
    auto *csvBtn = new QPushButton(QStringLiteral("导出对比 CSV…"), page);
    selRow->addWidget(runBtn);
    selRow->addWidget(csvBtn);
    lay->addLayout(selRow);

    m_cmpInfo = new QLabel(QStringLiteral("选择两种材料进行逐参数对比（同一材料也可跨机型对比）"), page);
    m_cmpInfo->setStyleSheet(QStringLiteral("color:#666;"));
    lay->addWidget(m_cmpInfo);

    m_cmpTable = new QTableWidget(page);
    m_cmpTable->setColumnCount(4);
    m_cmpTable->setHorizontalHeaderLabels({QStringLiteral("参数"), QStringLiteral("左侧值"),
                                           QStringLiteral("右侧值"), QStringLiteral("差异")});
    m_cmpTable->horizontalHeader()->setStretchLastSection(true);
    m_cmpTable->setColumnWidth(0, 240);
    m_cmpTable->setColumnWidth(1, 320);
    m_cmpTable->setColumnWidth(2, 320);
    m_cmpTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_cmpTable->setAlternatingRowColors(true);
    m_cmpTable->verticalHeader()->setVisible(false);
    lay->addWidget(m_cmpTable, 1);

    connect(m_cmpLeftMachine, &QComboBox::currentIndexChanged, this, &MainWindow::onCompareLeftChanged);
    connect(m_cmpLeftMaterial, &QComboBox::currentIndexChanged, this, &MainWindow::onCompareLeftChanged);
    connect(m_cmpLeftThk, &QComboBox::currentIndexChanged, this, &MainWindow::onCompareLeftChanged);
    connect(m_cmpRightMachine, &QComboBox::currentIndexChanged, this, &MainWindow::onCompareRightChanged);
    connect(m_cmpRightMaterial, &QComboBox::currentIndexChanged, this, &MainWindow::onCompareRightChanged);
    connect(m_cmpRightThk, &QComboBox::currentIndexChanged, this, &MainWindow::onCompareRightChanged);
    connect(runBtn, &QPushButton::clicked, this, &MainWindow::onCompareRun);
    connect(csvBtn, &QPushButton::clicked, this, &MainWindow::onCompareExportCsv);

    refreshCompareSelectors();
}

void MainWindow::refreshCompareSelectors()
{
    auto fillMaterials = [this](QComboBox *machine, QComboBox *material) {
        material->blockSignals(true);
        material->clear();
        const QString m = machine->currentData().toString();
        for (const QString &code : m_physLib.materials(m))
            material->addItem(code, code);
        material->blockSignals(false);
    };
    fillMaterials(m_cmpLeftMachine, m_cmpLeftMaterial);
    fillMaterials(m_cmpRightMachine, m_cmpRightMaterial);

    auto fillThk = [this](QComboBox *machine, QComboBox *material, QComboBox *thk) {
        thk->blockSignals(true);
        thk->clear();
        const QString m = machine->currentData().toString();
        const QString mat = material->currentData().toString();
        for (const PhysicsRecord &r : m_physLib.byMaterial(m, mat))
            thk->addItem(QStringLiteral("%1 mm").arg(r.thickness), r.thickness);
        thk->blockSignals(false);
    };
    fillThk(m_cmpLeftMachine, m_cmpLeftMaterial, m_cmpLeftThk);
    fillThk(m_cmpRightMachine, m_cmpRightMaterial, m_cmpRightThk);
    onCompareLeftChanged();
    onCompareRightChanged();
}

void MainWindow::onCompareLeftChanged()
{
    m_cmpLeftThk->blockSignals(true);
    m_cmpLeftThk->clear();
    const QString m = m_cmpLeftMachine->currentData().toString();
    const QString mat = m_cmpLeftMaterial->currentData().toString();
    for (const PhysicsRecord &r : m_physLib.byMaterial(m, mat))
        m_cmpLeftThk->addItem(QStringLiteral("%1 mm").arg(r.thickness), r.thickness);
    m_cmpLeftThk->blockSignals(false);
}

void MainWindow::onCompareRightChanged()
{
    m_cmpRightThk->blockSignals(true);
    m_cmpRightThk->clear();
    const QString m = m_cmpRightMachine->currentData().toString();
    const QString mat = m_cmpRightMaterial->currentData().toString();
    for (const PhysicsRecord &r : m_physLib.byMaterial(m, mat))
        m_cmpRightThk->addItem(QStringLiteral("%1 mm").arg(r.thickness), r.thickness);
    m_cmpRightThk->blockSignals(false);
}

void MainWindow::onCompareRun()
{
    const QString lm = m_cmpLeftMachine->currentData().toString();
    const QString lmat = m_cmpLeftMaterial->currentData().toString();
    const double lt = m_cmpLeftThk->currentData().toDouble();
    const QString rm = m_cmpRightMachine->currentData().toString();
    const QString rmat = m_cmpRightMaterial->currentData().toString();
    const double rt = m_cmpRightThk->currentData().toDouble();
    m_cmpLeftValid = m_physLib.find(lm, lmat, lt, &m_cmpLeftRec);
    m_cmpRightValid = m_physLib.find(rm, rmat, rt, &m_cmpRightRec);
    if (!m_cmpLeftValid || !m_cmpRightValid) {
        m_cmpInfo->setText(QStringLiteral("所选组合在参数库中不存在。"));
        m_cmpTable->setRowCount(0);
        return;
    }
    m_cmpInfo->setText(QStringLiteral("左：%1   右：%2")
        .arg(physRowSummary(m_cmpLeftRec), physRowSummary(m_cmpRightRec)));
    fillCompareTable();
}

void MainWindow::fillCompareTable()
{
    const PhysicsRecord &a = m_cmpLeftRec;
    const PhysicsRecord &b = m_cmpRightRec;
    struct Row { QString label, l, r, d; };
    QVector<Row> rows;
    auto num = [](double v){ return QString::number(v, 'g', 10); };
    auto addNum = [&](const QString &label, double lv, double rv) {
        QString diff = (std::abs(lv - rv) < 1e-9) ? QStringLiteral("—")
                      : QStringLiteral("%1 (%2%)").arg(num(rv - lv))
                        .arg(lv != 0 ? num((rv - lv) / lv * 100.0) : QStringLiteral("∞"));
        rows.push_back({label, num(lv), num(rv), diff});
    };
    auto addStr = [&](const QString &label, const QString &lv, const QString &rv) {
        rows.push_back({label, lv, rv, (lv == rv ? QStringLiteral("—") : QStringLiteral("不同"))});
    };
    addStr(QStringLiteral("机型"), PhysicsLibrary::machineDisplayName(a.machine),
           PhysicsLibrary::machineDisplayName(b.machine));
    addStr(QStringLiteral("材料码"), a.material, b.material);
    addNum(QStringLiteral("层厚 (mm)"), a.thickness, b.thickness);
    addNum(QStringLiteral("标准层光强 I (mW/cm²)"), a.iNorm, b.iNorm);
    addNum(QStringLiteral("标准层能量 E (mJ/cm²)"), a.eNorm, b.eNorm);
    addNum(QStringLiteral("主曝光秒数 t=E/I (s)"), a.tNormal, b.tNormal);
    addNum(QStringLiteral("底层光强 I_early"), a.iEarly, b.iEarly);
    addNum(QStringLiteral("底层首层能量 E_early[0]"), a.eEarlyFirst, b.eEarlyFirst);
    addNum(QStringLiteral("底曝秒数 (s)"), a.tEarly, b.tEarly);
    addNum(QStringLiteral("起印温度 T_start (°C)"), a.tStart, b.tStart);
    addNum(QStringLiteral("打印温度 T_print (°C)"), a.tPrint, b.tPrint);
    addNum(QStringLiteral("最低温度 T_min (°C)"), a.tMin, b.tMin);
    addStr(QStringLiteral("离型膜"), a.film, b.film);
    addStr(QStringLiteral("过渡层能量 E_trans"), jarr(a.eTrans), jarr(b.eTrans));
    addStr(QStringLiteral("静置等待 t_wait"), jarr(a.tWait), jarr(b.tWait));
    addStr(QStringLiteral("面积断点 A_bp"), jarr(a.aBp), jarr(b.aBp));
    addStr(QStringLiteral("面积等待 area_wait"), jarr(a.areaWait), jarr(b.areaWait));
    addStr(QStringLiteral("Z 分段 Z_stage"), jarr(a.zStage), jarr(b.zStage));
    addStr(QStringLiteral("剥离速度 v1"), jarr(a.v1), jarr(b.v1));
    addStr(QStringLiteral("剥离速度 v2"), jarr(a.v2), jarr(b.v2));
    addStr(QStringLiteral("剥离速度 v3"), jarr(a.v3), jarr(b.v3));
    addNum(QStringLiteral("像素 X (mm)"), a.pixX, b.pixX);
    addNum(QStringLiteral("像素 Y (mm)"), a.pixY, b.pixY);

    m_cmpTable->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        m_cmpTable->setItem(i, 0, new QTableWidgetItem(rows[i].label));
        m_cmpTable->setItem(i, 1, new QTableWidgetItem(rows[i].l));
        m_cmpTable->setItem(i, 2, new QTableWidgetItem(rows[i].r));
        auto *d = new QTableWidgetItem(rows[i].d);
        if (rows[i].d != QStringLiteral("—"))
            d->setForeground(QBrush(QColor(180, 60, 0)));
        m_cmpTable->setItem(i, 3, d);
    }
}

void MainWindow::onCompareExportCsv()
{
    if (!m_cmpLeftValid || !m_cmpRightValid) { onCompareRun(); }
    if (!m_cmpLeftValid || !m_cmpRightValid) {
        QMessageBox::information(this, QStringLiteral("导出对比"), QStringLiteral("请先选择并对比两种材料。"));
        return;
    }
    const QString suggested = QStringLiteral("compare__%1_%2mm__vs__%3_%4mm.csv")
        .arg(m_cmpLeftRec.material).arg(m_cmpLeftRec.thickness)
        .arg(m_cmpRightRec.material).arg(m_cmpRightRec.thickness);
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出对比 CSV"), suggested,
                                                QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)) path += QStringLiteral(".csv");
    QString text = QStringLiteral("参数,左侧值,右侧值,差异\n");
    for (int i = 0; i < m_cmpTable->rowCount(); ++i) {
        text += escapedCsv(m_cmpTable->item(i,0)->text()) + ',' +
                escapedCsv(m_cmpTable->item(i,1)->text()) + ',' +
                escapedCsv(m_cmpTable->item(i,2)->text()) + ',' +
                escapedCsv(m_cmpTable->item(i,3)->text()) + '\n';
    }
    QString err;
    if (!writeTextFile(path, text, &err)) {
        QMessageBox::critical(this, QStringLiteral("导出失败"), err);
        return;
    }
    statusBar()->showMessage(QStringLiteral("已导出对比 CSV：%1").arg(path), 6000);
}

// ---------------- F4→ST1 转换 ----------------
void MainWindow::buildConvertTab(QWidget *page)
{
    auto *lay = new QVBoxLayout(page);
    auto *row = new QHBoxLayout;
    m_cvtMaterial = new QComboBox(page);
    for (const QString &code : m_physLib.materials(QStringLiteral("FORM-4-0")))
        m_cvtMaterial->addItem(code, code);
    m_cvtThk = new QComboBox(page);
    row->addWidget(new QLabel(QStringLiteral("F4 材料："), page));
    row->addWidget(m_cvtMaterial);
    row->addWidget(new QLabel(QStringLiteral("层厚："), page));
    row->addWidget(m_cvtThk);
    auto *runBtn = new QPushButton(QStringLiteral("转换"), page);
    auto *csvBtn = new QPushButton(QStringLiteral("导出转换 CSV…"), page);
    auto *applyBtn = new QPushButton(QStringLiteral("应用到当前 .fps 的 public_fields"), page);
    row->addWidget(runBtn);
    row->addWidget(csvBtn);
    row->addWidget(applyBtn);
    row->addStretch();
    lay->addLayout(row);

    m_cvtInfo = new QLabel(QStringLiteral("把 Form 4 材料参数按实测锚点换算为 Form Studio 1 目标物理参数（不含加密下发的底层 knob，仅换算可辨识物理量）"), page);
    m_cvtInfo->setWordWrap(true);
    m_cvtInfo->setStyleSheet(QStringLiteral("color:#666;"));
    lay->addWidget(m_cvtInfo);

    m_cvtTable = new QTableWidget(page);
    m_cvtTable->setColumnCount(3);
    m_cvtTable->setHorizontalHeaderLabels({QStringLiteral("参数"), QStringLiteral("F4 值"), QStringLiteral("ST1 目标值")});
    m_cvtTable->horizontalHeader()->setStretchLastSection(true);
    m_cvtTable->setColumnWidth(0, 300);
    m_cvtTable->setColumnWidth(1, 200);
    m_cvtTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_cvtTable->setAlternatingRowColors(true);
    m_cvtTable->verticalHeader()->setVisible(false);
    lay->addWidget(m_cvtTable, 1);

    connect(m_cvtMaterial, &QComboBox::currentIndexChanged, this, &MainWindow::onConvertMaterialChanged);
    connect(m_cvtThk, &QComboBox::currentIndexChanged, this, &MainWindow::onConvertRun);
    connect(runBtn, &QPushButton::clicked, this, &MainWindow::onConvertRun);
    connect(csvBtn, &QPushButton::clicked, this, &MainWindow::onConvertExportCsv);
    connect(applyBtn, &QPushButton::clicked, this, &MainWindow::onConvertApplyToDoc);
    onConvertMaterialChanged();
}

void MainWindow::onConvertMaterialChanged()
{
    m_cvtThk->blockSignals(true);
    m_cvtThk->clear();
    const QString mat = m_cvtMaterial->currentData().toString();
    for (const ConvertedRecord &c : m_physLib.converted())
        if (c.material == mat) m_cvtThk->addItem(QStringLiteral("%1 mm").arg(c.thickness), c.thickness);
    m_cvtThk->blockSignals(false);
    onConvertRun();
}

void MainWindow::onConvertRun()
{
    const QString mat = m_cvtMaterial->currentData().toString();
    const double thk = m_cvtThk->currentData().toDouble();
    m_cvtValid = m_physLib.findConverted(mat, thk, &m_cvtRec);
    if (!m_cvtValid) {
        m_cvtInfo->setText(QStringLiteral("该材料/层厚没有转换记录。"));
        m_cvtTable->setRowCount(0);
        return;
    }
    m_cvtInfo->setText(QStringLiteral("%1 %2mm  |  k_E=%3 (%4)  |  F4 %5 mW/cm² → ST1 %6 mW/cm²")
        .arg(mat).arg(thk).arg(m_cvtRec.kE, 0, 'f', 4).arg(m_cvtRec.kESource)
        .arg(m_cvtRec.f4I).arg(m_cvtRec.st1I));
    fillConvertTable();
}

void MainWindow::fillConvertTable()
{
    const ConvertedRecord &c = m_cvtRec;
    struct Row { QString label, f4, st1; };
    auto num = [](double v){ return QString::number(v, 'g', 10); };
    QVector<Row> rows = {
        {QStringLiteral("标准层光强 I (mW/cm²)"), num(c.f4I), num(c.st1I)},
        {QStringLiteral("标准层能量 E (mJ/cm²)"), num(c.f4E), num(c.st1E)},
        {QStringLiteral("主曝光秒数 t=E/I (s)"), num(c.f4TNorm), num(c.st1TNorm)},
        {QStringLiteral("底层首层能量 E_early[0]"), num(c.f4EEarly), num(c.st1EEarly)},
        {QStringLiteral("底曝秒数 (s)"), num(c.f4TEarly), num(c.st1TEarly)},
        {QStringLiteral("起印温度 T_start (°C)"), num(c.tStart), num(c.tStart)},
        {QStringLiteral("打印温度 T_print (°C)"), num(c.tPrint), num(c.tPrint)},
        {QStringLiteral("最低温度 T_min (°C)"), num(c.tMin), num(c.tMin)},
        {QStringLiteral("能量折减系数 k_E"), QStringLiteral("—"), num(c.kE)},
    };
    m_cvtTable->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        m_cvtTable->setItem(i, 0, new QTableWidgetItem(rows[i].label));
        m_cvtTable->setItem(i, 1, new QTableWidgetItem(rows[i].f4));
        m_cvtTable->setItem(i, 2, new QTableWidgetItem(rows[i].st1));
    }
}

void MainWindow::onConvertExportCsv()
{
    if (!m_cvtValid) onConvertRun();
    if (!m_cvtValid) {
        QMessageBox::information(this, QStringLiteral("导出转换"), QStringLiteral("请先选择材料并转换。"));
        return;
    }
    const QString suggested = QStringLiteral("convert__%1__%2mm.csv").arg(m_cvtRec.material).arg(m_cvtRec.thickness);
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出转换 CSV"), suggested, QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)) path += QStringLiteral(".csv");
    QString text = QStringLiteral("参数,F4值,ST1目标值\n");
    for (int i = 0; i < m_cvtTable->rowCount(); ++i) {
        text += escapedCsv(m_cvtTable->item(i,0)->text()) + ',' +
                escapedCsv(m_cvtTable->item(i,1)->text()) + ',' +
                escapedCsv(m_cvtTable->item(i,2)->text()) + '\n';
    }
    QString err;
    if (!writeTextFile(path, text, &err)) { QMessageBox::critical(this, QStringLiteral("导出失败"), err); return; }
    statusBar()->showMessage(QStringLiteral("已导出转换 CSV：%1").arg(path), 6000);
}

void MainWindow::onConvertApplyToDoc()
{
    if (!m_doc.isOpen()) {
        QMessageBox::information(this, QStringLiteral("应用转换"), QStringLiteral("请先打开一份 .fps。"));
        return;
    }
    if (!m_cvtValid) onConvertRun();
    if (!m_cvtValid) return;
    // 写入 public_fields.categories 的 Material_Form_4_Family_Print 分类（可识别的物理量）
    QJsonObject root = m_doc.root();
    QJsonObject pf = root.value(QStringLiteral("public_fields")).toObject();
    QJsonArray cats = pf.value(QStringLiteral("categories")).toArray();
    QJsonObject values;
    values.insert(QStringLiteral("I_norm_mWcm2"), m_cvtRec.st1I);
    values.insert(QStringLiteral("E_norm_mJcm2"), m_cvtRec.st1E);
    values.insert(QStringLiteral("t_normal_exp_s"), m_cvtRec.st1TNorm);
    values.insert(QStringLiteral("t_early_exp_s"), m_cvtRec.st1TEarly);
    values.insert(QStringLiteral("T_start_C"), m_cvtRec.tStart);
    values.insert(QStringLiteral("T_print_C"), m_cvtRec.tPrint);
    values.insert(QStringLiteral("T_min_C"), m_cvtRec.tMin);
    // 查找/更新分类
    bool found = false;
    for (int i = 0; i < cats.size(); ++i) {
        QJsonObject c = cats.at(i).toObject();
        if (c.value(QStringLiteral("key")).toString() == QStringLiteral("Material_Form_4_Family_Print")) {
            QJsonObject old = c.value(QStringLiteral("values")).toObject();
            for (auto it = values.begin(); it != values.end(); ++it) old.insert(it.key(), it.value());
            c.insert(QStringLiteral("values"), old);
            if (!c.contains(QStringLiteral("version"))) c.insert(QStringLiteral("version"), 1);
            cats.replace(i, c);
            found = true;
            break;
        }
    }
    if (!found) {
        QJsonObject c;
        c.insert(QStringLiteral("key"), QStringLiteral("Material_Form_4_Family_Print"));
        c.insert(QStringLiteral("values"), values);
        c.insert(QStringLiteral("version"), 1);
        cats.append(c);
    }
    pf.insert(QStringLiteral("categories"), cats);
    root.insert(QStringLiteral("public_fields"), pf);
    m_doc.replaceRoot(root);
    refreshTree();
    updateHeader();
    statusBar()->showMessage(QStringLiteral("已把转换结果写入 public_fields（Ctrl+S 保存）"), 7000);
}

// ---------------- 未知值编辑：knob 表可编辑 + 写入 public_fields ----------------
void MainWindow::onKnobTableDoubleClicked(int row, int col)
{
    if (row < 0 || row >= m_knobParams.size()) return;
    // 仅允许编辑“默认值”列（col 4）
    if (col != 4) return;
    m_knobTable->editItem(m_knobTable->item(row, 4));
}

void MainWindow::onKnobApplyToPublicFields()
{
    if (!m_doc.isOpen() || m_knobParams.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("写入 public_fields"),
                                 QStringLiteral("请先打开一份 .fps 且已匹配到解密参数。"));
        return;
    }
    // 从表格读取（用户可能改过“默认值”列），按分类重组为 public_fields.categories
    QMap<QString, QJsonObject> catValues;   // categoryKey -> values{knobKey: value}
    QMap<QString, QJsonArray> catOrder;     // preserve order not critical
    for (int i = 0; i < m_knobTable->rowCount() && i < m_knobParams.size(); ++i) {
        KnobParam &p = m_knobParams[i];
        auto *valItem = m_knobTable->item(i, 4);
        QJsonValue v = p.value;
        if (valItem) {
            const QString txt = valItem->text().trimmed();
            QJsonValue parsed;
            QString perr;
            if (FpsDocument::parseScalar(txt, p.value, &parsed, &perr)) v = parsed;
        }
        QJsonObject o = catValues.value(p.categoryKey);
        o.insert(p.key, v);
        catValues.insert(p.categoryKey, o);
    }
    QJsonArray cats;
    for (auto it = catValues.begin(); it != catValues.end(); ++it) {
        QJsonObject c;
        c.insert(QStringLiteral("key"), it.key());
        c.insert(QStringLiteral("values"), it.value());
        c.insert(QStringLiteral("version"), 1);
        cats.append(c);
    }
    QJsonObject pf;
    pf.insert(QStringLiteral("categories"), cats);
    QJsonObject root = m_doc.root();
    root.insert(QStringLiteral("public_fields"), pf);
    m_doc.replaceRoot(root);
    refreshTree();
    updateHeader();
    statusBar()->showMessage(QStringLiteral("已把 %1 个参数（含未解真名的键）写入 public_fields，Ctrl+S 保存")
                                 .arg(m_knobParams.size()), 8000);
}

// ---------------- 自动定位 PreForm + 提取内嵌 FPS ----------------
void MainWindow::onLocatePreForm()
{
    QString how;
    const QStringList cands = FpsExtractor::candidatePreFormPaths();
    if (cands.isEmpty()) {
        // 让用户手动选
        const QString p = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择 PreForm.exe"), QStringLiteral("C:/Program Files/Formlabs"),
            QStringLiteral("PreForm.exe (PreForm.exe);;可执行文件 (*.exe)"));
        if (p.isEmpty()) return;
        m_preFormPath = p;
    } else {
        m_preFormPath = cands.first();
    }
    if (m_extractInfo)
        m_extractInfo->setText(QStringLiteral("PreForm：%1").arg(m_preFormPath));
    statusBar()->showMessage(QStringLiteral("已定位 PreForm：%1").arg(m_preFormPath), 6000);
}

void MainWindow::onExtractFromPreForm()
{
    QString how;
    QString exe = FpsExtractor::autoLocate(&how);
    if (exe.isEmpty()) {
        // 自动失败 -> 手动选择
        const QString p = QFileDialog::getOpenFileName(
            this, QStringLiteral("请选择 PreForm.exe（用于提取内嵌 FPS）"),
            QStringLiteral("C:/Program Files/Formlabs"),
            QStringLiteral("PreForm.exe (PreForm.exe);;可执行文件 (*.exe)"));
        if (p.isEmpty()) return;
        exe = p;
    }
    m_preFormPath = exe;
    if (m_extractInfo)
        m_extractInfo->setText(QStringLiteral("正在从 %1 提取…").arg(exe));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QVector<ExtractedFps> items;
    QString err;
    const bool ok = FpsExtractor::extract(exe, &items, &err);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        if (m_extractInfo) m_extractInfo->setText(QStringLiteral("提取失败：%1").arg(err));
        QMessageBox::warning(this, QStringLiteral("自动提取"), err);
        return;
    }

    // 询问保存位置：默认覆盖/更新内置 fps-library
    const QString defaultDir = m_libraryDir.isEmpty()
        ? QCoreApplication::applicationDirPath() + QStringLiteral("/fps-library")
        : m_libraryDir;
    QString targetDir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择提取输出目录（默认更新素材库）"), defaultDir);
    if (targetDir.isEmpty()) {
        if (m_extractInfo)
            m_extractInfo->setText(QStringLiteral("已提取 %1 份（未保存）").arg(items.size()));
        return;
    }

    // 去重：统计各机型
    QMap<QString,int> byMach;
    for (const ExtractedFps &it : items) byMach[it.machine]++;

    QStringList written;
    if (!FpsExtractor::saveAll(items, targetDir, &written, &err)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), err);
        return;
    }

    // 重建 index.json（素材库格式），供素材库面板读取
    {
        QJsonArray profiles;
        int sla = 0, sls = 0;
        for (const ExtractedFps &it : items) {
            QJsonObject o;
            const bool isSla = (it.machine != QStringLiteral("FUSX-1-0"));
            o.insert(QStringLiteral("file"), QStringLiteral("%1__%2__%3mm__%4.fps")
                     .arg(it.machine, it.material).arg(it.thickness)
                     .arg(it.name.isEmpty()?QStringLiteral("Default"):it.name));
            o.insert(QStringLiteral("machine_type_id"), it.machine);
            o.insert(QStringLiteral("machine_name"), PhysicsLibrary::machineDisplayName(it.machine));
            o.insert(QStringLiteral("is_sla"), isSla);
            o.insert(QStringLiteral("material_code"), it.material);
            o.insert(QStringLiteral("layer_thickness_mm"), it.thickness);
            o.insert(QStringLiteral("name"), it.name);
            o.insert(QStringLiteral("source"), 0);
            if (isSla) ++sla; else ++sls;
            profiles.append(o);
        }
        QJsonObject idx;
        idx.insert(QStringLiteral("generated_from"), exe);
        idx.insert(QStringLiteral("total"), items.size());
        idx.insert(QStringLiteral("sla_total"), sla);
        idx.insert(QStringLiteral("sls_total"), sls);
        idx.insert(QStringLiteral("profiles"), profiles);
        const QString idxPath = QDir(targetDir).filePath(QStringLiteral("index.json"));
        QFile f(idxPath);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(QJsonDocument(idx).toJson(QJsonDocument::Indented));
            f.close();
        }
    }

    // 让素材库指向新目录并刷新
    m_libraryDir = targetDir;
    refreshLibrary();

    QString machSummary;
    for (auto it = byMach.begin(); it != byMach.end(); ++it)
        machSummary += QStringLiteral("%1:%2  ").arg(it.key()).arg(it.value());
    if (m_extractInfo)
        m_extractInfo->setText(QStringLiteral("已从 %1 提取 %2 份（已去重），保存到 %3\n%4")
                               .arg(QFileInfo(exe).fileName()).arg(items.size()).arg(targetDir, machSummary));
    statusBar()->showMessage(QStringLiteral("已提取并去重 %1 份 FPS，素材库已更新").arg(items.size()), 9000);
}

