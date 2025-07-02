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
    , m_trackerModel(nullptr)
{
    initializeComponents();
    connectSignals();
    setupInitialState();
}

TMFLERController::~TMFLERController()
{
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

    Logger::instance().info("TMFLER controller signals connected");
}

void TMFLERController::setupInitialState()
{
    m_jobDataLocked = false;
    m_postageDataLocked = false;
    m_currentHtmlState = UninitializedState;
    m_capturingNASPath = false;
    m_lastExecutedScript.clear();
    m_capturedNASPath.clear();

    // Set up tracker model
    setupTrackerModel();

    // Update UI states
    updateLockStates();
    updateButtonStates();
    updateHtmlDisplay();

    outputToTerminal("TMFLER controller initialized", Success);
    outputToTerminal("Ready to process FL ER jobs", Info);
}

void TMFLERController::setupTrackerModel()
{
    if (m_tmFlerDBManager) {
        m_trackerModel = m_tmFlerDBManager->getTrackerModel();

        if (m_tracker && m_trackerModel) {
            m_tracker->setModel(m_trackerModel);

            // Hide the ID column
            m_tracker->setColumnHidden(0, true);

            // Set column widths
            QHeaderView* header = m_tracker->horizontalHeader();
            if (header) {
                header->setStretchLastSection(false);
                m_tracker->setColumnWidth(1, 80);   // JOB
                m_tracker->setColumnWidth(2, 200);  // DESCRIPTION
                m_tracker->setColumnWidth(3, 100);  // POSTAGE
                m_tracker->setColumnWidth(4, 80);   // COUNT
                m_tracker->setColumnWidth(5, 100);  // AVG RATE
                m_tracker->setColumnWidth(6, 60);   // CLASS
                m_tracker->setColumnWidth(7, 60);   // SHAPE
                m_tracker->setColumnWidth(8, 80);   // PERMIT
                m_tracker->setColumnWidth(9, 100);  // DATE
            }

            outputToTerminal("Tracker model configured", Info);
        }
    }
}

// UI Widget setters
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
    if (m_textBrowser) {
        // Force initial HTML load by resetting state
        m_currentHtmlState = UninitializedState;
        updateHtmlDisplay();
    }
}

void TMFLERController::setTracker(QTableView* tableView)
{
    m_tracker = tableView;
    setupTrackerModel();
}

// Public getters
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

// BaseTrackerController implementation
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

// Lock button handlers
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
        outputToTerminal("Job data locked", Success);
        saveJobState();
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

// Script execution handlers
void TMFLERController::onRunInitialClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot run initial script: Job data must be locked first", Error);
        return;
    }

    executeScript("01INITIAL");
}

void TMFLERController::onFinalStepClicked()
{
    if (!m_postageDataLocked) {
        outputToTerminal("Cannot run final step: Postage data must be locked first", Error);
        return;
    }

    executeScript("02FINALPROCESS");
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

    // Execute the Python script
    QStringList arguments;
    arguments << scriptPath;
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
        if (m_lastExecutedScript == "02FINALPROCESS") {
            // Open DATA folder
            QString dataPath = m_fileManager->getDataPath();
            QDesktopServices::openUrl(QUrl::fromLocalFile(dataPath));
            outputToTerminal("Opened DATA folder: " + dataPath, Info);

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
    // Look for NAS path markers
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

// State management
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
    // Show instructions only when job data is LOCKED (not just when data exists)
    if (m_jobDataLocked) {
        return InstructionsState;  // Show instructions.html when job is locked
    } else {
        return DefaultState;       // Show default.html when no job is locked
    }
}

// Validation methods - REMOVED outputToTerminal calls from const methods
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

// Job management
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

        outputToTerminal(QString("Loaded job: %1 for %2/%3").arg(jobNumber, year, month), Success);
        return true;
    } else {
        outputToTerminal(QString("No job found for %1/%2").arg(year, month), Warning);
        return false;
    }
}

void TMFLERController::resetToDefaults()
{
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

    // Update UI
    updateLockStates();
    updateButtonStates();
    updateHtmlDisplay();

    outputToTerminal("Reset to defaults", Info);
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

// Tracker operations
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

// Directory management
void TMFLERController::createBaseDirectories()
{
    if (!m_fileManager) return;

    if (m_fileManager->createBaseDirectories()) {
        outputToTerminal("Base directories verified/created", Success);
    } else {
        outputToTerminal("Failed to create some base directories", Warning);
    }
}

void TMFLERController::onFileSystemChanged()
{
    // Handle file system changes if needed
    outputToTerminal("File system changed", Info);
}
