#include "tmtermcontroller.h"
#include "logger.h"
#include "naslinkdialog.h"
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>

class FormattedSqlModel : public QSqlTableModel {
public:
    FormattedSqlModel(QObject *parent, QSqlDatabase db, TMTermController *ctrl)
        : QSqlTableModel(parent, db), controller(ctrl) {}
    QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole) {
            QVariant val = QSqlTableModel::data(idx, role);
            return controller->formatCellData(idx.column(), val.toString());
        }
        return QSqlTableModel::data(idx, role);
    }
private:
    TMTermController *controller;
};

TMTermController::TMTermController(QObject *parent)
    : BaseTrackerController(parent)
    , m_dbManager(DatabaseManager::instance())
    , m_fileManager(nullptr)
    , m_tmTermDBManager(TMTermDBManager::instance())
    , m_scriptRunner(nullptr)
    , m_openBulkMailerBtn(nullptr)
    , m_runInitialBtn(nullptr)
    , m_finalStepBtn(nullptr)
    , m_lockBtn(nullptr)
    , m_editBtn(nullptr)
    , m_postageLockBtn(nullptr)
    , m_yearDDbox(nullptr)
    , m_monthDDbox(nullptr)
    , m_jobNumberBox(nullptr)
    , m_postageBox(nullptr)
    , m_countBox(nullptr)
    , m_terminalWindow(nullptr)
    , m_tracker(nullptr)
    , m_textBrowser(nullptr)
    , m_jobDataLocked(false)
    , m_postageDataLocked(false)
    , m_currentHtmlState(UninitializedState)
    , m_lastExecutedScript("")
    , m_capturedNASPath("")
    , m_capturingNASPath(false)
    , m_trackerModel(nullptr)
{
    // Initialize file manager for TERM
    QSettings* settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "GojiApp", "Goji");
    m_fileManager = new TMTermFileManager(settings);

    // Initialize script runner
    m_scriptRunner = new ScriptRunner(this);

    // Setup the model for the tracker table
    if (m_dbManager && m_dbManager->isInitialized()) {
        m_trackerModel = new FormattedSqlModel(this, m_dbManager->getDatabase(), this);
        m_trackerModel->setTable("tm_term_log");
        m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
        m_trackerModel->select();
    } else {
        Logger::instance().warning("Cannot setup tracker model - database not available");
        m_trackerModel = nullptr;
    }
}

TMTermController::~TMTermController()
{
    Logger::instance().info("TMTermController destroyed");
}

void TMTermController::initializeUI(
    QPushButton* openBulkMailerBtn, QPushButton* runInitialBtn,
    QPushButton* finalStepBtn, QToolButton* lockBtn, QToolButton* editBtn,
    QToolButton* postageLockBtn, QComboBox* yearDDbox, QComboBox* monthDDbox,
    QLineEdit* jobNumberBox, QLineEdit* postageBox, QLineEdit* countBox,
    QTextEdit* terminalWindow, QTableView* tracker, QTextBrowser* textBrowser)
{
    Logger::instance().info("Initializing TM TERM UI elements");

    // Store UI element pointers
    m_openBulkMailerBtn = openBulkMailerBtn;
    m_runInitialBtn = runInitialBtn;
    m_finalStepBtn = finalStepBtn;
    m_lockBtn = lockBtn;
    m_editBtn = editBtn;
    m_postageLockBtn = postageLockBtn;
    m_yearDDbox = yearDDbox;
    m_monthDDbox = monthDDbox;
    m_jobNumberBox = jobNumberBox;
    m_postageBox = postageBox;
    m_countBox = countBox;
    m_terminalWindow = terminalWindow;
    m_tracker = tracker;
    m_textBrowser = textBrowser;

    // Setup tracker table with optimized layout
    if (m_tracker) {
        m_tracker->setModel(m_trackerModel);
        m_tracker->setEditTriggers(QAbstractItemView::NoEditTriggers); // Read-only

        // Setup optimized table layout
        setupOptimizedTableLayout();

        // Connect contextual menu for copying
        m_tracker->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_tracker, &QTableView::customContextMenuRequested, this,
                &TMTermController::showTableContextMenu);
    }

    // Connect UI signals to slots
    connectSignals();

    // Setup initial UI state
    setupInitialUIState();

    // Populate dropdowns
    populateDropdowns();

    // Initialize HTML display with default state
    updateHtmlDisplay();

    Logger::instance().info("TM TERM UI initialization complete");
}

// FIXED: Enhanced connectSignals with auto-save functionality
void TMTermController::connectSignals()
{
    // Connect buttons with null pointer checks
    if (m_openBulkMailerBtn) {
        connect(m_openBulkMailerBtn, &QPushButton::clicked, this, &TMTermController::onOpenBulkMailerClicked);
    }
    if (m_runInitialBtn) {
        connect(m_runInitialBtn, &QPushButton::clicked, this, &TMTermController::onRunInitialClicked);
    }
    if (m_finalStepBtn) {
        connect(m_finalStepBtn, &QPushButton::clicked, this, &TMTermController::onFinalStepClicked);
    }

    // Connect toggle buttons with null pointer checks
    if (m_lockBtn) {
        connect(m_lockBtn, &QToolButton::clicked, this, &TMTermController::onLockButtonClicked);
    }
    if (m_editBtn) {
        connect(m_editBtn, &QToolButton::clicked, this, &TMTermController::onEditButtonClicked);
    }
    if (m_postageLockBtn) {
        connect(m_postageLockBtn, &QToolButton::clicked, this, &TMTermController::onPostageLockButtonClicked);
    }

    // Connect dropdowns with null pointer checks
    if (m_yearDDbox) {
        connect(m_yearDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
                this, &TMTermController::onYearChanged);
    }
    if (m_monthDDbox) {
        connect(m_monthDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
                this, &TMTermController::onMonthChanged);
    }

    // Connect input formatting
    if (m_postageBox) {
        connect(m_postageBox, &QLineEdit::textChanged, this, &TMTermController::formatPostageInput);

        // CRITICAL FIX: Auto-save on postage changes when job is locked
        connect(m_postageBox, &QLineEdit::textChanged, this, [this]() {
            if (m_jobDataLocked) {
                saveJobState(); // Auto-save when job is locked
            }
        });
    }

    if (m_countBox) {
        connect(m_countBox, &QLineEdit::textChanged, this, &TMTermController::formatCountInput);

        // CRITICAL FIX: Auto-save on count changes when job is locked
        connect(m_countBox, &QLineEdit::textChanged, this, [this]() {
            if (m_jobDataLocked) {
                saveJobState(); // Auto-save when job is locked
            }
        });
    }

    // Connect script runner signals
    if (m_scriptRunner) {
        connect(m_scriptRunner, &ScriptRunner::scriptOutput, this, &TMTermController::onScriptOutput);
        connect(m_scriptRunner, &ScriptRunner::scriptFinished, this, &TMTermController::onScriptFinished);
    }

    Logger::instance().info("TM TERM signal connections complete");
}

void TMTermController::setupInitialUIState()
{
    Logger::instance().info("Setting up initial TM TERM UI state...");

    // Initial lock states - all unlocked
    m_jobDataLocked = false;
    m_postageDataLocked = false;

    // Update control states
    updateControlStates();

    Logger::instance().info("Initial TM TERM UI state setup complete");
}

void TMTermController::populateDropdowns()
{
    Logger::instance().info("Populating TM TERM dropdowns...");

    // Populate year dropdown: [blank], last year, current year, next year
    if (m_yearDDbox) {
        m_yearDDbox->clear();
        m_yearDDbox->addItem(""); // Blank default

        QDate currentDate = QDate::currentDate();
        int currentYear = currentDate.year();

        m_yearDDbox->addItem(QString::number(currentYear - 1)); // Last year
        m_yearDDbox->addItem(QString::number(currentYear));     // Current year
        m_yearDDbox->addItem(QString::number(currentYear + 1)); // Next year
    }

    // Populate month dropdown: 01-12
    if (m_monthDDbox) {
        m_monthDDbox->clear();
        m_monthDDbox->addItem(""); // Blank default

        for (int i = 1; i <= 12; i++) {
            m_monthDDbox->addItem(QString("%1").arg(i, 2, 10, QChar('0')));
        }
    }

    Logger::instance().info("TM TERM dropdown population complete");
}

void TMTermController::formatPostageInput(const QString& text)
{
    if (!m_postageBox) return;

    QString cleanText = text;
    static const QRegularExpression nonNumericRegex("[^0-9.]");
    cleanText.remove(nonNumericRegex);

    // Prevent multiple decimal points
    int decimalPos = cleanText.indexOf('.');
    if (decimalPos != -1) {
        QString beforeDecimal = cleanText.left(decimalPos + 1);
        QString afterDecimal = cleanText.mid(decimalPos + 1).remove('.');
        cleanText = beforeDecimal + afterDecimal;
    }

    // Format with dollar sign and thousand separators if there's content
    QString formatted;
    if (!cleanText.isEmpty() && cleanText != ".") {
        // Parse the number to add thousand separators
        bool ok;
        double value = cleanText.toDouble(&ok);
        if (ok) {
            // Format with thousand separators and 2 decimal places
            formatted = QString("$%L1").arg(value, 0, 'f', 2);
        } else {
            // Fallback if parsing fails
            formatted = "$" + cleanText;
        }
    }

    // Prevent infinite loop by checking if update is needed
    if (m_postageBox->text() != formatted) {
        m_postageBox->blockSignals(true);
        m_postageBox->setText(formatted);
        m_postageBox->blockSignals(false);
    }
}

void TMTermController::formatCountInput(const QString& text)
{
    if (!m_countBox) return;

    // Remove any non-digit characters
    QString cleanText = text;
    static const QRegularExpression nonDigitRegex("[^0-9]");
    cleanText.remove(nonDigitRegex);

    // Format with commas for thousands separator
    QString formatted;
    if (!cleanText.isEmpty()) {
        bool ok;
        int number = cleanText.toInt(&ok);
        if (ok) {
            formatted = QString("%L1").arg(number);
        } else {
            formatted = cleanText; // Fallback if conversion fails
        }
    }

    // Prevent infinite loop by checking if update is needed
    if (m_countBox->text() != formatted) {
        m_countBox->blockSignals(true);
        m_countBox->setText(formatted);
        m_countBox->blockSignals(false);
    }
}



void TMTermController::setTextBrowser(QTextBrowser* textBrowser)
{
    m_textBrowser = textBrowser;
    updateHtmlDisplay();
}

// FIXED: Enhanced saveJobState to always save postage and count data
void TMTermController::saveJobState()
{
    if (!m_tmTermDBManager) return;

    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty()) return;

    // CRITICAL FIX: Always get current postage and count values from UI
    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";

    // Save complete job state including postage data and lock states
    bool success = m_tmTermDBManager->saveJobState(year, month,
                                                   static_cast<int>(m_currentHtmlState),
                                                   m_jobDataLocked, m_postageDataLocked,
                                                   postage, count, m_lastExecutedScript);

    if (success) {
        outputToTerminal(QString("Job state saved: postage=%1, count=%2, postage_locked=%3")
                             .arg(postage, count, m_postageDataLocked ? "true" : "false"), Info);
    } else {
        outputToTerminal("Failed to save job state", Warning);
    }
}

// FIXED: Enhanced loadJobState to restore postage and count data
void TMTermController::loadJobState()
{
    if (!m_tmTermDBManager) return;

    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty()) return;

    int htmlState;
    bool jobLocked, postageLocked;
    QString postage, count, lastExecutedScript;

    if (m_tmTermDBManager->loadJobState(year, month, htmlState, jobLocked, postageLocked, postage, count, lastExecutedScript)) {
        m_currentHtmlState = static_cast<HtmlDisplayState>(htmlState);
        m_jobDataLocked = jobLocked;
        m_postageDataLocked = postageLocked;
        m_lastExecutedScript = lastExecutedScript;

        // CRITICAL FIX: Restore postage and count data to UI
        if (m_postageBox && !postage.isEmpty()) {
            m_postageBox->setText(postage);
        }
        if (m_countBox && !count.isEmpty()) {
            m_countBox->setText(count);
        }

        updateControlStates();
        updateHtmlDisplay();

        outputToTerminal(QString("Job state loaded: postage=%1, count=%2, postage_locked=%3")
                             .arg(postage, count, m_postageDataLocked ? "true" : "false"), Info);
    } else {
        // No saved state found, set defaults
        m_currentHtmlState = DefaultState;
        m_jobDataLocked = false;
        m_postageDataLocked = false;
        m_lastExecutedScript = "";
        updateControlStates();
        updateHtmlDisplay();
        outputToTerminal("No saved job state found, using defaults", Info);
    }
}

void TMTermController::saveJobToDatabase()
{
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty()) {
        outputToTerminal("Cannot save job: missing required data", Warning);
        return;
    }

    if (m_tmTermDBManager->saveJob(jobNumber, year, month)) {
        outputToTerminal("Job saved to database", Success);
    } else {
        outputToTerminal("Failed to save job to database", Error);
    }
}

bool TMTermController::loadJob(const QString& year, const QString& month)
{
    if (!m_tmTermDBManager) return false;

    QString jobNumber;
    if (m_tmTermDBManager->loadJob(year, month, jobNumber)) {
        // Load job data into UI
        if (m_jobNumberBox) m_jobNumberBox->setText(jobNumber);
        if (m_yearDDbox) m_yearDDbox->setCurrentText(year);
        if (m_monthDDbox) m_monthDDbox->setCurrentText(month);

        // Force UI to process the dropdown changes before locking
        QCoreApplication::processEvents();

        // DEBUG: Check if tables exist
        debugCheckTables();

        // Load job state FIRST (this restores the saved lock states)
        loadJobState();

        // If loadJobState didn't set job as locked, default to locked
        if (!m_jobDataLocked) {
            m_jobDataLocked = true;
            outputToTerminal("DEBUG: Job state not found, defaulting to locked", Info);
        }

        // Update UI to reflect the lock state
        if (m_lockBtn) m_lockBtn->setChecked(m_jobDataLocked);

        // If job data is locked, handle file operations and auto-save
        if (m_jobDataLocked) {
            copyFilesFromHomeFolder();
            outputToTerminal("Files copied from ARCHIVE to DATA folder", Info);

            // Start auto-save timer since job is locked/open
            emit jobOpened();
            outputToTerminal("Auto-save timer started (15 minutes)", Info);
        }

        // Update control states and HTML display
        updateControlStates();
        updateHtmlDisplay();

        outputToTerminal("Job loaded: " + jobNumber, Success);
        return true;
    }

    outputToTerminal("Failed to load job for " + year + "/" + month, Error);
    return false;
}

// FIXED: Enhanced addLogEntry with proper count formatting and correct parameters
void TMTermController::addLogEntry()
{
    if (!m_tmTermDBManager) {
        outputToTerminal("Database manager not available for log entry", Error);
        return;
    }

    // Get current data from UI
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";

    // Validate required data
    if (jobNumber.isEmpty() || month.isEmpty() || postage.isEmpty() || count.isEmpty()) {
        outputToTerminal(QString("Cannot add log entry: missing required data. Job: '%1', Month: '%2', Postage: '%3', Count: '%4'")
                             .arg(jobNumber, month, postage, count), Warning);
        return;
    }

    // Convert month to abbreviation for description
    QString monthAbbrev = convertMonthToAbbreviation(month);
    QString description = QString("TM %1 TERM").arg(monthAbbrev);

    // CRITICAL FIX: Clean and format count (remove commas, ensure integer)
    QString cleanCount = count;
    cleanCount.remove(',').remove(' '); // Remove commas and spaces
    int countValue = cleanCount.toInt();
    QString formattedCount = QString::number(countValue);

    // Format postage with $ symbol and 2 decimal places
    QString formattedPostage = postage;
    if (!formattedPostage.startsWith("$")) {
        formattedPostage = "$" + formattedPostage;
    }
    double postageAmount = formattedPostage.remove("$").toDouble();
    formattedPostage = QString("$%1").arg(postageAmount, 0, 'f', 2);

    // Calculate per piece rate (X.XXX format)
    double perPiece = (countValue > 0) ? (postageAmount / countValue) : 0.0;
    QString formattedPerPiece = QString("%1").arg(perPiece, 0, 'f', 3);

    // Add log entry with correct parameter order and count
    QString mailClass = "FIRST-CLASS MAIL";
    QString shape = "LTR";  // CRITICAL FIX: Added missing shape parameter
    QString permit = "NKLN";
    QString date = QDate::currentDate().toString("MM/dd/yyyy");

    if (m_tmTermDBManager->addLogEntry(jobNumber, description, formattedPostage, formattedCount,
                                       formattedPerPiece, mailClass, shape, permit, date)) {
        outputToTerminal(QString("Log entry added: %1 pieces at %2 (%3 per piece)")
                             .arg(formattedCount, formattedPostage, formattedPerPiece), Success);
        
        // Refresh the tracker model
        if (m_trackerModel) {
            m_trackerModel->select();
        }
    } else {
        outputToTerminal("Failed to add log entry", Error);
    }
}

void TMTermController::resetToDefaults()
{
    // CRITICAL FIX: Save current job state to database BEFORE resetting
    // This ensures lock states are preserved when job is reopened
    saveJobState();

    // CRITICAL FIX: Move files to HOME folder BEFORE clearing UI fields
    // This ensures we have access to job number, year, and month when moving files
    moveFilesToHomeFolder();

    // Now reset all internal state variables
    m_jobDataLocked = false;
    m_postageDataLocked = false;
    m_currentHtmlState = DefaultState;
    m_capturedNASPath.clear();
    m_capturingNASPath = false;
    m_lastExecutedScript.clear();

    // Clear all form fields (now safe to do after file move)
    if (m_jobNumberBox) m_jobNumberBox->clear();
    if (m_postageBox) m_postageBox->clear();
    if (m_countBox) m_countBox->clear();

    // Reset all dropdowns to index 0 (empty)
    if (m_yearDDbox) m_yearDDbox->setCurrentIndex(0);
    if (m_monthDDbox) m_monthDDbox->setCurrentIndex(0);

    // Reset all lock buttons to unchecked
    if (m_lockBtn) m_lockBtn->setChecked(false);
    if (m_editBtn) m_editBtn->setChecked(false);
    if (m_postageLockBtn) m_postageLockBtn->setChecked(false);

    // Clear terminal window
    if (m_terminalWindow) m_terminalWindow->clear();

    // Update control states and HTML display
    updateControlStates();
    updateHtmlDisplay();

    // Force load default.html regardless of state
    loadHtmlFile(":/resources/tmterm/default.html");

    // Emit signal to stop auto-save timer since no job is open
    emit jobClosed();
    outputToTerminal("Job state reset to defaults", Info);
    outputToTerminal("Auto-save timer stopped - no job open", Info);
}

// FIXED: Enhanced year/month change handlers to load job state
void TMTermController::onYearChanged(const QString& year)
{
    Q_UNUSED(year)
    loadJobState(); // Load state when year changes
    updateHtmlDisplay(); // Update HTML based on loaded state
}

void TMTermController::onMonthChanged(const QString& month)
{
    Q_UNUSED(month)
    loadJobState(); // Load state when month changes
    updateHtmlDisplay(); // Update HTML based on loaded state
}

// Button handlers
void TMTermController::onLockButtonClicked()
{
    if (m_lockBtn->isChecked()) {
        // User is trying to lock the job
        if (!validateJobData()) {
            m_lockBtn->setChecked(false);
            outputToTerminal("Cannot lock job: Please correct the validation errors above.", Error);
            // Edit button stays checked so user can fix the data
            return;
        }

        // Lock job data
        m_jobDataLocked = true;
        if (m_editBtn) m_editBtn->setChecked(false); // Auto-uncheck edit button
        outputToTerminal("Job data locked.", Success);

        // Create folder for the job
        createJobFolder();

        // Copy files from HOME folder to DATA folder when opening
        copyFilesFromHomeFolder();

        // Save to database
        saveJobToDatabase();

        // Save job state whenever lock button is clicked
        saveJobState();

        // Update control states and HTML display
        updateControlStates();
        updateHtmlDisplay();

        // Start auto-save timer since job is now locked/open
        if (m_jobDataLocked) {
            // Emit signal to MainWindow to start auto-save timer
            emit jobOpened();
            outputToTerminal("Auto-save timer started (15 minutes)", Info);
        }
    } else {
        // User unchecked lock button - this shouldn't happen in normal flow
        m_lockBtn->setChecked(true); // Force it back to checked
    }
}

void TMTermController::onEditButtonClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot edit job data until it is locked.", Error);
        m_editBtn->setChecked(false);
        return;
    }

    if (m_editBtn->isChecked()) {
        // Edit button was just checked - unlock job data for editing
        m_jobDataLocked = false;
        if (m_lockBtn) m_lockBtn->setChecked(false); // Unlock the lock button

        outputToTerminal("Job data unlocked for editing.", Info);
        updateControlStates();
        updateHtmlDisplay(); // This will switch back to default.html since job is no longer locked
    }
    // If edit button is unchecked, do nothing (ignore the click)
}

// FIXED: Postage lock button handler with proper persistence
void TMTermController::onPostageLockButtonClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot lock postage data until job data is locked.", Error);
        m_postageLockBtn->setChecked(false);
        return;
    }

    if (m_postageLockBtn->isChecked()) {
        // Validate postage data before locking
        if (!validatePostageData()) {
            m_postageDataLocked = false;
            m_postageLockBtn->setChecked(false);
            return;
        }

        m_postageDataLocked = true;
        outputToTerminal("Postage data locked and saved.", Success);

        // Add log entry when postage is locked
        addLogEntry();
    } else {
        m_postageDataLocked = false;
        outputToTerminal("Postage data unlocked.", Info);
    }

    // CRITICAL FIX: Always save job state when postage lock changes
    saveJobState();
    updateControlStates();
}

void TMTermController::onOpenBulkMailerClicked()
{
    outputToTerminal("Opening Bulk Mailer...", Info);

    QString program = "BulkMailer.exe";
    if (!QProcess::startDetached(program)) {
        outputToTerminal("Failed to open Bulk Mailer", Error);
    } else {
        outputToTerminal("Bulk Mailer opened successfully", Success);
    }
}

void TMTermController::onRunInitialClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Error: Job data must be locked before running initial script", Error);
        return;
    }

    if (!m_fileManager || !m_scriptRunner) {
        outputToTerminal("Error: Missing file manager or script runner", Error);
        return;
    }

    QString scriptPath = m_fileManager->getScriptPath("01TERMFIRSTSTEP");
    if (!QFile::exists(scriptPath)) {
        outputToTerminal("Error: Initial script not found: " + scriptPath, Error);
        return;
    }

    outputToTerminal("Starting initial processing script...", Info);
    m_lastExecutedScript = "01TERMFIRSTSTEP";

    QStringList arguments;
    // No arguments needed for initial script - it processes files from Downloads

    m_scriptRunner->runScript(scriptPath, arguments);
}

void TMTermController::onFinalStepClicked()
{
    if (!m_postageDataLocked) {
        outputToTerminal("Error: Postage data must be locked before running final script", Error);
        return;
    }

    if (!m_fileManager || !m_scriptRunner) {
        outputToTerminal("Error: Missing file manager or script runner", Error);
        return;
    }

    QString scriptPath = m_fileManager->getScriptPath("02TERMFINALSTEP");
    if (!QFile::exists(scriptPath)) {
        outputToTerminal("Error: Final script not found: " + scriptPath, Error);
        return;
    }

    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString monthAbbrev = convertMonthToAbbreviation(m_monthDDbox ? m_monthDDbox->currentText() : "");
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";

    if (jobNumber.isEmpty() || monthAbbrev.isEmpty() || year.isEmpty()) {
        outputToTerminal("Error: Job number, month, or year not available", Error);
        return;
    }

    outputToTerminal("Starting final processing script...", Info);
    outputToTerminal(QString("Job: %1, Month: %2, Year: %3").arg(jobNumber, monthAbbrev, year), Info);
    m_lastExecutedScript = "02TERMFINALSTEP";

    QStringList arguments;
    arguments << jobNumber << monthAbbrev << year;  // Added year argument

    m_scriptRunner->runScript(scriptPath, arguments);
}

void TMTermController::onScriptOutput(const QString& output)
{
    outputToTerminal(output, Info);
    parseScriptOutput(output);
}

void TMTermController::onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)

    outputToTerminal("Script execution completed", Info);

    // Re-enable buttons after script completion
    if (m_runInitialBtn) m_runInitialBtn->setEnabled(true);
    if (m_finalStepBtn) m_finalStepBtn->setEnabled(true);
}

// Validation methods
bool TMTermController::validateJobData()
{
    if (!validateJobNumber(m_jobNumberBox ? m_jobNumberBox->text() : "")) {
        outputToTerminal("Error: Job number must be exactly 5 digits", Error);
        return false;
    }

    if (!validateMonthSelection(m_monthDDbox ? m_monthDDbox->currentText() : "")) {
        outputToTerminal("Error: Month must be selected (01-12)", Error);
        return false;
    }

    if (!m_yearDDbox || m_yearDDbox->currentText().isEmpty()) {
        outputToTerminal("Error: Year must be selected", Error);
        return false;
    }

    return true;
}

bool TMTermController::validatePostageData()
{
    if (!m_postageBox || m_postageBox->text().isEmpty()) {
        outputToTerminal("Error: Postage amount is required", Error);
        return false;
    }

    if (!m_countBox || m_countBox->text().isEmpty()) {
        outputToTerminal("Error: Count is required", Error);
        return false;
    }

    return true;
}

bool TMTermController::validateJobNumber(const QString& jobNumber)
{
    if (jobNumber.length() != 5) {
        return false;
    }

    // Check if all characters are digits
    for (const QChar& ch : jobNumber) {
        if (!ch.isDigit()) {
            return false;
        }
    }

    return true;
}

bool TMTermController::validateMonthSelection(const QString& month)
{
    if (month.isEmpty()) {
        return false;
    }

    bool ok;
    int monthInt = month.toInt(&ok);
    return ok && monthInt >= 1 && monthInt <= 12;
}

void TMTermController::updateControlStates()
{
    // Job data fields - enabled when not locked
    bool jobFieldsEnabled = !m_jobDataLocked;
    if (m_jobNumberBox) m_jobNumberBox->setEnabled(jobFieldsEnabled);
    if (m_yearDDbox) m_yearDDbox->setEnabled(jobFieldsEnabled);
    if (m_monthDDbox) m_monthDDbox->setEnabled(jobFieldsEnabled);

    // Postage data fields - enabled when postage not locked
    if (m_postageBox) m_postageBox->setEnabled(!m_postageDataLocked);
    if (m_countBox) m_countBox->setEnabled(!m_postageDataLocked);

    // Lock button states
    if (m_lockBtn) m_lockBtn->setChecked(m_jobDataLocked);
    if (m_postageLockBtn) m_postageLockBtn->setChecked(m_postageDataLocked);

    // Edit button only enabled when job data is locked
    if (m_editBtn) m_editBtn->setEnabled(m_jobDataLocked);

    // Postage lock can only be engaged if job data is locked
    if (m_postageLockBtn) m_postageLockBtn->setEnabled(m_jobDataLocked);

    // Script buttons enabled based on lock states
    if (m_runInitialBtn) m_runInitialBtn->setEnabled(m_jobDataLocked);
    if (m_finalStepBtn) m_finalStepBtn->setEnabled(m_postageDataLocked);
}

void TMTermController::updateHtmlDisplay()
{
    if (!m_textBrowser) {
        outputToTerminal("DEBUG: No text browser available!", Error);
        return;
    }

    HtmlDisplayState targetState = determineHtmlState();

    // DEBUG OUTPUT
    outputToTerminal(QString("DEBUG: Job locked = %1").arg(m_jobDataLocked ? "TRUE" : "FALSE"), Info);
    outputToTerminal(QString("DEBUG: Current HTML state = %1").arg(m_currentHtmlState), Info);
    outputToTerminal(QString("DEBUG: Target HTML state = %1").arg(targetState), Info);
    outputToTerminal(QString("DEBUG: Target state name = %1").arg(targetState == InstructionsState ? "INSTRUCTIONS" : "DEFAULT"), Info);

    if (m_currentHtmlState == UninitializedState || m_currentHtmlState != targetState) {
        m_currentHtmlState = targetState;

        // Load appropriate HTML file based on state
        if (targetState == InstructionsState) {
            outputToTerminal("DEBUG: Loading instructions.html", Info);
            loadHtmlFile(":/resources/tmterm/instructions.html");
        } else {
            outputToTerminal("DEBUG: Loading default.html", Info);
            loadHtmlFile(":/resources/tmterm/default.html");
        }
    } else {
        outputToTerminal("DEBUG: HTML state unchanged, not loading new file", Info);
    }
}

void TMTermController::loadHtmlFile(const QString& resourcePath)
{
    if (!m_textBrowser) return;

    QFile file(resourcePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        QString htmlContent = stream.readAll();
        m_textBrowser->setHtml(htmlContent);
        file.close();
        Logger::instance().info("Loaded HTML file: " + resourcePath);
    } else {
        Logger::instance().warning("Failed to load HTML file: " + resourcePath);
        m_textBrowser->setHtml("<p>Instructions not available</p>");
    }
}

TMTermController::HtmlDisplayState TMTermController::determineHtmlState() const
{
    // DEBUG OUTPUT
    qDebug() << "DEBUG determineHtmlState: m_jobDataLocked =" << (m_jobDataLocked ? "TRUE" : "FALSE");

    // Show instructions when job data is locked
    if (m_jobDataLocked) {
        qDebug() << "DEBUG determineHtmlState: Returning InstructionsState";
        return InstructionsState;  // Show instructions.html when job is locked
    } else {
        qDebug() << "DEBUG determineHtmlState: Returning DefaultState";
        return DefaultState;       // Show default.html otherwise
    }
}

void TMTermController::outputToTerminal(const QString& message, MessageType type)
{
    if (!m_terminalWindow) return;

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString colorClass;

    switch (type) {
    case Error:
        colorClass = "error";
        break;
    case Success:
        colorClass = "success";
        break;
    case Warning:
        colorClass = "warning";
        break;
    case Info:
    default:
        colorClass = "";
        break;
    }

    QString formattedMessage = QString("[%1] %2").arg(timestamp, message);

    if (!colorClass.isEmpty()) {
        formattedMessage = QString("<span class=\"%1\">%2</span>").arg(colorClass, formattedMessage);
    }

    m_terminalWindow->append(formattedMessage);

    // Auto-scroll to bottom
    QTextCursor cursor = m_terminalWindow->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_terminalWindow->setTextCursor(cursor);
}

void TMTermController::createBaseDirectories()
{
    if (!m_fileManager) return;

    QString basePath = m_fileManager->getBasePath();
    QDir baseDir(basePath);

    if (!baseDir.exists()) {
        if (baseDir.mkpath(".")) {
            outputToTerminal("Created base directory: " + basePath, Info);
        } else {
            outputToTerminal("Failed to create base directory: " + basePath, Error);
            return;
        }
    }

    // Create required subdirectories
    QStringList subdirs = {"DATA", "ARCHIVE"};
    for (const QString& subdir : subdirs) {
        QString subdirPath = basePath + "/" + subdir;
        QDir dir(subdirPath);
        if (!dir.exists()) {
            if (dir.mkpath(".")) {
                outputToTerminal("Created directory: " + subdirPath, Info);
            } else {
                outputToTerminal("Failed to create directory: " + subdirPath, Error);
            }
        }
    }
}

void TMTermController::createJobFolder()
{
    if (!m_fileManager) return;

    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty()) {
        outputToTerminal("Cannot create job folder: missing job data", Warning);
        return;
    }

    if (m_fileManager->createJobFolder(year, month)) {
        outputToTerminal("Job folder created successfully", Info);
    } else {
        outputToTerminal("Failed to create job folder", Error);
    }
}

void TMTermController::parseScriptOutput(const QString& output)
{
    // Check for NAS path in output
    if (output.contains("NAS Path:")) {
        QString nasPath = output.split("NAS Path:").last().trimmed();
        if (!nasPath.isEmpty()) {
            m_capturedNASPath = nasPath;
            showNASLinkDialog(nasPath);
        }
    }
}

void TMTermController::showNASLinkDialog(const QString& nasPath)
{
    NASLinkDialog* dialog = new NASLinkDialog(nasPath, qobject_cast<QWidget*>(parent()));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

QString TMTermController::convertMonthToAbbreviation(const QString& monthNumber) const
{
    QMap<QString, QString> monthMap = {
        {"01", "JAN"}, {"02", "FEB"}, {"03", "MAR"}, {"04", "APR"},
        {"05", "MAY"}, {"06", "JUN"}, {"07", "JUL"}, {"08", "AUG"},
        {"09", "SEP"}, {"10", "OCT"}, {"11", "NOV"}, {"12", "DEC"}
    };

    return monthMap.value(monthNumber, "");
}

QString TMTermController::getJobDescription() const
{
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString monthAbbrev = convertMonthToAbbreviation(month);

    if (monthAbbrev.isEmpty()) {
        return "TM TERM";
    }

    return QString("TM %1 TERM").arg(monthAbbrev);
}

bool TMTermController::hasJobData() const
{
    // Check if we have essential job data
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    return !jobNumber.isEmpty() && !year.isEmpty() && !month.isEmpty();
}

void TMTermController::debugCheckTables()
{
    if (!m_tmTermDBManager) {
        outputToTerminal("DEBUG: No database manager available", Info);
        return;
    }

    outputToTerminal("DEBUG: Checking database tables...", Info);
    // Add any debug table checking logic here if needed
}

bool TMTermController::moveFilesToHomeFolder()
{
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";

    if (jobNumber.isEmpty() || month.isEmpty()) {
        outputToTerminal("Cannot move files: missing job number or month", Warning);
        return false;
    }

    // Convert month to three-letter abbreviation
    QString monthAbbrev = convertMonthToAbbreviation(month);
    if (monthAbbrev.isEmpty()) {
        outputToTerminal("Cannot move files: invalid month format", Warning);
        return false;
    }

    QString basePath = "C:/Goji/TRACHMAR/TERM";
    QString dataFolder = basePath + "/DATA";
    QString homeFolder = jobNumber + " " + monthAbbrev;  // Format: "37580 JUL"
    QString homeFolderPath = basePath + "/ARCHIVE/" + homeFolder;

    // Create home folder if it doesn't exist
    QDir homeDir(homeFolderPath);
    if (!homeDir.exists()) {
        if (!homeDir.mkpath(".")) {
            outputToTerminal("Failed to create HOME folder: " + homeFolderPath, Error);
            return false;
        }
        outputToTerminal("Created HOME folder: " + homeFolderPath, Info);
    }

    // Move files from DATA to HOME folder
    QDir dataDir(dataFolder);
    if (dataDir.exists()) {
        QStringList files = dataDir.entryList(QDir::Files);
        for (const QString& fileName : files) {
            QString sourcePath = dataFolder + "/" + fileName;
            QString destPath = homeFolderPath + "/" + fileName;

            // Remove existing file in destination if it exists
            if (QFile::exists(destPath)) {
                QFile::remove(destPath);
            }

            // Move file
            if (!QFile::rename(sourcePath, destPath)) {
                outputToTerminal("Failed to move file: " + sourcePath, Error);
                return false;
            } else {
                outputToTerminal("Moved file: " + fileName + " to " + homeFolder, Info);
            }
        }
    }

    return true;
}

bool TMTermController::copyFilesFromHomeFolder()
{
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";

    if (jobNumber.isEmpty() || month.isEmpty()) {
        outputToTerminal("Cannot copy files: missing job number or month", Warning);
        return false;
    }

    // Convert month to three-letter abbreviation
    QString monthAbbrev = convertMonthToAbbreviation(month);
    if (monthAbbrev.isEmpty()) {
        outputToTerminal("Cannot copy files: invalid month format", Warning);
        return false;
    }

    QString basePath = "C:/Goji/TRACHMAR/TERM";
    QString dataFolder = basePath + "/DATA";
    QString homeFolder = jobNumber + " " + monthAbbrev;  // Format: "37580 JUL"
    QString homeFolderPath = basePath + "/ARCHIVE/" + homeFolder;

    // Check if home folder exists
    QDir homeDir(homeFolderPath);
    if (!homeDir.exists()) {
        outputToTerminal("HOME folder does not exist: " + homeFolderPath, Info);
        outputToTerminal("This is normal for new jobs - no files to copy", Info);
        return true; // Not an error if no previous job exists
    }

    // Create DATA folder if it doesn't exist
    QDir dataDir(dataFolder);
    if (!dataDir.exists()) {
        if (!dataDir.mkpath(".")) {
            outputToTerminal("Failed to create DATA folder: " + dataFolder, Error);
            return false;
        }
    }

    // Copy files from HOME to DATA folder
    QStringList files = homeDir.entryList(QDir::Files);
    for (const QString& fileName : files) {
        QString sourcePath = homeFolderPath + "/" + fileName;
        QString destPath = dataFolder + "/" + fileName;

        // Remove existing file in destination if it exists
        if (QFile::exists(destPath)) {
            QFile::remove(destPath);
        }

        // Copy file
        if (!QFile::copy(sourcePath, destPath)) {
            outputToTerminal("Failed to copy file: " + sourcePath, Error);
            return false;
        } else {
            outputToTerminal("Copied file: " + fileName + " from " + homeFolder, Info);
        }
    }

    return true;
}

bool TMTermController::moveFilesToBasicHomeFolder(const QString& year, const QString& month)
{
    // Get job number for proper folder naming
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    
    if (jobNumber.isEmpty() || month.isEmpty()) {
        outputToTerminal("Cannot move files: missing job number or month", Warning);
        return false;
    }

    // Convert month to three-letter abbreviation
    QString monthAbbrev = convertMonthToAbbreviation(month);
    if (monthAbbrev.isEmpty()) {
        outputToTerminal("Cannot move files: invalid month format", Warning);
        return false;
    }

    QString basePath = "C:/Goji/TRACHMAR/TERM";
    QString dataFolder = basePath + "/DATA";
    QString homeFolder = jobNumber + " " + monthAbbrev;  // Format: "37580 JUL"
    QString homeFolderPath = basePath + "/ARCHIVE/" + homeFolder;

    // Create home folder if it doesn't exist
    QDir homeDir(homeFolderPath);
    if (!homeDir.exists()) {
        if (!homeDir.mkpath(".")) {
            outputToTerminal("Failed to create HOME folder: " + homeFolderPath, Error);
            return false;
        }
        outputToTerminal("Created HOME folder: " + homeFolderPath, Info);
    }

    // Move files from DATA to HOME folder
    QDir dataDir(dataFolder);
    if (dataDir.exists()) {
        QStringList files = dataDir.entryList(QDir::Files);
        for (const QString& fileName : files) {
            QString sourcePath = dataFolder + "/" + fileName;
            QString destPath = homeFolderPath + "/" + fileName;

            // Remove existing file in destination if it exists
            if (QFile::exists(destPath)) {
                QFile::remove(destPath);
            }

            // Move file
            if (!QFile::rename(sourcePath, destPath)) {
                outputToTerminal("Failed to move file: " + sourcePath, Error);
                return false;
            } else {
                outputToTerminal("Moved file: " + fileName + " to " + homeFolder, Info);
            }
        }
    }

    return true;
}

void TMTermController::setupOptimizedTableLayout()
{
    if (!m_tracker) return;

    // Calculate optimal font size and column widths
    const int tableWidth = 611; // Fixed widget width from UI
    const int borderWidth = 2;   // Account for table borders
    const int availableWidth = tableWidth - borderWidth;

    // Define maximum content widths based on TMTERM data format
    struct ColumnSpec {
        QString header;
        QString maxContent;
        int minWidth;
    };

    QList<ColumnSpec> columns = {
        {"JOB", "88888", 55},           // Same width as TMWEEKLYPC
        {"DESCRIPTION", "TM DEC TERM", 120}, // TMTERM description format
        {"POSTAGE", "$888,888.88", 49}, // Match TMWEEKLYPC reduced width
        {"COUNT", "88,888", 44},        // Keep for wider display
        {"PER PIECE", "0.888", 45},      // Keep same
        {"CLASS", "STD", 75},           // Reduced from 120 (was too wide)
        {"SHAPE", "LTR", 32},           // Same width as TMWEEKLYPC
        {"PERMIT", "NKLN", 35}          // TMTERM permit format
    };

    // Calculate optimal font size - START BIGGER
    QFont testFont("Consolas", 7); // Start with slightly bigger font
    QFontMetrics fm(testFont);

    // Find the largest font size that fits all columns - increase max size to 11
    int optimalFontSize = 7;
    for (int fontSize = 11; fontSize >= 7; fontSize--) {
        testFont.setPointSize(fontSize);
        fm = QFontMetrics(testFont);

        int totalWidth = 0;
        bool fits = true;

        for (const auto& col : columns) {
            int headerWidth = fm.horizontalAdvance(col.header) + 12; // Increased padding
            int contentWidth = fm.horizontalAdvance(col.maxContent) + 12; // Increased padding
            int colWidth = qMax(headerWidth, qMax(contentWidth, col.minWidth));
            totalWidth += colWidth;

            if (totalWidth > availableWidth) {
                fits = false;
                break;
            }
        }

        if (fits) {
            optimalFontSize = fontSize;
            break;
        }
    }

    // Apply the optimal font
    QFont tableFont("Consolas", optimalFontSize);
    m_tracker->setFont(tableFont);

    // Set up the model with proper ordering (newest first)
    m_trackerModel->setSort(0, Qt::DescendingOrder); // Sort by ID descending
    m_trackerModel->select();

    // Set custom headers - SAME AS TMWEEKLYPC (except PER PIECE instead of AVG RATE)
    m_trackerModel->setHeaderData(1, Qt::Horizontal, tr("JOB"));
    m_trackerModel->setHeaderData(2, Qt::Horizontal, tr("DESCRIPTION"));
    m_trackerModel->setHeaderData(3, Qt::Horizontal, tr("POSTAGE"));
    m_trackerModel->setHeaderData(4, Qt::Horizontal, tr("COUNT"));
    m_trackerModel->setHeaderData(5, Qt::Horizontal, tr("PER PIECE"));
    m_trackerModel->setHeaderData(6, Qt::Horizontal, tr("CLASS"));
    m_trackerModel->setHeaderData(7, Qt::Horizontal, tr("SHAPE"));
    m_trackerModel->setHeaderData(8, Qt::Horizontal, tr("PERMIT"));

    // Hide ALL unwanted columns (assuming columns 0, 9, 10 are id, date, created_at)
    m_tracker->setColumnHidden(0, true);  // Hide ID column

    // Check total column count and hide extra columns
    int totalCols = m_trackerModel->columnCount();
    for (int i = 9; i < totalCols; i++) {
        m_tracker->setColumnHidden(i, true);  // Hide date, created_at, etc.
    }

    // Calculate and set precise column widths
    fm = QFontMetrics(tableFont);
    for (int i = 0; i < columns.size(); i++) {
        const auto& col = columns[i];
        int headerWidth = fm.horizontalAdvance(col.header) + 12; // Increased padding
        int contentWidth = fm.horizontalAdvance(col.maxContent) + 12; // Increased padding
        int colWidth = qMax(headerWidth, qMax(contentWidth, col.minWidth));

        m_tracker->setColumnWidth(i + 1, colWidth); // +1 because we hide column 0
    }

    // Disable horizontal header resize to maintain fixed widths
    m_tracker->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    // Enable only vertical scrolling
    m_tracker->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tracker->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Apply enhanced styling for better readability
    m_tracker->setStyleSheet(
        "QTableView {"
        "   border: 1px solid black;"
        "   selection-background-color: #d0d0ff;"
        "   alternate-background-color: #f8f8f8;"
        "   gridline-color: #cccccc;"
        "}"
        "QHeaderView::section {"
        "   background-color: #e0e0e0;"
        "   padding: 4px;"      // Increased padding for better visibility
        "   border: 1px solid black;"
        "   font-weight: bold;"
        "   font-family: 'Consolas';"
        "}"
        "QTableView::item {"
        "   padding: 3px;"      // Increased padding for better visibility
        "   border-right: 1px solid #cccccc;"
        "}"
        );

    // Enable alternating row colors
    m_tracker->setAlternatingRowColors(true);
}

void TMTermController::showTableContextMenu(const QPoint& pos)
{
    QMenu menu(m_tracker);
    QAction* copyAction = menu.addAction("Copy Selected Row");
    QAction* selectedAction = menu.exec(m_tracker->mapToGlobal(pos));
    if (selectedAction == copyAction) {
        QString result = copyFormattedRow();
        if (result == "Row copied to clipboard") {
            outputToTerminal("Row copied to clipboard", Success);
        } else {
            outputToTerminal("Failed to copy row: " + result, Error);
        }
    }
}



// BaseTrackerController implementation methods
QTableView* TMTermController::getTrackerWidget() const
{
    return m_tracker;
}

QSqlTableModel* TMTermController::getTrackerModel() const
{
    return m_trackerModel;
}

QStringList TMTermController::getTrackerHeaders() const
{
    // Same headers as TMWEEKLYPC except PER PIECE instead of AVG RATE
    return {"JOB", "DESCRIPTION", "POSTAGE", "COUNT", "PER PIECE", "CLASS", "SHAPE", "PERMIT"};
}

QList<int> TMTermController::getVisibleColumns() const
{
    return {1, 2, 3, 4, 5, 6, 7, 8}; // Skip column 0 (ID), exclude date column
}

QString TMTermController::formatCellData(int columnIndex, const QString& cellData) const
{
    // Format POSTAGE column to include $ symbol and thousand separators
    if (columnIndex == 2 && !cellData.isEmpty()) {
        QString cleanData = cellData;
        cleanData.remove("$");
        bool ok;
        double number = cleanData.toDouble(&ok);
        if (ok) {
            return QString("$%L1").arg(number, 0, 'f', 2);
        } else if (!cellData.startsWith("$")) {
            return "$" + cellData;
        }
    }
    // Format COUNT column with thousand separators
    if (columnIndex == 3 && !cellData.isEmpty()) {
        bool ok;
        int number = cellData.toInt(&ok);
        if (ok) {
            return QString("%L1").arg(number);
        }
    }
    return cellData;
}

// ADDED: Save job functionality for menu system consistency
void TMTermController::onSaveJobClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Job must be locked before it can be saved", Warning);
        return;
    }

    // Save job to database
    saveJobToDatabase();

    // Save complete job state (including postage data and lock states)
    saveJobState();

    outputToTerminal("Job saved successfully", Success);
}

// ADDED: Close job functionality for menu system consistency
void TMTermController::onCloseJobClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("No job is currently open to close", Warning);
        return;
    }

    // Save current job state before closing
    outputToTerminal("Saving job state before closing...", Info);
    saveJobState();

    // Reset to defaults (this will clear UI, reset states, and move files)
    resetToDefaults();

    outputToTerminal("Job closed successfully", Success);
}
