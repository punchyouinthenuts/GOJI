#ifndef TMWEEKLYPCCONTROLLER_H
#define TMWEEKLYPCCONTROLLER_H

#include "basetrackercontroller.h"
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
#include <QRegularExpression>
#include "databasemanager.h"
#include "tmweeklypcdbmanager.h"
#include "scriptrunner.h"
#include "tmweeklypcfilemanager.h"
#include "naslinkdialog.h"

class TMWeeklyPCController : public BaseTrackerController
{
    Q_OBJECT

public:
    // HTML display states
    enum HtmlDisplayState {
        UninitializedState = -1,  // Initial state before any HTML is loaded
        DefaultState = 0,         // Base state - shows default.html
        ProofState = 1,           // Job locked - shows proof.html
        PrintState = 2            // Proof approved - shows print.html
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

    // Public getters for external access
    QString getJobNumber() const { return m_jobNumberBox ? m_jobNumberBox->text() : ""; }
    QString getYear() const { return m_yearDDbox ? m_yearDDbox->currentText() : ""; }
    QString getMonth() const { return m_monthDDbox ? m_monthDDbox->currentText() : ""; }
    QString getWeek() const { return m_weekDDbox ? m_weekDDbox->currentText() : ""; }
    bool isJobDataLocked() const { return m_jobDataLocked; }
    bool isPostageDataLocked() const { return m_postageDataLocked; }

    // BaseTrackerController implementation
    void outputToTerminal(const QString& message, MessageType type) override;
    QTableView* getTrackerWidget() const override;
    QSqlTableModel* getTrackerModel() const override;
    QStringList getTrackerHeaders() const override;
    QList<int> getVisibleColumns() const override;
    QString formatCellData(int columnIndex, const QString& cellData) const override;

signals:
    void jobOpened();
    void jobClosed();

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
    void onWeekChanged(const QString& week);
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

private:
    // Core components
    DatabaseManager* m_dbManager;
    TMWeeklyPCFileManager* m_fileManager;
    TMWeeklyPCDBManager* m_tmWeeklyPCDBManager;
    ScriptRunner* m_scriptRunner;

    // UI element pointers
    QPushButton* m_runInitialBtn;
    QPushButton* m_openBulkMailerBtn;
    QPushButton* m_runProofDataBtn;
    QPushButton* m_openProofFileBtn;
    QPushButton* m_runWeeklyMergedBtn;
    QPushButton* m_openPrintFileBtn;
    QPushButton* m_runPostPrintBtn;
    QToolButton* m_lockBtn;
    QToolButton* m_editBtn;
    QToolButton* m_postageLockBtn;
    QComboBox* m_proofDDbox;
    QComboBox* m_printDDbox;
    QComboBox* m_yearDDbox;
    QComboBox* m_monthDDbox;
    QComboBox* m_weekDDbox;
    QComboBox* m_classDDbox;
    QComboBox* m_permitDDbox;
    QLineEdit* m_jobNumberBox;
    QLineEdit* m_postageBox;
    QLineEdit* m_countBox;
    QTextEdit* m_terminalWindow;
    QTableView* m_tracker;
    QTextBrowser* m_textBrowser;
    QCheckBox* m_proofApprovalCheckBox;

    // State variables
    bool m_jobDataLocked;
    bool m_postageDataLocked;
    HtmlDisplayState m_currentHtmlState;
    QString m_lastExecutedScript;
    QString m_capturedNASPath;
    bool m_capturingNASPath;

    // Tracker model
    QSqlTableModel* m_trackerModel;

    // Private methods
    void connectSignals();
    void setupInitialUIState();
    void setupOptimizedTableLayout();
    void populateDropdowns();
    void populateWeekDDbox();
    void formatPostageInput();
    void formatCountInput(const QString& text);
    bool validateJobData();
    bool validatePostageData();
    void updateControlStates();
    void updateHtmlDisplay();
    void loadHtmlFile(const QString& resourcePath);
    HtmlDisplayState determineHtmlState() const;
    void saveJobState();
    void loadJobState();
    void savePostageData();
    void loadPostageData(const QString& year = "", const QString& month = "", const QString& week = "");
    void createBaseDirectories();
    void createJobFolder();
    void saveJobToDatabase();
    void addLogEntry();
    double getMeterRateFromDatabase();
    void refreshTrackerTable();

    // Script output parsing methods
    void parseScriptOutput(const QString& output);
    void showNASLinkDialog();

    // Inherited method implementation
    QString copyFormattedRow();
    bool createExcelAndCopy(const QStringList& headers, const QStringList& rowData);

    /**
     * @brief Move files from JOB folders to HOME folders when closing job
     * @return True if files were moved successfully
     */
    bool moveFilesToHomeFolder();

    /**
     * @brief Copy files from HOME folders to JOB folders when opening job
     * @return True if files were copied successfully
     */
    bool copyFilesFromHomeFolder();
};

#endif // TMWEEKLYPCCONTROLLER_H
