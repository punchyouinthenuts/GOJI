#include "tmflercontroller.h"
#include "logger.h"
#include "naslinkdialog.h"
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

TMFLERController::TMFLERController(QObject *parent)
    : BaseTrackerController(parent)
    , m_fileManager(nullptr)
    , m_tmFlerDBManager(nullptr)
    , m_scriptRunner(nullptr)
    , m_jobNumberBox(nullptr)
    , m_yearDDbox(nullptr)
    , m_monthDDbox(nullptr)
    , m_jobDataLockBtn(nullptr)
    , m_postageLockBtn(nullptr)
    , m_runInitialBtn(nullptr)
    , m_finalStepBtn(nullptr)
    , m_terminalWindow(nullptr)
    , m_textBrowser(nullptr)
    , m_tracker(nullptr)
    , m_jobDataLocked(false)
    , m_postageDataLocked(false)
    , m_currentHtmlState(UninitializedState)
    , m_capturingNASPath(false)
    , m_waitingForEmailConfirmation(false)
    , m_emailDialog(nullptr)
    , m_trackerModel(nullptr)
{
    initializeComponents();
    connectSignals();
    setupInitialState();
}

TMFLERController::~TMFLERController()
{
    // Clean up email dialog if it exists
    if (m_emailDialog) {
        m_emailDialog->deleteLater();
        m_emailDialog = nullptr;
    }
    // Note: UI widgets are managed by Qt's parent-child system
    // File manager and script runner will be cleaned up automatically
}

void TMFLERController::initializeComponents()
{
    // Initialize file manager
    m_fileManager = new TMFLERFileManager(nullptr);

    // Initialize database manager
    m_tmFlerDBManager = TMFLERDBManager::instance();

    // Initialize script runner
    m_scriptRunner = new ScriptRunner(this);

    // Create base directories
    createBaseDirectories();

    Logger::instance().info("TMFLER controller components initialized");
}

void TMFLERController::connectSignals()
{
    // Connect script runner signals
    if (m_scriptRunner) {
        connect(m_scriptRunner, &ScriptRunner::scriptOutput, this, &TMFLERController::onScriptOutput);
        connect(m_scriptRunner, &ScriptRunner::scriptFinished, this, &TMFLERController::onScriptFinished);
    }
}

void TMFLERController::setupInitialState()
{
    // Initialize states
    m_jobDataLocked = false;
    m_postageDataLocked = false;
    m_currentHtmlState = DefaultState;
    m_waitingForEmailConfirmation = false;

    // Update UI states
    updateLockStates();
    updateButtonStates();
    updateHtmlDisplay();

    Logger::instance().info("TMFLER controller initial state set");
}

// UI Widget setters (unchanged from original implementation)
void TMFLERController::setJobNumberBox(QLineEdit* lineEdit)
{
    m_jobNumberBox = lineEdit;
}

void TMFLERController::setYearDropdown(QComboBox* comboBox)
{
    m_yearDDbox = comboBox;
}

void TMFLERController::setMonthDropdown(QComboBox* comboBox)
{
    m_monthDDbox = comboBox;
}

void TMFLERController::setJobDataLockButton(QPushButton* button)
{
    m_jobDataLockBtn = button;
    if (m_jobDataLockBtn) {
        connect(m_jobDataLockBtn, &QPushButton::clicked, this, &TMFLERController::onJobDataLockClicked);
    }
}

void TMFLERController::setPostageLockButton(QPushButton* button)
{
    m_postageLockBtn = button;
    if (m_postageLockBtn) {
        connect(m_postageLockBtn, &QPushButton::clicked, this, &TMFLERController::onPostageLockClicked);
    }
}

void TMFLERController::setRunInitialButton(QPushButton* button)
{
    m_runInitialBtn = button;
    if (m_runInitialBtn) {
        connect(m_runInitialBtn, &QPushButton::clicked, this, &TMFLERController::onRunInitialClicked);
    }
}

void TMFLERController::setFinalStepButton(QPushButton* button)
{
    m_finalStepBtn = button;
    if (m_finalStepBtn) {
        connect(m_finalStepBtn, &QPushButton::clicked, this, &TMFLERController::onFinalStepClicked);
    }
}

void TMFLERController::setTerminalWindow(QTextEdit* textEdit)
{
    m_terminalWindow = textEdit;
}

void TMFLERController::setTextBrowser(QTextBrowser* textBrowser)
{
    m_textBrowser = textBrowser;
    updateHtmlDisplay();
}

void TMFLERController::setTracker(QTableView* tableView)
{
    m_tracker = tableView;
    setupTrackerModel();
}

// Public getters (unchanged from original implementation)
QString TMFLERController::getJobNumber() const
{
    return m_jobNumberBox ? m_jobNumberBox->text() : "";
}

QString TMFLERController::getYear() const
{
    return m_yearDDbox ? m_yearDDbox->currentText() : "";
}

QString TMFLERController::getMonth() const
{
    return m_monthDDbox ? m_monthDDbox->currentText() : "";
}

bool TMFLERController::isJobDataLocked() const
{
    return m_jobDataLocked;
}

bool TMFLERController::isPostageDataLocked() const
{
    return m_postageDataLocked;
}

// BaseTrackerController implementation (unchanged from original)
void TMFLERController::outputToTerminal(const QString& message, MessageType type)
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

QTableView* TMFLERController::getTrackerWidget() const
{
    return m_tracker;
}

QSqlTableModel* TMFLERController::getTrackerModel() const
{
    return m_trackerModel;
}

QStringList TMFLERController::getTrackerHeaders() const
{
    return {"JOB", "DESCRIPTION", "POSTAGE", "COUNT", "AVG RATE", "CLASS", "SHAPE", "PERMIT"};
}

QList<int> TMFLERController::getVisibleColumns() const
{
    // Skip column 0 (ID), return columns 1-8
    return {1, 2, 3, 4, 5, 6, 7, 8};
}

QString TMFLERController::formatCellData(int columnIndex, const QString& cellData) const
{
    // Format POSTAGE column to include $ symbol if it doesn't have one
    if (columnIndex == 2 && !cellData.isEmpty() && !cellData.startsWith("$")) {
        return "$" + cellData;
    }
    return cellData;
}

// Lock button handlers (unchanged from original implementation)
void TMFLERController::onJobDataLockClicked()
{
    if (!validateJobData()) {
        outputToTerminal("Cannot lock job data: Missing required information", Error);
        return;
    }

    m_jobDataLocked = !m_jobDataLocked;
    updateLockStates();
    updateButtonStates();
    updateHtmlDisplay();

    if (m_jobDataLocked) {
        // Create folder for the job
        createJobFolder();

        // Copy files from HOME folder to DATA folder when opening
        copyFilesFromHomeFolder();

        outputToTerminal("Job data locked", Success);
        saveJobState();

        // Emit signal to start auto-save timer since job is now locked/open
        emit jobOpened();
        outputToTerminal("Auto-save timer started (15 minutes)", Info);
    } else {
        outputToTerminal("Job data unlocked", Info);
    }
}

void TMFLERController::onPostageLockClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot lock postage data: Job data must be locked first", Error);
        return;
    }

    m_postageDataLocked = !m_postageDataLocked;
    updateLockStates();
    updateButtonStates();

    if (m_postageDataLocked) {
        outputToTerminal("Postage data locked", Success);
    } else {
        outputToTerminal("Postage data unlocked", Info);
    }
}

// Script execution handlers - UPDATED to pass command line arguments
void TMFLERController::onRunInitialClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot run initial script: Job data must be locked first", Error);
        return;
    }

    executeScript("01 INITIAL");
}

void TMFLERController::onFinalStepClicked()
{
    if (!m_postageDataLocked) {
        outputToTerminal("Cannot run final step: Postage data must be locked first", Error);
        return;
    }

    executeScript("02 FINAL PROCESS");
}

void TMFLERController::executeScript(const QString& scriptName)
{
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

    outputToTerminal(QString("Executing script: %1").arg(scriptName), Info);
    outputToTerminal(QString("Script path: %1").arg(scriptPath), Info);

    // Prepare command line arguments
    QStringList arguments;
    arguments << scriptPath;

    // Add job data as arguments
    QString jobNumber = getJobNumber();
    QString year = getYear();
    QString month = getMonth();

    arguments << jobNumber << year << month;

    outputToTerminal(QString("Arguments: Job=%1, Year=%2, Month=%3").arg(jobNumber, year, month), Info);

    // Execute the Python script
    m_scriptRunner->runScript("python", arguments);
}

void TMFLERController::onScriptOutput(const QString& output)
{
    // Parse script output for special markers
    parseScriptOutput(output);

    // Output to terminal
    outputToTerminal(output, Info);
}

void TMFLERController::onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::CrashExit) {
        outputToTerminal("Script crashed unexpectedly", Error);
        return;
    }

    if (exitCode == 0) {
        outputToTerminal("Script completed successfully", Success);

        // Handle script-specific post-processing
        if (m_lastExecutedScript == "02 FINAL PROCESS") {
            // Show NAS link dialog if we captured a path
            if (!m_capturedNASPath.isEmpty()) {
                showNASLinkDialog(m_capturedNASPath);
            }

            // Refresh tracker
            if (m_trackerModel) {
                m_trackerModel->select();
            }
        }
    } else {
        outputToTerminal(QString("Script failed with exit code: %1").arg(exitCode), Error);
    }
}

void TMFLERController::parseScriptOutput(const QString& output)
{
    // Look for email dialog markers - NEW FUNCTIONALITY
    if (output.contains("=== SHOW_EMAIL_DIALOG ===")) {
        m_waitingForEmailConfirmation = true;
        return;
    }

    if (output.contains("=== END_SHOW_EMAIL_DIALOG ===")) {
        m_waitingForEmailConfirmation = false;
        // Show the email confirmation dialog
        if (!m_emailDialogPath.isEmpty()) {
            showEmailConfirmationDialog(m_emailDialogPath);
        }
        return;
    }

    // Capture email dialog path
    if (m_waitingForEmailConfirmation && !output.trimmed().isEmpty()) {
        m_emailDialogPath = output.trimmed();
        outputToTerminal("Email dialog path captured: " + m_emailDialogPath, Info);
        return;
    }

    // Look for NAS path markers (existing functionality)
    if (output.contains("=== NAS_FOLDER_PATH ===")) {
        m_capturingNASPath = true;
        return;
    }

    if (output.contains("=== END_NAS_FOLDER_PATH ===")) {
        m_capturingNASPath = false;
        return;
    }

    // Capture the NAS path
    if (m_capturingNASPath && !output.trimmed().isEmpty()) {
        m_capturedNASPath = output.trimmed();
        outputToTerminal("Captured NAS path: " + m_capturedNASPath, Success);
    }
}

void TMFLERController::showEmailConfirmationDialog(const QString& directoryPath)
{
    if (directoryPath.isEmpty()) {
        outputToTerminal("No directory path provided for email confirmation dialog", Warning);
        return;
    }

    outputToTerminal("Showing email confirmation dialog...", Info);

    // Clean up existing dialog if any
    if (m_emailDialog) {
        m_emailDialog->deleteLater();
    }

    // Create and show the email confirmation dialog
    m_emailDialog = new EmailConfirmationDialog(directoryPath, nullptr);

    // Connect dialog signals
    connect(m_emailDialog, &EmailConfirmationDialog::confirmed, this, &TMFLERController::onEmailDialogConfirmed);
    connect(m_emailDialog, &EmailConfirmationDialog::cancelled, this, &TMFLERController::onEmailDialogCancelled);

    m_emailDialog->show();
}

void TMFLERController::onEmailDialogConfirmed()
{
    outputToTerminal("Email confirmation received, continuing script...", Success);

    // Since ScriptRunner doesn't have writeToScript, we'll use a different approach
    // The Python script is waiting for input, so we'll handle this through process termination
    // and restart if needed, or use a file-based communication

    // For now, we'll create a temporary file to signal continuation
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString signalFile = QDir(tempDir).filePath("tmfler_email_confirmed.signal");

    QFile file(signalFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << "continue_email_confirmed";
        file.close();
        outputToTerminal("Email confirmation signal file created", Info);
    } else {
        outputToTerminal("Failed to create email confirmation signal file", Error);
    }

    // Clean up dialog
    if (m_emailDialog) {
        m_emailDialog->deleteLater();
        m_emailDialog = nullptr;
    }
}

void TMFLERController::onEmailDialogCancelled()
{
    outputToTerminal("Email confirmation cancelled, terminating script...", Warning);

    // Create cancellation signal file
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString signalFile = QDir(tempDir).filePath("tmfler_email_cancelled.signal");

    QFile file(signalFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << "cancel_script";
        file.close();
        outputToTerminal("Email cancellation signal file created", Info);
    }

    // Terminate the script if it's running
    if (m_scriptRunner && m_scriptRunner->isRunning()) {
        m_scriptRunner->terminate();
        outputToTerminal("Script terminated due to email confirmation cancellation", Warning);
    }

    // Clean up dialog
    if (m_emailDialog) {
        m_emailDialog->deleteLater();
        m_emailDialog = nullptr;
    }
}

void TMFLERController::showNASLinkDialog(const QString& nasPath)
{
    if (nasPath.isEmpty()) {
        outputToTerminal("No NAS path provided - cannot display location dialog", Warning);
        return;
    }

    outputToTerminal("Opening print file location dialog...", Info);

    // Create and show the dialog with custom text for FL ER
    NASLinkDialog* dialog = new NASLinkDialog(
        "Print File Location",           // Window title
        "Print data file located below", // Description text
        nasPath,                        // Network path
        nullptr                         // Parent
        );
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

// State management methods (unchanged from original implementation)
void TMFLERController::updateLockStates()
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

void TMFLERController::updateButtonStates()
{
    // Job data lock is always enabled
    if (m_jobDataLockBtn) m_jobDataLockBtn->setEnabled(true);

    // Postage lock can only be engaged if job data is locked
    if (m_postageLockBtn) m_postageLockBtn->setEnabled(m_jobDataLocked);

    // Script buttons enabled based on lock states
    if (m_runInitialBtn) m_runInitialBtn->setEnabled(m_jobDataLocked);
    if (m_finalStepBtn) m_finalStepBtn->setEnabled(m_postageDataLocked);
}

void TMFLERController::updateHtmlDisplay()
{
    if (!m_textBrowser) return;

    HtmlDisplayState targetState = determineHtmlState();
    if (m_currentHtmlState == UninitializedState || m_currentHtmlState != targetState) {
        m_currentHtmlState = targetState;

        // Load appropriate HTML file based on state
        if (targetState == InstructionsState) {
            loadHtmlFile(":/resources/tmfler/instructions.html");
        } else {
            loadHtmlFile(":/resources/tmfler/default.html");
        }
    }
}

void TMFLERController::loadHtmlFile(const QString& resourcePath)
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

TMFLERController::HtmlDisplayState TMFLERController::determineHtmlState() const
{
    // Show instructions only when job data is LOCKED
    if (m_jobDataLocked) {
        return InstructionsState;  // Show instructions.html when job is locked
    } else {
        return DefaultState;       // Show default.html when no job is locked
    }
}

// Validation methods (unchanged from original implementation)
bool TMFLERController::validateJobData() const
{
    QString jobNumber = getJobNumber();
    QString year = getYear();
    QString month = getMonth();

    if (jobNumber.isEmpty() || jobNumber.length() != 5) {
        return false;
    }

    if (year.isEmpty() || month.isEmpty()) {
        return false;
    }

    // Validate job number is numeric
    bool ok;
    jobNumber.toInt(&ok);
    return ok;
}

bool TMFLERController::validateScriptExecution(const QString& scriptName) const
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

// Job management methods (unchanged from original implementation)
bool TMFLERController::loadJob(const QString& year, const QString& month)
{
    if (!m_tmFlerDBManager) {
        outputToTerminal("Database manager not initialized", Error);
        return false;
    }

    QString jobNumber;
    if (m_tmFlerDBManager->loadJob(year, month, jobNumber)) {
        // Set UI values
        if (m_jobNumberBox) m_jobNumberBox->setText(jobNumber);
        if (m_yearDDbox) m_yearDDbox->setCurrentText(year);
        if (m_monthDDbox) m_monthDDbox->setCurrentText(month);

        // Lock the job data
        m_jobDataLocked = true;
        updateLockStates();
        updateButtonStates();
        updateHtmlDisplay();

        // NEW: Copy files from archive back to DATA folder since job is now locked
        copyFilesFromHomeFolder();
        outputToTerminal("Files copied from ARCHIVE to DATA folder", Info);

        // Start auto-save timer since job is locked/open
        emit jobOpened();
        outputToTerminal("Auto-save timer started (15 minutes)", Info);

        outputToTerminal(QString("Loaded job: %1 for %2/%3").arg(jobNumber, year, month), Success);
        return true;
    } else {
        outputToTerminal(QString("No job found for %1/%2").arg(year, month), Warning);
        return false;
    }
}

void TMFLERController::resetToDefaults()
{
    // CRITICAL FIX: Save current job state to database BEFORE resetting
    // This ensures lock states are preserved when job is reopened
    saveJobState();

    // Move files to HOME folder before closing
    moveFilesToHomeFolder();

    // Clear UI fields
    if (m_jobNumberBox) m_jobNumberBox->clear();
    if (m_yearDDbox) m_yearDDbox->setCurrentIndex(0);
    if (m_monthDDbox) m_monthDDbox->setCurrentIndex(0);

    // Reset state
    m_jobDataLocked = false;
    m_postageDataLocked = false;
    m_lastExecutedScript.clear();
    m_capturedNASPath.clear();
    m_capturingNASPath = false;
    m_waitingForEmailConfirmation = false;
    m_emailDialogPath.clear();

    // Clean up email dialog if open
    if (m_emailDialog) {
        m_emailDialog->deleteLater();
        m_emailDialog = nullptr;
    }

    // Clear terminal window
    if (m_terminalWindow) m_terminalWindow->clear();

    // Update UI
    updateLockStates();
    updateButtonStates();
    updateHtmlDisplay();

    // Force load default.html regardless of state
    loadHtmlFile(":/resources/tmfler/default.html");

    // Emit signal to stop auto-save timer since no job is open
    emit jobClosed();
    outputToTerminal("Job state reset to defaults", Info);
    outputToTerminal("Auto-save timer stopped - no job open", Info);
}

void TMFLERController::saveJobState()
{
    if (!m_tmFlerDBManager) return;

    QString jobNumber = getJobNumber();
    QString year = getYear();
    QString month = getMonth();

    if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty()) {
        outputToTerminal("Cannot save job: Missing required data", Warning);
        return;
    }

    if (m_tmFlerDBManager->saveJob(jobNumber, year, month)) {
        outputToTerminal("Job saved to database", Success);
    } else {
        outputToTerminal("Failed to save job to database", Error);
    }
}

// Tracker operations (unchanged from original implementation)
void TMFLERController::onAddToTracker()
{
    if (!validateJobData()) {
        outputToTerminal("Cannot add to tracker: Invalid job data", Error);
        return;
    }

    // This would typically be called from a UI action with postage data
    // For now, this is a placeholder for the functionality
    outputToTerminal("Add to tracker functionality ready", Info);
}

void TMFLERController::onCopyRowClicked()
{
    QString result = copyFormattedRow(); // Uses inherited BaseTrackerController method
    outputToTerminal(result, result.contains("success") ? Success : Error);
}

void TMFLERController::refreshTrackerTable()
{
    if (m_trackerModel) {
        m_trackerModel->select();
        outputToTerminal("Tracker table refreshed", Info);
    }
}

void TMFLERController::setupTrackerModel()
{
    if (!m_tracker) return;

    // Setup tracker model (placeholder - needs actual database table setup)
    outputToTerminal("Tracker model setup placeholder", Info);
}

// Directory management (unchanged from original implementation)
void TMFLERController::createJobFolder()
{
    if (!m_fileManager) return;

    QString year = getYear();
    QString month = getMonth();

    if (year.isEmpty() || month.isEmpty()) {
        outputToTerminal("Cannot create job folder: year or month not selected", Warning);
        return;
    }

    QString basePath = "C:/Goji/TRACHMAR/FL ER";
    QString jobFolder = basePath + "/ARCHIVE/" + month + " " + year;
    QDir dir(jobFolder);

    if (!dir.exists()) {
        if (dir.mkpath(".")) {
            outputToTerminal("Created job folder: " + jobFolder, Success);
        } else {
            outputToTerminal("Failed to create job folder: " + jobFolder, Error);
        }
    } else {
        outputToTerminal("Job folder already exists: " + jobFolder, Info);
    }
}

void TMFLERController::onFileSystemChanged()
{
    // Handle file system changes if needed
    outputToTerminal("File system changed", Info);
}

// ============================================================================
// EmailConfirmationDialog Implementation
// ============================================================================

EmailConfirmationDialog::EmailConfirmationDialog(const QString& directoryPath, QWidget *parent)
    : QDialog(parent)
    , m_messageLabel(nullptr)
    , m_continueButton(nullptr)
    , m_cancelButton(nullptr)
    , m_countdownTimer(nullptr)
    , m_secondsRemaining(10)
    , m_directoryPath(directoryPath)
{
    setWindowTitle("Email Confirmation Required");
    setModal(true);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);

    setupUI();

    // Start countdown timer
    m_countdownTimer = new QTimer(this);
    connect(m_countdownTimer, &QTimer::timeout, this, &EmailConfirmationDialog::onTimerTick);
    m_countdownTimer->start(1000); // 1 second intervals

    // Open directory immediately
    openDirectory();

    // Update button text with initial countdown
    updateButtonText();
}

void EmailConfirmationDialog::setupUI()
{
    setFixedSize(500, 200);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Message label
    m_messageLabel = new QLabel("ATTACH MERGED FILE TO EMAIL, THEN CLICK CONTINUE BUTTON TO CONTINUE SCRIPT", this);
    m_messageLabel->setWordWrap(true);
    m_messageLabel->setAlignment(Qt::AlignCenter);
    m_messageLabel->setStyleSheet(
        "QLabel {"
        "   font-size: 14px;"
        "   font-weight: bold;"
        "   color: #2c3e50;"
        "   padding: 10px;"
        "   background-color: #ecf0f1;"
        "   border: 2px solid #bdc3c7;"
        "   border-radius: 5px;"
        "}"
        );
    mainLayout->addWidget(m_messageLabel);

    // Directory path label
    QLabel* pathLabel = new QLabel(QString("Directory: %1").arg(m_directoryPath), this);
    pathLabel->setStyleSheet(
        "QLabel {"
        "   font-size: 10px;"
        "   color: #7f8c8d;"
        "   font-family: monospace;"
        "}"
        );
    pathLabel->setWordWrap(true);
    mainLayout->addWidget(pathLabel);

    // Button layout
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);

    // Cancel button
    m_cancelButton = new QPushButton("Cancel", this);
    m_cancelButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #e74c3c;"
        "   color: white;"
        "   border: none;"
        "   padding: 8px 16px;"
        "   border-radius: 4px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: #c0392b;"
        "}"
        );
    connect(m_cancelButton, &QPushButton::clicked, this, &EmailConfirmationDialog::onCancelClicked);
    buttonLayout->addWidget(m_cancelButton);

    buttonLayout->addStretch();

    // Continue button
    m_continueButton = new QPushButton("CONTINUE", this);
    m_continueButton->setEnabled(false); // Disabled initially
    m_continueButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #95a5a6;"
        "   color: white;"
        "   border: none;"
        "   padding: 8px 16px;"
        "   border-radius: 4px;"
        "   font-weight: bold;"
        "   min-width: 120px;"
        "}"
        "QPushButton:enabled {"
        "   background-color: #27ae60;"
        "}"
        "QPushButton:enabled:hover {"
        "   background-color: #229954;"
        "}"
        "QPushButton:disabled {"
        "   background-color: #95a5a6;"
        "   color: #ecf0f1;"
        "}"
        );
    connect(m_continueButton, &QPushButton::clicked, this, &EmailConfirmationDialog::onContinueClicked);
    buttonLayout->addWidget(m_continueButton);

    mainLayout->addLayout(buttonLayout);
}

void EmailConfirmationDialog::updateButtonText()
{
    if (m_secondsRemaining > 0) {
        m_continueButton->setText(QString("CONTINUE (%1 sec)").arg(m_secondsRemaining));
        m_continueButton->setEnabled(false);
    } else {
        m_continueButton->setText("CONTINUE");
        m_continueButton->setEnabled(true);
    }
}

void EmailConfirmationDialog::openDirectory()
{
    if (!m_directoryPath.isEmpty() && QDir(m_directoryPath).exists()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_directoryPath));
    }
}

void EmailConfirmationDialog::onTimerTick()
{
    m_secondsRemaining--;
    updateButtonText();

    if (m_secondsRemaining <= 0) {
        m_countdownTimer->stop();
    }
}

void EmailConfirmationDialog::onContinueClicked()
{
    if (m_secondsRemaining <= 0) {
        emit confirmed();
        accept();
    }
}

void EmailConfirmationDialog::onCancelClicked()
{
    emit cancelled();
    reject();
}

bool TMFLERController::moveFilesToHomeFolder()
{
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty()) {
        return false;
    }

    QString basePath = "C:/Goji/TRACHMAR/FL ER";
    QString homeFolder = month + " " + year; // FLER uses "MM YYYY" format
    QString jobFolder = basePath + "/DATA";
    QString homeFolderPath = basePath + "/ARCHIVE/" + homeFolder;

    // Create home folder structure if it doesn't exist
    QDir homeDir(homeFolderPath);
    if (!homeDir.exists()) {
        if (!homeDir.mkpath(".")) {
            outputToTerminal("Failed to create HOME folder: " + homeFolderPath, Error);
            return false;
        }
    }

    // Move files from DATA folder to HOME folder
    QDir sourceDir(jobFolder);
    if (sourceDir.exists()) {
        QStringList files = sourceDir.entryList(QDir::Files);
        bool allMoved = true;
        for (const QString& fileName : files) {
            QString sourcePath = jobFolder + "/" + fileName;
            QString destPath = homeFolderPath + "/" + fileName;

            // Remove existing file in destination if it exists
            if (QFile::exists(destPath)) {
                QFile::remove(destPath);
            }

            // Move file (rename)
            if (!QFile::rename(sourcePath, destPath)) {
                outputToTerminal("Failed to move file: " + sourcePath, Error);
                allMoved = false;
            } else {
                outputToTerminal("Moved file: " + fileName + " to ARCHIVE", Info);
            }
        }
        return allMoved;
    }

    return true;
}

bool TMFLERController::copyFilesFromHomeFolder()
{
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty()) {
        return false;
    }

    QString basePath = "C:/Goji/TRACHMAR/FL ER";
    QString homeFolder = month + " " + year; // FLER uses "MM YYYY" format
    QString jobFolder = basePath + "/DATA";
    QString homeFolderPath = basePath + "/ARCHIVE/" + homeFolder;

    // Check if home folder exists
    QDir homeDir(homeFolderPath);
    if (!homeDir.exists()) {
        outputToTerminal("HOME folder does not exist: " + homeFolderPath, Warning);
        return true; // Not an error if no previous job exists
    }

    // Create DATA folder if it doesn't exist
    QDir dataDir(jobFolder);
    if (!dataDir.exists() && !dataDir.mkpath(".")) {
        outputToTerminal("Failed to create DATA folder: " + jobFolder, Error);
        return false;
    }

    // Copy files from HOME folder to DATA folder
    QStringList files = homeDir.entryList(QDir::Files);
    bool allCopied = true;
    for (const QString& fileName : files) {
        QString sourcePath = homeFolderPath + "/" + fileName;
        QString destPath = jobFolder + "/" + fileName;

        // Remove existing file in destination if it exists
        if (QFile::exists(destPath)) {
            QFile::remove(destPath);
        }

        // Copy file
        if (!QFile::copy(sourcePath, destPath)) {
            outputToTerminal("Failed to copy file: " + sourcePath, Error);
            allCopied = false;
        } else {
            outputToTerminal("Copied file: " + fileName + " to DATA", Info);
        }
    }

    return allCopied;
}

bool TMFLERController::createExcelAndCopy(const QStringList& headers, const QStringList& rowData)
{
    // Use the inherited BaseTrackerController implementation
    return BaseTrackerController::createExcelAndCopy(headers, rowData);
}
