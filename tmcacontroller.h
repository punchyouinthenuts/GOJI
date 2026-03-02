#ifndef TMCACONTROLLER_H
#define TMCACONTROLLER_H

#include "basetrackercontroller.h"
#include "tmcafilemanager.h"
#include "tmcadbmanager.h"
#include "scriptrunner.h"

#include <QObject>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QTextBrowser>
#include <QTableView>
#include <QSqlTableModel>
#include <QToolButton>
#include <QProcess>
#include <QStringList>
#include <QJsonObject>

// Forward declarations
class DropWindow;
class TMCAEmailDialog;

/**
 * @brief Controller for TM CA (CA EDR/BA) tab functionality.
 *
 * Implements the TMCA integration specification (Parts 1-7):
 *
 *  PREFLIGHT
 *    - Scans BA/INPUT and EDR/INPUT for eligible files (.csv/.xls/.xlsx
 *      containing LA_BA/SA_BA or LA_EDR/SA_EDR tokens, case-insensitive).
 *    - Aborts with red WARNING!!! if both folders have eligible files.
 *    - Aborts with error if neither folder has eligible files.
 *    - Validates job number is exactly 5 digits before invoking script.
 *
 *  TWO-PHASE SCRIPT EXECUTION
 *    Phase 1 (--phase process): invoked on RUN INITIAL click after preflight passes.
 *    Phase 2 (--phase archive): invoked only after TMCAEmailDialog is closed by user.
 *
 *  JSON CONTRACT
 *    - Captures stdout between === TMCA_RESULT_BEGIN === and === TMCA_RESULT_END ===.
 *    - Strictly validates all required fields (job_type, la_valid_count, sa_valid_count,
 *      la_blank_count, sa_blank_count, merged_files, nas_dest, w_dest).
 *    - Aborts without DB insert/popup/archive if JSON is missing or invalid.
 *
 *  FINANCIAL AUTHORITY
 *    - Queries meter_rates table (latest created_at DESC); defaults to 0.69 if empty.
 *    - Computes la_postage = la_valid_count * rate; sa_postage = sa_valid_count * rate.
 *    - Never allows script to compute postage.
 *
 *  DATABASE INSERTION
 *    - Inserts LA row only if la_valid_count > 0.
 *    - Inserts SA row only if sa_valid_count > 0.
 *    - Description: "TM CA BA LA", "TM CA BA SA", "TM CA EDR LA", "TM CA EDR SA".
 *    - CLASS=FC, SHAPE=LTR, PERMIT=METER always.
 *    - Never inserts a total/aggregate row.
 *    - DB insert failure is fatal: no popup, no archive.
 *
 *  POPUP + ARCHIVE SEQUENCING
 *    - TMCAEmailDialog launched modal only after Phase 1 success + DB insert + tracker refresh.
 *    - CLOSE always enabled; no timers; no gating.
 *    - Archive (Phase 2) triggered only after dialog closes.
 *
 *  TRACKER
 *    - Loads all historical rows from tm_ca_log.
 *    - Refreshes immediately after Phase 1 DB inserts.
 *    - Right-click "Copy Selected Row" parity with other TM trackers.
 */
class TMCAController : public BaseTrackerController
{
    Q_OBJECT

public:
    explicit TMCAController(QObject *parent = nullptr);
    ~TMCAController();

    // ---- UI Widget setters (wired from MainWindow) ----
    void setJobNumberBox(QLineEdit* lineEdit);
    void setYearDropdown(QComboBox* comboBox);
    void setMonthDropdown(QComboBox* comboBox);
    void setPostageBox(QLineEdit* lineEdit);
    void setCountBox(QLineEdit* lineEdit);
    void setJobDataLockButton(QToolButton* button);
    void setEditButton(QToolButton* button);
    void setPostageLockButton(QToolButton* button);
    void setRunInitialButton(QPushButton* button);
    void setFinalStepButton(QPushButton* button);
    void setTerminalWindow(QTextEdit* textEdit);
    void setTextBrowser(QTextBrowser* textBrowser);
    void setTracker(QTableView* tableView);
    void setDropWindow(DropWindow* dropWindow);

    // ---- Job management (mirrors TMFLER/TMTERM patterns) ----
    bool loadJob(const QString& jobNumber, const QString& year, const QString& month);
    void resetToDefaults();
    void saveJobState();
    void loadJobState();
    void loadJobState(const QString& jobNumber);

    // ---- Public accessors used by MainWindow ----
    QString getJobNumber() const;
    QString getYear() const;
    QString getMonth() const;
    bool isJobDataLocked() const;
    bool isPostageDataLocked() const;
    bool hasJobData() const;

    // ---- BaseTrackerController implementation ----
    void outputToTerminal(const QString& message, MessageType type) override;
    QTableView* getTrackerWidget() const override;
    QSqlTableModel* getTrackerModel() const override;
    QStringList getTrackerHeaders() const override;
    QList<int> getVisibleColumns() const override;
    QString formatCellData(int columnIndex, const QString& cellData) const override;
    QString formatCellDataForCopy(int columnIndex, const QString& cellData) const override;

    /** Refresh tracker view from DB (public so MainWindow can call if needed). */
    void refreshTrackerTable();

    /** Auto-save and close current job before opening a new one. */
    void autoSaveAndCloseCurrentJob();

    /** Safe post-construction initializer called by MainWindow after all widget wiring. */
    void initializeAfterConstruction();

signals:
    void jobOpened();
    void jobClosed();

private slots:
    // ---- Lock button handlers ----
    void onJobDataLockClicked();
    void onEditButtonClicked();
    void onPostageLockClicked();

    // ---- Script buttons ----
    // RUN INITIAL: runs preflight then Phase 1 (--phase process).
    void onRunInitialClicked();
    // FINAL STEP: reserved slot; TMCA primary flow uses onRunInitialClicked + popup close.
    void onFinalStepClicked();

    // ---- ScriptRunner signal handlers ----
    void onScriptOutput(const QString& output);
    void onScriptError(const QString& output);   ///< stderr -> always Error (red)
    void onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus);

    // ---- Drop window handlers ----
    void onFilesDropped(const QStringList& filePaths);
    void onFileDropError(const QString& errorMessage);

    // ---- Archive phase trigger (connected to TMCAEmailDialog::dialogClosed signal) ----
    void triggerArchivePhase();

    // ---- Dropdown change handlers ----
    void onYearChanged(const QString& year);
    void onMonthChanged(const QString& month);

    // ---- Tracker context menu ----
    void showTableContextMenu(const QPoint& pos);

private:
    // ====================================================================
    // Enumerations
    // ====================================================================

    /** Tracks which script phase is currently active or was last completed. */
    enum RunPhase {
        PhaseNone    = 0,
        PhaseProcess = 1,   ///< Phase 1: --phase process
        PhaseArchive = 2    ///< Phase 2: --phase archive
    };

    /** Controls which HTML resource is loaded into textBrowser. */
    enum HtmlDisplayState {
        UninitializedState = -1,
        DefaultState       = 0,
        InstructionsState  = 1
    };

    // ====================================================================
    // Initialization helpers
    // ====================================================================
    void initializeComponents();
    void connectSignals();
    void setupInitialState();
    void createBaseDirectories();
    void setupDropWindow();
    void populateDropdowns();

    // ====================================================================
    // Lock / button state management
    // ====================================================================
    void updateLockStates();
    void updateButtonStates();

    // ====================================================================
    // HTML display management
    // ====================================================================
    void updateHtmlDisplay();
    void loadHtmlFile(const QString& resourcePath);
    HtmlDisplayState determineHtmlState() const;

    // ====================================================================
    // Preflight scan (Part 3, Sections 1-2)
    // ====================================================================
    /**
     * Scans BA/INPUT and EDR/INPUT for eligible files.
     * Eligible = (.csv|.xls|.xlsx) AND filename contains required token (case-insensitive):
     *   BA tokens: LA_BA or SA_BA
     *   EDR tokens: LA_EDR or SA_EDR
     *
     * Rules:
     *   Both folders have eligible files -> red WARNING!!! to terminal, returns false.
     *   Neither folder has eligible files -> error to terminal, returns false.
     *   Exactly one folder has eligible files -> sets detectedJobType ("BA" or "EDR"), returns true.
     *
     * Job number must be exactly 5 digits; if not, prints error and returns false.
     */
    bool preflightScan(QString& detectedJobType);

    /**
     * Returns list of eligible file paths in folderPath whose names contain
     * any of the given tokens (case-insensitive) and have a valid extension.
     */
    QStringList scanEligibleFiles(const QString& folderPath,
                                  const QStringList& tokens) const;

    /**
     * Outputs a red WARNING!!! header followed by body lines to terminalWindowTMCA.
     * Uses the existing GOJI terminal coloring mechanism (Warning type => orange/red span).
     * The firstLine is prefixed with "WARNING!!!" and emitted at Error severity.
     */
    void outputRedWarning(const QString& firstLine, const QStringList& bodyLines);

    // ====================================================================
    // Phase 1 invocation (Part 3, Section 4)
    // ====================================================================
    /**
     * Invokes C:\Goji\scripts\TRACHMAR\CA\TMCA.py with:
     *   --phase process
     *   --job <5-digit job number>
     *   --ba-input C:\Goji\TRACHMAR\CA\BA\INPUT
     *   --edr-input C:\Goji\TRACHMAR\CA\EDR\INPUT
     *   --w-dest W:\
     *   --nas-base \\NAS1069D9\AMPrintData
     *   --year <YYYY>
     */
    void runPhase1(const QString& jobType);

    // ====================================================================
    // Phase 2 invocation (Part 3, Section 7)
    // ====================================================================
    /**
     * Invokes TMCA.py with:
     *   --phase archive
     *   --job <same 5-digit job>
     *   --ba-input C:\Goji\TRACHMAR\CA\BA\INPUT
     *   --edr-input C:\Goji\TRACHMAR\CA\EDR\INPUT
     * Called only from triggerArchivePhase() after popup CLOSE.
     */
    void runPhase2();

    // ====================================================================
    // JSON parsing (Part 3, Sections 5-6)
    // ====================================================================
    /**
     * Parses and validates the raw JSON string captured between markers.
     * Required fields: job_type (string, "BA"|"EDR"), la_valid_count (int),
     * sa_valid_count (int), la_blank_count (int), sa_blank_count (int),
     * merged_files (array of strings), nas_dest (string), w_dest (string).
     * Returns false and prints fatal error if any field is missing or wrong type.
     */
    bool parseAndValidateJson(const QString& rawJson, QJsonObject& out);

    // ====================================================================
    // Post-Phase-1 success handler (Part 3, Section 6)
    // ====================================================================
    /**
     * Called from onScriptFinished when Phase 1 exits with code 0 and JSON is valid.
     * Executes in strict order:
     *   1. Query meter rate (queryMeterRate()).
     *   2. Compute per-side postage.
     *   3. Insert LA/SA DB rows (insertLogRows()); abort if insert fails.
     *   4. Refresh trackerTMCA.
     *   5. Update countBoxTMCA and postageBoxTMCA (informational totals).
     *   6. Launch TMCAEmailDialog (modal).
     */
    void handlePhase1Success();

    // ====================================================================
    // Post-Phase-2 result handler (Part 3, Section 7 / Part 6, Section 6.4)
    // ====================================================================
    /**
     * Called from onScriptFinished when Phase 2 completes.
     * Prints success or fatal "archive failed" error to terminal.
     */
    void handlePhase2Result(bool success);

    // ====================================================================
    // DB helpers (Part 2, Section 2 / Part 1, Section 8)
    // ====================================================================
    /**
     * Queries meter_rates table: SELECT rate FROM meter_rates ORDER BY created_at DESC LIMIT 1.
     * Returns 0.69 if table is empty.
     */
    double queryMeterRate() const;

    /**
     * Inserts per-side log rows into tm_ca_log.
     *   - Inserts LA row only if laValidCount > 0.
     *   - Inserts SA row only if saValidCount > 0.
     *   - description = "TM CA <jobType> LA" or "TM CA <jobType> SA"
     *   - postage formatted $X.XX; per_piece formatted 0.XXX; count as integer string.
     *   - class=FC, shape=LTR, permit=METER always.
     * Returns false on any DB error (caller must treat as fatal).
     */
    bool insertLogRows(const QString& jobType,
                       int           laValidCount,
                       int           saValidCount,
                       double        laPostage,
                       double        saPostage,
                       double        rate,
                       const QString& jobNumber);

    // ====================================================================
    // Drop routing
    // ====================================================================
    void routeDroppedFile(const QString& absoluteFilePath);
    void handleZipExtractAndRoute(const QString& zipPath);

    // ====================================================================
    // Validation utilities
    // ====================================================================
    bool validateJobData() const;
    bool validatePostageData() const;
    /** Returns true iff jobNumber is exactly 5 ASCII digits. */
    bool validateJobNumber(const QString& jobNumber) const;

    // ====================================================================
    // Core components
    // ====================================================================
    TMCAFileManager* m_fileManager;
    TMCADBManager*   m_tmcaDBManager;
    ScriptRunner*    m_scriptRunner;

    // ====================================================================
    // UI Widgets
    // ====================================================================
    QLineEdit*    m_jobNumberBox;
    QComboBox*    m_yearDDbox;
    QComboBox*    m_monthDDbox;
    QLineEdit*    m_postageBox;
    QLineEdit*    m_countBox;
    QToolButton*  m_jobDataLockBtn;
    QToolButton*  m_editBtn;
    QToolButton*  m_postageLockBtn;
    QPushButton*  m_runInitialBtn;
    QPushButton*  m_finalStepBtn;
    QTextEdit*    m_terminalWindow;
    QTextBrowser* m_textBrowser;
    QTableView*   m_tracker;
    DropWindow*   m_dropWindow;

    // ====================================================================
    // Lock / HTML display state
    // ====================================================================
    bool             m_jobDataLocked;
    bool             m_postageDataLocked;
    HtmlDisplayState m_currentHtmlState;
    QString          m_lastExecutedScript;

    // ====================================================================
    // Tracker model
    // ====================================================================
    QSqlTableModel* m_trackerModel;

    // ====================================================================
    // Two-phase run state
    // Populated during Phase 1; consumed by handlePhase1Success() and Phase 2.
    // ====================================================================

    /** Which phase is currently active or was last completed. */
    RunPhase m_currentPhase;

    /** "BA" or "EDR" — set by preflightScan(), used throughout Phase 1 & 2. */
    QString m_pendingJobType;

    /** Job number used in the current run (cached so Phase 2 uses the same value). */
    QString m_pendingJobNumber;

    /** Year used in the current run. */
    QString m_pendingYear;

    /** Counts returned by Phase 1 JSON (authoritative; GOJI never recounts). */
    int m_pendingLaValidCount;
    int m_pendingSaValidCount;
    int m_pendingLaBlankCount;
    int m_pendingSaBlankCount;

    /** GOJI-computed postage per side (rate * valid_count). */
    double m_pendingLaPostage;
    double m_pendingSaPostage;

    /** Meter rate resolved from DB for this run. */
    double m_pendingRate;

    /** NAS destination path returned by script JSON (for display in popup). */
    QString m_pendingNasDest;

    /** W drive destination path returned by script JSON. */
    QString m_pendingWDest;

    /** Merged file paths returned by script JSON (for popup drag list). */
    QStringList m_pendingMergedFiles;

    // ====================================================================
    // JSON marker capture state
    // ====================================================================

    /** True while accumulating lines between === TMCA_RESULT_BEGIN === and === TMCA_RESULT_END ===. */
    bool m_capturingJson;

    /** Accumulates raw JSON text between the two markers. */
    QString m_jsonAccumulator;

    // ====================================================================
    // Drop routing display state
    // ====================================================================

    /** Tracks the INPUT folder files were routed to during the current drop. */
    QString m_lastRoutedInputDir;
};

#endif // TMCACONTROLLER_H
