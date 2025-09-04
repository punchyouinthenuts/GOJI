#ifndef TMFARMCONTROLLER_H
#define TMFARMCONTROLLER_H

#include "basetrackercontroller.h"
#include "databasemanager.h"
#include "tmfarmdbmanager.h"
#include "tmfarmfilemanager.h"
#include "scriptrunner.h"

#include <QObject>
#include <QPushButton>
#include <QToolButton>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QTableView>
#include <QTextBrowser>
#include <QPoint>
#include <QSqlTableModel>
#include <QSqlRecord>
#include <QSqlField>
#include <QRegularExpressionValidator>
#include <QClipboard>
#include <QMenu>
#include <QAction>

/**
 * @brief Controller for TM FARMWORKERS tab
 *
 * Handles UI interactions, DB persistence, file management, and script execution.
 * FARMWORKERS uses quarter-based jobs (e.g., ARCHIVE/12345_3RD2025).
 */
class TMFarmController : public BaseTrackerController
{
    Q_OBJECT
public:
    explicit TMFarmController(QObject *parent = nullptr);
    ~TMFarmController();

    void initializeUI(QPushButton* openBulkMailerBtn, QPushButton* runInitialBtn,
                      QPushButton* finalStepBtn, QToolButton* lockBtn, QToolButton* editBtn,
                      QToolButton* postageLockBtn, QComboBox* yearDDbox, QComboBox* quarterDDbox,
                      QLineEdit* jobNumberBox, QLineEdit* postageBox, QLineEdit* countBox,
                      QTextEdit* terminalWindow, QTableView* tracker, QTextBrowser* textBrowser);

    void setTextBrowser(QTextBrowser* textBrowser);

    void saveJobState();
    void loadJobState();
    void saveJobToDatabase();
    bool loadJob(const QString& year, const QString& quarter);
    void addLogEntry();
    void resetToDefaults();

    void onYearChanged(const QString& year);
    void onMonthChanged(const QString& quarter);

    void onLockButtonClicked();
    void onEditButtonClicked();
    void onPostageLockButtonClicked();
    void onOpenBulkMailerClicked();
    void onRunInitialClicked();
    void onFinalStepClicked();
    void onScriptOutput(const QString& output);
    void onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus);

    void onSaveJobClicked();
    void onCloseJobClicked();
    void refreshTrackerTable();
    void autoSaveAndCloseCurrentJob();

    void applySavedHtmlState();

    QTableView* getTrackerWidget() const override;
    QSqlTableModel* getTrackerModel() const override;
    QStringList getTrackerHeaders() const override;
    QList<int> getVisibleColumns() const override;
    QString formatCellData(int columnIndex, const QString& cellData) const override;
    QString formatCellDataForCopy(int columnIndex, const QString& cellData) const override;
    void createBaseDirectories();
    void createJobFolder();
    void parseScriptOutput(const QString& output);
    void showFarmEmailDialog(const QString& networkPath);
    QString convertMonthToAbbreviation(const QString& monthNumber) const;
    QString getJobDescription() const;
    bool hasJobData() const;
    void debugCheckTables();

    bool moveFilesToHomeFolder();
    bool copyFilesFromHomeFolder();
    bool moveFilesToBasicHomeFolder(const QString& year, const QString& quarter);

protected:
    void connectSignals();
    void setupInitialUIState();
    void populateDropdowns();
    void formatPostageInput();
    void formatCountInput(const QString& text);
    void updateControlStates();
    void updateHtmlDisplay();
    void loadHtmlFile(const QString& resourcePath);
    void setupOptimizedTableLayout();
    void showTableContextMenu(const QPoint& pos);
    bool validateJobNumber(const QString& jobNumber) const;

    enum HtmlDisplayState {
        UninitializedState,
        DefaultState,
        InstructionsState
    };

    HtmlDisplayState determineHtmlState() const;

    using BaseTrackerController::MessageType;
    void outputToTerminal(const QString& message, MessageType type = BaseTrackerController::Info) override;
private:
    DatabaseManager* m_dbManager;
    TMFarmFileManager* m_fileManager;
    TMFarmDBManager* m_tmFarmDBManager;
    ScriptRunner* m_scriptRunner;

    QPushButton* m_openBulkMailerBtn;
    QPushButton* m_runInitialBtn;
    QPushButton* m_finalStepBtn;
    QToolButton* m_lockBtn;
    QToolButton* m_editBtn;
    QToolButton* m_postageLockBtn;
    QComboBox* m_yearDDbox;
    QComboBox* m_quarterDDbox;
    QLineEdit* m_jobNumberBox;
    QLineEdit* m_postageBox;
    QLineEdit* m_countBox;
    QTextEdit* m_terminalWindow;
    QTableView* m_tracker;
    QTextBrowser* m_textBrowser;

    bool m_jobDataLocked;
    bool m_postageDataLocked;
    HtmlDisplayState m_currentHtmlState;
    QString m_lastExecutedScript;
    QString m_capturedNASPath;
    bool m_capturingNASPath;
    QString m_finalNASPath;
    QString m_cachedJobNumber;
    QSqlTableModel* m_trackerModel;
};

#endif // TMFARMCONTROLLER_H
