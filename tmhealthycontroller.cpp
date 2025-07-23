#include "tmhealthycontroller.h"
#include "logger.h"
#include "naslinkdialog.h"
#include "tmhealthynetworkdialog.h"
#include "dropwindow.h"
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
#include <QSignalBlocker>
#include <QSqlQuery>
#include <QProcess>
#include <QLocale>
#include <QIODevice>
#include <type_traits>

class FormattedSqlModel : public QSqlTableModel {
public:
    FormattedSqlModel(QObject *parent, QSqlDatabase db, TMHealthyController *ctrl)
        : QSqlTableModel(parent, db), controller(ctrl) {}
    QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const {
        if (role == Qt::DisplayRole) {
            QVariant val = QSqlTableModel::data(idx, role);
            return controller->formatCellData(idx.column(), val.toString());
        }
        return QSqlTableModel::data(idx, role);
    }
private:
    TMHealthyController *controller;
};

TMHealthyController::TMHealthyController(QObject *parent)
    : BaseTrackerController(parent)
    , m_dbManager(DatabaseManager::instance())
    , m_fileManager(nullptr)
    , m_tmHealthyDBManager(TMHealthyDBManager::instance())
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
    , m_dropWindow(nullptr)
    , m_jobDataLocked(false)
    , m_postageDataLocked(false)
    , m_currentHtmlState(UninitializedState)
    , m_lastExecutedScript("")
    , m_capturedNASPath("")
    , m_capturingNASPath(false)
    , m_trackerModel(nullptr)
{
    // Initialize file manager for HEALTHY
    QSettings* settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "GojiApp", "Goji");
    m_fileManager = new TMHealthyFileManager(settings);

    // Initialize script runner
    m_scriptRunner = new ScriptRunner(this);

    // Setup the model for the tracker table
    if (m_dbManager && m_dbManager->isInitialized()) {
        m_trackerModel = new FormattedSqlModel(this, m_dbManager->getDatabase(), this);
        m_trackerModel->setTable("tmhealthy_log");
        m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
        m_trackerModel->select();
    } else {
        Logger::instance().warning("Cannot setup tracker model - database not available");
        m_trackerModel = nullptr;
    }
}

TMHealthyController::~TMHealthyController()
{
    Logger::instance().info("TMHealthyController destroyed");
}

void TMHealthyController::initializeUI(
    QPushButton* openBulkMailerBtn, QPushButton* runInitialBtn,
    QPushButton* finalStepBtn, QToolButton* lockBtn, QToolButton* editBtn,
    QToolButton* postageLockBtn, QComboBox* yearDDbox, QComboBox* monthDDbox,
    QLineEdit* jobNumberBox, QLineEdit* postageBox, QLineEdit* countBox,
    QTextEdit* terminalWindow, QTableView* tracker, QTextBrowser* textBrowser,
    DropWindow* dropWindow)
{
    Logger::instance().info("Initializing TM HEALTHY UI elements");

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
    m_dropWindow = dropWindow;

    // Setup drop window
    if (m_dropWindow) {
        setupDropWindow();
    }

    // Setup tracker table with optimized layout
    if (m_tracker) {
        m_tracker->setModel(m_trackerModel);
        m_tracker->setEditTriggers(QAbstractItemView::NoEditTriggers); // Read-only

        // Setup optimized table layout
        setupOptimizedTableLayout();

        // Connect contextual menu for copying
        m_tracker->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_tracker, &QTableView::customContextMenuRequested, this,
                &TMHealthyController::showTableContextMenu);
    }

    // Connect UI signals to slots
    connectSignals();

    // Setup initial UI state
    setupInitialUIState();

    // Populate dropdowns
    populateDropdowns();

    // Initialize HTML display with default state
    updateHtmlDisplay();

    Logger::instance().info("TM HEALTHY UI initialization complete");
}

void TMHealthyController::connectSignals()
{
    // Connect buttons with null pointer checks
    if (m_openBulkMailerBtn) {
        connect(m_openBulkMailerBtn, &QPushButton::clicked, this, &TMHealthyController::onOpenBulkMailerClicked);
    }
    if (m_runInitialBtn) {
        connect(m_runInitialBtn, &QPushButton::clicked, this, &TMHealthyController::onRunInitialClicked);
    }
    if (m_finalStepBtn) {
        connect(m_finalStepBtn, &QPushButton::clicked, this, &TMHealthyController::onFinalStepClicked);
    }

    // Connect toggle buttons with null pointer checks
    if (m_lockBtn) {
        connect(m_lockBtn, &QToolButton::clicked, this, &TMHealthyController::onLockButtonClicked);
    }
    if (m_editBtn) {
        connect(m_editBtn, &QToolButton::clicked, this, &TMHealthyController::onEditButtonClicked);
    }
    if (m_postageLockBtn) {
        connect(m_postageLockBtn, &QToolButton::clicked, this, &TMHealthyController::onPostageLockButtonClicked);
    }

    // Connect dropdowns with null pointer checks
    if (m_yearDDbox) {
        connect(m_yearDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
                this, &TMHealthyController::onYearChanged);
    }
    if (m_monthDDbox) {
        connect(m_monthDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
                this, &TMHealthyController::onMonthChanged);
    }

    // Connect input field handlers
    if (m_jobNumberBox) {
        connect(m_jobNumberBox, &QLineEdit::textChanged, this, &TMHealthyController::onJobNumberChanged);
    }
    if (m_postageBox) {
        connect(m_postageBox, &QLineEdit::textChanged, this, &TMHealthyController::onPostageChanged);
    }
    if (m_countBox) {
        connect(m_countBox, &QLineEdit::textChanged, this, &TMHealthyController::onCountChanged);
    }

    // Connect script runner signals
    if (m_scriptRunner) {
        connect(m_scriptRunner, &ScriptRunner::scriptOutput, this, &TMHealthyController::onScriptOutput);
        connect(m_scriptRunner, &ScriptRunner::scriptFinished, this, &TMHealthyController::onScriptFinished);
    }

    Logger::instance().info("TM HEALTHY signal connections complete");
}

void TMHealthyController::setupInitialUIState()
{
    Logger::instance().info("Setting up initial TM HEALTHY UI state...");

    // Initial lock states - all unlocked
    m_jobDataLocked = false;
    m_postageDataLocked = false;

    // Update control states
    updateControlStates();

    Logger::instance().info("Initial TM HEALTHY UI state setup complete");
}

void TMHealthyController::setupDropWindow()
{
    if (!m_dropWindow) {
        return;
    }

    Logger::instance().info("Setting up TM HEALTHY drop window...");

    // Set target directory to TMHEALTHY INPUT ZIP folder
    QString targetDirectory = "C:/Goji/TRACHMAR/HEALTHY BEGINNINGS/INPUT ZIP";
    m_dropWindow->setTargetDirectory(targetDirectory);
    
    // Connect drop window signals
    connect(m_dropWindow, &DropWindow::filesDropped,
            this, &TMHealthyController::onFilesDropped);
    connect(m_dropWindow, &DropWindow::fileDropError,
            this, &TMHealthyController::onFileDropError);
    
    // Clear any existing files from display
    m_dropWindow->clearFiles();
    
    outputToTerminal(QString("Drop window configured for directory: %1").arg(targetDirectory), Info);
    Logger::instance().info("TM HEALTHY drop window setup complete");
}

void TMHealthyController::populateDropdowns()
{
    Logger::instance().info("Populating TM HEALTHY dropdowns...");

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

    Logger::instance().info("TM HEALTHY dropdown population complete");
}

void TMHealthyController::setupOptimizedTableLayout()
{
    if (!m_tracker) return;

    // Implementation of table layout setup
    // Simplified version to avoid compilation issues
    if (m_trackerModel) {
        m_trackerModel->select();
    }
}

void TMHealthyController::updateControlStates()
{
    // Update lock button states
    if (m_lockBtn) {
        m_lockBtn->setChecked(m_jobDataLocked);
    }
    
    if (m_postageLockBtn) {
        m_postageLockBtn->setChecked(m_postageDataLocked);
    }

    // Lock/unlock job data fields based on lock state
    bool enableJobFields = !m_jobDataLocked;
    if (m_yearDDbox) m_yearDDbox->setEnabled(enableJobFields);
    if (m_monthDDbox) m_monthDDbox->setEnabled(enableJobFields);
    if (m_jobNumberBox) m_jobNumberBox->setEnabled(enableJobFields);

    // Lock/unlock postage data fields based on postage lock state
    bool enablePostageFields = !m_postageDataLocked;
    if (m_postageBox) m_postageBox->setEnabled(enablePostageFields);
    if (m_countBox) m_countBox->setEnabled(enablePostageFields);

    // Update button availability based on data completeness
    bool hasJobData = validateJobData();
    bool hasPostageData = validatePostageData();
    
    if (m_runInitialBtn) {
        m_runInitialBtn->setEnabled(hasJobData);
    }
    
    if (m_finalStepBtn) {
        m_finalStepBtn->setEnabled(hasJobData && hasPostageData && m_postageDataLocked);
    }
    
    if (m_openBulkMailerBtn) {
        m_openBulkMailerBtn->setEnabled(hasJobData);
    }

    // Update edit button state - only enabled when job data is locked
    if (m_editBtn) {
        m_editBtn->setEnabled(m_jobDataLocked);
    }

    // Postage lock can only be engaged if job data is locked
    if (m_postageLockBtn) {
        m_postageLockBtn->setEnabled(m_jobDataLocked);
    }
}

void TMHealthyController::updateHtmlDisplay()
{
    if (!m_textBrowser) {
        return;
    }

    HtmlDisplayState targetState = determineHtmlState();

    if (m_currentHtmlState == UninitializedState || m_currentHtmlState != targetState) {
        m_currentHtmlState = targetState;

        // Load appropriate HTML file based on state
        if (targetState == InstructionsState) {
            loadHtmlFile(":/resources/tmhealthy/instructions.html");
        } else {
            loadHtmlFile(":/resources/tmhealthy/default.html");
        }
    }
}

void TMHealthyController::outputToTerminal(const QString& message, MessageType type)
{
    if (!m_terminalWindow) return;

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString formattedMessage = QString("[%1] %2").arg(timestamp, message);
    m_terminalWindow->append(formattedMessage);
}

// Button handlers
void TMHealthyController::onOpenBulkMailerClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Please lock job data before opening Bulk Mailer.", Warning);
        return;
    }

    outputToTerminal("Opening Bulk Mailer application...", Info);

    // Standard BCC Software path - update as needed based on installation
    QString bulkMailerPath = "C:/Program Files (x86)/BCC Software/Bulk Mailer/BulkMailer.exe";

    // Check if file exists
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

void TMHealthyController::onRunInitialClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Please lock job data before running Initial script.", Warning);
        return;
    }

    if (!m_fileManager || !m_scriptRunner) {
        outputToTerminal("Error: Missing file manager or script runner", Error);
        return;
    }

    // Get script path from file manager
    QString scriptPath = m_fileManager->getScriptPath("01INITIAL");
    if (scriptPath.isEmpty() || !QFile::exists(scriptPath)) {
        outputToTerminal("Error: Initial script not found: " + scriptPath, Error);
        return;
    }

    // Get job data for script arguments
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty()) {
        outputToTerminal("Error: Missing required job data", Error);
        return;
    }

    outputToTerminal("Starting initial processing script...", Info);
    outputToTerminal(QString("Job: %1, Year: %2, Month: %3").arg(jobNumber, year, month), Info);
    
    // Set last executed script for HTML state management
    m_lastExecutedScript = "01INITIAL";

    // Disable the button while running
    if (m_runInitialBtn) {
        m_runInitialBtn->setEnabled(false);
    }

    // Prepare arguments: job number, year, month
    QStringList arguments;
    arguments << jobNumber << year << month;

    // Run the script
    m_scriptRunner->runScript("python", QStringList() << scriptPath << arguments);
}

void TMHealthyController::onFinalStepClicked()
{
    if (!m_jobDataLocked || !m_postageDataLocked) {
        outputToTerminal("Please lock job data and postage data before running Final Step script.", Warning);
        return;
    }

    if (!m_fileManager || !m_scriptRunner) {
        outputToTerminal("Error: Missing file manager or script runner", Error);
        return;
    }

    // Get script path from file manager
    QString scriptPath = m_fileManager->getScriptPath("02FINALPROCESS");
    if (scriptPath.isEmpty() || !QFile::exists(scriptPath)) {
        outputToTerminal("Error: Final process script not found: " + scriptPath, Error);
        return;
    }

    // Get job data for script arguments
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";

    if (jobNumber.isEmpty()) {
        outputToTerminal("Error: Missing job number", Error);
        return;
    }

    outputToTerminal("Starting final processing script...", Info);
    outputToTerminal(QString("Job: %1, Postage: %2, Count: %3").arg(jobNumber, postage, count), Info);
    
    // Disable the button while running
    if (m_finalStepBtn) {
        m_finalStepBtn->setEnabled(false);
    }

    // Prepare arguments: add --pause-before-cleanup flag for popup integration
    QStringList arguments;
    arguments << scriptPath << "--pause-before-cleanup";

    // Set up signal monitoring for pause detection
    m_capturingNASPath = false; // Reset any previous capture state
    
    // Run the script
    m_scriptRunner->runScript("python", arguments);
}

void TMHealthyController::onLockButtonClicked()
{
    m_jobDataLocked = !m_jobDataLocked;
    updateControlStates();
    
    if (m_jobDataLocked) {
        outputToTerminal("Job data locked", Info);
        // Save job state to database when locked
        saveJobState();
        emit jobOpened();
    } else {
        outputToTerminal("Job data unlocked", Info);
        emit jobClosed();
    }
}

void TMHealthyController::onEditButtonClicked()
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
        emit jobClosed();
    }
    // If edit button is unchecked, do nothing (ignore the click)
}

void TMHealthyController::onPostageLockButtonClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot lock postage data until job data is locked.", Error);
        m_postageLockBtn->setChecked(false);
        return;
    }

    if (m_postageLockBtn->isChecked()) {
        // Validate postage data before locking
        if (!validatePostageData()) {
            m_postageLockBtn->setChecked(false);
            return;
        }

        m_postageDataLocked = true;
        outputToTerminal("Postage data locked", Info);
        // Add log entry when postage is locked
        addLogEntry();
        // Calculate and display per-piece rate
        QString perPieceRate = calculatePerPiece(m_postageBox->text(), m_countBox->text());
        outputToTerminal(QString("Per piece rate: %1¢").arg(perPieceRate), Info);
    } else {
        m_postageDataLocked = false;
        outputToTerminal("Postage data unlocked", Info);
    }

    updateControlStates();
}

// Input handlers with formatting and validation
void TMHealthyController::onJobNumberChanged()
{
    updateControlStates();
}

void TMHealthyController::onPostageChanged()
{
    if (m_postageBox && !m_postageDataLocked) {
        formatPostageInput();
    }
    updateControlStates();
}

void TMHealthyController::onCountChanged()
{
    if (m_countBox && !m_postageDataLocked) {
        formatCountInput(m_countBox->text());
    }
    updateControlStates();
}

void TMHealthyController::onYearChanged(const QString& year)
{
    Q_UNUSED(year)
    updateControlStates();
}

void TMHealthyController::onMonthChanged(const QString& month)
{
    Q_UNUSED(month)
    updateControlStates();
}

void TMHealthyController::onAutoSaveTimer()
{
    if (m_jobDataLocked || m_postageDataLocked) {
        saveJobState();
        outputToTerminal("Auto-saved job state", Info);
    }
}

// Drop window handlers
void TMHealthyController::onFilesDropped(const QStringList& filePaths)
{
    outputToTerminal(QString("Files received: %1 file(s) dropped").arg(filePaths.size()), Success);
    
    // Log each dropped file
    for (const QString& filePath : filePaths) {
        QFileInfo fileInfo(filePath);
        QString fileName = fileInfo.fileName();
        outputToTerminal(QString("  - %1").arg(fileName), Info);
    }
    
    outputToTerminal("Files are ready for processing in INPUT ZIP folder", Info);
}

void TMHealthyController::onFileDropError(const QString& errorMessage)
{
    outputToTerminal(QString("File drop error: %1").arg(errorMessage), Warning);
}

// Script handlers
void TMHealthyController::onScriptOutput(const QString& output)
{
    outputToTerminal(output, Info);
    
    // Check for pause signal from final process script
    if (output.contains("=== PAUSE_SIGNAL ===")) {
        outputToTerminal("Script paused - displaying network file dialog...", Info);
        
        // Extract network path for dialog (we'll determine this based on current year and job number)
        QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
        QDate currentDate = QDate::currentDate();
        int currentYear = currentDate.year();
        QString networkPath = QString("\\\\NAS1069D9\\AMPrintData\\%1_SrcFiles\\T\\Trachmar\\%2_HealthyBeginnings\\HP Indigo\\DATA")
                             .arg(currentYear).arg(jobNumber);
        
        // Show the popup dialog
        showNASLinkDialog(networkPath);
        
        return; // Don't display the pause signal in terminal
    }
    
    // Check for resume signal
    if (output.contains("=== RESUME_PROCESSING ===")) {
        outputToTerminal("Script resumed processing...", Info);
        return; // Don't display the resume signal in terminal
    }
}

void TMHealthyController::onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // Re-enable buttons after script completion
    updateControlStates();
    
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        outputToTerminal("Script completed successfully", Success);
    } else {
        outputToTerminal(QString("Script finished with exit code: %1").arg(exitCode), 
                        exitCode == 0 ? Info : Warning);
    }
    
    // Update HTML display in case state changed
    updateHtmlDisplay();
}

// Job management
bool TMHealthyController::loadJob(const QString& year, const QString& month)
{
    Q_UNUSED(year)
    Q_UNUSED(month)
    return true;
}

void TMHealthyController::resetToDefaults()
{
    // Reset all form fields
    if (m_jobNumberBox) m_jobNumberBox->clear();
    if (m_postageBox) m_postageBox->clear();
    if (m_countBox) m_countBox->clear();
    if (m_yearDDbox) m_yearDDbox->setCurrentIndex(0);
    if (m_monthDDbox) m_monthDDbox->setCurrentIndex(0);
    
    // Reset lock states
    m_jobDataLocked = false;
    m_postageDataLocked = false;
    
    // Reset script execution state
    m_lastExecutedScript = "";
    
    // Reset HTML state to force reload
    m_currentHtmlState = UninitializedState;
    
    // Reset script execution state
    m_lastExecutedScript = "";
    
    // Reset HTML state to force reload
    m_currentHtmlState = UninitializedState;
    
    // Update UI state
    updateControlStates();
    updateHtmlDisplay();
    
    // Clear terminal
    if (m_terminalWindow) {
        m_terminalWindow->clear();
    }
    
    outputToTerminal("Job state reset to defaults", Info);
    emit jobClosed();
}

void TMHealthyController::saveJobState()
{
    // Implementation
}

void TMHealthyController::setTextBrowser(QTextBrowser* textBrowser)
{
    m_textBrowser = textBrowser;
}

// BaseTrackerController implementation methods
QTableView* TMHealthyController::getTrackerWidget() const
{
    return m_tracker;
}

QSqlTableModel* TMHealthyController::getTrackerModel() const
{
    return m_trackerModel;
}

QStringList TMHealthyController::getTrackerHeaders() const
{
    return {"JOB", "DESCRIPTION", "POSTAGE", "COUNT", "AVG RATE", "CLASS", "SHAPE", "PERMIT"};
}

QList<int> TMHealthyController::getVisibleColumns() const
{
    return {1, 2, 3, 4, 5, 6, 7, 8};
}

QString TMHealthyController::formatCellData(int columnIndex, const QString& cellData) const
{
    Q_UNUSED(columnIndex)
    return cellData;
}

// Private helper methods
void TMHealthyController::saveCurrentJobData()
{
    // Implementation
}

void TMHealthyController::loadJobData()
{
    // Implementation
}

bool TMHealthyController::validateJobData()
{
    if (!m_jobNumberBox || !m_yearDDbox || !m_monthDDbox) {
        return false;
    }

    QString jobNumber = m_jobNumberBox->text().trimmed();
    QString year = m_yearDDbox->currentText().trimmed();
    QString month = m_monthDDbox->currentText().trimmed();

    return validateJobNumber(jobNumber) && !year.isEmpty() && validateMonthSelection(month);
}

bool TMHealthyController::validatePostageData()
{
    if (!m_postageBox || !m_countBox) {
        return false;
    }

    QString postage = m_postageBox->text().trimmed();
    QString count = m_countBox->text().trimmed();

    // Check if postage is a valid monetary amount
    bool postageValid = false;
    QString cleanPostage = postage;
    cleanPostage.remove('$').remove(',');
    double postageValue = cleanPostage.toDouble(&postageValid);
    
    // Check if count is a valid positive integer
    bool countValid = false;
    QString cleanCount = count;
    cleanCount.remove(',');
    qlonglong countValue = cleanCount.toLongLong(&countValid);

    return postageValid && postageValue >= 0 && countValid && countValue > 0;
}

bool TMHealthyController::validateJobNumber(const QString& jobNumber)
{
    return jobNumber.length() == 5 && jobNumber.toInt() > 0;
}

bool TMHealthyController::validateMonthSelection(const QString& month)
{
    // Check if month is in valid range 01-12
    bool ok;
    int monthNum = month.toInt(&ok);
    return ok && monthNum >= 1 && monthNum <= 12;
}

QString TMHealthyController::convertMonthToAbbreviation(const QString& monthNumber) const
{
    QMap<QString, QString> monthMap = {
        {"01", "JAN"}, {"02", "FEB"}, {"03", "MAR"}, {"04", "APR"},
        {"05", "MAY"}, {"06", "JUN"}, {"07", "JUL"}, {"08", "AUG"},
        {"09", "SEP"}, {"10", "OCT"}, {"11", "NOV"}, {"12", "DEC"}
    };
    return monthMap.value(monthNumber, "");
}

QString TMHealthyController::getJobDescription() const
{
    return "TM HEALTHY BEGINNINGS";
}

bool TMHealthyController::hasJobData() const
{
    return validateJobData();
}

void TMHealthyController::updateJobDataUI()
{
    // Update UI based on current job data
    updateControlStates();
    updateHtmlDisplay();
}

void TMHealthyController::updateLockStates()
{
    updateControlStates();
}

void TMHealthyController::lockInputs(bool locked)
{
    m_jobDataLocked = locked;
    updateControlStates();
}

void TMHealthyController::enableEditMode(bool enabled)
{
    if (enabled) {
        m_jobDataLocked = false;
        m_postageDataLocked = false;
    }
    updateControlStates();
}

void TMHealthyController::updateTrackerTable()
{
    if (m_trackerModel) {
        m_trackerModel->select();
    }
}

void TMHealthyController::loadHtmlFile(const QString& resourcePath)
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
            "<h2>TM HEALTHY BEGINNINGS</h2>"
            "<p>Instructions not available</p>"
            "<p>Please check that HTML resources are properly installed.</p>"
            "</body></html>"
        );
        m_textBrowser->setHtml(fallbackContent);
    }
}

TMHealthyController::HtmlDisplayState TMHealthyController::determineHtmlState() const
{
    // Show instructions when job data is locked
    if (m_jobDataLocked) {
        return InstructionsState;  // Show instructions.html when job is locked
    } else {
        return DefaultState;       // Show default.html otherwise
    }
}

void TMHealthyController::formatPostageInput()
{
    if (!m_postageBox) return;
    
    QString text = m_postageBox->text();
    QString cleanText = text;
    cleanText.remove('$').remove(',');
    
    bool ok;
    double value = cleanText.toDouble(&ok);
    
    if (ok && value >= 0) {
        QString formatted = QString("$%1").arg(value, 0, 'f', 2);
        if (text != formatted) {
            QSignalBlocker blocker(m_postageBox);
            m_postageBox->setText(formatted);
        }
    }
}

void TMHealthyController::formatCountInput(const QString& text)
{
    if (!m_countBox) return;
    
    QString cleanText = text;
    cleanText.remove(',');
    
    bool ok;
    qlonglong value = cleanText.toLongLong(&ok);
    
    if (ok && value > 0) {
        QString formatted = QLocale().toString(value);
        if (text != formatted) {
            QSignalBlocker blocker(m_countBox);
            m_countBox->setText(formatted);
        }
    }
}

void TMHealthyController::parseScriptOutput(const QString& output)
{
    Q_UNUSED(output)
}

void TMHealthyController::showNASLinkDialog(const QString& nasPath)
{
    if (nasPath.isEmpty()) {
        outputToTerminal("No NAS path provided - cannot display location dialog", Warning);
        return;
    }

    outputToTerminal("Opening specialized network file dialog...", Info);

    // Get job number for file list population
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    
    // Create dialog using factory method that checks for fallback mode
    TMHealthyNetworkDialog* dialog = TMHealthyNetworkDialog::createDialog(
        nasPath,                          // Network path
        jobNumber,                        // Job number for file filtering
        nullptr                           // Parent
        );
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    
    // The script will automatically resume processing when the dialog is closed
    // because the ScriptRunner's input handler automatically sends input
    outputToTerminal("Specialized dialog displayed with network/fallback files and drag-drop support", Info);
    outputToTerminal("Script will resume automatically after dialog display", Info);
}

QString TMHealthyController::copyFormattedRow()
{
    return BaseTrackerController::copyFormattedRow();
}

bool TMHealthyController::createExcelAndCopy(const QStringList& headers, const QStringList& rowData)
{
    return BaseTrackerController::createExcelAndCopy(headers, rowData);
}

void TMHealthyController::showTableContextMenu(const QPoint& pos)
{
    Q_UNUSED(pos)
}

void TMHealthyController::loadJobState()
{
    // Implementation
}

void TMHealthyController::addLogEntry()
{
    if (!m_trackerModel) return;
    
    // Add entry to tracker table
    int row = m_trackerModel->rowCount();
    m_trackerModel->insertRow(row);
    
    QModelIndex jobIndex = m_trackerModel->index(row, 0);
    QModelIndex descIndex = m_trackerModel->index(row, 1);
    QModelIndex postageIndex = m_trackerModel->index(row, 2);
    QModelIndex countIndex = m_trackerModel->index(row, 3);
    QModelIndex rateIndex = m_trackerModel->index(row, 4);
    
    m_trackerModel->setData(jobIndex, getJobNumber());
    m_trackerModel->setData(descIndex, getJobDescription());
    m_trackerModel->setData(postageIndex, m_postageBox ? m_postageBox->text() : "");
    m_trackerModel->setData(countIndex, m_countBox ? m_countBox->text() : "");
    m_trackerModel->setData(rateIndex, calculatePerPiece(
        m_postageBox ? m_postageBox->text() : "",
        m_countBox ? m_countBox->text() : ""
    ));
    
    m_trackerModel->submitAll();
}

QString TMHealthyController::calculatePerPiece(const QString& postage, const QString& count) const
{
    // Clean postage string
    QString cleanPostage = postage;
    cleanPostage.remove('$').remove(',');

    // Clean count string
    QString cleanCount = count;
    cleanCount.remove(',');

    bool postageOk, countOk;
    double postageValue = cleanPostage.toDouble(&postageOk);
    qlonglong countValue = cleanCount.toLongLong(&countOk);

    if (postageOk && countOk && countValue > 0) {
        double perPiece = postageValue / countValue;
        return QString("0.%1").arg(QString::number(perPiece * 1000, 'f', 0).rightJustified(3, '0'));
    }

    return "0.000";
}

void TMHealthyController::refreshTrackerTable()
{
    if (m_trackerModel) {
        m_trackerModel->select();
    }
}

void TMHealthyController::saveJobToDatabase()
{
    // Implementation
}

void TMHealthyController::debugCheckTables()
{
    // Implementation
}
