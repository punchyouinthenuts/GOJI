#ifndef TMTARRAGONCONTROLLER_H
#define TMTARRAGONCONTROLLER_H

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
#include "tmtarragondbmanager.h"
#include "scriptrunner.h"
#include "tmtarragonfilemanager.h"
#include "naslinkdialog.h"

class TMTarragonController : public QObject
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

    explicit TMTarragonController(QObject *parent = nullptr);
    ~TMTarragonController();

    // Initialize with UI elements from mainwindow
    void initializeUI(
        QPushButton* openBulkMailerBtn, QPushButton* runInitialBtn,
        QPushButton* finalStepBtn, QToolButton* lockBtn, QToolButton* editBtn,
        QToolButton* postageLockBtn, QComboBox* yearDDbox, QComboBox* monthDDbox,
        QComboBox* dropNumberDDbox, QLineEdit* jobNumberBox, QLineEdit* postageBox,
        QLineEdit* countBox, QTextEdit* terminalWindow, QTableView* tracker,
        QTextBrowser* textBrowser
        );

    // Load saved data
    bool loadJob(const QString& year, const QString& month, const QString& dropNumber);

    void setTextBrowser(QTextBrowser* textBrowser);

    void resetToDefaults();

signals:
    void jobOpened();
    void jobClosed();

private slots:
    // Button handlers
    void onOpenBulkMailerClicked();
    void onRunInitialClicked();
    void onFinalStepClicked();

    // Lock button handlers
    void onLockButtonClicked();
    void onEditButtonClicked();
    void onPostageLockButtonClicked();

    // Dropdown change handlers
    void onYearChanged(const QString& year);
    void onMonthChanged(const QString& month);
    void onDropNumberChanged(const QString& dropNumber);

    // Script output handlers
    void onScriptOutput(const QString& output);
    void onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus);

    // Table context menu
    void showTableContextMenu(const QPoint& pos);

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
    QComboBox* m_dropNumberDDbox = nullptr;
    QLineEdit* m_jobNumberBox = nullptr;
    QLineEdit* m_postageBox = nullptr;
    QLineEdit* m_countBox = nullptr;
    QTextEdit* m_terminalWindow = nullptr;
    QTableView* m_tracker = nullptr;
    QTextBrowser* m_textBrowser = nullptr;

    // Support objects
    DatabaseManager* m_dbManager = nullptr;
    TMTarragonDBManager* m_tmTarragonDBManager = nullptr;
    ScriptRunner* m_scriptRunner = nullptr;
    TMTarragonFileManager* m_fileManager = nullptr;
    QSqlTableModel* m_trackerModel = nullptr;

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
    bool createExcelAndCopy(const QStringList& headers, const QStringList& rowData);

    // Validation and utility
    bool validateJobData();
    bool validatePostageData();
    bool validateJobNumber(const QString& jobNumber);
    bool validateDropNumber(const QString& dropNumber);
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

#endif // TMTARRAGONCONTROLLER_H
