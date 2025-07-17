// Message type enum for colored terminal output
    enum MessageType {
        Info,
        Warning,
        Error,
        Success
    };

    // HTML display states
    enum HtmlDisplayState {
        DefaultState = 0,
        InstructionsState = 1
    };

    // HTML display states
    enum HtmlDisplayState {
        DefaultState = 0,#include "tmhealthycontroller.h"
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
#include <QSignalBlocker>
#include <QSqlQuery>
#include <QProcess>

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
    , m_jobDataLocked(false)
    , m_postageDataLocked(false)
    , m_currentHtmlState(DefaultState)
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
    QTextEdit* terminalWindow, QTableView* tracker, QTextBrowser* textBrowser)
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

    // Calculate optimal font size and column widths
    const int tableWidth = 611; // Fixed widget width from UI
    const int borderWidth = 2;   // Account for table borders
    const int availableWidth = tableWidth - borderWidth;

    // Define maximum content widths based on TMHEALTHY data format
    struct ColumnSpec {
        QString header;
        QString maxContent;
        int minWidth;
    };

    QList<ColumnSpec> columns = {
        {"JOB", "88888", 56},
        {"DESCRIPTION", "TM DEC HEALTHY BEGINNINGS", 140},
        {"POSTAGE", "$888,888.88", 29},
        {"COUNT", "88,888", 45},
        {"AVG RATE", "0.888", 45},
        {"CLASS", "STD", 60},
        {"SHAPE", "LTR", 33},
        {"PERMIT", "NKLN", 36}
    };

    // Calculate optimal font size
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

    // Apply the optimal font
    QFont tableFont("Blender Pro Bold", optimalFontSize);
    m_tracker->setFont(tableFont);

    // Set up the model with proper ordering (newest first)
    m_trackerModel->setSort(0, Qt::DescendingOrder); // Sort by ID descending
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

    // Hide unwanted columns
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
        int headerWidth = fm.horizontalAdvance(col.header) + 12;
        int contentWidth = fm.horizontalAdvance(col.maxContent) + 12;
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

void TMHealthyController::updateHtmlDisplay()
{
    if (!m_textBrowser) {
        outputToTerminal("DEBUG: No text browser available!", Error);
        return;
    }

    enum HtmlDisplayState {
        DefaultState = 0,
        InstructionsState = 1
    };

    HtmlDisplayState targetState = m_jobDataLocked ? InstructionsState : DefaultState;

    if (m_currentHtmlState != targetState) {
        m_currentHtmlState = targetState;

        // Load appropriate HTML file based on state
        if (targetState == InstructionsState) {
            loadHtmlFile(":/resources/tmhealthy/instructions.html");
        } else {
            loadHtmlFile(":/resources/tmhealthy/default.html");
        }
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
        m_textBrowser->setHtml("<p>Instructions not available</p>");
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

void TMHealthyController::outputToTerminal(const QString& message, MessageType type)
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

bool TMHealthyController::hasJobData() const
{
    // Check if we have essential job data
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    return !jobNumber.isEmpty() && !year.isEmpty() && !month.isEmpty();
}

// Button handlers
void TMHealthyController::onOpenBulkMailerClicked()
{
    outputToTerminal("Opening Bulk Mailer...", Info);

    QString program = "BulkMailer.exe";
    if (!QProcess::startDetached(program)) {
        outputToTerminal("Failed to open Bulk Mailer", Error);
    } else {
        outputToTerminal("Bulk Mailer opened successfully", Success);
    }
}

void TMHealthyController::onRunInitialClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Error: Job data must be locked before running initial script", Error);
        return;
    }

    if (!m_scriptRunner) {
        outputToTerminal("Error: Missing script runner", Error);
        return;
    }

    QString scriptPath = "C:/Goji/scripts/TRACHMAR/HEALTHY BEGINNINGS/01 INITIAL.py";
    if (!QFile::exists(scriptPath)) {
        outputToTerminal("Error: Initial script not found: " + scriptPath, Error);
        return;
    }

    outputToTerminal("Starting initial processing script...", Info);
    m_lastExecutedScript = "01 INITIAL";

    QStringList arguments;
    // No arguments needed for initial script

    m_scriptRunner->runScript(scriptPath, arguments);
}

void TMHealthyController::onFinalStepClicked()
{
    if (!m_postageDataLocked) {
        outputToTerminal("Error: Postage data must be locked before running final script", Error);
        return;
    }

    if (!m_scriptRunner) {
        outputToTerminal("Error: Missing script runner", Error);
        return;
    }

    QString scriptPath = "C:/Goji/scripts/TRACHMAR/HEALTHY BEGINNINGS/02 FINAL PROCESS.py";
    if (!QFile::exists(scriptPath)) {
        outputToTerminal("Error: Final script not found: " + scriptPath, Error);
        return;
    }

    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty()) {
        outputToTerminal("Error: Job number, year, or month not available", Error);
        return;
    }

    outputToTerminal("Starting final processing script...", Info);
    outputToTerminal(QString("Job: %1, Year: %2, Month: %3").arg(jobNumber, year, month), Info);
    m_lastExecutedScript = "02 FINAL PROCESS";

    QStringList arguments;
    arguments << jobNumber << year << month;

    m_scriptRunner->runScript(scriptPath, arguments);
}

void TMHealthyController::onLockButtonClicked()
{
    if (m_lockBtn->isChecked()) {
        // User is trying to lock the job
        if (!validateJobData()) {
            m_lockBtn->setChecked(false);
            outputToTerminal("Cannot lock job: Please correct the validation errors above.", Error);
            return;
        }

        // Lock job data
        m_jobDataLocked = true;
        if (m_editBtn) m_editBtn->setChecked(false); // Auto-uncheck edit button
        outputToTerminal("Job data locked.", Success);

        // TMHEALTHY uses direct DATA folder access - no HOME folder logic needed
        outputToTerminal("Job opened - working with DATA folder: C:/Goji/TRACHMAR/HEALTHY BEGINNINGS/DATA", Info);

        // Save job state
        saveCurrentJobData();

        // Update control states and HTML display
        updateControlStates();
        updateHtmlDisplay();

        // Start auto-save timer since job is now locked/open
        emit jobOpened();
        outputToTerminal("Auto-save timer started (15 minutes)", Info);
    } else {
        // User unchecked lock button - this shouldn't happen in normal flow
        m_lockBtn->setChecked(true); // Force it back to checked
    }
}

void TMHealthyController::onEditButtonClicked()
{
    if (m_editBtn->isChecked()) {
        // User is trying to edit
        if (!m_jobDataLocked) {
            m_editBtn->setChecked(false);
            outputToTerminal("Cannot edit: job data must be locked first", Warning);
            return;
        }

        // Allow editing
        m_jobDataLocked = false;
        if (m_lockBtn) m_lockBtn->setChecked(false);
        outputToTerminal("Job data unlocked for editing", Success);
    } else {
        // User unchecked edit button
        m_editBtn->setChecked(false);
    }

    updateControlStates();
    updateHtmlDisplay();
}

void TMHealthyController::onPostageLockButtonClicked()
{
    if (!m_jobDataLocked) {
        m_postageLockBtn->setChecked(false);
        outputToTerminal("Cannot lock postage: job data must be locked first", Warning);
        return;
    }

    if (!validatePostageData()) {
        m_postageLockBtn->setChecked(false);
        outputToTerminal("Cannot lock postage: please correct validation errors", Error);
        return;
    }

    if (m_postageLockBtn->isChecked()) {
        m_postageDataLocked = true;
        outputToTerminal("Postage data locked", Success);

        // Add log entry to tracker when postage is locked
        addLogEntry();
    } else {
        m_postageDataLocked = false;
        outputToTerminal("Postage data unlocked", Info);
    }

    // Save job state whenever postage lock button is clicked
    saveJobState();
    updateControlStates();
}

// Input validation and formatting handlers
void TMHealthyController::onJobNumberChanged()
{
    if (!m_jobDataLocked) {
        saveCurrentJobData();
    }
}

void TMHealthyController::onPostageChanged()
{
    if (!m_postageDataLocked) {
        formatPostageInput();
        saveCurrentJobData();
    }
}

void TMHealthyController::onCountChanged()
{
    if (!m_postageDataLocked) {
        formatCountInput(m_countBox->text());
        saveCurrentJobData();
    }
}

void TMHealthyController::onYearChanged(const QString& year)
{
    Q_UNUSED(year)
    if (!m_jobDataLocked) {
        saveCurrentJobData();
    }
}

void TMHealthyController::onMonthChanged(const QString& month)
{
    Q_UNUSED(month)
    if (!m_jobDataLocked) {
        saveCurrentJobData();
    }
}

void TMHealthyController::onAutoSaveTimer()
{
    if (m_jobDataLocked) {
        saveJobState();
        outputToTerminal("Auto-save completed", Info);
    }
}

// Script output handling
void TMHealthyController::onScriptOutput(const QString& output)
{
    outputToTerminal(output, Info);
    parseScriptOutput(output);
}

void TMHealthyController::onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QString script = m_lastExecutedScript;

    if (exitStatus == QProcess::NormalExit) {
        if (exitCode == 0) {
            outputToTerminal(script + " completed successfully", Success);
        } else {
            outputToTerminal(script + " completed with exit code " + QString::number(exitCode), Warning);
        }
    } else {
        outputToTerminal(script + " crashed or was terminated", Error);
    }

    // Reset captured script name
    m_lastExecutedScript = "";
    m_capturingNASPath = false;
}

// Validation methods
bool TMHealthyController::validateJobData()
{
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    bool valid = true;

    // Validate job number
    if (!validateJobNumber(jobNumber)) {
        outputToTerminal("Invalid job number: must be exactly 5 digits", Error);
        valid = false;
    }

    // Validate year
    if (year.isEmpty()) {
        outputToTerminal("Year must be selected", Error);
        valid = false;
    }

    // Validate month
    if (!validateMonthSelection(month)) {
        outputToTerminal("Invalid month selection", Error);
        valid = false;
    }

    return valid;
}

bool TMHealthyController::validatePostageData()
{
    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";

    bool valid = true;

    // Validate postage
    if (postage.isEmpty()) {
        outputToTerminal("Postage cannot be empty", Error);
        valid = false;
    } else {
        QString cleanPostage = postage;
        cleanPostage.remove('$').remove(',');
        bool ok;
        double postageValue = cleanPostage.toDouble(&ok);
        if (!ok || postageValue <= 0) {
            outputToTerminal("Postage must be a valid positive number", Error);
            valid = false;
        }
    }

    // Validate count
    if (count.isEmpty()) {
        outputToTerminal("Count cannot be empty", Error);
        valid = false;
    } else {
        QString cleanCount = count;
        cleanCount.remove(',');
        bool ok;
        int countValue = cleanCount.toInt(&ok);
        if (!ok || countValue <= 0) {
            outputToTerminal("Count must be a valid positive integer", Error);
            valid = false;
        }
    }

    return valid;
}

bool TMHealthyController::validateJobNumber(const QString& jobNumber)
{
    return jobNumber.length() == 5 && jobNumber.toInt() > 0;
}

bool TMHealthyController::validateMonthSelection(const QString& month)
{
    if (month.isEmpty()) return false;
    bool ok;
    int monthNum = month.toInt(&ok);
    return ok && monthNum >= 1 && monthNum <= 12;
}

QString TMHealthyController::getJobDescription() const
{
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString monthAbbrev = convertMonthToAbbreviation(month);

    if (!monthAbbrev.isEmpty()) {
        return QString("TM %1 HEALTHY BEGINNINGS").arg(monthAbbrev);
    }
    return "TM HEALTHY BEGINNINGS";
}

// Formatting methods
void TMHealthyController::formatPostageInput()
{
    if (!m_postageBox) return;

    QString text = m_postageBox->text();
    QString cleanText = text;

    // Remove non-numeric characters except decimal point
    cleanText.remove(QRegularExpression("[^0-9.]"));

    // Convert to double and back to ensure proper formatting
    bool ok;
    double value = cleanText.toDouble(&ok);
    if (ok && value >= 0) {
        QString formatted = QString("$%L1").arg(value, 0, 'f', 2);

        QSignalBlocker blocker(m_postageBox);
        m_postageBox->setText(formatted);
    }
}

void TMHealthyController::formatCountInput(const QString& text)
{
    if (!m_countBox) return;

    QString cleanText = text;
    cleanText.remove(QRegularExpression("[^0-9]"));

    bool ok;
    qlonglong value = cleanText.toLongLong(&ok);
    if (ok && value >= 0) {
        QString formatted = QString("%L1").arg(value);

        QSignalBlocker blocker(m_countBox);
        m_countBox->setText(formatted);
    }
}

// Database and state management
void TMHealthyController::saveJobState()
{
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty()) {
        return;
    }

    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";

    // For now, just output to terminal since TMHealthyDBManager methods need to be implemented
    outputToTerminal("Job state save requested (DB implementation needed)", Info);
}

void TMHealthyController::loadJobState()
{
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty()) {
        return;
    }

    // For now, set defaults since TMHealthyDBManager methods need to be implemented
    m_currentHtmlState = DefaultState;
    m_jobDataLocked = false;
    m_postageDataLocked = false;
    m_lastExecutedScript = "";
    updateControlStates();
    updateHtmlDisplay();
    outputToTerminal("No saved job state found, using defaults", Info);
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

    // For now, just output to terminal since TMHealthyDBManager methods need to be implemented
    outputToTerminal("Job save to database requested (DB implementation needed)", Info);
}

void TMHealthyController::saveCurrentJobData()
{
    if (m_jobDataLocked) {
        saveJobState();
        saveJobToDatabase();
    }
}

bool TMHealthyController::loadJob(const QString& year, const QString& month)
{
    if (!m_tmHealthyDBManager) return false;

    // For now, simulate loading since TMHealthyDBManager methods need to be implemented
    QString jobNumber = "12345"; // Placeholder

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

    // If job data is locked, handle auto-save
    if (m_jobDataLocked) {
        outputToTerminal("Job opened - working with DATA folder: C:/Goji/TRACHMAR/HEALTHY BEGINNINGS/DATA", Info);

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

void TMHealthyController::addLogEntry()
{
    if (!m_tmHealthyDBManager) {
        outputToTerminal("Database manager not available for log entry", Error);
        return;
    }

    // Get current data from UI
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";

    if (jobNumber.isEmpty() || postage.isEmpty() || count.isEmpty()) {
        outputToTerminal("Cannot add log entry: missing job data", Warning);
        return;
    }

    QString description = getJobDescription();

    // Format count for display (add thousand separators)
    QString cleanCount = count;
    cleanCount.remove(',');
    bool countOk;
    qlonglong countValue = cleanCount.toLongLong(&countOk);
    QString formattedCount = countOk ? QString("%L1").arg(countValue) : count;

    // Calculate per piece rate
    QString perPieceStr = calculatePerPiece(postage, count);

    // Static values for HEALTHY BEGINNINGS
    QString mailClass = "STD";
    QString shape = "LTR";
    QString permit = "NKLN";

    // Get current date
    QDateTime now = QDateTime::currentDateTime();
    QString date = now.toString("M/d/yyyy");

    // For now, just output to terminal since TMHealthyDBManager addLogEntry method signature needs to be checked
    outputToTerminal("Log entry requested: " + jobNumber + " - " + description, Info);
    outputToTerminal("Postage: " + postage + ", Count: " + formattedCount + ", Rate: " + perPieceStr, Info);

    // Force refresh the table view
    refreshTrackerTable();
}

QString TMHealthyController::calculatePerPiece(const QString& postage, const QString& count) const
{
    // Clean postage string
    QString cleanPostage = postage;
    cleanPostage.remove(').remove(',');

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
        outputToTerminal("Tracker table refreshed", Info);
    }
}

void TMHealthyController::debugCheckTables()
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        outputToTerminal("DEBUG: Database not initialized", Warning);
        return;
    }

    QSqlQuery query(m_dbManager->getDatabase());

    // Check if tmhealthy_log table exists
    query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='tmhealthy_log'");
    if (query.next()) {
        outputToTerminal("DEBUG: tmhealthy_log table exists", Info);
    } else {
        outputToTerminal("DEBUG: tmhealthy_log table NOT found", Warning);
    }

    // Check if tmhealthy_jobs table exists
    query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='tmhealthy_jobs'");
    if (query.next()) {
        outputToTerminal("DEBUG: tmhealthy_jobs table exists", Info);
    } else {
        outputToTerminal("DEBUG: tmhealthy_jobs table NOT found", Warning);
    }
}

// Script output processing
void TMHealthyController::parseScriptOutput(const QString& output)
{
    // Look for network path markers from the final script
    if (output.contains("=== NAS_FOLDER_PATH ===")) {
        m_capturingNASPath = true;
        return;
    }

    if (m_capturingNASPath && output.contains("=== END_NAS_FOLDER_PATH ===")) {
        m_capturingNASPath = false;
        if (!m_capturedNASPath.isEmpty()) {
            showNASLinkDialog(m_capturedNASPath);
            m_capturedNASPath.clear();
        }
        return;
    }

    if (m_capturingNASPath) {
        QString line = output.trimmed();
        if (!line.isEmpty() && !line.startsWith("===")) {
            m_capturedNASPath = line;
        }
    }
}

void TMHealthyController::showNASLinkDialog(const QString& nasPath)
{
    NASLinkDialog* dialog = new NASLinkDialog(nasPath, nullptr);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->exec();
}

// Table context menu and copying
void TMHealthyController::showTableContextMenu(const QPoint& pos)
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

QString TMHealthyController::copyFormattedRow()
{
    return BaseTrackerController::copyFormattedRow(); // Call inherited method
}

bool TMHealthyController::createExcelAndCopy(const QStringList& headers, const QStringList& rowData)
{
    return BaseTrackerController::createExcelAndCopy(headers, rowData);
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
    // Same headers as TMTERM except using AVG RATE instead of PER PIECE
    return {"JOB", "DESCRIPTION", "POSTAGE", "COUNT", "AVG RATE", "CLASS", "SHAPE", "PERMIT"};
}

QList<int> TMHealthyController::getVisibleColumns() const
{
    return {1, 2, 3, 4, 5, 6, 7, 8}; // Skip column 0 (ID), exclude date column
}

QString TMHealthyController::formatCellData(int columnIndex, const QString& cellData) const
{
    // Format POSTAGE column to include $ symbol and thousand separators
    if (columnIndex == 3 && !cellData.isEmpty()) {
        QString cleanData = cellData;
        if (cleanData.startsWith("$")) cleanData.remove(0, 1);
        cleanData.remove(',');
        bool ok;
        double val = cleanData.toDouble(&ok);
        if (ok) {
            return QString("$%L1").arg(val, 0, 'f', 2);
        } else {
            return cellData;
        }
    }

    // Format COUNT column to include thousand separators
    if (columnIndex == 4 && !cellData.isEmpty()) {
        QString cleanData = cellData;
        cleanData.remove(',');
        bool ok;
        qlonglong val = cleanData.toLongLong(&ok);
        if (ok) {
            return QString("%L1").arg(val);
        } else {
            return cellData;
        }
    }

    return cellData;
}

void TMHealthyController::resetToDefaults()
{
    // Clear input fields
    if (m_jobNumberBox) m_jobNumberBox->clear();
    if (m_postageBox) m_postageBox->clear();
    if (m_countBox) m_countBox->clear();

    // Reset dropdowns to blank
    if (m_yearDDbox) m_yearDDbox->setCurrentIndex(0);
    if (m_monthDDbox) m_monthDDbox->setCurrentIndex(0);

    // Reset lock states
    m_jobDataLocked = false;
    m_postageDataLocked = false;

    // Update UI
    updateControlStates();
    updateHtmlDisplay();

    outputToTerminal("Reset to defaults", Info);
}

void TMHealthyController::setTextBrowser(QTextBrowser* textBrowser)
{
    m_textBrowser = textBrowser;
    updateHtmlDisplay();
}
