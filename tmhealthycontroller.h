#ifndef TMHEALTHYCONTROLLER_H
#define TMHEALTHYCONTROLLER_H

#include "basetrackercontroller.h"
#include "tmhealthyfilemanager.h"
#include "tmhealthydbmanager.h"
#include "scriptrunner.h"
#include "databasemanager.h"
#include <QObject>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QTextEdit>
#include <QTextBrowser>
#include <QTableView>
#include <QSqlTableModel>
#include <QTimer>
#include <QRegularExpression>
#include <QSignalBlocker>

// Forward declaration
class NASLinkDialog;

class TMHealthyController : public BaseTrackerController
{
    Q_OBJECT

public:
    explicit TMHealthyController(QObject *parent = nullptr);
    ~TMHealthyController();

    // Main UI initialization method
    void initializeUI(
        QPushButton* openBulkMailerBtn, QPushButton* runInitialBtn,
        QPushButton* finalStepBtn, QToolButton* lockBtn, QToolButton* editBtn,
        QToolButton* postageLockBtn, QComboBox* yearDDbox, QComboBox* monthDDbox,
        QLineEdit* jobNumberBox, QLineEdit* postageBox, QLineEdit* countBox,
        QTextEdit* terminalWindow, QTableView* tracker, QTextBrowser* textBrowser);

    // Text browser setter (called separately after UI setup)
    void setTextBrowser(QTextBrowser* textBrowser);

    // Job management
    bool loadJob(const QString& year, const QString& month);
    void resetToDefaults();
    void saveJobState();

    // Public getters for external access
    QString getJobNumber() const { return m_jobNumberBox ? m_jobNumberBox->text() : ""; }
    QString getYear() const { return m_yearDDbox ? m_yearDDbox->currentText() : ""; }
    QString getMonth() const { return m_monthDDbox ? m_monthDDbox->currentText() : ""; }

    // Text formatting for cell data (called from model)
    QString formatCellData(int columnIndex, const QString& cellData) const;

    // Message type enum inherited from BaseTrackerController

    // HTML display states
    enum HtmlDisplayState {
        DefaultState = 0,
        InstructionsState = 1
    };

signals:
    void jobOpened();
    void jobClosed();

private slots:
    // Button handlers
    void onOpenBulkMailerClicked();
    void onRunInitialClicked();
    void onFinalStepClicked();

    // Toggle button handlers
    void onLockButtonClicked();
    void onEditButtonClicked();
    void onPostageLockButtonClicked();

    // Dropdown handlers
    void onYearChanged(const QString& year);
    void onMonthChanged(const QString& month);

    // Input validation and formatting
    void onJobNumberChanged();
    void onPostageChanged();
    void onCountChanged();

    // Script output handling
    void onScriptOutput(const QString& output);
    void onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus);

    // Auto-save timer
    void onAutoSaveTimer();

private:
    // UI initialization helpers
    void connectSignals();
    void setupInitialUIState();
    void populateDropdowns();
    void setupOptimizedTableLayout();

    // TableView context menu
    void showTableContextMenu(const QPoint& pos);

    // Core functionality

    // State management
    void updateJobDataUI();
    void updateLockStates();
    void lockInputs(bool locked);
    void enableEditMode(bool enabled);
    void updateTrackerTable();

    // Data persistence
    void loadJobData();
    void saveCurrentJobData();

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
    void formatPostageInput();
    void formatCountInput(const QString& text);

    // Script output processing
    void parseScriptOutput(const QString& output);
    void showNASLinkDialog(const QString& nasPath);

    // Inherited method implementation
    QString copyFormattedRow();
    bool createExcelAndCopy(const QStringList& headers, const QStringList& rowData);

    /**
     * @brief Load job state from database including postage and count data
     */
    void loadJobState();

    /**
     * @brief Add log entry when postage is locked
     */
    void addLogEntry();

    /**
     * @brief Calculate per piece rate
     */
    QString calculatePerPiece(const QString& postage, const QString& count) const;

    /**
     * @brief Refresh the tracker table
     */
    void refreshTrackerTable();

    /**
     * @brief Save job to database
     */
    void saveJobToDatabase();



    /**
     * @brief Debug check tables
     */
    void debugCheckTables();

    // BaseTrackerController implementation methods
    void outputToTerminal(const QString& message, MessageType type) override;
    QTableView* getTrackerWidget() const override;
    QSqlTableModel* getTrackerModel() const override;
    QStringList getTrackerHeaders() const override;
    QList<int> getVisibleColumns() const override;

private:
    // Database and file managers
    DatabaseManager* m_dbManager;
    TMHealthyFileManager* m_fileManager;
    TMHealthyDBManager* m_tmHealthyDBManager;
    ScriptRunner* m_scriptRunner;

    // UI element pointers
    QPushButton* m_openBulkMailerBtn;
    QPushButton* m_runInitialBtn;
    QPushButton* m_finalStepBtn;
    QToolButton* m_lockBtn;
    QToolButton* m_editBtn;
    QToolButton* m_postageLockBtn;
    QComboBox* m_yearDDbox;
    QComboBox* m_monthDDbox;
    QLineEdit* m_jobNumberBox;
    QLineEdit* m_postageBox;
    QLineEdit* m_countBox;
    QTextEdit* m_terminalWindow;
    QTableView* m_tracker;
    QTextBrowser* m_textBrowser;

    // State management
    bool m_jobDataLocked;
    bool m_postageDataLocked;
    HtmlDisplayState m_currentHtmlState;

    // Script and dialog management
    QString m_lastExecutedScript;
    QString m_capturedNASPath;
    bool m_capturingNASPath;

    // Table model
    QSqlTableModel* m_trackerModel;
};

#endif // TMHEALTHYCONTROLLER_H
