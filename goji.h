#ifndef GOJI_H
#define GOJI_H

#include <QMainWindow>
#include <QAbstractButton>
#include <QProgressBar>
#include <QScrollBar>
#include <QFileSystemWatcher>
#include <QTimer>
#include "QRecentFilesMenu.h"
#include <functional>
#include <QMap>
#include <QFile>
#include <QMessageBox>
#include <QCoreApplication>
#include <QStringList>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCheckBox>
#include <QRegExpValidator>
#include <QPair>
#include <QMenu>

#include "ui_GOJI.h"  // Include the UI header for full definition of Ui::MainWindow

class Goji : public QMainWindow
{
    Q_OBJECT

public:
    explicit Goji(QWidget *parent = nullptr);
    ~Goji();

private slots:
    // **Button handlers for QPushButton**
    void onOpenIZClicked();
    void onRunInitialClicked();
    void onRunPreProofClicked();
    void onOpenProofFilesClicked();
    void onRunPostProofClicked();
    void onOpenPrintFilesClicked();
    void onRunPostPrintClicked();

    // **Button handlers for QToolButton**
    void onLockButtonToggled(bool checked);
    void onEditButtonToggled(bool checked);
    void onProofRegenToggled(bool checked);
    void onPostageLockToggled(bool checked);

    // **ComboBox handlers**
    void onProofDDboxChanged(const QString &text);
    void onPrintDDboxChanged(const QString &text);
    void onYearDDboxChanged(const QString &text);
    void onMonthDDboxChanged(const QString &text);
    void onWeekDDboxChanged(const QString &text);

    // **Recent files handler**
    void openRecentFile(QAction* action);

    // **Checkbox handlers**
    void onAllCBStateChanged(int state);
    void updateAllCBState();

    // **New slots for file and script handling**
    void onPrintDirChanged(const QString &path);
    void checkPrintFiles(const QString& selection);  // Added with parameter
    void checkProofFiles(const QString& selection);  // Updated with parameter

    // **Slot for currency formatting**
    void formatCurrencyOnFinish();

    // **Slot for getting count table**
    void onGetCountTableClicked();

    // **Slot for proof regeneration**
    void onRegenProofButtonClicked();

    // **New slots for menu actions**
    void onActionExitTriggered();
    void onActionCloseJobTriggered();
    void onActionSaveJobTriggered();

    // **Dynamic menu slots**
    void buildWeeklyMenu();          // Builds the Weekly menu dynamically
    void openJobFromWeekly(int year, int month, int week);  // Opens the selected job
    void onCheckForUpdatesTriggered();  // Checks for application updates
private:
    Ui::MainWindow *ui;
    QRecentFilesMenu *recentFilesMenu;
    QMenu *openJobMenu;  // For the Open Job submenu
    QMenu *weeklyMenu;   // For the Weekly submenu

    // **Standard Qt widget pointers**
    QAbstractButton *openIZButton;
    QAbstractButton *runPreProofButton;
    QAbstractButton *openProofFilesButton;
    QAbstractButton *runPostProofButton;
    QAbstractButton *openPrintFilesButton;
    QAbstractButton *runPostPrintButton;
    QAbstractButton *lockButton;
    QAbstractButton *editButton;
    QAbstractButton *proofRegenButton;
    QProgressBar *progressBar;

    // **New members for file monitoring**
    QStringList m_printDirs;
    QFileSystemWatcher *m_printWatcher;
    QTimer *m_inactivityTimer;

    // **New members for file mappings**
    QMap<QString, QStringList> proofFiles;
    QMap<QString, QStringList> printFiles;
    QMap<QString, QCheckBox*> regenCheckboxes;
    QMap<QCheckBox*, QPair<QString, QString>> checkboxFileMap;

    // **Database and state management**
    QSqlDatabase db;
    bool isJobSaved;
    QString originalYear;
    QString originalMonth;
    QString originalWeek;
    bool isPostageLocked;
    bool isProofRegenMode;
    QRegExpValidator *validator;

    // **Process completion flags**
    bool isOpenIZComplete;
    bool isRunInitialComplete;
    bool isRunPreProofComplete;
    bool isOpenProofFilesComplete;
    bool isRunPostProofComplete;
    bool isOpenPrintFilesComplete;
    bool isRunPostPrintComplete;

    // **Weekly progress bar tracking**
    double stepWeights[9];    // Weights for each step in the weekly progress bar
    int totalSubtasks[9];     // Total subtasks for each step in the weekly process
    int completedSubtasks[9]; // Completed subtasks for each step in the weekly process

    // **State management for lock and edit buttons**
    bool isJobDataLocked;

    // **Helper methods**
    void logToTerminal(const QString &message);
    void clearJobNumbers();
    QString getProofFolderPath(const QString &jobType);
    void initializePrintFileMonitoring();
    void runScript(const QString &program, const QStringList &arguments);
    void ensureInDesignIsOpen(const std::function<void()>& callback);
    void openProofFiles(const QString& selection);
    void openPrintFiles(const QString& selection);
    void lockJobDataFields(bool lock);
    void lockPostageFields(bool lock);
    void updateLEDs();
    void updateButtonStates(bool enabled);
    int getNextProofVersion(const QString& filePath);
    void regenerateProofs();
    void enableProofApprovalCheckboxes();
    void updateWidgetStatesBasedOnJobState();  // New method for widget state management
    void updateProgressBar();                  // New method for updating the progress bar

    // **Database helper methods**
    bool jobExists(const QString& year, const QString& month, const QString& week);
    void insertJob();
    void updateJob();
    void deleteJob(const QString& year, const QString& month, const QString& week);

    // **New helper methods**
    void savePostProofCounts();
    void runProofRegenScript(const QString& jobType, const QStringList& files, int version); // Updated to include version
    QString getJobNumberForJobType(const QString& jobType);
    void copyFilesFromHomeToWorking(const QString& year, const QString& month, const QString& week); // Critical function not originally implemented

    // **Dynamic menu helpers**
    void buildWeeklyMenu(QMenu* menu);
    void openJobFromWeekly(const QString& year, const QString& month, const QString& week);

    // **New folder and file management helpers**
    void createJobFolders(const QString& year, const QString& month, const QString& week);
    void copyFilesToWorkingFolders(const QString& year, const QString& month, const QString& week);
    void moveFilesToHomeFolders(const QString& year, const QString& month, const QString& week);
};

#endif // GOJI_H
