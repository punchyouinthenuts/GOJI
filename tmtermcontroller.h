#ifndef TMTERMCONTROLLER_H
#define TMTERMCONTROLLER_H

#include <QObject>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QTextEdit>
#include <QTableView>
#include <QTextBrowser>
#include <QSqlTableModel>
#include <QTimer>
#include "databasemanager.h"
#include "tmtermdbmanager.h"
#include "scriptrunner.h"
#include "tmtermfilemanager.h"
#include "naslinkdialog.h"

class TMTermController : public QObject
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
        DefaultState
    };

    explicit TMTermController(QObject *parent = nullptr);
    ~TMTermController();

    // Initialize with UI elements from mainwindow
    void initializeUI(
        QPushButton* openBulkMailerBtn, QPushButton* runInitialBtn,
        QPushButton* finalStepBtn, QToolButton* lockBtn, QToolButton* editBtn,
        QToolButton* postageLockBtn, QComboBox* yearDDbox, QComboBox* monthDDbox,
        QLineEdit* jobNumberBox, QLineEdit* postageBox, QLineEdit* countBox,
        QTextEdit* terminalWindow, QTableView* tracker, QTextBrowser* textBrowser
        );

    // Load saved data
    bool loadJob(const QString& year, const QString& month);

    void setTextBrowser(QTextBrowser* textBrowser);

private slots:
    // Button handlers
    void onLockButtonClicked();
    void onEditButtonClicked();
    void onPostageLockButtonClicked();
    void onOpenBulkMailerClicked();
    void onRunInitialClicked();
    void onFinalStepClicked();

    // Dropdown handlers
    void onYearChanged(const QString& year);
    void onMonthChanged(const QString& month);

    // Table context menu
    void showTableContextMenu(const QPoint& pos);

    // Script signals
    void onScriptOutput(const QString& output);
    void onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    // UI element pointers
    QPushButton* m_openBulkMailerBtn = nullptr;
    QPushButton* m_runInitialBtn = nullptr;
    QPushButton* m_finalStepBtn = nullptr;
    QToolButton* m_lockBtn = nullptr;
    QToolButton* m_editBtn = nullptr;
    QToolButton* m_postageLockBtn = nullptr;
    QComboBox* m_yearDDbox = nullptr;
    QComboBox* m_monthDDbox = nullptr;
    QLineEdit* m_jobNumberBox = nullptr;
    QLineEdit* m_postageBox = nullptr;
    QLineEdit* m_countBox = nullptr;
    QTextEdit* m_terminalWindow = nullptr;
    QTableView* m_tracker = nullptr;
    QTextBrowser* m_textBrowser = nullptr;

    // Support objects
    DatabaseManager* m_dbManager = nullptr;
    TMTermDBManager* m_tmTermDBManager = nullptr;
    ScriptRunner* m_scriptRunner = nullptr;
    QSqlTableModel* m_trackerModel = nullptr;
    TMTermFileManager* m_fileManager = nullptr;

    // State variables
    bool m_jobDataLocked = false;
    bool m_postageDataLocked = false;
    HtmlDisplayState m_currentHtmlState;

    // Script output parsing variables
    QString m_capturedNASPath;     // Stores the NAS path from script output
    bool m_capturingNASPath;       // Flag to indicate we're capturing NAS path
    QString m_lastExecutedScript;  // Track which script was last executed

    // Utility methods
    void connectSignals();
    void setupInitialUIState();
    void setupOptimizedTableLayout();
    void populateDropdowns();
    void formatPostageInput();
    bool validateJobData();
    bool validatePostageData();
    void updateControlStates();
    void updateHtmlDisplay();
    void loadHtmlFile(const QString& resourcePath);
    HtmlDisplayState determineHtmlState() const;
    void saveJobState();
    void loadJobState();
    void outputToTerminal(const QString& message, MessageType type = Info);
    void createBaseDirectories();
    void createJobFolder();
    void saveJobToDatabase();
    void addLogEntry();
    QString copyFormattedRow();

    // Script output parsing methods
    void parseScriptOutput(const QString& output);
    void showNASLinkDialog();

    // TERM-specific utility methods
    QString convertMonthToAbbreviation(const QString& monthNumber) const;
    bool validateJobNumber(const QString& jobNumber);
    bool validateMonthSelection(const QString& month);
    QString getJobDescription() const;
};

#endif // TMTERMCONTROLLER_H
