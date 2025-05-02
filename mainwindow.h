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
#include <QCloseEvent>
#include <QFile>
#include <QTextStream>
#include <QFontDatabase>

#include "jobcontroller.h"
#include "databasemanager.h"
#include "filesystemmanager.h"
#include "scriptrunner.h"
#include "updatemanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// Enum to track the current instruction state
enum class InstructionState {
    None,           // No job loaded
    Default,        // Default view when no job loaded but RAC WEEKLY tab is active
    Initial,        // Job created until runPreProof clicked
    PreProof,       // After runPreProof clicked until runPostProof clicked
    PostProof,      // After runPostProof clicked until allCB checked
    Final           // After allCB checked
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // Menu actions
    void onActionExitTriggered();
    void onActionSaveJobTriggered();
    void onActionCloseJobTriggered();
    void onCheckForUpdatesTriggered();
    void onUpdateSettingsTriggered();
    void onRegenerateEmailClicked();

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
    void onAllCBcheckStateChanged(int state);
    void updateAllCBState();

    // Other utility slots
    void formatCurrencyOnFinish();
    void onPrintDirChanged(const QString &path);
    void onInactivityTimeout();
    void onLogMessage(const QString& message);
    void onJobProgressUpdated(int progress);
    void fixCurrentPostProofState();

    // Script handling
    void onScriptStarted();
    void onScriptFinished(bool success);

    // Instructions handling
    void updateInstructions();

    // Bug Nudge actions
    void onForcePreProofComplete();
    void onForceProofFilesComplete();
    void onForcePostProofComplete();
    void onForceProofApprovalComplete();
    void onForcePrintFilesComplete();
    void onForcePostPrintComplete();

    // Helper to update Bug Nudge menu based on current tab
    void updateBugNudgeMenu();

    // Bug Nudge menu setup
    void setupBugNudgeMenu();

private:
    Ui::MainWindow* ui;
    QSettings* m_settings;

    DatabaseManager* m_dbManager;
    FileSystemManager* m_fileManager;
    ScriptRunner* m_scriptRunner;
    JobController* m_jobController;
    UpdateManager* m_updateManager;

    QMenu* openJobMenu;
    QMenu* weeklyMenu;
    QRegularExpressionValidator* validator;
    QFileSystemWatcher* m_printWatcher;
    QTimer* m_inactivityTimer;

    QString currentJobType;
    QMap<QString, QCheckBox*> regenCheckboxes;
    QMap<QCheckBox*, QPair<QString, QString>> checkboxFileMap;

    // Instructions state tracking
    InstructionState m_currentInstructionState;
    QMap<InstructionState, QString> m_instructionFiles;

    // Bug Nudge menu items
    QMenu* m_bugNudgeMenu;
    QAction* m_forcePreProofAction;
    QAction* m_forceProofFilesAction;
    QAction* m_forcePostProofAction;
    QAction* m_forceProofApprovalAction;
    QAction* m_forcePrintFilesAction;
    QAction* m_forcePostPrintAction;

    void setupUi();
    void initializeValidators();
    void setupMenus();
    void setupSignalSlots();
    void setupRegenCheckboxes();
    void initWatchersAndTimers();
    void initializeInstructions();
    void loadInstructionContent(InstructionState state);

    void buildWeeklyMenu();
    void openJobFromWeekly(const QString& year, const QString& month, const QString& week);
    void populateScriptMenu(QMenu* menu, const QString& dirPath);
    void openScriptFile(const QString& filePath);

    void updateWidgetStatesBasedOnJobState();
    void updateLEDs();
    void populateWeekDDbox();

    void logToTerminal(const QString& message);
    bool closeAllJobs();

    // Helper to determine current instruction state from job state
    InstructionState determineInstructionState();
};

#endif // MAINWINDOW_H
