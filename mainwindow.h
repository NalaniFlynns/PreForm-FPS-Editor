#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QJsonValue>
#include <QVector>
#include <QStringList>

#include "fpsdocument.h"
#include "knoblibrary.h"
#include "physicslibrary.h"
#include "fpsextractor.h"

class QTreeWidget;
class QTreeWidgetItem;
class QTableWidget;
class QLineEdit;
class QLabel;
class QPushButton;
class QComboBox;
class QTabWidget;
class QCheckBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    void openFile(const QString &path);
    void setLibraryDir(const QString &dir);

private slots:
    void onOpen();
    void onSave();
    void onSaveAs();
    void onLibraryActivated();
    void onOpenFromLibrary();
    void onTreeSelectionChanged();
    void onApplyValue();
    void onRevertValue();
    void onMakeCustom();
    void onValidate();
    void onViewJson();
    void onCipherInfo();
    void onAbout();
    void onLibraryFilterChanged(const QString &text);
    void onKnobRefresh();
    void onKnobExportCsv();
    void onKnobApplyToPublicFields();
    void onKnobTableDoubleClicked(int row, int col);
    void onExtractFromPreForm();
    void onLocatePreForm();

    // 材料对比
    void onCompareLeftChanged();
    void onCompareRightChanged();
    void onCompareRun();
    void onCompareExportCsv();

    // F4 -> ST1 转换
    void onConvertMaterialChanged();
    void onConvertRun();
    void onConvertExportCsv();
    void onConvertApplyToDoc();

private:
    void buildUi();
    void buildMenus();
    void refreshLibrary();
    void refreshKnobParams();
    void refreshTree();
    void updateHeader();
    void markDirty();
    void addChild(QTreeWidgetItem *parent, const QVector<QJsonValue> &path,
                  const QJsonValue &value);
    QTreeWidgetItem *addValueItem(QTreeWidgetItem *parent, const QString &key,
                                  const QJsonValue &value, const QVector<QJsonValue> &path);
    void reselect(const QVector<QJsonValue> &path);
    QJsonValue resolvePath(const QVector<QJsonValue> &path, bool *ok) const;
    bool setPathValue(const QVector<QJsonValue> &path, const QJsonValue &value, QString *error);
    QString hintFor(const QVector<QJsonValue> &path, const QJsonValue &value) const;

    // 对比/转换的辅助
    void buildCompareTab(class QWidget *page);
    void buildConvertTab(class QWidget *page);
    void refreshCompareSelectors();
    void fillCompareTable();
    void fillConvertTable();
    static QString physRowSummary(const PhysicsRecord &r);
    static QString escapedCsv(const QString &s);
    bool writeTextFile(const QString &path, const QString &text, QString *error);

    FpsDocument m_doc;
    QString m_libraryDir;
    QStringList m_libraryFiles;

    QTreeWidget *m_tree = nullptr;
    QTableWidget *m_library = nullptr;
    QLineEdit *m_filter = nullptr;
    QLabel *m_fileLabel = nullptr;
    QLabel *m_stateLabel = nullptr;
    QLabel *m_pathLabel = nullptr;
    QLabel *m_typeLabel = nullptr;
    QLabel *m_hintLabel = nullptr;
    QLabel *m_libInfo = nullptr;
    QLabel *m_knobInfo = nullptr;
    QLabel *m_knobHint = nullptr;
    QLabel *m_extractInfo = nullptr;
    QString m_preFormPath;
    QTableWidget *m_knobTable = nullptr;
    KnobLibrary m_knobLib;
    QVector<KnobParam> m_knobParams;
    int m_knobSchema = -1;
    QLineEdit *m_valueEdit = nullptr;
    QPushButton *m_applyBtn = nullptr;
    QPushButton *m_revertBtn = nullptr;

    // 物理参数库（真实解密数据）
    PhysicsLibrary m_physLib;
    QTabWidget *m_rightTabs = nullptr;

    // 材料对比控件
    QComboBox *m_cmpLeftMachine = nullptr;
    QComboBox *m_cmpLeftMaterial = nullptr;
    QComboBox *m_cmpLeftThk = nullptr;
    QComboBox *m_cmpRightMachine = nullptr;
    QComboBox *m_cmpRightMaterial = nullptr;
    QComboBox *m_cmpRightThk = nullptr;
    QTableWidget *m_cmpTable = nullptr;
    QLabel *m_cmpInfo = nullptr;
    PhysicsRecord m_cmpLeftRec;
    PhysicsRecord m_cmpRightRec;
    bool m_cmpLeftValid = false, m_cmpRightValid = false;

    // 转换控件
    QComboBox *m_cvtMaterial = nullptr;
    QComboBox *m_cvtThk = nullptr;
    QTableWidget *m_cvtTable = nullptr;
    QLabel *m_cvtInfo = nullptr;
    ConvertedRecord m_cvtRec;
    bool m_cvtValid = false;

    QVector<QJsonValue> m_selectedPath;
    bool m_hasSelection = false;
};

#endif // MAINWINDOW_H
