#ifndef AILICONTROLLER_H
#define AILICONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QProcess>

#include "ailidbmanager.h"

class MainWindow;
class QTimer;

namespace Ui
{
class MainWindow;
}

struct AILIJobData
{
    QString jobNumber;
    QString issueNumber;
    int month;
    int year;
    QString version;
    int pageCount;
    double postage;
    int count;
};

class AILIFileManager;
class AILIEmailDialog;

/*
 * AILIController
 *
 * Controls the full AILI tab workflow:
 *
 * 1) Accept dropped source XLSX file
 * 2) Detect version from filename (SPOTLIGHT / AO SPOTLIGHT)
 * 3) Lock and save required job metadata
 * 4) Run Script 01 to create INPUT CSV files
 * 5) Allow Bulk Mailer launch / manual external processing
 * 6) Lock postage + count and update the database row
 * 7) Run Script 02
 * 8) Present the final email dialog
 * 9) Continue archive flow
 * 10) Reset the AILI UI manually or by timer
 *
 * This controller owns the AILI-specific workflow state and delegates
 * persistence and filesystem concerns to:
 *
 * - AILIDBManager
 * - AILIFileManager
 */

class AILIController : public QObject
{
    Q_OBJECT

public:
    explicit AILIController(MainWindow *mainWindow,
                            Ui::MainWindow *ui,
                            QObject *parent = nullptr);
    ~AILIController() override;

    bool initializeAfterConstruction();

    void resetJob();

    bool hasActiveJob() const;

signals:
    void jobOpened();
    void jobClosed();

private slots:
    void handleDropWindowFileDropped(const QString &filePath);
    void handleLockButtonClicked();
    void handleRunInitialClicked();
    void handleOpenBulkMailerClicked();
    void handlePostageLockClicked();
    void handleFinalStepClicked();

    void handleScriptStarted();
    void handleScriptFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleScriptErrorOccurred(QProcess::ProcessError error);
    void handleScriptReadyReadStandardOutput();
    void handleScriptReadyReadStandardError();

    void handleAutoResetTimeout();

private:
    enum PendingAction
    {
        NoPendingAction,
        PendingInitialProcess,
        PendingFinalProcess
    };

    bool initializeManagers();
    void initializeUiState();
    void initializeValidators();
    void connectSignals();
    void ensureVersionBoxReadOnly();

    bool validateDroppedFile(const QString &filePath) const;
    QString detectVersionFromFilename(const QString &filePath) const;
    bool captureDroppedFile(const QString &filePath);

    bool validateJobMetadata(QString &errorMessage) const;
    bool validatePostageAndCount(QString &errorMessage) const;

    AILIJobData buildCurrentJobData() const;
    bool persistInitialJob();
    bool persistPostageAndCount();

    void lockJobMetadataFields(bool locked);
    void lockPostageFields(bool locked);

    void updateButtonStates();
    void loadDefaultHtml();
    void loadInstructionHtmlForVersion(const QString &version);
    void appendTerminalMessage(const QString &message);
    void appendTerminalError(const QString &message);
    void clearTerminal();
    void clearDisplayedResults();

    QString script01Path() const;
    QString script02Path() const;

    QString pythonExecutablePath() const;
    bool startPythonScript(const QString &scriptPath,
                           const QStringList &arguments,
                           PendingAction pendingAction);

    QString currentJobNumber() const;
    QString currentIssueNumber() const;
    int currentMonth() const;
    int currentYear() const;
    int currentPageCount() const;
    QString currentVersion() const;
    int currentCount() const;
    double currentPostage() const;

    bool openBulkMailerIfNeeded();

    QVector<QStringList> buildEmailTableDataFromFinalProcess() const;
    bool showEmailDialogAndWait(const QVector<QStringList> &tableData,
                                const QString &invalidAddressFilePath);

    void beginAutoResetTimer();
    void stopAutoResetTimer();

    void setJobActive(bool active);
    void setOriginalFilePath(const QString &path);
    void setDetectedVersion(const QString &version);

    MainWindow *m_mainWindow;
    Ui::MainWindow *m_ui;

    AILIDBManager *m_dbManager;
    AILIFileManager *m_fileManager;
    AILIEmailDialog *m_emailDialog;

    QProcess *m_scriptProcess;
    QTimer *m_autoResetTimer;

    PendingAction m_pendingAction;

    bool m_jobActive;
    bool m_initialLocked;
    bool m_postageLocked;

    QString m_originalFilePath;
    QString m_detectedVersion;
};

#endif // AILICONTROLLER_H
