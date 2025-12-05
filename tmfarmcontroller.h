#ifndef TMFARMCONTROLLER_H
#define TMFARMCONTROLLER_H

#include <QObject>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTableView>
#include <QSqlTableModel>
#include <QComboBox>
#include <QLineEdit>
#include <QAbstractButton>
#include <QProcess> // REQUIRED for QProcess::ExitStatus
#include <memory>

class ScriptRunner;
class TMFarmEmailDialog;
class TMFarmFileManager;
class TMFarmDBManager;

/**
 * @brief Controller for TM FARMWORKERS subsystem
 * 
 * Implements full TRACHMAR pattern with Lock/Edit/Postage workflow, database persistence,
 * and HTML state management. Job folder naming: jobNumber_yearquarter (e.g., 12345_20253RD)
 */
class TMFarmController : public QObject
{
    Q_OBJECT

signals:
    /** Emitted when a job is opened/locked (triggers auto-save timer start in MainWindow) */
    void jobOpened();

    /** Emitted when a job is closed (triggers auto-save timer stop in MainWindow) */
    void jobClosed();

public:
    explicit TMFarmController(QObject *parent = nullptr);
    ~TMFarmController();

    void setTextBrowser(QTextBrowser *browser);

    /** MainWindow wires all these for FARMWORKERS */
    void initializeUI(
        QAbstractButton *openBulkMailerBtn,
        QAbstractButton *runInitialBtn,
        QAbstractButton *finalStepBtn,
        QAbstractButton *lockButton,
        QAbstractButton *editButton,
        QAbstractButton *postageLockButton,
        QComboBox  *yearDD,
        QComboBox  *quarterDD,
        QLineEdit  *jobNumberBox,
        QLineEdit  *postageBox,
        QLineEdit  *countBox,
        QTextEdit  *terminalWindow,
        QTableView *trackerView,
        QTextBrowser *textBrowser
    );

    /** Refresh tracker table for specific job */
    void refreshTracker(const QString &jobNumber);

    /** Load job for specific year and quarter (called by MainWindow Open Job menu) */
    bool loadJob(const QString& year, const QString& quarter);

    /** Check if job data is currently locked (for MainWindow job management) */
    bool isJobDataLocked() const { return m_jobDataLocked; }

    /** Auto-save and close current job (called by MainWindow on tab switch or exit) */
    void autoSaveAndCloseCurrentJob();

    /** Reset UI to defaults (called by MainWindow) */
    void resetToDefaults();

    /** Save complete job state to database (called by MainWindow File > Save Job) */
    void saveJobState();

private slots:
    // Lock/Edit/Postage workflow (TRACHMAR pattern)
    void onLockButtonClicked();
    void onEditButtonClicked();
    void onPostageLockButtonClicked();

    // Year/Quarter change handlers
    void onYearChanged(const QString& year);
    void onQuarterChanged(const QString& quarter);

    // Final step (entry point for two-phase process)
    void onFinalStepClicked();
    void triggerArchivePhase(); // legacy alias -> runArchivePhase()

    // Other buttons
    void onRunInitialClicked();
    void onOpenBulkMailerClicked();

    // ScriptRunner (prearchive)
    void onScriptOutput(const QString& line);
    void onScriptError(const QString& line);
    void onScriptFinished(int exitCode, QProcess::ExitStatus status);

    // Archive (fresh QProcess)
    void runArchivePhase();
    void onArchiveFinished(int exitCode, QProcess::ExitStatus status);

    // Formatting slots
    void onPostageEditingFinished();
    void onCountEditingFinished();

    // HTML refresh signals
    void updateHtmlDisplay();

private:
    // Tracker setup helpers
    void setupTrackerModel();
    void setupOptimizedTableLayout();
    void applyHeaderLabels();
    void enforceVisibilityMask();
    void applyFixedColumnWidths();
    int  computeOptimalFontSize() const;

    // Widget behavior (mirrors TERM)
    void initYearDropdown();         // ["", prev, current, next]
    void setupTextBrowserInitial();  // ensure default loads initially
    void wireFormattingForInputs();  // connect editingFinished
    void formatPostageBoxDisplay();  // $ + thousands + 2 decimals
    void formatCountBoxDisplay();    // thousands

    // Dynamic HTML rule (mirrors TERM)
    enum HtmlDisplayState {
        DefaultState = 0,      // default.html - shown when job not locked or postage locked
        InstructionsState = 1  // instructions.html - shown when job locked but postage not locked
    };
    
    int  determineHtmlState() const; // 0=default, 1=instructions
    void loadHtmlFile(const QString& resourcePath);

    // Script/flow helpers
    void parseScriptOutputLine(const QString& line);
    void updateControlStates();

    // Database operations (TRACHMAR pattern)
    void saveJobToDatabase();
    void loadJobState();
    bool validateJobData();
    bool validatePostageData();
    void addLogEntry();

    // File operations (TRACHMAR pattern)
    void createJobFolder();
    void copyFilesFromHomeFolder();
    void moveFilesToHomeFolder();

    // Terminal output helpers
    enum OutputType { Info, Success, Warning, Error };
    void outputToTerminal(const QString& message, OutputType type = Info);

private:
    // UI (non-owning)
    QTableView *m_trackerView = nullptr;
    QTextBrowser *m_textBrowser = nullptr;

    QAbstractButton *m_openBulkMailerBtn = nullptr;
    QAbstractButton *m_runInitialBtn = nullptr;
    QAbstractButton *m_finalStepBtn = nullptr;
    QAbstractButton *m_lockButton = nullptr;
    QAbstractButton *m_editButton = nullptr;
    QAbstractButton *m_postageLockButton = nullptr;

    QComboBox   *m_yearDD = nullptr;
    QComboBox   *m_quarterDD = nullptr;

    QLineEdit   *m_jobNumberBox = nullptr;
    QLineEdit   *m_postageBox = nullptr;
    QLineEdit   *m_countBox = nullptr;

    QTextEdit   *m_terminalWindow = nullptr;

    // ScriptRunner
    ScriptRunner* m_scriptRunner = nullptr;
    QString m_lastExecutedScript;
    QString m_capturedNASPath;
    bool    m_capturingNASPath = false;

    // Model
    std::unique_ptr<QSqlTableModel> m_trackerModel;

    // File and database managers
    TMFarmFileManager* m_fileManager = nullptr;
    TMFarmDBManager* m_dbManager = nullptr;

    // Job state (TRACHMAR pattern)
    bool m_jobDataLocked = false;
    bool m_postageDataLocked = false;
    HtmlDisplayState m_currentHtmlState = DefaultState;
    bool m_initializing = false;

    // Cache for year/quarter to detect changes
    QString m_cachedYear;
    QString m_cachedQuarter;
    QString m_cachedJobNumber;
};

#endif // TMFARMCONTROLLER_H
