#ifndef GOJI_H
#define GOJI_H

#include <QMainWindow>
#include <QFileSystemWatcher>
#include <QTimer>
#include <functional>
#include <QMap>
#include <QFile>
#include <QMessageBox>
#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCheckBox>
#include <QRegularExpressionValidator>
#include <QPair>
#include <QMenu>
#include <QSettings>
#include <array>

#include "ui_GOJI.h"

class Goji : public QMainWindow {
    Q_OBJECT

public:
    Goji(QWidget *parent = nullptr);
    ~Goji();

private slots:
    void onOpenIZClicked();
    void onRunInitialClicked();
    void onRunPreProofClicked();
    void onOpenProofFilesClicked();
    void onRunPostProofClicked();
    void onOpenPrintFilesClicked();
    void onRunPostPrintClicked();
    void onLockButtonToggled(bool checked);
    void onEditButtonToggled(bool checked);
    void onProofRegenToggled(bool checked);
    void onPostageLockToggled(bool checked);
    void onProofDDboxChanged(const QString &text);
    void onPrintDDboxChanged(const QString &text);
    void onYearDDboxChanged(const QString &text);
    void onMonthDDboxChanged(const QString &text);
    void onWeekDDboxChanged(const QString &text);
    void onAllCBStateChanged(Qt::CheckState state);
    void updateAllCBState();
    void onActionExitTriggered();
    void onActionCloseJobTriggered();
    void onActionSaveJobTriggered();
    void onCheckForUpdatesTriggered();
    void onPrintDirChanged(const QString &path);
    void formatCurrencyOnFinish();
    void onGetCountTableClicked();
    void onRegenProofButtonClicked();
    void buildWeeklyMenu();
    void openJobFromWeekly(int year, int month, int week);
    void openJobFromWeekly(const QString& year, const QString& month, const QString& week);
    void onInactivityTimeout();

private:
    Ui::MainWindow *ui;
    QMenu *openJobMenu;
    QMenu *weeklyMenu;
    QStringList m_printDirs;
    QFileSystemWatcher *m_printWatcher;
    QTimer *m_inactivityTimer;
    QMap<QString, QStringList> proofFiles;
    QMap<QString, QStringList> printFiles;
    QMap<QString, QCheckBox*> regenCheckboxes;
    QMap<QCheckBox*, QPair<QString, QString>> checkboxFileMap;
    QSqlDatabase db;
    bool isJobSaved;
    QString originalYear;
    QString originalMonth;
    QString originalWeek;
    bool isPostageLocked;
    bool isProofRegenMode;
    QRegularExpressionValidator *validator;
    bool isOpenIZComplete;
    bool isRunInitialComplete;
    bool isRunPreProofComplete;
    bool isOpenProofFilesComplete;
    bool isRunPostProofComplete;
    bool isOpenPrintFilesComplete;
    bool isRunPostPrintComplete;
    bool isJobDataLocked;
    static constexpr size_t NUM_STEPS = 9;
    std::array<double, NUM_STEPS> stepWeights;
    std::array<int, NUM_STEPS> totalSubtasks;
    std::array<int, NUM_STEPS> completedSubtasks;
    QSettings *settings;

    void logToTerminal(const QString &message);
    void clearJobNumbers();
    QString getProofFolderPath(const QString &jobType);
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
    void updateWidgetStatesBasedOnJobState();
    void updateProgressBar();
    bool jobExists(const QString& year, const QString& month, const QString& week);
    void insertJob();
    void updateJob();
    void deleteJob(const QString& year, const QString& month, const QString& week);
    void savePostProofCounts();
    void runProofRegenScript(const QString& jobType, const QStringList& files, int version);
    QString getJobNumberForJobType(const QString& jobType);
    void copyFilesFromHomeToWorking(const QString& month, const QString& week);
    void createJobFolders(const QString& year, const QString& month, const QString& week);
    void copyFilesToWorkingFolders(const QString& month, const QString& week);
    void moveFilesToHomeFolders(const QString& month, const QString& week);
    void initWatchersAndTimers();
    void checkPrintFiles(const QString &selection);
    void checkProofFiles(const QString &selection);
};

#endif // GOJI_H
