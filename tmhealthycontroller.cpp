#include "tmhealthycontroller.h"
#include "logger.h"
#include "naslinkdialog.h"
#include "configmanager.h"
#include "tmhealthynetworkdialog.h"
#include "tmhealthyemaildialog.h"
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
#include <QMenu>
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
    , m_autoSaveTimer(nullptr)
    , m_trackerModel(nullptr)
{
    if (!m_tmHealthyDBManager->initializeDatabase()) {
        qWarning() << "[TMHealthyController] Failed to initialize database. Job persistence will be unavailable.";
        m_databaseAvailable = false;
    } else {
        m_databaseAvailable = true;
    }
    
    // Direct instantiation used per ADR-001 — factory pattern was deprecated for simplicity
    QSettings* settings = ConfigManager::instance().getSettings();
    m_fileManager = new TMHealthyFileManager(settings, this);

    // Initialize script runner
    m_scriptRunner = new ScriptRunner(this);

    // Initialize auto-save timer
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setSingleShot(true);
    m_autoSaveTimer->setInterval(500); // 500ms debounce
    connect(m_autoSaveTimer, &QTimer::timeout, this, &TMHealthyController::onAutoSaveTimer);
    
    // === INSERTED ===
    // Initialize m_finalNASPath
    m_finalNASPath.clear();
    // === END INSERTED ===

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
    if (m_autoSaveTimer) {
        m_autoSaveTimer->stop();
    }
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

    // Connect input formatting
    if (m_postageBox) {
        QRegularExpressionValidator* validator = new QRegularExpressionValidator(QRegularExpression("[0-9]*\\.?[0-9]*\\$?"), this);
        m_postageBox->setValidator(validator);
        connect(m_postageBox, &QLineEdit::editingFinished, this, &TMHealthyController::formatPostageInput);

        // Auto-save on postage changes when job is locked (debounced)
        connect(m_postageBox, &QLineEdit::textChanged, this, [this]() {
            if (m_jobDataLocked && m_autoSaveTimer) {
                m_autoSaveTimer->start(); // Restart timer for debounced save
            }
        });
    }

    if (m_countBox) {
        connect(m_countBox, &QLineEdit::textChanged, this, &TMHealthyController::formatCountInput);

        // Auto-save on count changes when job is locked (debounced)
        connect(m_countBox, &QLineEdit::textChanged, this, [this]() {
            if (m_jobDataLocked && m_autoSaveTimer) {
                m_autoSaveTimer->start(); // Restart timer for debounced save
            }
        });
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
    m_dropWindow->setSupportedExtensions({"xlsx", "xls", "csv", "zip"});
    m_dropWindow->setInstructionText("Drag ZIP, CSV, or XLSX files here\nto upload to INPUT ZIP folder");

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

    const int tableWidth = 611;
    const int borderWidth = 2;
    const int availableWidth = tableWidth - borderWidth;

    struct ColumnSpec {
        QString header;
        QString maxContent;
        int minWidth;
    };

    QList<ColumnSpec> columns = {
        {"JOB", "88888", 56},
        {"DESCRIPTION", "TM HEALTHY BEGINNINGS", 140},
        {"POSTAGE", "$888,888.88", 29},
        {"COUNT", "88,888", 45},
        {"AVG RATE", "0.888", 45},
        {"CLASS", "STD", 60},
        {"SHAPE", "LTR", 33},
        {"PERMIT", "NKLN", 36}
    };

    QFont testFont("Blender Pro Bold", 7);
    QFontMetrics fm(testFont);

    int optimalFontSize = 7;
    for (int fontSize = 11; fontSize >= 7; fontSize--) {
        testFont.setPointSize(fontSize);
        fm = QFontMetrics(testFont);

        int totalWidth = 0;
        bool fits = true;

        for (const auto& col : columns) {
            int headerWidth = fm.horizontalAdvance(col.header) + 12;
            int contentWidth = fm.horizontalAdvance(col.maxContent) + 12;
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

    QFont tableFont("Blender Pro Bold", optimalFontSize);
    m_tracker->setFont(tableFont);

    // Set up the model with proper ordering (newest first)
    m_trackerModel->setSort(0, Qt::DescendingOrder);
    m_trackerModel->select();
    if (m_tracker) {
        m_tracker->setSortingEnabled(true);
        m_tracker->sortByColumn(0, Qt::DescendingOrder);
    }

    // Set custom headers - SAME AS TMTERM
    m_trackerModel->setHeaderData(1, Qt::Horizontal, tr("JOB"));
    m_trackerModel->setHeaderData(2, Qt::Horizontal, tr("DESCRIPTION"));
    m_trackerModel->setHeaderData(3, Qt::Horizontal, tr("POSTAGE"));
    m_trackerModel->setHeaderData(4, Qt::Horizontal, tr("COUNT"));
    m_trackerModel->setHeaderData(5, Qt::Horizontal, tr("AVG RATE"));
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

    fm = QFontMetrics(tableFont);
    for (int i = 0; i < columns.size(); i++) {
        const auto& col = columns[i];
        int headerWidth = fm.horizontalAdvance(col.header) + 12;
        int contentWidth = fm.horizontalAdvance(col.maxContent) + 12;
        int colWidth = qMax(headerWidth, qMax(contentWidth, col.minWidth));

        m_tracker->setColumnWidth(i + 1, colWidth); // +1 because we hide column 0
    }

    m_tracker->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    // Enable only vertical scrolling
    m_tracker->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tracker->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

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

void TMHealthyController::updateControlStates()
{
    // Update lock button states
    if (m_lockBtn) {
        m_lockBtn->setChecked(m_jobDataLocked);
    }
    
    if (m_postageLockBtn) {
        m_postageLockBtn->setChecked(m_postageDataLocked);
        m_postageLockBtn->setEnabled(m_jobDataLocked);
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
        m_editBtn->setChecked(false); // Reset edit button when updating states
    }

    // Postage lock can only be engaged if job data is locked
    // (Already handled above in the postage lock button state update)
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

// === INSERTED ===
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
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : QString::number(QDate::currentDate().year());

    if (jobNumber.isEmpty() || year.isEmpty()) {
        outputToTerminal("Error: Job number and year are required", Error);
        return;
    }
    
    if (!validateJobNumber(jobNumber)) {
        outputToTerminal("Error: Invalid job number format (must be 5 digits)", Error);
        return;
    }

    // Validate required files before starting script
    QString inputFile = m_fileManager->getInputDirectory() + "/INPUT.csv";
    QString outputFile1 = m_fileManager->getOutputDirectory() + "/TRACHMAR HEALTHY BEGINNINGS.csv";
    QString outputFile2 = m_fileManager->getOutputDirectory() + "/TMHB14 CODE LIST.csv";
    
    QStringList missingFiles;
    if (!QFile::exists(inputFile)) {
        missingFiles << "INPUT.csv";
    }
    if (!QFile::exists(outputFile1)) {
        missingFiles << "TRACHMAR HEALTHY BEGINNINGS.csv";
    }
    if (!QFile::exists(outputFile2)) {
        missingFiles << "TMHB14 CODE LIST.csv";
    }
    
    if (!missingFiles.isEmpty()) {
        outputToTerminal("Error: Missing required files: " + missingFiles.join(", "), Error);
        outputToTerminal("Please ensure all required files are in the correct directories", Error);
        return;
    }

    outputToTerminal("Starting final processing script...", Info);
    outputToTerminal(QString("Job: %1, Year: %2").arg(jobNumber, year), Info);
    
    // Clear previous NAS path
    m_finalNASPath.clear();

    // Disable the button while running
    if (m_finalStepBtn) {
        m_finalStepBtn->setEnabled(false);
    }

    // Connect to script runner signals for output parsing
    connect(m_scriptRunner, QOverload<const QString&>::of(&ScriptRunner::scriptOutput),
            this, &TMHealthyController::parseScriptOutput);
    connect(m_scriptRunner, QOverload<int, QProcess::ExitStatus>::of(&ScriptRunner::scriptFinished),
            this, QOverload<int, QProcess::ExitStatus>::of(&TMHealthyController::onScriptFinished));

    // Prepare arguments: job number and year only
    QStringList arguments;
    arguments << jobNumber << year;

    // Run the script
    m_scriptRunner->runScript("python", QStringList() << scriptPath << arguments);
}
// === END INSERTED ===

void TMHealthyController::onLockButtonClicked()
{
    m_jobDataLocked = !m_jobDataLocked;
    updateControlStates();
    updateHtmlDisplay();
    
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
        outputToTerminal("Postage data locked", Success);
        // Add log entry when postage is locked
        addLogEntry();
        // Calculate and display per-piece rate
        QString perPieceRate = calculatePerPiece(m_postageBox->text(), m_countBox->text());
        outputToTerminal(QString("Per piece rate: %1¢").arg(perPieceRate), Info);
        
        // Save postage data to database when locked
        saveJobState();
    } else {
        m_postageDataLocked = false;
        outputToTerminal("Postage data unlocked", Info);
        
        // Save unlocked state to database
        saveJobState();
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
    
    // >>> BEGIN NAS PATCH
    // Parse script output for NAS markers
    parseScriptOutput(output);
    // <<< END NAS PATCH
    
    // Check for pause signal from final process script
    if (output.contains("=== PAUSE_SIGNAL ===")) {
        outputToTerminal("Script paused - displaying email dialog...", Info);
        
        // Extract job details for dialog
        QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
        QString year = m_yearDDbox ? m_yearDDbox->currentText() : QString::number(QDate::currentDate().year());
        
        // Construct network path
        QString networkPath = QString("\\\\NAS1069D9\\AMPrintData\\%1_SrcFiles\\T\\Trachmar\\%2_HealthyBeginnings\\HP Indigo\\DATA")
                             .arg(year).arg(jobNumber);
        
        // Create and show the email dialog
        TMHealthyEmailDialog* emailDialog = new TMHealthyEmailDialog(networkPath, jobNumber, nullptr);
        emailDialog->setAttribute(Qt::WA_DeleteOnClose);
        
        // Use exec() to block the script until dialog is closed
        int result = emailDialog->exec();
        
        if (result == QDialog::Accepted) {
            outputToTerminal("Email dialog completed - resuming script...", Info);
            
            // Send input to script to resume processing
            if (m_scriptRunner && m_scriptRunner->isRunning()) {
                m_scriptRunner->writeToScript("\n");
            }
        } else {
            outputToTerminal("Email dialog cancelled", Warning);
        }
        
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

// Job management methods are implemented later in the file

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
    if (columnIndex == 3) { // POSTAGE
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
    if (columnIndex == 4) { // COUNT
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

QString TMHealthyController::formatCellDataForCopy(int columnIndex, const QString& cellData) const
{
    // For copy operations, COUNT should be plain integer without thousand separators
    if (columnIndex == 2) { // POSTAGE column
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
    if (columnIndex == 3) { // COUNT column - return as plain integer
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

// Private helper methods
void TMHealthyController::saveCurrentJobData()
{
    // Implementation
}

void TMHealthyController::loadJobData()
{
    // Implementation
}

bool TMHealthyController::validateJobData() const
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

bool TMHealthyController::validateJobNumber(const QString& jobNumber) const
{
    return jobNumber.length() == 5 && jobNumber.toInt() > 0;
}

bool TMHealthyController::validateMonthSelection(const QString& month) const
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
    // Show instructions only when job data is locked AND a script has been executed
    if (m_jobDataLocked && !m_lastExecutedScript.isEmpty()) {
        return InstructionsState;  // Show instructions.html when job is locked and script run
    } else {
        return DefaultState;       // Show default.html otherwise
    }
}

void TMHealthyController::formatPostageInput()
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

void TMHealthyController::formatCountInput(const QString& text)
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

// >>> BEGIN NAS PATCH
void TMHealthyController::parseScriptOutput(const QString& line)
{
    // Parse output for NAS path markers
    
    if (line.trimmed() == "=== NAS_FOLDER_PATH ===") {
        m_capturingNASPath = true;
        m_finalNASPath.clear();
        return;
    }
    
    if (line.trimmed() == "=== END_NAS_FOLDER_PATH ===") {
        m_capturingNASPath = false;
        if (!m_finalNASPath.isEmpty()) {
            outputToTerminal(QString("Captured NAS path: %1").arg(m_finalNASPath), Info);
            // Immediately show the NAS dialog when marker is detected
            showNASLinkDialog(m_finalNASPath);
        }
        return;
    }
    
    if (m_capturingNASPath) {
        m_finalNASPath = line.trimmed();
    }
    
    // Forward the output to terminal as usual - don't duplicate here
    // The original onScriptOutput will handle terminal output
}
// <<< END NAS PATCH

// >>> BEGIN NAS PATCH
void TMHealthyController::showNASLinkDialog(const QString& nasPath)
{
    if (nasPath.isEmpty()) {
        outputToTerminal("No NAS path provided - cannot display location dialog", Warning);
        return;
    }

    outputToTerminal("Opening specialized network file dialog...", Info);

    // Get job number for file list population
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    
    // Create new modal dialog matching final spec
    TMHealthyNetworkDialog* dialog = new TMHealthyNetworkDialog(
        nasPath,                          // Network path
        jobNumber,                        // Job number for file filtering
        nullptr                           // Parent
        );
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    
    // Use exec() to make dialog modal and pause script execution
    int result = dialog->exec();
    
    if (result == QDialog::Accepted) {
        outputToTerminal("NAS dialog completed - script will continue", Info);
    } else {
        outputToTerminal("NAS dialog cancelled", Warning);
    }
    
    outputToTerminal("Network dialog displayed with ZIP files and drag-drop support", Info);
}
// <<< END NAS PATCH

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
    QMenu menu(m_tracker);
    QAction* copyAction = menu.addAction("Copy Selected Row");
    QAction* selectedAction = menu.exec(m_tracker->mapToGlobal(pos));
    if (selectedAction == copyAction) {
        QString result = copyFormattedRow();
        if (result == "Row copied to clipboard") {
            outputToTerminal("Row copied to clipboard with formatting", Success);
        } else {
            outputToTerminal(result, Warning);
        }
    }
}

void TMHealthyController::onCopyRow()
{
    QString result = copyFormattedRow();
    outputToTerminal("Copy Row: " + result, result.contains("success") ? Success : Warning);
    Logger::instance().info("TM HEALTHY: Copy row action triggered");
}

void TMHealthyController::loadJobState() {
    if (!m_databaseAvailable) {
        qWarning() << "[TMHealthyController] Cannot load job state: database not available.";
        return;
    }
    if (!m_tmHealthyDBManager) return;
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    if (year.isEmpty() || month.isEmpty()) return;
    QVariantMap jobData = m_tmHealthyDBManager->loadJobData(year, month);
    if (!jobData.isEmpty()) {
        m_jobDataLocked = jobData["job_data_locked"].toBool();
        m_postageDataLocked = jobData["postage_data_locked"].toBool();
        m_lastExecutedScript = jobData["last_executed_script"].toString();
        QString postage = jobData["postage"].toString();
        QString count = jobData["count"].toString();
        if (m_postageBox && !postage.isEmpty()) {
            m_postageBox->setText(postage);
        }
        if (m_countBox && !count.isEmpty()) {
            m_countBox->setText(count);
        }
        // Explicitly set m_currentHtmlState based on m_jobDataLocked
        m_currentHtmlState = m_jobDataLocked ? InstructionsState : DefaultState;
        outputToTerminal(QString("Job state loaded: job_locked=%1, html_state=%2")
                             .arg(m_jobDataLocked ? "true" : "false",
                                  m_currentHtmlState == InstructionsState ? "Instructions" : "Default"), Info);
    } else {
        m_jobDataLocked = false;
        m_postageDataLocked = false;
        m_lastExecutedScript = "";
        m_currentHtmlState = DefaultState;
        outputToTerminal("No saved job state found, using defaults", Info);
    }
    updateControlStates();
    updateHtmlDisplay();
}

void TMHealthyController::addLogEntry()
{
    if (!m_tmHealthyDBManager) {
        outputToTerminal("Database manager not available for log entry", Error);
        return;
    }

    // Get current data from UI
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";

    if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty()) {
        outputToTerminal("Cannot add log entry: missing required data", Error);
        return;
    }

    // Build description
    QString monthAbbrev = convertMonthToAbbreviation(month);
    QString description = QString("TM HEALTHY %1").arg(monthAbbrev);

    // Format count with thousand separators for display
    bool ok;
    qlonglong countValue = count.remove(",").toLongLong(&ok);
    QString formattedCount = ok ? QString("%L1").arg(countValue) : QString::number(countValue);

    // Calculate per piece rate with exactly 3 decimal places
    double postageAmount = postage.remove("$").remove(",").toDouble();
    double perPiece = (countValue > 0) ? (postageAmount / countValue) : 0.0;
    QString perPieceStr = QString::number(perPiece, 'f', 3);

    // Default values for TM HEALTHY
    QString classAbbrev = "STD"; // Standard class
    QString permitShort = "1662"; // Correct permit for TM HEALTHY
    QString shape = "LTR"; // Letter shape

    // Get current date
    QDateTime now = QDateTime::currentDateTime();
    QString date = now.toString("M/d/yyyy");

    // Create log entry map
    QVariantMap logEntry;
    logEntry["job_number"] = jobNumber;
    logEntry["description"] = description;
    logEntry["postage"] = QString("$%L1").arg(postageAmount, 0, 'f', 2);
    logEntry["count"] = formattedCount;
    logEntry["per_piece"] = perPieceStr;
    logEntry["mail_class"] = classAbbrev;
    logEntry["shape"] = shape;
    logEntry["permit"] = permitShort;
    logEntry["date"] = date;
    logEntry["year"] = year;
    logEntry["month"] = month;

    // Add to database using the TM HEALTHY database manager
    if (m_tmHealthyDBManager->addLogEntry(logEntry)) {
        outputToTerminal("Added log entry to database", Success);

        // Force refresh the table view
        refreshTrackerTable();
    } else {
        outputToTerminal("Failed to add log entry to database", Error);
    }
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
        m_trackerModel->setSort(0, Qt::DescendingOrder);
        m_trackerModel->select();
        if (m_tracker) {
            m_tracker->setSortingEnabled(true);
            m_tracker->sortByColumn(0, Qt::DescendingOrder);
        }
        outputToTerminal("Tracker table refreshed with newest entries at top", Info);
    }
}

void TMHealthyController::saveJobState() {
    if (!m_tmHealthyDBManager) return;
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    if (year.isEmpty() || month.isEmpty()) return;
    QVariantMap jobData;
    jobData["year"] = year;
    jobData["month"] = month;
    jobData["job_number"] = m_jobNumberBox ? m_jobNumberBox->text() : "";
    jobData["postage"] = m_postageBox ? m_postageBox->text() : "";
    jobData["count"] = m_countBox ? m_countBox->text() : "";
    jobData["job_data_locked"] = m_jobDataLocked;
    jobData["postage_data_locked"] = m_postageDataLocked;
    jobData["html_display_state"] = m_jobDataLocked ? InstructionsState : DefaultState;
    jobData["last_executed_script"] = m_lastExecutedScript;
    if (m_tmHealthyDBManager->saveJobData(jobData)) {
        outputToTerminal("Job state saved", Info);
    } else {
        outputToTerminal("Failed to save job state", Warning);
    }
}

void TMHealthyController::saveJobToDatabase()
{
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty()) {
        outputToTerminal("Cannot save job: missing required data", Warning);
        return;
    }

    if (m_tmHealthyDBManager->saveJob(jobNumber, year, month)) {
        outputToTerminal("Job saved to database", Success);
    } else {
        outputToTerminal("Failed to save job to database", Error);
    }
}

bool TMHealthyController::loadJob(const QString& year, const QString& month) {
    if (!m_tmHealthyDBManager) return false;
    QVariantMap jobData = m_tmHealthyDBManager->loadJobData(year, month);
    if (!jobData.isEmpty()) {
        QString jobNumber = jobData["job_number"].toString();
        if (m_jobNumberBox) m_jobNumberBox->setText(jobNumber);
        if (m_yearDDbox) m_yearDDbox->setCurrentText(year);
        if (m_monthDDbox) m_monthDDbox->setCurrentText(month);
        QCoreApplication::processEvents();
        loadJobState();
        if (!m_jobDataLocked) {
            m_jobDataLocked = true;
            outputToTerminal("Job state not found, defaulting to locked", Info);
        }
        if (m_lockBtn) m_lockBtn->setChecked(m_jobDataLocked);
        if (m_jobDataLocked) {
            emit jobOpened();
            outputToTerminal("Auto-save timer started (15 minutes)", Info);
        }
        updateControlStates();
        m_currentHtmlState = UninitializedState;  // Force a full refresh
        updateHtmlDisplay();
        refreshTrackerTable();
        outputToTerminal("Job loaded: " + jobNumber, Success);
        return true;
    }
    outputToTerminal("Failed to load job for " + year + "/" + month, Error);
    return false;
}

void TMHealthyController::resetToDefaults()
{
    // Save current job state to database BEFORE resetting
    saveJobState();

    // Now reset all internal state variables
    m_jobDataLocked = false;
    m_postageDataLocked = false;
    m_currentHtmlState = DefaultState;
    m_capturedNASPath.clear();
    m_capturingNASPath = false;
    m_lastExecutedScript.clear();

    // Clear all form fields
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
    loadHtmlFile(":/resources/tmhealthy/default.html");

    // Emit signal to stop auto-save timer since no job is open
    emit jobClosed();
    outputToTerminal("Job state reset to defaults", Info);
    outputToTerminal("Auto-save timer stopped - no job open", Info);
}

void TMHealthyController::debugCheckTables()
{
    // Implementation
}

void TMHealthyController::autoSaveAndCloseCurrentJob()
{
    saveJobState();
    // Move files to home folder when closing the job
    if (m_fileManager) {
        QString year = getYear();
        QString month = getMonth();
        if (!year.isEmpty() && !month.isEmpty()) {
            m_fileManager->moveFilesToHomeDirectory(year, month);
        }
    }
}
