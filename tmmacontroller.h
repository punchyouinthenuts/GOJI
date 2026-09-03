#ifndef TMMACONTROLLER_H
#define TMMACONTROLLER_H

#include "basetrackercontroller.h"

#include <QPointer>
#include <QProcess>
#include <QString>

class DropWindow;
class MeterRateService;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSqlTableModel;
class QTextBrowser;
class QTextEdit;
class QToolButton;
class TMMAEmailDialog;
class TMMAFileManager;
class TMMADBManager;
class ScriptRunner;
struct TMMAJobState;

class TMMAController : public BaseTrackerController
{
    Q_OBJECT

public:
    explicit TMMAController(QObject* parent = nullptr);
    ~TMMAController() override;

    void initializeUI(QLineEdit* jobNumberBox,
                      QComboBox* yearDropdown,
                      QComboBox* monthDropdown,
                      QToolButton* lockButton,
                      QToolButton* editButton,
                      QToolButton* postageLockButton,
                      DropWindow* dropWindow,
                      QPushButton* runInitialButton,
                      QPushButton* openBulkMailerButton,
                      QComboBox* classDropdown,
                      QComboBox* permitDropdown,
                      QLineEdit* postageBox,
                      QLineEdit* countBox,
                      QTableView* tracker,
                      QTextEdit* terminalWindow,
                      QTextBrowser* textBrowser,
                      QPushButton* finalStepButton);

    bool loadJob(const QString& jobNumber, const QString& year, const QString& month);
    bool saveJobState();
    bool autoSaveAndCloseCurrentJob();
    void resetToDefaults();

    QString getJobNumber() const;
    QString getYear() const;
    QString getMonth() const;
    bool isJobDataLocked() const;
    bool isPostageDataLocked() const;
    bool hasActiveJob() const;

    void outputToTerminal(const QString& message, MessageType type) override;
    QTableView* getTrackerWidget() const override;
    QSqlTableModel* getTrackerModel() const override;
    QStringList getTrackerHeaders() const override;
    QList<int> getVisibleColumns() const override;
    QString formatCellData(int columnIndex, const QString& cellData) const override;
    QString formatCellDataForCopy(int columnIndex, const QString& cellData) const override;

signals:
    void jobOpened();
    void jobClosed();

private slots:
    void onJobDataLockClicked();
    void onEditButtonClicked();
    void onPostageLockClicked();
    void onRunInitialClicked();
    void onOpenBulkMailerClicked();
    void onFinalStepClicked();
    void onScriptOutput(const QString& output);
    void onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onFilesDropped(const QStringList& filePaths);
    void onFileDropError(const QString& errorMessage);
    void calculateMeteredPostage();
    void showTableContextMenu(const QPoint& position);

private:
    enum HtmlDisplayState {
        DefaultState = 0,
        InstructionsState = 1
    };

    bool validateJobData() const;
    bool validatePostageData() const;
    bool hasActiveIdentity() const;
    bool parsePositiveCount(qint64& count) const;
    bool parsePostage(double& postage) const;
    TMMAJobState currentJobState(bool jobDataLocked) const;
    bool recoverPendingIdentityMigration(bool* recoveredNewIdentity = nullptr);
    void blockForIdentityRecovery(const QString& errorMessage);
    void finishSuccessfulLock(const QString& jobNumber,
                              const QString& year,
                              const QString& month,
                              bool identityChanged);
    bool addOrUpdateLogEntry();
    bool startScript(const QString& scriptName, const QStringList& arguments = {});
    void setupDropWindow();
    void setupTrackerModel();
    void setupOptimizedTableLayout();
    void applyTrackerHeaders();
    void updateControlStates();
    void updateHtmlDisplay();
    void loadHtmlFile(const QString& resourcePath);
    void refreshTracker();
    void showFinalEmailDialog(const QString& outputFilePath,
                              const QString& mergedFilePath);
    void clearActiveIdentity();

    TMMAFileManager* m_fileManager;
    TMMADBManager* m_dbManager;
    ScriptRunner* m_scriptRunner;
    MeterRateService* m_meterRateService;

    QLineEdit* m_jobNumberBox;
    QComboBox* m_yearDropdown;
    QComboBox* m_monthDropdown;
    QToolButton* m_lockButton;
    QToolButton* m_editButton;
    QToolButton* m_postageLockButton;
    DropWindow* m_dropWindow;
    QPushButton* m_runInitialButton;
    QPushButton* m_openBulkMailerButton;
    QComboBox* m_classDropdown;
    QComboBox* m_permitDropdown;
    QLineEdit* m_postageBox;
    QLineEdit* m_countBox;
    QTableView* m_tracker;
    QTextEdit* m_terminalWindow;
    QTextBrowser* m_textBrowser;
    QPushButton* m_finalStepButton;
    QSqlTableModel* m_trackerModel;

    bool m_uiInitialized;
    bool m_jobDataLocked;
    bool m_postageDataLocked;
    bool m_scriptRunning;
    bool m_meterRateErrorShown;
    bool m_identityRecoveryBlocked;
    HtmlDisplayState m_htmlDisplayState;
    QString m_activeJobNumber;
    QString m_activeYear;
    QString m_activeMonth;
    QString m_activeScript;
    QString m_lastExecutedScript;
    QString m_finalOutputFilePath;
    QString m_finalMergedFilePath;
    QPointer<TMMAEmailDialog> m_emailDialog;
};

#endif // TMMACONTROLLER_H
