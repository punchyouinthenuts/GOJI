#ifndef GOJI_H
#define GOJI_H

#include <QMainWindow>
#include <QMap>
#include <QSettings>
#include <QSqlDatabase>
#include <QString>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QMenu>
#include <QRegularExpressionValidator>
#include <QCheckBox>
#include <QPair>
#include <array>
#include <functional>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class Goji : public QMainWindow
{
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
    void onActionExitTriggered();
    void onProofDDboxChanged(const QString &text);
    void onPrintDDboxChanged(const QString &text);
    void onYearDDboxChanged(const QString &text);
    void onMonthDDboxChanged(const QString &text);
    void onWeekDDboxChanged(const QString &text);
    void onLockButtonToggled(bool checked);
    void onEditButtonToggled(bool checked);
    void onProofRegenToggled(bool checked);
    void onPostageLockToggled(bool checked);
    void onActionCloseJobTriggered();
    void onActionSaveJobTriggered();
    void onCheckForUpdatesTriggered();
    void onPrintDirChanged(const QString &path);
    void formatCurrencyOnFinish();
    void onGetCountTableClicked();
    void onRegenProofButtonClicked();
    void onInactivityTimeout();
    void onAllCBStateChanged(int state);
    void updateAllCBState();

private:
    void logToTerminal(const QString &message);
    void runScript(const QString &program, const QStringList &arguments);
    void checkProofFiles(const QString &selection);
    void checkPrintFiles(const QString &selection);
    void regenerateProofs();
    int getNextProofVersion(const QString& filePath);
    void runProofRegenScript(const QString& jobType, const QStringList& files, int version);
    void insertJob();
    void updateJob();
    void deleteJob(const QString& year, const QString& month, const QString& week);
    bool jobExists(const QString& year, const QString& month, const QString& week);
    bool confirmOverwrite(const QString& year, const QString& month, const QString& week);
    void buildWeeklyMenu();
    void openJobFromWeekly(const QString& year, const QString& month, const QString& week);
    void copyFilesFromHomeToWorking(const QString& month, const QString& week);
    void copyFilesToWorkingFolders(const QString& month, const QString& week);
    void moveFilesToHomeFolders(const QString& month, const QString& week);
    void savePostProofCounts();
    void updateLEDs();
    void enableProofApprovalCheckboxes();
    void lockJobDataFields(bool lock);
    void lockPostageFields(bool lock);
    void updateWidgetStatesBasedOnJobState();
    void initWatchersAndTimers();
    void clearJobNumbers();
    QString getProofFolderPath(const QString &jobType);
    void ensureInDesignIsOpen(const std::function<void()>& callback);
    void updateButtonStates(bool enabled);
    void openProofFiles(const QString& selection);
    void openPrintFiles(const QString& selection);
    QString getJobNumberForJobType(const QString& jobType);
    void createJobFolders(const QString& year, const QString& month, const QString& week);
    void updateProgressBar();
    void populateWeekDDbox();

    Ui::MainWindow *ui;
    QSqlDatabase db;
    QMenu *openJobMenu;
    QMenu *weeklyMenu;
    QRegularExpressionValidator *validator;
    QFileSystemWatcher *m_printWatcher;
    QTimer *m_inactivityTimer;
    QSettings *settings;
    QString currentJobType;
    QString originalYear;
    QString originalMonth;
    QString originalWeek;
    bool isJobSaved;
    bool isJobDataLocked;
    bool isOpenIZComplete;
    bool isRunInitialComplete;
    bool isRunPreProofComplete;
    bool isOpenProofFilesComplete;
    bool isRunPostProofComplete;
    bool isOpenPrintFilesComplete;
    bool isRunPostPrintComplete;
    bool isProofRegenMode;
    bool isPostageLocked;
    QMap<QString, QStringList> proofFiles;
    QMap<QString, QStringList> printFiles;
    QMap<QString, QCheckBox*> regenCheckboxes;
    QMap<QCheckBox*, QPair<QString, QString>> checkboxFileMap;
    std::array<double, 9> stepWeights;
    std::array<int, 9> totalSubtasks;
    std::array<int, 9> completedSubtasks;

    static constexpr size_t NUM_STEPS = 9;
};

#endif // GOJI_H
