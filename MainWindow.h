#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QMenu>
#include <QAction>
#include <QRegularExpressionValidator>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QMap>
#include <QCheckBox>
#include <QPair>

#include "jobcontroller.h"
#include "databasemanager.h"
#include "filesystemmanager.h"
#include "scriptrunner.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    // Menu actions
    void onActionExitTriggered();
    void onActionSaveJobTriggered();
    void onActionCloseJobTriggered();
    void onCheckForUpdatesTriggered();

    // Button actions
    void onOpenIZClicked();
    void onRunInitialClicked();
    void onRunPreProofClicked();
    void onOpenProofFilesClicked();
    void onRunPostProofClicked();
    void onOpenPrintFilesClicked();
    void onRunPostPrintClicked();
    void onGetCountTableClicked();
    void onRegenProofButtonClicked();

    // UI state changes
    void onProofDDboxChanged(const QString &text);
    void onPrintDDboxChanged(const QString &text);
    void onYearDDboxChanged(const QString &text);
    void onMonthDDboxChanged(const QString &text);
    void onWeekDDboxChanged(const QString &text);
    void onLockButtonToggled(bool checked);
    void onEditButtonToggled(bool checked);
    void onProofRegenToggled(bool checked);
    void onPostageLockToggled(bool checked);
    void onAllCBStateChanged(int state);
    void updateAllCBState();

    // Other utility slots
    void formatCurrencyOnFinish();
    void onPrintDirChanged(const QString &path);
    void onInactivityTimeout();
    void onLogMessage(const QString& message);
    void onJobProgressUpdated(int progress);

    // Script handling
    void onScriptStarted();
    void onScriptFinished(bool success);

private:
    Ui::MainWindow* ui;
    QSettings* m_settings;

    DatabaseManager* m_dbManager;
    FileSystemManager* m_fileManager;
    ScriptRunner* m_scriptRunner;
    JobController* m_jobController;

    QMenu* openJobMenu;
    QMenu* weeklyMenu;
    QRegularExpressionValidator* validator;
    QFileSystemWatcher* m_printWatcher;
    QTimer* m_inactivityTimer;

    QString currentJobType;
    QMap<QString, QCheckBox*> regenCheckboxes;
    QMap<QCheckBox*, QPair<QString, QString>> checkboxFileMap;

    void setupUi();
    void initializeValidators();
    void setupMenus();
    void setupSignalSlots();
    void setupRegenCheckboxes();
    void initWatchersAndTimers();

    void buildWeeklyMenu();
    void openJobFromWeekly(const QString& year, const QString& month, const QString& week);
    void populateScriptMenu(QMenu* menu, const QString& dirPath);
    void openScriptFile(const QString& filePath);

    void updateWidgetStatesBasedOnJobState();
    void updateLEDs();
    void populateWeekDDbox();

    void logToTerminal(const QString& message);
};

#endif // MAINWINDOW_H
