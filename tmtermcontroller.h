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
        DefaultState,      // When no job is loaded - shows default.html
        InstructionsState  // When job is locked - shows instructions.html
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

    void resetToDefaults();

signals:
    void jobOpened();  // Signal to start auto-save timer
    void jobClosed();  // Signal to stop auto-save timer

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
    HtmlDisplayState m_currentHtmlState = DefaultState;
    QString m_capturedNASPath;
    bool m_capturingNASPath = false;
    QString m_lastExecutedScript;

    // Private helper methods
    void connectSignals();
    void setupInitialUIState();
    void populateDropdowns();
    void setupOptimizedTableLayout();
    void outputToTerminal(const QString& message, MessageType type = Info);
    void createBaseDirectories();
    void createJobFolder();

    // Data management
    void saveJobState();
    void loadJobState();
    void saveJobToDatabase();

    // Job and log management
    void addLogEntry();
    QString copyFormattedRow();

    // Validation and utility
    bool validateJobData();
    bool validatePostageData();
    bool validateJobNumber(const QString& jobNumber);
    bool validateMonthSelection(const QString& month);
    QString convertMonthToAbbreviation(const QString& monthNumber) const;
    QString getJobDescription() const;
    bool hasJobData() const;

    // UI state management
    void updateControlStates();
    void updateHtmlDisplay();
    void loadHtmlFile(const QString& resourcePath);
    HtmlDisplayState determineHtmlState() const;
    void formatPostageInput(const QString& text);

    // Script output processing
    void parseScriptOutput(const QString& output);
    void showNASLinkDialog(const QString& nasPath);
};

#endif // TMTERMCONTROLLER_H
