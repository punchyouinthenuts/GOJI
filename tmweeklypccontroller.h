#ifndef TMWEEKLYPCCONTROLLER_H
#define TMWEEKLYPCCONTROLLER_H

#include <QObject>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QTextEdit>
#include <QTableView>
#include <QTableWidget>
#include <QTextBrowser>
#include <QCheckBox>
#include <QSqlTableModel>
#include <QTimer>
#include "databasemanager.h"
#include "tmweeklypcdbmanager.h"
#include "scriptrunner.h"
#include "tmweeklypcfilemanager.h"
#include "naslinkdialog.h"

class TMWeeklyPCController : public QObject
{
    Q_OBJECT

public:
    // Message type enum for colored terminal output
    enum MessageType {
        Info,
        Warning,
        Error,
        Success
    };

    // HTML display states
    enum HtmlDisplayState {
        UninitializedState,  // Initial state before any HTML is loaded
        DefaultState,
        ProofState,
        PrintState
    };

    explicit TMWeeklyPCController(QObject *parent = nullptr);
    ~TMWeeklyPCController();

    // Initialize with UI elements from mainwindow
    void initializeUI(
        QPushButton* runInitialBtn, QPushButton* openBulkMailerBtn,
        QPushButton* runProofDataBtn, QPushButton* openProofFilesBtn,
        QPushButton* runWeeklyMergedBtn, QPushButton* openPrintFilesBtn,
        QPushButton* runPostPrintBtn, QToolButton* lockBtn, QToolButton* editBtn,
        QToolButton* postageLockBtn, QComboBox* proofDDbox, QComboBox* printDDbox,
        QComboBox* yearDDbox, QComboBox* monthDDbox, QComboBox* weekDDbox,
        QComboBox* classDDbox, QComboBox* permitDDbox, QLineEdit* jobNumberBox,
        QLineEdit* postageBox, QLineEdit* countBox, QTextEdit* terminalWindow,
        QTableView* tracker, QTextBrowser* textBrowser, QCheckBox* proofApprovalCheckBox
        );

    // Load saved data
    bool loadJob(const QString& year, const QString& month, const QString& week);

    void setTextBrowser(QTextBrowser* textBrowser);

    void resetToDefaults();

private slots:
    // Button handlers
    void onLockButtonClicked();
    void onEditButtonClicked();
    void onPostageLockButtonClicked();
    void onRunInitialClicked();
    void onOpenBulkMailerClicked();
    void onRunProofDataClicked();
    void onOpenProofFileClicked();
    void onRunWeeklyMergedClicked();
    void onOpenPrintFileClicked();
    void onRunPostPrintClicked();

    // Dropdown handlers
    void onYearChanged(const QString& year);
    void onMonthChanged(const QString& month);
    void onClassChanged(const QString& mailClass);

    // Checkbox handlers
    void onProofApprovalChanged(bool checked);

    // Table context menu
    void showTableContextMenu(const QPoint& pos);

    // Script signals
    void onScriptStarted();
    void onScriptOutput(const QString& output);
    void onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void calculateMeterPostage();

signals:
    void jobOpened();
    void jobClosed();

private:
    // UI element pointers
    QPushButton* m_runInitialBtn = nullptr;
    QPushButton* m_openBulkMailerBtn = nullptr;
    QPushButton* m_runProofDataBtn = nullptr;
    QPushButton* m_openProofFileBtn = nullptr;
    QPushButton* m_runWeeklyMergedBtn = nullptr;
    QPushButton* m_openPrintFileBtn = nullptr;
    QPushButton* m_runPostPrintBtn = nullptr;
    QToolButton* m_lockBtn = nullptr;
    QToolButton* m_editBtn = nullptr;
    QToolButton* m_postageLockBtn = nullptr;
    QComboBox* m_proofDDbox = nullptr;
    QComboBox* m_printDDbox = nullptr;
    QComboBox* m_yearDDbox = nullptr;
    QComboBox* m_monthDDbox = nullptr;
    QComboBox* m_weekDDbox = nullptr;
    QComboBox* m_classDDbox = nullptr;
    QComboBox* m_permitDDbox = nullptr;
    QLineEdit* m_jobNumberBox = nullptr;
    QLineEdit* m_postageBox = nullptr;
    QLineEdit* m_countBox = nullptr;
    QTextEdit* m_terminalWindow = nullptr;
    QTableView* m_tracker = nullptr;
    QTextBrowser* m_textBrowser = nullptr;
    QCheckBox* m_proofApprovalCheckBox = nullptr;

    // Support objects
    DatabaseManager* m_dbManager = nullptr;
    TMWeeklyPCDBManager* m_tmWeeklyPCDBManager = nullptr;
    ScriptRunner* m_scriptRunner = nullptr;
    QSqlTableModel* m_trackerModel = nullptr;
    TMWeeklyPCFileManager* m_fileManager = nullptr;

    // State variables
    bool m_jobDataLocked = false;
    bool m_postageDataLocked = false;
    HtmlDisplayState m_currentHtmlState = UninitializedState;

    // Script output parsing variables
    QString m_capturedNASPath;     // Stores the NAS path from script output
    bool m_capturingNASPath;       // Flag to indicate we're capturing NAS path
    QString m_lastExecutedScript;  // Track which script was last executed

    // Utility methods
    void connectSignals();
    void setupInitialUIState();
    void setupOptimizedTableLayout();
    void populateDropdowns();
    void populateWeekDDbox();
    void formatPostageInput();
    bool validateJobData();
    bool validatePostageData();
    void updateControlStates();
    void updateHtmlDisplay();
    void loadHtmlFile(const QString& resourcePath);
    HtmlDisplayState determineHtmlState() const;
    void saveJobState();
    void loadJobState();
    void savePostageData();
    void loadPostageData();
    void outputToTerminal(const QString& message, MessageType type = Info);
    void createBaseDirectories();
    void createJobFolder();
    void saveJobToDatabase();
    void addLogEntry();
    QString copyFormattedRow();
    double getMeterRateFromDatabase();
    void refreshTrackerTable();

    bool createExcelAndCopy(const QStringList& headers, const QStringList& rowData);

    // Script output parsing methods
    void parseScriptOutput(const QString& output);
    void showNASLinkDialog();
};

#endif // TMWEEKLYPCCONTROLLER_H
