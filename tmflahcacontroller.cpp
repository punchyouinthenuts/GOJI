#include "tmflahcacontroller.h"
#include "logger.h"
#include "naslinkdialog.h"
#include "dropwindow.h"
#include "dropbindinghelper.h"
#include "scriptrunnerbindinghelper.h"
#include "monthcomboboxhelper.h"
#include "yearcomboboxhelper.h"
#include "terminaloutputhelper.h"
#include <QDesktopServices>
#include <QUrl>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QHeaderView>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QAbstractItemView>
#include <QToolButton>
#include <QMenu>
#include <QAction>

#include "tmflahcaemaildialog.h"
#include "tmflahcafilemanager.h"
#include "scriptrunner.h"
#include <QPointer>
#include <QRegularExpression>

class FLAHCAFormattedSqlModel : public QSqlTableModel {
public:
    FLAHCAFormattedSqlModel(QObject *parent, QSqlDatabase db, TMFLAHCAController *ctrl)
        : QSqlTableModel(parent, db), controller(ctrl) {}
    QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole) {
            QVariant val = QSqlTableModel::data(idx, role);
            return controller->formatCellData(idx.column(), val.toString());
        }
        return QSqlTableModel::data(idx, role);
    }
private:
    TMFLAHCAController *controller;
};

TMFLAHCAController::TMFLAHCAController(QObject *parent, const QString& basePath)
    : BaseTrackerController(parent)
    , m_fileManager(nullptr)
    , m_tmFlahcaDBManager(nullptr)
    , m_scriptRunner(nullptr)
    , m_jobNumberBox(nullptr)
    , m_yearDDbox(nullptr)
    , m_monthDDbox(nullptr)
    , m_postageBox(nullptr)
    , m_countBox(nullptr)
    , m_jobDataLockBtn(nullptr)
    , m_editBtn(nullptr)
    , m_postageLockBtn(nullptr)
    , m_openBulkMailerBtn(nullptr)
    , m_runInitialBtn(nullptr)
    , m_finalStepBtn(nullptr)
    , m_terminalWindow(nullptr)
    , m_textBrowser(nullptr)
    , m_tracker(nullptr)
    , m_dropWindow(nullptr)
    , m_jobDataLocked(false)
    , m_postageDataLocked(false)
    , m_currentHtmlState(UninitializedState)
    , m_capturingNASPath(false)
    , m_waitingForEmailConfirmation(false)
    , m_emailDialog(nullptr)
    , m_showEmailAfterScript(false)
    , m_trackerModel(nullptr)
    , m_lastYear(-1)
    , m_lastMonth(-1)
{
    initializeComponents(basePath);
    connectSignals();
    setupInitialState();
    initializeAfterConstruction();
}

TMFLAHCAController::~TMFLAHCAController()
{
    // Clean up email dialog if it exists
    if (m_emailDialog) {
        m_emailDialog->deleteLater();
        m_emailDialog = nullptr;
    }
    // Note: UI widgets are managed by Qt's parent-child system
    // Script runner is QObject-owned; the file manager is ordinary C++.
    delete m_fileManager;
}

void TMFLAHCAController::initializeComponents(const QString& basePath)
{
    // Initialize file manager
    m_fileManager = new TMFLAHCAFileManager(nullptr, basePath);

    // Initialize database manager
    m_tmFlahcaDBManager = TMFLAHCADBManager::instance();

    // Initialize script runner
    m_scriptRunner = new ScriptRunner(this);

    // NOTE: Do NOT call createBaseDirectories() here.


    // We move it to initializeAfterConstruction() to avoid virtual calls during construction.

    Logger::instance().info("TMFLAHCA controller components initialized");
}

// Safe post-construction initializer properly placed
void TMFLAHCAController::initializeAfterConstruction()
{
    // Safe point to perform actions that might call virtuals / logging
    createBaseDirectories();
}


// Added safe post-construction initializer


void TMFLAHCAController::createBaseDirectories()
{
    if (m_fileManager) {
        if (m_fileManager->createBaseDirectories()) {
            outputToTerminal("Base directories created successfully", Success);
        } else {
            outputToTerminal("Failed to create some base directories", Warning);
        }
    } else {
        outputToTerminal("File manager not initialized - cannot create directories", Error);
    }
}

void TMFLAHCAController::connectSignals()
{
    // Connect script runner signals
    if (m_scriptRunner) {
        ScriptRunnerBindingHelper::setupBaselineBindings(
            m_scriptRunner,
            this,
            [this](const QString& output) { onScriptOutput(output); },
            [this](int exitCode, QProcess::ExitStatus exitStatus) { onScriptFinished(exitCode, exitStatus); });
    }
}

void TMFLAHCAController::setupInitialState()
{
    // Initialize states
    m_jobDataLocked = false;
    m_postageDataLocked = false;
    m_currentHtmlState = UninitializedState;
    m_waitingForEmailConfirmation = false;

    // Update UI states
    updateLockStates();
    updateButtonStates();
    updateHtmlDisplay();

    Logger::instance().info("TMFLAHCA controller initial state set");
}

// UI Widget setters
void TMFLAHCAController::setJobNumberBox(QLineEdit* lineEdit)
{
    m_jobNumberBox = lineEdit;
    if (m_jobNumberBox) {
        connect(m_jobNumberBox, &QLineEdit::editingFinished, this, [this]() {
            const QString newNum = m_jobNumberBox->text().trimmed();
            if (newNum.isEmpty() || !validateJobNumber(newNum)) return;

            if (newNum != m_cachedJobNumber) {
                saveJobState();
                TMFLAHCADBManager::instance()->updateLogJobNumber(m_cachedJobNumber, newNum);
                m_cachedJobNumber = newNum;
                refreshTrackerTable();
            }
        });
    }
}

void TMFLAHCAController::setYearDropdown(QComboBox* comboBox)
{
    m_yearDDbox = comboBox;
    
    // Populate year dropdown on initialization
    if (m_yearDDbox) {
        populateYearDropdown();
        
        // Connect year change signal
        connect(m_yearDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
                this, &TMFLAHCAController::onYearChanged);
    }
}

void TMFLAHCAController::setMonthDropdown(QComboBox* comboBox)
{
    m_monthDDbox = comboBox;
    
    // Populate month dropdown on initialization
    if (m_monthDDbox) {
        populateMonthDropdown();
        
        // Connect month change signal
        connect(m_monthDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
                this, &TMFLAHCAController::onMonthChanged);
    }
}

void TMFLAHCAController::setPostageBox(QLineEdit* lineEdit)
{
    m_postageBox = lineEdit;
    if (m_postageBox) {
        QRegularExpressionValidator* validator = new QRegularExpressionValidator(QRegularExpression("[0-9]*\\.?[0-9]*\\$?"), this);
        m_postageBox->setValidator(validator);
        connect(m_postageBox, &QLineEdit::editingFinished, this, &TMFLAHCAController::formatPostageInput);

        // Auto-save on postage changes when job is locked
        connect(m_postageBox, &QLineEdit::textChanged, this, [this]() {
            if (m_jobDataLocked) {
                saveJobState();
            }
        });
    }
}

void TMFLAHCAController::setCountBox(QLineEdit* lineEdit)
{
    m_countBox = lineEdit;
    if (m_countBox) {
        connect(m_countBox, &QLineEdit::textChanged, this, &TMFLAHCAController::formatCountInput);

        // Auto-save on count changes when job is locked
        connect(m_countBox, &QLineEdit::textChanged, this, [this]() {
            if (m_jobDataLocked) {
                saveJobState();
            }
        });
    }
}

void TMFLAHCAController::setJobDataLockButton(QToolButton* button)
{
    m_jobDataLockBtn = button;
    if (m_jobDataLockBtn) {
        connect(m_jobDataLockBtn, &QToolButton::clicked, this, &TMFLAHCAController::onJobDataLockClicked);
    }
}

void TMFLAHCAController::setEditButton(QToolButton* button)
{
    m_editBtn = button;
    if (m_editBtn) {
        connect(m_editBtn, &QToolButton::clicked, this, &TMFLAHCAController::onEditButtonClicked);
    }
}

void TMFLAHCAController::setPostageLockButton(QToolButton* button)
{
    m_postageLockBtn = button;
    if (m_postageLockBtn) {
        connect(m_postageLockBtn, &QToolButton::clicked, this, &TMFLAHCAController::onPostageLockClicked);
    }
}

void TMFLAHCAController::setOpenBulkMailerButton(QPushButton* button)
{
    m_openBulkMailerBtn = button;
    if (m_openBulkMailerBtn) {
        connect(m_openBulkMailerBtn, &QPushButton::clicked,
                this, &TMFLAHCAController::onOpenBulkMailerClicked);
    }
}

void TMFLAHCAController::setRunInitialButton(QPushButton* button)
{
    m_runInitialBtn = button;
    if (m_runInitialBtn) {
        connect(m_runInitialBtn, &QPushButton::clicked, this, &TMFLAHCAController::onRunInitialClicked);
    }
}

void TMFLAHCAController::setFinalStepButton(QPushButton* button)
{
    m_finalStepBtn = button;
    if (m_finalStepBtn) {
        connect(m_finalStepBtn, &QPushButton::clicked, this, &TMFLAHCAController::onFinalStepClicked);
    }
}

void TMFLAHCAController::setTerminalWindow(QTextEdit* textEdit)
{
    m_terminalWindow = textEdit;
}

void TMFLAHCAController::setTextBrowser(QTextBrowser* textBrowser)
{
    m_textBrowser = textBrowser;
    updateHtmlDisplay();
}

void TMFLAHCAController::setTracker(QTableView* tableView)
{
    m_tracker = tableView;
    setupTrackerModel();
}

void TMFLAHCAController::setDropWindow(DropWindow* dropWindow)
{
    m_dropWindow = dropWindow;
    if (m_dropWindow) {
        setupDropWindow();
    }
    updateLockStates();
    updateButtonStates();
}

// Public getters
QString TMFLAHCAController::getJobNumber() const
{
    return m_jobNumberBox ? m_jobNumberBox->text() : "";
}

QString TMFLAHCAController::getYear() const
{
    return m_yearDDbox ? m_yearDDbox->currentText() : "";
}

QString TMFLAHCAController::getMonth() const
{
    return m_monthDDbox ? m_monthDDbox->currentText() : "";
}

bool TMFLAHCAController::isJobDataLocked() const
{
    return m_jobDataLocked;
}

bool TMFLAHCAController::isPostageDataLocked() const
{
    return m_postageDataLocked;
}

bool TMFLAHCAController::hasJobData() const
{
    QString jobNumber = getJobNumber();
    QString year = getYear();
    QString month = getMonth();

    return !jobNumber.isEmpty() && !year.isEmpty() && !month.isEmpty();
}

// Utility method to convert month number to abbreviation
QString TMFLAHCAController::convertMonthToAbbreviation(const QString& monthNumber) const
{
    QMap<QString, QString> monthMap = {
        {"01", "JAN"}, {"02", "FEB"}, {"03", "MAR"}, {"04", "APR"},
        {"05", "MAY"}, {"06", "JUN"}, {"07", "JUL"}, {"08", "AUG"},
        {"09", "SEP"}, {"10", "OCT"}, {"11", "NOV"}, {"12", "DEC"}
    };
    return monthMap.value(monthNumber, monthNumber);
}

// BaseTrackerController implementation
void TMFLAHCAController::outputToTerminal(const QString& message, MessageType type)
{
    TerminalSeverity severity = TerminalSeverity::Info;
    switch (type) {
    case Error:
        severity = TerminalSeverity::Error;
        break;
    case Success:
        severity = TerminalSeverity::Success;
        break;
    case Warning:
        severity = TerminalSeverity::Warning;
        break;
    case Info:
    default:
        severity = TerminalSeverity::Info;
        break;
    }

    TerminalOutputHelper::append(m_terminalWindow, message, severity);
}

QTableView* TMFLAHCAController::getTrackerWidget() const
{
    return m_tracker;
}

QSqlTableModel* TMFLAHCAController::getTrackerModel() const
{
    return m_trackerModel;
}

QStringList TMFLAHCAController::getTrackerHeaders() const
{
    return {"JOB", "DESCRIPTION", "POSTAGE", "COUNT", "AVG RATE", "CLASS", "SHAPE", "PERMIT"};
}

QList<int> TMFLAHCAController::getVisibleColumns() const
{
    // Skip column 0 (ID), return columns 1-8
    return {1, 2, 3, 4, 5, 6, 7, 8};
}

QString TMFLAHCAController::formatCellData(int columnIndex, const QString& cellData) const
{
    // NOTE: This method is called from FLAHCAFormattedSqlModel with database column indices
    // POSTAGE is database column 3, COUNT is database column 4
    if (columnIndex == 3) { // POSTAGE (database column)
        QString clean = cellData;
        if (clean.startsWith("$")) clean.remove(0, 1);
        bool ok;
        double val = clean.toDouble(&ok);
        if (ok) {
            return QString("$%L1").arg(val, 0, 'f', 2);
        } else {
            return cellData;
        }
    }
    if (columnIndex == 4) { // COUNT (database column)
        bool ok;
        qlonglong val = cellData.toLongLong(&ok);
        if (ok) {
            return QString("%L1").arg(val);
        } else {
            return cellData;
        }
    }
    return cellData;
}

QString TMFLAHCAController::formatCellDataForCopy(int columnIndex, const QString& cellData) const
{
    // For copy operations: columnIndex is position in visible columns list
    // POSTAGE is visible column 2, COUNT is visible column 3
    if (columnIndex == 2) { // POSTAGE (visible column position)
        QString clean = cellData;
        if (clean.startsWith("$")) clean.remove(0, 1);
        bool ok;
        double val = clean.toDouble(&ok);
        if (ok) {
            return QString("$%L1").arg(val, 0, 'f', 2);
        } else {
            return cellData;
        }
    }
    if (columnIndex == 3) { // COUNT (visible column position) - return as plain integer
        QString cleanData = cellData;
        cleanData.remove(',');
        bool ok;
        qlonglong val = cleanData.toLongLong(&ok);
        if (ok) {
            return QString::number(val); // Plain integer for copy
        } else {
            return cellData;
        }
    }
    return cellData;
}

// Lock button handlers
void TMFLAHCAController::onJobDataLockClicked()
{
    if (m_jobDataLockBtn->isChecked()) {
        // User is trying to lock the job
        if (!validateJobData()) {
            m_jobDataLockBtn->setChecked(false);
            outputToTerminal("Cannot lock job: Please correct the validation errors above.", Error);
            return;
        }

        // Get current values from UI
        int newYear = getYear().toInt();
        int newMonth = getMonth().toInt();
        QString newJobNumber = getJobNumber();
        
        // CRITICAL FIX FOR FL AHCA: Check if we're re-locking after editing with period/job change
        // If the period changed, delete the old row to prevent duplicates
        if (m_lastYear > 0 && m_lastMonth > 0 && !m_cachedJobNumber.isEmpty()) {
            if (newYear != m_lastYear || newMonth != m_lastMonth) {
                outputToTerminal(QString("Period changed during edit: OLD=%1/%2, NEW=%3/%4")
                               .arg(QString("%1").arg(m_lastMonth, 2, 10, QChar('0')), 
                                    QString::number(m_lastYear),
                                    QString("%1").arg(newMonth, 2, 10, QChar('0')), 
                                    QString::number(newYear)), Info);
                
                // Delete the old period row to prevent duplicate rows
                if (m_tmFlahcaDBManager->deleteJob(m_cachedJobNumber, m_lastYear, m_lastMonth)) {
                    outputToTerminal(QString("Deleted old period row: %1/%2")
                                   .arg(QString("%1").arg(m_lastMonth, 2, 10, QChar('0')),
                                        QString::number(m_lastYear)), Success);
                } else {
                    outputToTerminal("Warning: Could not delete old period row", Warning);
                }
            }
        }

        // Lock the job data
        m_jobDataLocked = true;
        if (m_editBtn) m_editBtn->setChecked(false);
        outputToTerminal("Job data locked.", Success);

        // Update cache to NEW values
        m_lastYear = newYear;
        m_lastMonth = newMonth;
        m_cachedJobNumber = newJobNumber;

        // Create folder for the job
        createJobFolder();

        // Copy files from HOME folder to DATA folder
        if (!copyFilesFromHomeFolder()) {
            m_jobDataLocked = false;
            updateLockStates();
            updateButtonStates();
            return;
        }

        // Save to database (UPDATE if exists for this year/month, INSERT if not)
        QString jobNumber = getJobNumber();
        QString year = getYear();
        QString month = getMonth();
        if (m_tmFlahcaDBManager->saveJob(jobNumber, year, month)) {
            outputToTerminal("Job saved to database", Success);
        } else {
            outputToTerminal("Failed to save job to database", Error);
        }

        // Save job state
        saveJobState();

        // Update control states and HTML display
        updateLockStates();
        updateButtonStates();
        m_currentHtmlState = UninitializedState;  // Force HTML refresh on job open
        updateHtmlDisplay();

        // Start auto-save timer since job is now locked/open
        emit jobOpened();
        outputToTerminal("Auto-save timer started (15 minutes)", Info);
    } else {
        // User unchecked lock button - this shouldn't happen in normal flow
        m_jobDataLockBtn->setChecked(true); // Force it back to checked
    }
}

void TMFLAHCAController::onEditButtonClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot edit job data until it is locked.", Error);
        m_editBtn->setChecked(false);
        return;
    }

    if (m_editBtn->isChecked()) {
        // Edit button was just checked - unlock job data for editing
        m_jobDataLocked = false;
        if (m_jobDataLockBtn) m_jobDataLockBtn->setChecked(false); // Unlock the lock button

        outputToTerminal("Job data unlocked for editing.", Info);
        updateLockStates();
        updateButtonStates();
        updateHtmlDisplay(); // This will switch back to default.html since job is no longer locked
    }
    // If edit button is unchecked, do nothing (ignore the click)
}

void TMFLAHCAController::onPostageLockClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot lock postage data: Job data must be locked first", Error);
        m_postageLockBtn->setChecked(false);
        return;
    }

    if (m_postageLockBtn->isChecked()) {
        // Validate postage data before locking
        if (!validatePostageData()) {
            m_postageLockBtn->setChecked(false);
            return;
        }
        
        // User is locking postage data
        m_postageDataLocked = true;
        outputToTerminal("Postage data locked", Success);
        
        // CRITICAL FIX: Add log entry when postage is locked (like other controllers)
        addLogEntry();
        
        // Save postage data persistently (like TMTERM does)
        saveJobState();
    } else {
        // User is unlocking postage data
        m_postageDataLocked = false;
        outputToTerminal("Postage data unlocked", Info);
        
        // Save the unlocked state
        saveJobState();
    }

    updateLockStates();
    updateButtonStates();
}

void TMFLAHCAController::onOpenBulkMailerClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Please lock job data before opening Bulk Mailer.", Warning);
        return;
    }

    outputToTerminal("Opening Bulk Mailer application...", Info);

    // Standard BCC Software path - used in other tabs (HEALTHY/BROKEN)
    QString bulkMailerPath = "C:/Program Files (x86)/BCC Software/Bulk Mailer/BulkMailer.exe";

    QFileInfo fileInfo(bulkMailerPath);
    if (!fileInfo.exists()) {
        // Try alternative Satori Software path
        bulkMailerPath = "C:/Program Files (x86)/Satori Software/Bulk Mailer/BulkMailer.exe";
        QFileInfo altFileInfo(bulkMailerPath);
        if (!altFileInfo.exists()) {
            outputToTerminal("Bulk Mailer not found. Please verify installation.", Error);
            outputToTerminal("Checked paths:", Info);
            outputToTerminal("  - C:/Program Files (x86)/BCC Software/Bulk Mailer/BulkMailer.exe", Info);
            outputToTerminal("  - C:/Program Files (x86)/Satori Software/Bulk Mailer/BulkMailer.exe", Info);
            return;
        }
    }

    // Launch Bulk Mailer
    bool success = QProcess::startDetached(bulkMailerPath, QStringList());

    if (success) {
        outputToTerminal("Bulk Mailer launched successfully", Success);
    } else {
        outputToTerminal("Failed to launch Bulk Mailer", Error);
    }
}

// Script execution handlers
void TMFLAHCAController::onRunInitialClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot run initial script: Job data must be locked first", Error);
        return;
    }

    executeScript("01 INITIAL");
}

void TMFLAHCAController::onFinalStepClicked()
{
    if (!m_postageDataLocked) {
        outputToTerminal("Cannot run final step: Postage data must be locked first", Error);
        return;
    }

    executeScript("02 FINAL PROCESS");
}

void TMFLAHCAController::executeScript(const QString& scriptName)
{
    if (isProcessing()) {
        outputToTerminal("Finish the current AHCA step before starting another.", Warning);
        return;
    }
    if (!validateScriptExecution(scriptName)) {
        return;
    }

    QString scriptPath = m_fileManager->getScriptPath(scriptName);

    // Check if script file exists
    if (!QFile::exists(scriptPath)) {
        outputToTerminal(QString("Script file not found: %1").arg(scriptPath), Error);
        outputToTerminal("Please ensure scripts are installed in the correct location", Warning);
        return;
    }

    m_lastExecutedScript = scriptName;
    m_capturedNASPath.clear();
    m_capturingNASPath = false;
    m_capturedPrintPath.clear();
    m_capturingPrintPath = false;
    m_showEmailAfterScript = false;

    outputToTerminal(QString("Executing script: %1").arg(scriptName), Info);
    outputToTerminal(QString("Script path: %1").arg(scriptPath), Info);

    // Prepare command line arguments
    QString jobNumber = getJobNumber();
    QString year = getYear();
    QString month = getMonth();
    
    QStringList args;
    if (scriptName == "02 FINAL PROCESS") {
        args << jobNumber << year << month << "--mode" << "prearchive";
    } else {
        args << jobNumber << year << month;
    }

    outputToTerminal(QString("Arguments: Job=%1, Year=%2, Month=%3").arg(jobNumber, year, month), Info);

    // Execute the Python script
    if (!m_scriptRunner->runScript(scriptPath, args)) {
        outputToTerminal("Unable to start the AHCA script.", Error);
    }
    updateButtonStates();
}

void TMFLAHCAController::onScriptOutput(const QString &output)
{
    // Always log script output
    outputToTerminal(output, Info);

    if (output.trimmed() == "=== PRINT_FOLDER_PATH ===") {
        m_capturingPrintPath = true;
        m_capturedPrintPath.clear();
        return;
    }
    if (output.trimmed() == "=== END_PRINT_FOLDER_PATH ===") {
        m_capturingPrintPath = false;
        return;
    }
    if (m_capturingPrintPath) {
        m_capturedPrintPath = output.trimmed();
        return;
    }

    // 1) NAS path capture
    if (output.trimmed() == "=== NAS_FOLDER_PATH ===") {
        m_capturingNASPath = true;
        m_capturedNASPath.clear();
        return;
    }
    if (output.trimmed() == "=== END_NAS_FOLDER_PATH ===") {
        m_capturingNASPath = false;
        if (!m_capturedNASPath.isEmpty()) {
            outputToTerminal("Captured NAS folder path: " + m_capturedNASPath, Info);
        }
        return;
    }
    if (m_capturingNASPath) {
        const QString line = output.trimmed();
        if (!line.isEmpty())
            m_capturedNASPath = line;
        return;
    }

    // Open the handoff only after the preparation process has exited successfully.
    if (output.contains("=== PAUSE_FOR_EMAIL ===")) {
        m_showEmailAfterScript = true;
        return;
    }

    // 3) Resume notification
    if (output.contains("=== RESUME_PROCESSING ===")) {
        outputToTerminal("Script indicates resume processing.", Info);
        return;
    }
}

void TMFLAHCAController::onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    const bool handoff = m_showEmailAfterScript;
    m_showEmailAfterScript = false;
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        outputToTerminal(QString("AHCA script failed (exit %1). Job files were retained for recovery.").arg(exitCode), Error);
        updateButtonStates();
        return;
    }
    outputToTerminal("Script completed successfully", Success);
    if (m_trackerModel) m_trackerModel->select();
    if (handoff) showEmailDialog(m_capturedPrintPath, getJobNumber());
    updateButtonStates();
}









void TMFLAHCAController::showNASLinkDialog(const QString& nasPath)
{
    if (nasPath.isEmpty()) {
        outputToTerminal("No NAS path provided - cannot display location dialog", Warning);
        return;
    }

    outputToTerminal("Opening print file location dialog...", Info);

    // Create and show the dialog with custom text for FL AHCA
    NASLinkDialog* dialog = new NASLinkDialog(
        "Print File Location",           // Window title
        "Print data file located below", // Description text
        nasPath,                        // Network path
        nullptr                         // Parent
        );
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

// State management methods
void TMFLAHCAController::updateLockStates()
{
    if (m_jobDataLockBtn) {
        m_jobDataLockBtn->setChecked(m_jobDataLocked);
        m_jobDataLockBtn->setText(m_jobDataLocked ? "LOCKED" : "UNLOCKED");
    }

    if (m_postageLockBtn) {
        m_postageLockBtn->setChecked(m_postageDataLocked);
        m_postageLockBtn->setText(m_postageDataLocked ? "LOCKED" : "UNLOCKED");
    }
}

void TMFLAHCAController::updateButtonStates()
{
    // Job data fields - enabled when not locked
    const bool busy = isProcessing();
    bool jobFieldsEnabled = !m_jobDataLocked && !busy;
    if (m_jobNumberBox) m_jobNumberBox->setEnabled(jobFieldsEnabled);
    if (m_yearDDbox) m_yearDDbox->setEnabled(jobFieldsEnabled);
    if (m_monthDDbox) m_monthDDbox->setEnabled(jobFieldsEnabled);
    
    // Postage data fields - enabled when postage not locked
    if (m_postageBox) m_postageBox->setEnabled(!m_postageDataLocked && !busy);
    if (m_countBox) m_countBox->setEnabled(!m_postageDataLocked && !busy);

    // DropWindow - disabled when job data is locked
    if (m_dropWindow) {
        m_dropWindow->setEnabled(!m_jobDataLocked && !busy);
        m_dropWindow->setAcceptDrops(!m_jobDataLocked && !busy);
    }

    // Lock button states
    if (m_jobDataLockBtn) m_jobDataLockBtn->setChecked(m_jobDataLocked);
    if (m_postageLockBtn) m_postageLockBtn->setChecked(m_postageDataLocked);

    // Edit button only enabled when job data is locked
    if (m_editBtn) m_editBtn->setEnabled(m_jobDataLocked && !busy);

    // Postage lock can only be engaged if job data is locked
    if (m_postageLockBtn) m_postageLockBtn->setEnabled(m_jobDataLocked && !busy);

    // Script buttons enabled based on lock states
    if (m_runInitialBtn) m_runInitialBtn->setEnabled(m_jobDataLocked && !busy);
    if (m_finalStepBtn) m_finalStepBtn->setEnabled(m_postageDataLocked && !busy);
    if (m_jobDataLockBtn) m_jobDataLockBtn->setEnabled(!busy);
    if (m_openBulkMailerBtn) m_openBulkMailerBtn->setEnabled(m_jobDataLocked && !busy);
}

void TMFLAHCAController::updateHtmlDisplay()
{
    if (!m_textBrowser) return;

    HtmlDisplayState targetState = determineHtmlState();
    if (m_currentHtmlState == UninitializedState || m_currentHtmlState != targetState) {
        m_currentHtmlState = targetState;

        // Load appropriate HTML file based on state
        if (targetState == InstructionsState) {
            loadHtmlFile(":/resources/tmflahca/instructions.html");
        } else {
            loadHtmlFile(":/resources/tmflahca/default.html");
        }
    }
}

void TMFLAHCAController::loadHtmlFile(const QString& resourcePath)
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
        // Create fallback content if HTML file fails to load
        QString fallbackContent = QString(
            "<html><body style='font-family: Arial; padding: 20px;'>"
            "<h2>TM FL AHCA</h2>"
            "<p>Instructions not available</p>"
            "<p>Please check that HTML resources are properly installed.</p>"
            "</body></html>"
        );
        m_textBrowser->setHtml(fallbackContent);
    }
}

TMFLAHCAController::HtmlDisplayState TMFLAHCAController::determineHtmlState() const
{
    return m_jobDataLocked ? InstructionsState : DefaultState;
}

// Validation methods
bool TMFLAHCAController::validateJobData()
{
    QString jobNumber = getJobNumber();
    QString year = getYear();
    QString month = getMonth();

    if (jobNumber.isEmpty() || jobNumber.length() != 5) {
        outputToTerminal("Error: Job number must be exactly 5 digits", Error);
        return false;
    }

    if (year.isEmpty()) {
        outputToTerminal("Error: Year must be selected", Error);
        return false;
    }

    if (month.isEmpty()) {
        outputToTerminal("Error: Month must be selected", Error);
        return false;
    }

    // Validate job number is numeric
    bool ok;
    jobNumber.toInt(&ok);
    if (!ok) {
        outputToTerminal("Error: Job number must contain only digits", Error);
        return false;
    }

    return true;
}

bool TMFLAHCAController::validatePostageData()
{
    if (!m_postageBox || !m_countBox) {
        return true; // If no postage fields, validation passes
    }

    bool isValid = true;

    // Check postage
    QString postage = m_postageBox->text();
    if (postage.isEmpty() || postage == "$") {
        outputToTerminal("Postage amount is required.", Error);
        isValid = false;
    } else {
        // Validate postage format
        QString cleanPostage = postage;
        cleanPostage.remove("$");
        cleanPostage.remove(","); // Remove commas too
        bool ok;
        double postageValue = cleanPostage.toDouble(&ok);
        if (!ok || postageValue <= 0) {
            outputToTerminal("Invalid postage amount.", Error);
            isValid = false;
        }
    }

    // Check count
    QString count = m_countBox->text();
    if (count.isEmpty()) {
        outputToTerminal("Count is required.", Error);
        isValid = false;
    } else {
        QString cleanCount = count;
        cleanCount.remove(',').remove(' '); // Remove formatting
        bool ok;
        int countValue = cleanCount.toInt(&ok);
        if (!ok || countValue <= 0) {
            outputToTerminal("Invalid count. Must be a positive integer.", Error);
            isValid = false;
        }
    }

    return isValid;
}

void TMFLAHCAController::formatPostageInput()
{
    if (!m_postageBox) return;

    QString text = m_postageBox->text().trimmed();
    if (text.isEmpty()) return;

    // Remove any non-numeric characters except decimal point
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

    // Update the text
    m_postageBox->setText(formatted);
}

void TMFLAHCAController::formatCountInput(const QString& text)
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

bool TMFLAHCAController::validateScriptExecution(const QString& scriptName) const
{
    if (scriptName.isEmpty()) {
        return false;
    }

    if (!m_fileManager) {
        return false;
    }

    if (!m_scriptRunner) {
        return false;
    }

    return true;
}

// Job management methods
bool TMFLAHCAController::loadJob(const QString& jobNumber, const QString& year, const QString& month)
{
    if (!m_tmFlahcaDBManager) {
        outputToTerminal("Database manager not initialized", Error);
        return false;
    }

    // FL AHCA FIX: Job number now comes from parameter, not from UI
    if (jobNumber.isEmpty()) {
        outputToTerminal("Cannot load job: job number is required", Error);
        return false;
    }

    if (m_tmFlahcaDBManager->loadJob(jobNumber, year, month)) {
        // FL AHCA FIX: Set UI values from parameters (including job number)
        if (m_jobNumberBox) m_jobNumberBox->setText(jobNumber);
        if (m_yearDDbox) m_yearDDbox->setCurrentText(year);
        if (m_monthDDbox) m_monthDDbox->setCurrentText(month);

        // CRITICAL: Initialize cache BEFORE loading state
        // This ensures cache has correct values for any future edits
        m_lastYear = year.toInt();
        m_lastMonth = month.toInt();
        m_cachedJobNumber = jobNumber;

        // Force UI to process the dropdown changes before state load
        QCoreApplication::processEvents();

        // FL AHCA FIX: Load job state with explicit job number
        loadJobState(jobNumber);

        // If no state was loaded, default to locked
        if (!m_jobDataLocked) {
            m_jobDataLocked = true;
            outputToTerminal("Job state not found, defaulting to locked", Info);
        }

        // Update UI to reflect the lock state
        if (m_jobDataLockBtn) m_jobDataLockBtn->setChecked(m_jobDataLocked);

        // If job data is locked, handle file operations and auto-save
        if (m_jobDataLocked) {
            if (!copyFilesFromHomeFolder()) return false;
            outputToTerminal("Files copied from ARCHIVE to DATA folder", Info);

            // Start auto-save timer since job is locked/open
            emit jobOpened();
            outputToTerminal("Auto-save timer started (15 minutes)", Info);
        }

        // Update control states and HTML display
        updateLockStates();
        updateButtonStates();
        m_currentHtmlState = UninitializedState;  // Force HTML refresh on job open
        updateHtmlDisplay();

        outputToTerminal("Job loaded: " + jobNumber, Success);
        return true;
    } else {
        outputToTerminal(QString("No job found for %1/%2").arg(year, month), Warning);
        return false;
    }
}

void TMFLAHCAController::resetToDefaults()
{
    saveJobState();

    // Move files to HOME folder before closing
    moveFilesToHomeFolder();

    // Clear UI fields
    if (m_jobNumberBox) m_jobNumberBox->clear();
    if (m_postageBox) m_postageBox->clear();
    if (m_countBox) m_countBox->clear();
    if (m_yearDDbox) m_yearDDbox->setCurrentIndex(0);
    if (m_monthDDbox) m_monthDDbox->setCurrentIndex(0);

    // Reset state
    m_jobDataLocked = false;
    m_postageDataLocked = false;
    m_lastExecutedScript.clear();
    m_capturedNASPath.clear();
    m_capturingNASPath = false;
    m_waitingForEmailConfirmation = false;

    // Clear cache on reset
    m_lastYear = -1;
    m_lastMonth = -1;
    m_cachedJobNumber.clear();

    // Clean up email dialog if open
    if (m_emailDialog) {
        m_emailDialog->deleteLater();
        m_emailDialog = nullptr;
    }

    // Reset all lock buttons to unchecked
    if (m_jobDataLockBtn) m_jobDataLockBtn->setChecked(false);
    if (m_editBtn) m_editBtn->setChecked(false);
    if (m_postageLockBtn) m_postageLockBtn->setChecked(false);

    // Clear terminal window
    if (m_terminalWindow) m_terminalWindow->clear();

    // Update UI
    updateLockStates();
    updateButtonStates();
    updateHtmlDisplay();

    // Force load default.html regardless of state
    loadHtmlFile(":/resources/tmflahca/default.html");

    // Emit signal to stop auto-save timer since no job is open
    emit jobClosed();
    outputToTerminal("Job state reset to defaults", Info);
    outputToTerminal("Auto-save timer stopped - no job open", Info);
}

void TMFLAHCAController::saveJobState()
{
    if (!m_tmFlahcaDBManager) return;

    QString jobNumber = getJobNumber();
    QString year = getYear();
    QString month = getMonth();

    if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty()) {
        outputToTerminal("Cannot save job: Missing required data", Warning);
        return;
    }

    // Save basic job data
    if (m_tmFlahcaDBManager->saveJob(jobNumber, year, month)) {
        outputToTerminal("Job saved to database", Success);
    } else {
        outputToTerminal("Failed to save job to database", Error);
    }

    // Save job state including HTML display state, lock states, and script execution state
    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";
    
    // FL AHCA FIX: Pass job_number to saveJobState to identify which specific job to update
    if (m_tmFlahcaDBManager->saveJobState(jobNumber, year, month,
                                        static_cast<int>(m_currentHtmlState),
                                        m_jobDataLocked, m_postageDataLocked,
                                        postage, count, m_lastExecutedScript)) {
        outputToTerminal(QString("Job state saved to database: postage=%1, count=%2, postage_locked=%3")
                             .arg(postage, count, m_postageDataLocked ? "true" : "false"), Success);
    } else {
        outputToTerminal("Failed to save job state to database", Error);
    }
}

void TMFLAHCAController::loadJobState()
{
    // Delegate to new overload using cached job number
    loadJobState(m_cachedJobNumber);
}

void TMFLAHCAController::loadJobState(const QString& jobNumber)
{
    if (!m_tmFlahcaDBManager) return;

    QString year = getYear();
    QString month = getMonth();

    if (year.isEmpty() || month.isEmpty()) return;
    if (jobNumber.isEmpty()) return;

    int htmlState;
    bool jobLocked, postageLocked;
    QString postage, count, lastExecutedScript;

    // FL AHCA FIX: Pass job_number to loadJobState for explicit identity
    if (m_tmFlahcaDBManager->loadJobState(jobNumber, year, month, htmlState, jobLocked, postageLocked,
                                        postage, count, lastExecutedScript)) {
        m_currentHtmlState = static_cast<HtmlDisplayState>(htmlState);
        m_jobDataLocked = jobLocked;
        m_postageDataLocked = postageLocked;
        m_lastExecutedScript = lastExecutedScript;

        if (m_postageBox && !postage.isEmpty()) {
            m_postageBox->setText(postage);
        }
        if (m_countBox && !count.isEmpty()) {
            m_countBox->setText(count);
        }

        m_currentHtmlState = m_jobDataLocked ? InstructionsState : DefaultState;
        updateLockStates();
        updateButtonStates();
        updateHtmlDisplay();
        
        outputToTerminal(QString("Job state loaded: postage=%1, count=%2, postage_locked=%3")
                             .arg(postage, count, m_postageDataLocked ? "true" : "false"), Info);
    } else {
        // No saved state found, set defaults
        m_jobDataLocked = false;
        m_postageDataLocked = false;
        m_currentHtmlState = UninitializedState;
        m_lastExecutedScript = "";
        m_currentHtmlState = m_jobDataLocked ? InstructionsState : DefaultState;
        updateLockStates();
        updateButtonStates();
        updateHtmlDisplay();
        outputToTerminal("No saved job state found, using defaults", Info);
    }
}

// Tracker operations
void TMFLAHCAController::onAddToTracker()
{
    if (!validateJobData()) {
        outputToTerminal("Cannot add to tracker: Invalid job data", Error);
        return;
    }

    // This would typically be called from a UI action with postage data
    // For now, this is a placeholder for the functionality
    outputToTerminal("Add to tracker functionality ready", Info);
}

// CRITICAL FIX: Enhanced addLogEntry - now UPDATES existing row for current job instead of always inserting
void TMFLAHCAController::addLogEntry()
{
    if (!m_tmFlahcaDBManager) {
        outputToTerminal("Database manager not available for log entry", Error);
        return;
    }

    // Get current data from UI
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
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
    QString description = QString("TM %1 FL AHCA").arg(monthAbbrev);

    // Clean and format count (remove commas, ensure integer)
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

    // Calculate average rate (X.XXX format)
    double avgRate = (countValue > 0) ? (postageAmount / countValue) : 0.0;
    QString formattedAvgRate = QString("%1").arg(avgRate, 0, 'f', 3);

    // CRITICAL FIX: UPDATE existing log entry for this job instead of always inserting
    QString mailClass = "STD";
    QString shape = "LTR";
    QString permit = "1662";
    QString date = QDate::currentDate().toString("MM/dd/yyyy");

    // Check if log entry already exists for this specific job
    if (m_tmFlahcaDBManager->updateLogEntryForJob(jobNumber, description, formattedPostage, formattedCount,
                                                formattedAvgRate, mailClass, shape, permit, date)) {
        outputToTerminal(QString("Log entry updated for job %1: %2 pieces at %3 (%4 avg rate)")
                             .arg(jobNumber, formattedCount, formattedPostage, formattedAvgRate), Success);
    } else {
        // If update failed (no existing entry), then add new entry
        if (m_tmFlahcaDBManager->addLogEntry(jobNumber, description, formattedPostage, formattedCount,
                                           formattedAvgRate, mailClass, shape, permit, date)) {
            outputToTerminal(QString("Log entry added for job %1: %2 pieces at %3 (%4 avg rate)")
                                 .arg(jobNumber, formattedCount, formattedPostage, formattedAvgRate), Success);
        } else {
            outputToTerminal("Failed to add/update log entry", Error);
            return;
        }
    }
    
    // Refresh the tracker model
    if (m_trackerModel) {
        m_trackerModel->select();
    }
}

void TMFLAHCAController::onCopyRowClicked()
{
    QString result = copyFormattedRow(); // Uses inherited BaseTrackerController method
    outputToTerminal(result, result.contains("success") ? Success : Error);
}

void TMFLAHCAController::refreshTrackerTable()
{
    if (m_trackerModel) {
        m_trackerModel->select();
        applyTrackerHeaders();
        outputToTerminal("Tracker table refreshed", Info);
    }
}

void TMFLAHCAController::setupTrackerModel()
{
    if (!m_tracker || !m_tmFlahcaDBManager) return;

    m_trackerModel = new FLAHCAFormattedSqlModel(this, DatabaseManager::instance()->getDatabase(), this);
    m_trackerModel->setTable("tm_flahca_log");
    m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_trackerModel->select();
    applyTrackerHeaders();
    if (m_trackerModel) {
        m_tracker->setModel(m_trackerModel);

        // Hide ID column (column 0) and show visible columns
        QList<int> visibleColumns = getVisibleColumns();
        for (int i = 0; i < m_trackerModel->columnCount(); ++i) {
            m_tracker->setColumnHidden(i, !visibleColumns.contains(i));
        }

        // Configure selection behavior
        m_tracker->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_tracker->setSelectionMode(QAbstractItemView::SingleSelection);

        outputToTerminal("Tracker model initialized successfully", Success);
        setupOptimizedTableLayout();

        // Enable right-click copy on tracker (TERM-style context menu)
        if (m_tracker) {
            m_tracker->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(m_tracker, &QTableView::customContextMenuRequested,
                    this, &TMFLAHCAController::showTableContextMenu);
        }
    } else {
        outputToTerminal("Failed to initialize tracker model", Error);
    }
}

void TMFLAHCAController::setupOptimizedTableLayout()
{
    if (!m_tracker) return;

    // Calculate optimal font size and column widths (EXACT MATCH to TMFARM/TMTERM)
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
        {"JOB", "88888", 56},
        {"DESCRIPTION", "TM DEC TERM", 140},
        {"POSTAGE", "$888,888.88", 29},
        {"COUNT", "88,888", 45},
        {"AVG RATE", "0.888", 45},
        {"CLASS", "STD", 60},
        {"SHAPE", "LTR", 33},
        {"PERMIT", "NKLN", 36}
    };

    // Calculate optimal font size - START BIGGER
    QFont testFont("Blender Pro Bold", 7);
    QFontMetrics fm(testFont);

    int optimalFontSize = 7;
    for (int fontSize = 11; fontSize >= 7; fontSize--) {
        testFont.setPointSize(fontSize);
        fm = QFontMetrics(testFont);

        int totalWidth = 0;
        bool fits = true;

        for (auto it = columns.cbegin(); it != columns.cend(); ++it) {
            const auto& col = *it;
            const int headerWidth  = fm.horizontalAdvance(col.header) + 12;
            const int contentWidth = fm.horizontalAdvance(col.maxContent) + 12;
            const int colWidth     = qMax(headerWidth, qMax(contentWidth, col.minWidth));
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
    QFont tableFont("Blender Pro", optimalFontSize);
    tableFont.setBold(false);
    tableFont.setWeight(QFont::Normal);
    m_tracker->setFont(tableFont);

    // Set up the model with proper ordering (newest first)
    m_trackerModel->setSort(0, Qt::DescendingOrder);
    m_trackerModel->select();

    // Set custom headers
    m_trackerModel->setHeaderData(1, Qt::Horizontal, tr("JOB"));
    m_trackerModel->setHeaderData(2, Qt::Horizontal, tr("DESCRIPTION"));
    m_trackerModel->setHeaderData(3, Qt::Horizontal, tr("POSTAGE"));
    m_trackerModel->setHeaderData(4, Qt::Horizontal, tr("COUNT"));
    m_trackerModel->setHeaderData(5, Qt::Horizontal, tr("AVG RATE"));
    m_trackerModel->setHeaderData(6, Qt::Horizontal, tr("CLASS"));
    m_trackerModel->setHeaderData(7, Qt::Horizontal, tr("SHAPE"));
    m_trackerModel->setHeaderData(8, Qt::Horizontal, tr("PERMIT"));

    // Hide ALL unwanted columns (assuming columns 0, 9+ are id, date, created_at)
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

    // Apply enhanced styling for better readability (matches TMTERM)
    m_tracker->setStyleSheet(
        "QTableView {"
        "   border: 1px solid black;"
        "   selection-background-color: #d0d0ff;"
        "   alternate-background-color: #f8f8f8;"
        "   gridline-color: #cccccc;"
        "}"
        "QHeaderView::section {"
        "   background-color: #e0e0e0;"
        "   padding: 4px;"
        "   border: 1px solid black;"
        "   font-weight: bold;"
        "   font-family: 'Blender Pro Bold';"
        "}"
        "QTableView::item {"
        "   padding: 3px;"
        "   border-right: 1px solid #cccccc;"
        "}"
        );

    // Enable alternating row colors
    m_tracker->setAlternatingRowColors(true);
}

// Dropdown population methods
void TMFLAHCAController::populateYearDropdown()
{
    if (!m_yearDDbox) return;

    YearComboBoxHelper::populateWithBlankAndAdjacentYears(m_yearDDbox);
}

void TMFLAHCAController::populateMonthDropdown()
{
    if (!m_monthDDbox) return;

    MonthComboBoxHelper::populateWithBlankAndMonths(m_monthDDbox);
}

// Dropdown change handlers
void TMFLAHCAController::onYearChanged(const QString& year)
{
    Q_UNUSED(year)
    // FL AHCA FIX: Do NOT load job state on dropdown change
    // Only update HTML display - job loading happens when all fields are set and locked
    updateHtmlDisplay();
}

void TMFLAHCAController::onMonthChanged(const QString& month)
{
    Q_UNUSED(month)
    // FL AHCA FIX: Do NOT load job state on dropdown change
    // Only update HTML display - job loading happens when all fields are set and locked
    updateHtmlDisplay();
}

// Directory management
void TMFLAHCAController::createJobFolder()
{
    if (!m_fileManager || getJobNumber().isEmpty() || getYear().isEmpty() || getMonth().isEmpty()) return;
    const QString path = m_fileManager->getJobFolderPath(getJobNumber(), getYear(), getMonth());
    if (!QDir().mkpath(path)) outputToTerminal("Cannot create job archive folder: " + path, Error);
}

void TMFLAHCAController::setupDropWindow()
{
    if (!m_dropWindow) {
        return;
    }

    Logger::instance().info("Setting up TM FL AHCA drop window...");

    // Set target directory to FL AHCA RAW INPUT folder
    const QString targetDirectory = m_fileManager
        ? (m_fileManager->getBasePath() + "/RAW INPUT")
        : QString();
    if (targetDirectory.isEmpty()) {
        outputToTerminal("Drop window base path unavailable for TM FL AHCA", Error);
        return;
    }
    DropBindingHelper::setupDropWindow(
        m_dropWindow,
        targetDirectory,
        {"xlsx"},
        this,
        [this](const QStringList& filePaths) { onFilesDropped(filePaths); },
        [this](const QString& errorMessage) { onFileDropError(errorMessage); });

    outputToTerminal(QString("Drop window configured for directory: %1").arg(targetDirectory), Info);
    Logger::instance().info("TM FL AHCA drop window setup complete");
}

void TMFLAHCAController::onFileSystemChanged()
{
    // Handle file system changes if needed
    outputToTerminal("File system changed", Info);
}

// Drop window handlers
void TMFLAHCAController::onFilesDropped(const QStringList& filePaths)
{
    outputToTerminal(QString("Files received: %1 file(s) dropped").arg(filePaths.size()), Success);
    
    // Log each dropped file
    for (auto it = filePaths.cbegin(); it != filePaths.cend(); ++it) {
        const QString& filePath = *it;
        const QFileInfo fileInfo(filePath);
        const QString fileName = fileInfo.fileName();
        outputToTerminal(QString("  - %1").arg(fileName), Info);
    }
    
    outputToTerminal("Files are ready for processing in RAW INPUT folder", Info);
}

void TMFLAHCAController::onFileDropError(const QString& errorMessage)
{
    outputToTerminal(QString("File drop error: %1").arg(errorMessage), Warning);
}

bool TMFLAHCAController::moveFilesToHomeFolder()
{
    return transferWorkingFolders(false);
}

bool TMFLAHCAController::copyFilesFromHomeFolder()
{
    return transferWorkingFolders(true);
}

bool TMFLAHCAController::transferWorkingFolders(bool restore)
{
    if (!m_fileManager) return false;
    const QString job = m_cachedJobNumber.isEmpty() ? getJobNumber() : m_cachedJobNumber;
    const QString year = m_lastYear > 0 ? QString::number(m_lastYear) : getYear();
    const QString month = m_lastMonth > 0 ? QString("%1").arg(m_lastMonth, 2, 10, QChar('0')) : getMonth();
    const QString working = m_fileManager->getJobFolderPath(job, year, month) + "/WORKING";
    QList<QPair<QString, QString>> moves;
    for (const QString& name : {QString("DATA"), QString("RAW INPUT")}) {
        const QString source = (restore ? working : m_fileManager->getBasePath()) + "/" + name;
        const QString target = (restore ? m_fileManager->getBasePath() : working) + "/" + name;
        if (!QDir(source).exists() || QDir(source).isEmpty()) continue;
        if (QDir(target).exists() && !QDir(target).isEmpty()) {
            outputToTerminal("Job folder already contains files; nothing was overwritten: " + target, Error);
            return false;
        }
        moves.append(qMakePair(source, target));
    }
    QList<QPair<QString, QString>> completed;
    for (const auto& move : moves) {
        QDir().mkpath(QFileInfo(move.second).absolutePath());
        if (QDir(move.second).exists()) QDir().rmdir(move.second);
        if (!QDir().rename(move.first, move.second)) {
            for (auto i = completed.crbegin(); i != completed.crend(); ++i)
                QDir().rename(i->second, i->first);
            outputToTerminal("Unable to move AHCA working files; job remains open for recovery.", Error);
            return false;
        }
        completed.append(move);
    }
    QDir().mkpath(m_fileManager->getDataPath());
    QDir().mkpath(m_fileManager->getBasePath() + "/RAW INPUT");
    return true;
}

bool TMFLAHCAController::createExcelAndCopy(const QStringList& headers, const QStringList& rowData)
{
    // Use the inherited BaseTrackerController implementation
    return BaseTrackerController::createExcelAndCopy(headers, rowData);
}

void TMFLAHCAController::showTableContextMenu(const QPoint& pos)
{
    if (!m_tracker)
        return;

    QMenu menu(m_tracker);
    QAction* copyAction = menu.addAction("Copy Selected Row");

    QAction* selectedAction = menu.exec(m_tracker->mapToGlobal(pos));
    if (selectedAction == copyAction) {
        QString result = copyFormattedRow();  // inherited from BaseTrackerController
        if (result == "Row copied to clipboard") {
            outputToTerminal("Row copied to clipboard with formatting", Success);
        } else {
            outputToTerminal(result, Warning);
        }
    }
}

bool TMFLAHCAController::validateJobNumber(const QString& jobNumber) const {
    if (jobNumber.length() != 5) return false;
    for (const QChar ch : jobNumber) if (!ch.isDigit()) return false;
    return true;
}

bool TMFLAHCAController::autoSaveAndCloseCurrentJob()
{
    if (isProcessing()) {
        outputToTerminal("Finish the current AHCA step before closing the job.", Warning);
        return false;
    }
    // Check if we have a job currently open (locked)
    if (m_jobDataLocked) {
        // FL AHCA FIX: Verify cache is initialized before attempting save
        // If m_lastYear or m_lastMonth are -1, it means no job was properly locked yet
        if (m_lastYear <= 0 || m_lastMonth <= 0 || m_cachedJobNumber.isEmpty()) {
            outputToTerminal("Auto-save skipped: job cache not initialized", Warning);
            return false;
        }
        
        // Use CACHED values (old period) for save, NOT current UI values
        QString currentJobNumber = m_cachedJobNumber;
        QString currentYear = QString::number(m_lastYear);
        QString currentMonth = QString("%1").arg(m_lastMonth, 2, 10, QChar('0'));

        if (!currentJobNumber.isEmpty() && !currentYear.isEmpty() && !currentMonth.isEmpty()) {
            outputToTerminal(QString("Auto-saving current job %1 (%2-%3) before opening new job")
                                 .arg(currentJobNumber, currentYear, currentMonth), Info);

            TMFLAHCADBManager* dbManager = TMFLAHCADBManager::instance();
            if (dbManager) {
                if (dbManager->saveJob(currentJobNumber, currentYear, currentMonth)) {
                    outputToTerminal("Job saved to database", Success);
                } else {
                    outputToTerminal("Failed to save job to database", Error);
                    return false;
                }

                QString postage = m_postageBox ? m_postageBox->text() : "";
                QString count = m_countBox ? m_countBox->text() : "";

                // FL AHCA FIX: Pass cached job_number to saveJobState
                if (dbManager->saveJobState(currentJobNumber, currentYear, currentMonth,
                                            static_cast<int>(m_currentHtmlState),
                                            m_jobDataLocked, m_postageDataLocked,
                                            postage, count, m_lastExecutedScript)) {
                    outputToTerminal(QString("Job state saved to database: postage=%1, count=%2, postage_locked=%3")
                                         .arg(postage, count, m_postageDataLocked ? "true" : "false"), Success);
                } else {
                    outputToTerminal("Failed to save job state to database", Error);
                    return false;
                }

                outputToTerminal("Moving files from DATA folder back to ARCHIVE folder...", Info);
                if (moveFilesToHomeFolder()) {
                    outputToTerminal("Files moved successfully from DATA to ARCHIVE folder", Success);
                } else {
                    outputToTerminal("Job remains open because its working files could not be saved.", Error);
                    return false;
                }

                m_jobDataLocked = false;
                m_postageDataLocked = false;
                m_currentHtmlState = UninitializedState;
                m_lastYear = -1;
                m_lastMonth = -1;
                m_cachedJobNumber.clear();

                updateLockStates();
                updateButtonStates();
                updateHtmlDisplay();
                emit jobClosed();

                outputToTerminal("Current job auto-saved and closed", Success);
            } else {
                outputToTerminal("Database manager not initialized", Error);
                return false;
            }
        }
    }
    return true;
}

void TMFLAHCAController::applyTrackerHeaders()
{
    if (!m_trackerModel) return;

    const int idxJob         = m_trackerModel->fieldIndex(QStringLiteral("job"));
    const int idxDescription = m_trackerModel->fieldIndex(QStringLiteral("description"));
    const int idxPostage     = m_trackerModel->fieldIndex(QStringLiteral("postage"));
    const int idxCount       = m_trackerModel->fieldIndex(QStringLiteral("count"));
    const int idxAvgRate     = m_trackerModel->fieldIndex(QStringLiteral("avg_rate"));
    const int idxMailClass   = m_trackerModel->fieldIndex(QStringLiteral("mail_class"));
    const int idxShape       = m_trackerModel->fieldIndex(QStringLiteral("shape"));
    const int idxPermit      = m_trackerModel->fieldIndex(QStringLiteral("permit"));

    if (idxJob         >= 0) m_trackerModel->setHeaderData(idxJob,         Qt::Horizontal, tr("JOB"),         Qt::DisplayRole);
    if (idxDescription >= 0) m_trackerModel->setHeaderData(idxDescription, Qt::Horizontal, tr("DESCRIPTION"), Qt::DisplayRole);
    if (idxPostage     >= 0) m_trackerModel->setHeaderData(idxPostage,     Qt::Horizontal, tr("POSTAGE"),     Qt::DisplayRole);
    if (idxCount       >= 0) m_trackerModel->setHeaderData(idxCount,       Qt::Horizontal, tr("COUNT"),       Qt::DisplayRole);
    if (idxAvgRate     >= 0) m_trackerModel->setHeaderData(idxAvgRate,     Qt::Horizontal, tr("AVG RATE"),    Qt::DisplayRole);
    if (idxMailClass   >= 0) m_trackerModel->setHeaderData(idxMailClass,   Qt::Horizontal, tr("CLASS"),       Qt::DisplayRole);
    if (idxShape       >= 0) m_trackerModel->setHeaderData(idxShape,       Qt::Horizontal, tr("SHAPE"),       Qt::DisplayRole);
    if (idxPermit      >= 0) m_trackerModel->setHeaderData(idxPermit,      Qt::Horizontal, tr("PERMIT"),      Qt::DisplayRole);
}

void TMFLAHCAController::triggerArchivePhase()
{
    if (!m_fileManager || !m_scriptRunner || m_scriptRunner->isRunning()) return;
    m_lastExecutedScript = "02 FINAL PROCESS (archive)";
    const QStringList args = {getJobNumber(), getYear(), getMonth(), "--mode", "archive"};
    if (!m_scriptRunner->runScript(m_fileManager->getScriptPath("02 FINAL PROCESS"), args)) {
        outputToTerminal("Unable to start archive phase. Job files remain available.", Error);
    }
    updateButtonStates();
}

void TMFLAHCAController::showEmailDialog(const QString& nasPath, const QString& jobNumber)
{
    if (jobNumber.isEmpty() || m_emailDialog) return;
    m_waitingForEmailConfirmation = true;
    // Attachments remain local; the separately captured path is the delivered print location.
    m_emailDialog = new TMFLAHCAEmailDialog(jobNumber, qobject_cast<QWidget*>(parent()),
                                          m_fileManager->getDataPath(), nasPath);
    m_emailDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_emailDialog, &QDialog::finished, this, [this](int result) {
        m_waitingForEmailConfirmation = false;
        m_emailDialog = nullptr;
        if (result == QDialog::Accepted) triggerArchivePhase();
        else outputToTerminal("Email handoff cancelled. Files retained; run Final Step when ready.", Warning);
        updateButtonStates();
    });
    m_emailDialog->show();
}

bool TMFLAHCAController::isProcessing() const
{
    return (m_scriptRunner && m_scriptRunner->isRunning()) || m_waitingForEmailConfirmation;
}

