#include "tmflercontroller.h"
#include "logger.h"
#include "naslinkdialog.h"
#include "dropwindow.h"
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

#include "tmfleremaildialog.h"
#include "tmflerfilemanager.h"
#include "scriptrunner.h"
#include <QPointer>
#include <QRegularExpression>

class FormattedSqlModel : public QSqlTableModel {
public:
    FormattedSqlModel(QObject *parent, QSqlDatabase db, TMFLERController *ctrl)
        : QSqlTableModel(parent, db), controller(ctrl) {}
    QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole) {
            QVariant val = QSqlTableModel::data(idx, role);
            return controller->formatCellData(idx.column(), val.toString());
        }
        return QSqlTableModel::data(idx, role);
    }
private:
    TMFLERController *controller;
};

TMFLERController::TMFLERController(QObject *parent)
    : BaseTrackerController(parent)
    , m_fileManager(nullptr)
    , m_tmFlerDBManager(nullptr)
    , m_scriptRunner(nullptr)
    , m_jobNumberBox(nullptr)
    , m_yearDDbox(nullptr)
    , m_monthDDbox(nullptr)
    , m_postageBox(nullptr)
    , m_countBox(nullptr)
    , m_jobDataLockBtn(nullptr)
    , m_editBtn(nullptr)
    , m_postageLockBtn(nullptr)
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
    , m_trackerModel(nullptr)
    , m_lastYear(-1)
    , m_lastMonth(-1)
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

    // NOTE: Do NOT call createBaseDirectories() here.


    // We move it to initializeAfterConstruction() to avoid virtual calls during construction.

    Logger::instance().info("TMFLER controller components initialized");
}

// Safe post-construction initializer properly placed
void TMFLERController::initializeAfterConstruction()
{
    // Safe point to perform actions that might call virtuals / logging
    createBaseDirectories();
}


// Added safe post-construction initializer


void TMFLERController::createBaseDirectories()
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
    m_currentHtmlState = UninitializedState;
    m_waitingForEmailConfirmation = false;

    // Update UI states
    updateLockStates();
    updateButtonStates();
    updateHtmlDisplay();

    Logger::instance().info("TMFLER controller initial state set");
}

// UI Widget setters
void TMFLERController::setJobNumberBox(QLineEdit* lineEdit)
{
    m_jobNumberBox = lineEdit;
    if (m_jobNumberBox) {
        connect(m_jobNumberBox, &QLineEdit::editingFinished, this, [this]() {
            const QString newNum = m_jobNumberBox->text().trimmed();
            if (newNum.isEmpty() || !validateJobNumber(newNum)) return;

            if (newNum != m_cachedJobNumber) {
                saveJobState();
                TMFLERDBManager::instance()->updateLogJobNumber(m_cachedJobNumber, newNum);
                m_cachedJobNumber = newNum;
                refreshTrackerTable();
            }
        });
    }
}

void TMFLERController::setYearDropdown(QComboBox* comboBox)
{
    m_yearDDbox = comboBox;
    
    // Populate year dropdown on initialization
    if (m_yearDDbox) {
        populateYearDropdown();
        
        // Connect year change signal
        connect(m_yearDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
                this, &TMFLERController::onYearChanged);
    }
}

void TMFLERController::setMonthDropdown(QComboBox* comboBox)
{
    m_monthDDbox = comboBox;
    
    // Populate month dropdown on initialization
    if (m_monthDDbox) {
        populateMonthDropdown();
        
        // Connect month change signal
        connect(m_monthDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
                this, &TMFLERController::onMonthChanged);
    }
}

void TMFLERController::setPostageBox(QLineEdit* lineEdit)
{
    m_postageBox = lineEdit;
    if (m_postageBox) {
        QRegularExpressionValidator* validator = new QRegularExpressionValidator(QRegularExpression("[0-9]*\\.?[0-9]*\\$?"), this);
        m_postageBox->setValidator(validator);
        connect(m_postageBox, &QLineEdit::editingFinished, this, &TMFLERController::formatPostageInput);

        // Auto-save on postage changes when job is locked
        connect(m_postageBox, &QLineEdit::textChanged, this, [this]() {
            if (m_jobDataLocked) {
                saveJobState();
            }
        });
    }
}

void TMFLERController::setCountBox(QLineEdit* lineEdit)
{
    m_countBox = lineEdit;
    if (m_countBox) {
        connect(m_countBox, &QLineEdit::textChanged, this, &TMFLERController::formatCountInput);

        // Auto-save on count changes when job is locked
        connect(m_countBox, &QLineEdit::textChanged, this, [this]() {
            if (m_jobDataLocked) {
                saveJobState();
            }
        });
    }
}

void TMFLERController::setJobDataLockButton(QToolButton* button)
{
    m_jobDataLockBtn = button;
    if (m_jobDataLockBtn) {
        connect(m_jobDataLockBtn, &QToolButton::clicked, this, &TMFLERController::onJobDataLockClicked);
    }
}

void TMFLERController::setEditButton(QToolButton* button)
{
    m_editBtn = button;
    if (m_editBtn) {
        connect(m_editBtn, &QToolButton::clicked, this, &TMFLERController::onEditButtonClicked);
    }
}

void TMFLERController::setPostageLockButton(QToolButton* button)
{
    m_postageLockBtn = button;
    if (m_postageLockBtn) {
        connect(m_postageLockBtn, &QToolButton::clicked, this, &TMFLERController::onPostageLockClicked);
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

void TMFLERController::setDropWindow(DropWindow* dropWindow)
{
    m_dropWindow = dropWindow;
    if (m_dropWindow) {
        setupDropWindow();
    }
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

bool TMFLERController::hasJobData() const
{
    QString jobNumber = getJobNumber();
    QString year = getYear();
    QString month = getMonth();

    return !jobNumber.isEmpty() && !year.isEmpty() && !month.isEmpty();
}

// Utility method to convert month number to abbreviation
QString TMFLERController::convertMonthToAbbreviation(const QString& monthNumber) const
{
    QMap<QString, QString> monthMap = {
        {"01", "JAN"}, {"02", "FEB"}, {"03", "MAR"}, {"04", "APR"},
        {"05", "MAY"}, {"06", "JUN"}, {"07", "JUL"}, {"08", "AUG"},
        {"09", "SEP"}, {"10", "OCT"}, {"11", "NOV"}, {"12", "DEC"}
    };
    return monthMap.value(monthNumber, monthNumber);
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
    // NOTE: This method is called from FormattedSqlModel with database column indices
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

QString TMFLERController::formatCellDataForCopy(int columnIndex, const QString& cellData) const
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
void TMFLERController::onJobDataLockClicked()
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
        
        // CRITICAL FIX FOR FL ER: Check if we're re-locking after editing with period/job change
        // If the period changed, delete the old row to prevent duplicates
        if (m_lastYear > 0 && m_lastMonth > 0 && !m_cachedJobNumber.isEmpty()) {
            if (newYear != m_lastYear || newMonth != m_lastMonth) {
                outputToTerminal(QString("Period changed during edit: OLD=%1/%2, NEW=%3/%4")
                               .arg(QString("%1").arg(m_lastMonth, 2, 10, QChar('0')), 
                                    QString::number(m_lastYear),
                                    QString("%1").arg(newMonth, 2, 10, QChar('0')), 
                                    QString::number(newYear)), Info);
                
                // Delete the old period row to prevent duplicate rows
                if (m_tmFlerDBManager->deleteJob(m_lastYear, m_lastMonth)) {
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
        copyFilesFromHomeFolder();

        // Save to database (UPDATE if exists for this year/month, INSERT if not)
        QString jobNumber = getJobNumber();
        QString year = getYear();
        QString month = getMonth();
        if (m_tmFlerDBManager->saveJob(jobNumber, year, month)) {
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

void TMFLERController::onEditButtonClicked()
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

void TMFLERController::onPostageLockClicked()
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

// Script execution handlers
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
    m_scriptRunner->runScript(scriptPath, args);
}

void TMFLERController::onScriptOutput(const QString &output)
{
    // Always log script output
    outputToTerminal(output, Info);

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

    // 2) HEALTHY-style pause marker
    if (output.contains("=== PAUSE_FOR_EMAIL ===")) {
        outputToTerminal("Detected PAUSE_FOR_EMAIL. Opening FL ER email dialog...", Info);

        QString nasPath = m_capturedNASPath;
        if (nasPath.isEmpty() && m_fileManager) {
            nasPath = m_fileManager->getDataPath(); // fallback
        }
        const QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text().trimmed() : QString();

        showEmailDialog(nasPath, jobNumber);
        return;
    }

    // 3) Resume notification
    if (output.contains("=== RESUME_PROCESSING ===")) {
        outputToTerminal("Script indicates resume processing.", Info);
        return;
    }
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
    // Look for email dialog markers
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

    // Create temporary file to signal continuation
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

// State management methods
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
    // Job data fields - enabled when not locked
    bool jobFieldsEnabled = !m_jobDataLocked;
    if (m_jobNumberBox) m_jobNumberBox->setEnabled(jobFieldsEnabled);
    if (m_yearDDbox) m_yearDDbox->setEnabled(jobFieldsEnabled);
    if (m_monthDDbox) m_monthDDbox->setEnabled(jobFieldsEnabled);
    
    // Postage data fields - enabled when postage not locked
    if (m_postageBox) m_postageBox->setEnabled(!m_postageDataLocked);
    if (m_countBox) m_countBox->setEnabled(!m_postageDataLocked);

    // Lock button states
    if (m_jobDataLockBtn) m_jobDataLockBtn->setChecked(m_jobDataLocked);
    if (m_postageLockBtn) m_postageLockBtn->setChecked(m_postageDataLocked);

    // Edit button only enabled when job data is locked
    if (m_editBtn) m_editBtn->setEnabled(m_jobDataLocked);

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
        // Create fallback content if HTML file fails to load
        QString fallbackContent = QString(
            "<html><body style='font-family: Arial; padding: 20px;'>"
            "<h2>TM FL ER</h2>"
            "<p>Instructions not available</p>"
            "<p>Please check that HTML resources are properly installed.</p>"
            "</body></html>"
        );
        m_textBrowser->setHtml(fallbackContent);
    }
}

TMFLERController::HtmlDisplayState TMFLERController::determineHtmlState() const
{
    return m_jobDataLocked ? InstructionsState : DefaultState;
}

// Validation methods
bool TMFLERController::validateJobData()
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

bool TMFLERController::validatePostageData()
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

void TMFLERController::formatPostageInput()
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

void TMFLERController::formatCountInput(const QString& text)
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

// Job management methods
bool TMFLERController::loadJob(const QString& year, const QString& month)
{
    if (!m_tmFlerDBManager) {
        outputToTerminal("Database manager not initialized", Error);
        return false;
    }

    // FL ER FIX: Get job number from UI since we need all three fields to load
    QString jobNumber = getJobNumber();
    if (jobNumber.isEmpty()) {
        outputToTerminal("Cannot load job: job number is required", Error);
        return false;
    }

    if (m_tmFlerDBManager->loadJob(jobNumber, year, month)) {
        // Set UI values (job number already set)
        if (m_yearDDbox) m_yearDDbox->setCurrentText(year);
        if (m_monthDDbox) m_monthDDbox->setCurrentText(month);

        // CRITICAL: Initialize cache BEFORE loading state
        // This ensures cache has correct values for any future edits
        m_lastYear = year.toInt();
        m_lastMonth = month.toInt();
        m_cachedJobNumber = jobNumber;

        // Force UI to process the dropdown changes before state load
        QCoreApplication::processEvents();

        // Load job state (this will set lock states and HTML display)
        loadJobState();

        // If no state was loaded, default to locked
        if (!m_jobDataLocked) {
            m_jobDataLocked = true;
            outputToTerminal("Job state not found, defaulting to locked", Info);
        }

        // Update UI to reflect the lock state
        if (m_jobDataLockBtn) m_jobDataLockBtn->setChecked(m_jobDataLocked);

        // If job data is locked, handle file operations and auto-save
        if (m_jobDataLocked) {
            copyFilesFromHomeFolder();
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

void TMFLERController::resetToDefaults()
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
    m_emailDialogPath.clear();

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

    // Save basic job data
    if (m_tmFlerDBManager->saveJob(jobNumber, year, month)) {
        outputToTerminal("Job saved to database", Success);
    } else {
        outputToTerminal("Failed to save job to database", Error);
    }

    // Save job state including HTML display state, lock states, and script execution state
    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";
    
    // FL ER FIX: Pass job_number to saveJobState to identify which specific job to update
    if (m_tmFlerDBManager->saveJobState(jobNumber, year, month,
                                        static_cast<int>(m_currentHtmlState),
                                        m_jobDataLocked, m_postageDataLocked,
                                        postage, count, m_lastExecutedScript)) {
        outputToTerminal(QString("Job state saved to database: postage=%1, count=%2, postage_locked=%3")
                             .arg(postage, count, m_postageDataLocked ? "true" : "false"), Success);
    } else {
        outputToTerminal("Failed to save job state to database", Error);
    }
}

void TMFLERController::loadJobState()
{
    if (!m_tmFlerDBManager) return;

    QString year = getYear();
    QString month = getMonth();

    if (year.isEmpty() || month.isEmpty()) return;

    int htmlState;
    bool jobLocked, postageLocked;
    QString postage, count, lastExecutedScript;

    if (m_tmFlerDBManager->loadJobState(year, month, htmlState, jobLocked, postageLocked,
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

// CRITICAL FIX: Enhanced addLogEntry - now UPDATES existing row for current job instead of always inserting
void TMFLERController::addLogEntry()
{
    if (!m_tmFlerDBManager) {
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
    QString description = QString("TM %1 FL ER").arg(monthAbbrev);

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
    if (m_tmFlerDBManager->updateLogEntryForJob(jobNumber, description, formattedPostage, formattedCount,
                                                formattedAvgRate, mailClass, shape, permit, date)) {
        outputToTerminal(QString("Log entry updated for job %1: %2 pieces at %3 (%4 avg rate)")
                             .arg(jobNumber, formattedCount, formattedPostage, formattedAvgRate), Success);
    } else {
        // If update failed (no existing entry), then add new entry
        if (m_tmFlerDBManager->addLogEntry(jobNumber, description, formattedPostage, formattedCount,
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

void TMFLERController::onCopyRowClicked()
{
    QString result = copyFormattedRow(); // Uses inherited BaseTrackerController method
    outputToTerminal(result, result.contains("success") ? Success : Error);
}

void TMFLERController::refreshTrackerTable()
{
    if (m_trackerModel) {
        m_trackerModel->select();
        applyTrackerHeaders();
        outputToTerminal("Tracker table refreshed", Info);
    }
}

void TMFLERController::setupTrackerModel()
{
    if (!m_tracker || !m_tmFlerDBManager) return;

    m_trackerModel = new FormattedSqlModel(this, DatabaseManager::instance()->getDatabase(), this);
    m_trackerModel->setTable("tm_fler_log");
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
                    this, &TMFLERController::showTableContextMenu);
        }
    } else {
        outputToTerminal("Failed to initialize tracker model", Error);
    }
}

void TMFLERController::setupOptimizedTableLayout()
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
    QFont tableFont("Blender Pro Bold", optimalFontSize);
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
void TMFLERController::populateYearDropdown()
{
    if (!m_yearDDbox) return;
    
    m_yearDDbox->clear();
    m_yearDDbox->addItem(""); // Blank default
    
    QDate currentDate = QDate::currentDate();
    int currentYear = currentDate.year();
    
    m_yearDDbox->addItem(QString::number(currentYear - 1)); // Last year
    m_yearDDbox->addItem(QString::number(currentYear));     // Current year
    m_yearDDbox->addItem(QString::number(currentYear + 1)); // Next year
}

void TMFLERController::populateMonthDropdown()
{
    if (!m_monthDDbox) return;
    
    m_monthDDbox->clear();
    m_monthDDbox->addItem(""); // Blank default
    
    for (int i = 1; i <= 12; i++) {
        m_monthDDbox->addItem(QString("%1").arg(i, 2, 10, QChar('0')));
    }
}

// Dropdown change handlers
void TMFLERController::onYearChanged(const QString& year)
{
    Q_UNUSED(year)
    // FL ER FIX: Do NOT load job state on dropdown change
    // Only update HTML display - job loading happens when all fields are set and locked
    updateHtmlDisplay();
}

void TMFLERController::onMonthChanged(const QString& month)
{
    Q_UNUSED(month)
    // FL ER FIX: Do NOT load job state on dropdown change
    // Only update HTML display - job loading happens when all fields are set and locked
    updateHtmlDisplay();
}

// Directory management
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

void TMFLERController::setupDropWindow()
{
    if (!m_dropWindow) {
        return;
    }

    Logger::instance().info("Setting up TM FL ER drop window...");

    // Set target directory to FL ER RAW INPUT folder
    QString targetDirectory = "C:/Goji/TRACHMAR/FL ER/RAW INPUT";
    m_dropWindow->setTargetDirectory(targetDirectory);
    m_dropWindow->setSupportedExtensions({"xlsx", "xls", "csv", "zip"});

    // Connect drop window signals
    connect(m_dropWindow, &DropWindow::filesDropped,
            this, &TMFLERController::onFilesDropped);
    connect(m_dropWindow, &DropWindow::fileDropError,
            this, &TMFLERController::onFileDropError);

    // Clear any existing files from display
    m_dropWindow->clearFiles();

    outputToTerminal(QString("Drop window configured for directory: %1").arg(targetDirectory), Info);
    Logger::instance().info("TM FL ER drop window setup complete");
}

void TMFLERController::onFileSystemChanged()
{
    // Handle file system changes if needed
    outputToTerminal("File system changed", Info);
}

// Drop window handlers
void TMFLERController::onFilesDropped(const QStringList& filePaths)
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

void TMFLERController::onFileDropError(const QString& errorMessage)
{
    outputToTerminal(QString("File drop error: %1").arg(errorMessage), Warning);
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
        const QStringList files = sourceDir.entryList(QDir::Files);
        bool allMoved = true;
        for (auto it = files.cbegin(); it != files.cend(); ++it) {
            const QString& fileName = *it;
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
    const QStringList files = homeDir.entryList(QDir::Files);
    bool allCopied = true;
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
            const QString& fileName = *it;
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

void TMFLERController::showTableContextMenu(const QPoint& pos)
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

bool TMFLERController::validateJobNumber(const QString& jobNumber) const {
    if (jobNumber.length() != 5) return false;
    for (const QChar ch : jobNumber) if (!ch.isDigit()) return false;
    return true;
}

void TMFLERController::autoSaveAndCloseCurrentJob()
{
    // Check if we have a job currently open (locked)
    if (m_jobDataLocked) {
        // FL ER FIX: Verify cache is initialized before attempting save
        // If m_lastYear or m_lastMonth are -1, it means no job was properly locked yet
        if (m_lastYear <= 0 || m_lastMonth <= 0 || m_cachedJobNumber.isEmpty()) {
            outputToTerminal("Auto-save skipped: job cache not initialized", Warning);
            return;
        }
        
        // Use CACHED values (old period) for save, NOT current UI values
        QString currentJobNumber = m_cachedJobNumber;
        QString currentYear = QString::number(m_lastYear);
        QString currentMonth = QString("%1").arg(m_lastMonth, 2, 10, QChar('0'));

        if (!currentJobNumber.isEmpty() && !currentYear.isEmpty() && !currentMonth.isEmpty()) {
            outputToTerminal(QString("Auto-saving current job %1 (%2-%3) before opening new job")
                                 .arg(currentJobNumber, currentYear, currentMonth), Info);

            TMFLERDBManager* dbManager = TMFLERDBManager::instance();
            if (dbManager) {
                if (dbManager->saveJob(currentJobNumber, currentYear, currentMonth)) {
                    outputToTerminal("Job saved to database", Success);
                } else {
                    outputToTerminal("Failed to save job to database", Error);
                }

                QString postage = m_postageBox ? m_postageBox->text() : "";
                QString count = m_countBox ? m_countBox->text() : "";

                // FL ER FIX: Pass cached job_number to saveJobState
                if (dbManager->saveJobState(currentJobNumber, currentYear, currentMonth,
                                            static_cast<int>(m_currentHtmlState),
                                            m_jobDataLocked, m_postageDataLocked,
                                            postage, count, m_lastExecutedScript)) {
                    outputToTerminal(QString("Job state saved to database: postage=%1, count=%2, postage_locked=%3")
                                         .arg(postage, count, m_postageDataLocked ? "true" : "false"), Success);
                } else {
                    outputToTerminal("Failed to save job state to database", Error);
                }

                outputToTerminal("Moving files from DATA folder back to ARCHIVE folder...", Info);
                if (moveFilesToHomeFolder()) {
                    outputToTerminal("Files moved successfully from DATA to ARCHIVE folder", Success);
                } else {
                    outputToTerminal("Warning: Some files may not have been moved properly", Warning);
                }

                m_jobDataLocked = false;
                m_postageDataLocked = false;
                m_currentHtmlState = UninitializedState;

                updateLockStates();
                updateButtonStates();
                updateHtmlDisplay();
                emit jobClosed();

                outputToTerminal("Current job auto-saved and closed", Success);
            } else {
                outputToTerminal("Database manager not initialized", Error);
            }
        }
    }
}

void TMFLERController::applyTrackerHeaders()
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

void TMFLERController::triggerArchivePhase()
{
    if (!m_fileManager || !m_scriptRunner) {
        outputToTerminal("Error: Missing file manager or script runner", Error);
        return;
    }
    
    QString scriptPath = m_fileManager->getScriptPath("02 FINAL PROCESS");
    QString jobNumber = getJobNumber();
    QString year = getYear();
    QString month = getMonth();
    
    QStringList args;
    args << jobNumber << year << month << "--mode" << "archive";
    outputToTerminal("Starting FL ER archive phase...", Info);
    m_scriptRunner->runScript(scriptPath, args);
}

void TMFLERController::showEmailDialog(const QString &nasPath, const QString &jobNumber)
{
    Q_UNUSED(nasPath);
    
    if (jobNumber.isEmpty()) {
        outputToTerminal("No job number available for FL ER email dialog. Resuming without dialog.", Warning);
        return;
    }

    outputToTerminal(QString("Opening FL ER email dialog for job: %1").arg(jobNumber), Info);

    QPointer<TMFLEREmailDialog> dlg = new TMFLEREmailDialog(jobNumber, nullptr);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    connect(dlg, &TMFLEREmailDialog::dialogClosed, this, [this]() {
        triggerArchivePhase();
    });

    dlg->show();
}
