#include "tmweeklypccontroller.h"
#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QSignalBlocker>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QMenu>
#include <QAction>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QMimeData>
#include <QClipboard>
#include <QApplication>
#include <QHeaderView>
#include <QDateTime>
#include <QFontMetrics>
#include <QFile>
#include <QStandardPaths>
#include <QTimer>
#include "logger.h"
#include "validator.h"

TMWeeklyPCController::TMWeeklyPCController(QObject *parent)
    : QObject(parent),
    m_dbManager(nullptr),
    m_tmWeeklyPCDBManager(nullptr),
    m_scriptRunner(nullptr),
    m_fileManager(nullptr),
    m_jobDataLocked(false),
    m_postageDataLocked(false),
    m_currentHtmlState(DefaultState),
    m_capturedNASPath(),
    m_capturingNASPath(false)
{
    Logger::instance().info("Initializing TMWeeklyPCController...");

    // Get the database managers
    m_dbManager = DatabaseManager::instance();
    if (!m_dbManager) {
        Logger::instance().error("Failed to get DatabaseManager instance");
        return;
    }

    // Verify core database is initialized
    if (!m_dbManager->isInitialized()) {
        Logger::instance().error("Core DatabaseManager not initialized");
        return;
    }

    // Get TM Weekly PC database manager
    m_tmWeeklyPCDBManager = TMWeeklyPCDBManager::instance();
    if (!m_tmWeeklyPCDBManager) {
        Logger::instance().error("Failed to get TMWeeklyPCDBManager instance");
        return;
    }

    // Initialize the TM Weekly PC database manager
    if (!m_tmWeeklyPCDBManager->initialize()) {
        Logger::instance().error("Failed to initialize TMWeeklyPCDBManager");
        // Continue anyway - some functionality may still work
    } else {
        Logger::instance().info("TMWeeklyPCDBManager initialized successfully");
    }

    // Create a script runner
    m_scriptRunner = new ScriptRunner(this);

    // Get file manager - direct creation instead of using the factory
    // Use a new QSettings instance since DatabaseManager doesn't provide getSettings
    m_fileManager = new TMWeeklyPCFileManager(new QSettings(QSettings::IniFormat, QSettings::UserScope, "GojiApp", "Goji"));

    // Setup the model for the tracker table
    if (m_dbManager && m_dbManager->isInitialized()) {
        m_trackerModel = new QSqlTableModel(this, m_dbManager->getDatabase());
        m_trackerModel->setTable("tm_weekly_log");
        m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
        m_trackerModel->select();
    } else {
        Logger::instance().warning("Cannot setup tracker model - database not available");
        m_trackerModel = nullptr;
    }

    // Create base directories if they don't exist
    createBaseDirectories();

    Logger::instance().info("TMWeeklyPCController initialization complete");
}

TMWeeklyPCController::~TMWeeklyPCController()
{
    // Clean up settings if we created it
    if (m_fileManager && m_fileManager->getSettings()) {
        delete m_fileManager->getSettings();
    }

    // Clean up the model
    if (m_trackerModel) {
        delete m_trackerModel;
        m_trackerModel = nullptr;
    }

    // UI elements are not owned by this class, so don't delete them

    Logger::instance().info("TMWeeklyPCController destroyed");
}

void TMWeeklyPCController::setupOptimizedTableLayout()
{
    if (!m_tracker) return;

    // Calculate optimal font size and column widths
    const int tableWidth = 611; // Fixed widget width from UI
    const int borderWidth = 2;   // Account for table borders
    const int availableWidth = tableWidth - borderWidth;

    // Define maximum content widths based on your specifications
    struct ColumnSpec {
        QString header;
        QString maxContent;
        int minWidth;
    };

    QList<ColumnSpec> columns = {
        {"JOB", "88888", 55},           // Increased width for JOB
        {"DESCRIPTION", "TM WEEKLY 88.88", 150}, // Increased width for DESCRIPTION (was 130)
        {"POSTAGE", "$888.88", 55},     // Max $XXX.XX
        {"COUNT", "8,888", 40},         // Reduced by 10% (was 45)
        {"AVG RATE", "0.888", 45},      // Keep same
        {"CLASS", "STD", 32},           // Reduced by 10% (was 35)
        {"SHAPE", "LTR", 32},           // Reduced by 10% (was 35)
        {"PERMIT", "METER", 45}         // Changed from METERED
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

    // Set custom headers - ONLY for visible columns
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

void TMWeeklyPCController::initializeUI(
    QPushButton* runInitialBtn, QPushButton* openBulkMailerBtn,
    QPushButton* runProofDataBtn, QPushButton* openProofFileBtn,
    QPushButton* runWeeklyMergedBtn, QPushButton* openPrintFileBtn,
    QPushButton* runPostPrintBtn, QToolButton* lockBtn, QToolButton* editBtn,
    QToolButton* postageLockBtn, QComboBox* proofDDbox, QComboBox* printDDbox,
    QComboBox* yearDDbox, QComboBox* monthDDbox, QComboBox* weekDDbox,
    QComboBox* classDDbox, QComboBox* permitDDbox, QLineEdit* jobNumberBox,
    QLineEdit* postageBox, QLineEdit* countBox, QTextEdit* terminalWindow,
    QTableView* tracker, QTextBrowser* textBrowser, QCheckBox* proofApprovalCheckBox)
{
    Logger::instance().info("Initializing TM WEEKLY PC UI elements");

    // Store UI element pointers
    m_runInitialBtn = runInitialBtn;
    m_openBulkMailerBtn = openBulkMailerBtn;
    m_runProofDataBtn = runProofDataBtn;
    m_openProofFileBtn = openProofFileBtn;
    m_runWeeklyMergedBtn = runWeeklyMergedBtn;
    m_openPrintFileBtn = openPrintFileBtn;
    m_runPostPrintBtn = runPostPrintBtn;
    m_lockBtn = lockBtn;
    m_editBtn = editBtn;
    m_postageLockBtn = postageLockBtn;
    m_proofDDbox = proofDDbox;
    m_printDDbox = printDDbox;
    m_yearDDbox = yearDDbox;
    m_monthDDbox = monthDDbox;
    m_weekDDbox = weekDDbox;
    m_classDDbox = classDDbox;
    m_permitDDbox = permitDDbox;
    m_jobNumberBox = jobNumberBox;
    m_postageBox = postageBox;
    m_countBox = countBox;
    m_terminalWindow = terminalWindow;
    m_tracker = tracker;
    m_textBrowser = textBrowser;
    m_proofApprovalCheckBox = proofApprovalCheckBox;

    // Setup tracker table with optimized layout
    if (m_tracker) {
        m_tracker->setModel(m_trackerModel);
        m_tracker->setEditTriggers(QAbstractItemView::NoEditTriggers); // Read-only

        // Setup optimized table layout
        setupOptimizedTableLayout();

        // Connect contextual menu for copying
        m_tracker->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_tracker, &QTableView::customContextMenuRequested, this,
                &TMWeeklyPCController::showTableContextMenu);
    }

    // Connect UI signals to slots
    connectSignals();

    // Setup initial UI state
    setupInitialUIState();

    // Populate dropdowns
    populateDropdowns();

    // Initialize HTML display with default state
    updateHtmlDisplay();

    Logger::instance().info("TM WEEKLY PC UI initialization complete");
}

void TMWeeklyPCController::connectSignals()
{
    // Connect buttons
    connect(m_runInitialBtn, &QPushButton::clicked, this, &TMWeeklyPCController::onRunInitialClicked);
    connect(m_openBulkMailerBtn, &QPushButton::clicked, this, &TMWeeklyPCController::onOpenBulkMailerClicked);
    connect(m_runProofDataBtn, &QPushButton::clicked, this, &TMWeeklyPCController::onRunProofDataClicked);
    connect(m_openProofFileBtn, &QPushButton::clicked, this, &TMWeeklyPCController::onOpenProofFileClicked);
    connect(m_runWeeklyMergedBtn, &QPushButton::clicked, this, &TMWeeklyPCController::onRunWeeklyMergedClicked);
    connect(m_openPrintFileBtn, &QPushButton::clicked, this, &TMWeeklyPCController::onOpenPrintFileClicked);
    connect(m_runPostPrintBtn, &QPushButton::clicked, this, &TMWeeklyPCController::onRunPostPrintClicked);

    // Connect toggle buttons
    connect(m_lockBtn, &QToolButton::clicked, this, &TMWeeklyPCController::onLockButtonClicked);
    connect(m_editBtn, &QToolButton::clicked, this, &TMWeeklyPCController::onEditButtonClicked);
    connect(m_postageLockBtn, &QToolButton::clicked, this, &TMWeeklyPCController::onPostageLockButtonClicked);

    // Connect checkbox
    connect(m_proofApprovalCheckBox, &QCheckBox::toggled, this, &TMWeeklyPCController::onProofApprovalChanged);

    // Connect dropdowns
    connect(m_yearDDbox, &QComboBox::currentTextChanged, this, &TMWeeklyPCController::onYearChanged);
    connect(m_monthDDbox, &QComboBox::currentTextChanged, this, &TMWeeklyPCController::onMonthChanged);
    connect(m_classDDbox, &QComboBox::currentTextChanged, this, &TMWeeklyPCController::onClassChanged);

    // Connect fields for automatic meter postage calculation
    connect(m_postageBox, &QLineEdit::textChanged, this, &TMWeeklyPCController::calculateMeterPostage);
    connect(m_countBox, &QLineEdit::textChanged, this, &TMWeeklyPCController::calculateMeterPostage);
    connect(m_classDDbox, &QComboBox::currentTextChanged, this, &TMWeeklyPCController::calculateMeterPostage);
    connect(m_permitDDbox, &QComboBox::currentTextChanged, this, &TMWeeklyPCController::calculateMeterPostage);

    // Connect script runner signals
    connect(m_scriptRunner, &ScriptRunner::scriptOutput, this, &TMWeeklyPCController::onScriptOutput);
    connect(m_scriptRunner, &ScriptRunner::scriptFinished, this, &TMWeeklyPCController::onScriptFinished);

    // FIXED: Connect postage fields to auto-save
    connect(m_postageBox, &QLineEdit::textChanged, this, [this]() {
        if (m_jobDataLocked) {
            savePostageData();
        }
    });

    connect(m_countBox, &QLineEdit::textChanged, this, [this]() {
        if (m_jobDataLocked) {
            savePostageData();
        }
    });

    connect(m_classDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, [this]() {
                if (m_jobDataLocked) {
                    savePostageData();
                }
            });

    connect(m_permitDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, [this]() {
                if (m_jobDataLocked) {
                    savePostageData();
                }
            });
}

void TMWeeklyPCController::setupInitialUIState()
{
    // Initialize dropdown contents
    if (m_proofDDbox) {
        m_proofDDbox->clear();
        m_proofDDbox->addItem("");
        m_proofDDbox->addItem("SORTED");
        m_proofDDbox->addItem("UNSORTED");
    }

    if (m_printDDbox) {
        m_printDDbox->clear();
        m_printDDbox->addItem("");
        m_printDDbox->addItem("SORTED");
        m_printDDbox->addItem("UNSORTED");
    }

    if (m_classDDbox) {
        m_classDDbox->clear();
        m_classDDbox->addItem("");
        m_classDDbox->addItem("STANDARD");
        m_classDDbox->addItem("FIRST CLASS");
    }

    if (m_permitDDbox) {
        m_permitDDbox->clear();
        m_permitDDbox->addItem("");
        m_permitDDbox->addItem("1662");
        m_permitDDbox->addItem("METER");  // Changed from METERED
    }

    // Set validators for input fields
    if (m_postageBox) {
        QRegularExpressionValidator* validator = new QRegularExpressionValidator(QRegularExpression("[0-9]*\\.?[0-9]*"), this);
        m_postageBox->setValidator(validator);
        connect(m_postageBox, &QLineEdit::editingFinished, this, &TMWeeklyPCController::formatPostageInput);
    }

    // Set all control states based on current job
    updateControlStates();
}

void TMWeeklyPCController::populateDropdowns()
{
    // Populate year dropdown
    if (m_yearDDbox) {
        m_yearDDbox->clear();
        m_yearDDbox->addItem("");

        // Use QDateTime instead of QDate::currentDateTime
        const int currentYear = QDateTime::currentDateTime().date().year();
        m_yearDDbox->addItem(QString::number(currentYear - 1));
        m_yearDDbox->addItem(QString::number(currentYear));
        m_yearDDbox->addItem(QString::number(currentYear + 1));
    }

    // Populate month dropdown
    if (m_monthDDbox) {
        m_monthDDbox->clear();
        m_monthDDbox->addItem("");

        for (int i = 1; i <= 12; i++) {
            m_monthDDbox->addItem(QString("%1").arg(i, 2, 10, QChar('0')));
        }
    }

    // Week dropdown will be populated when month is selected
}

void TMWeeklyPCController::populateWeekDDbox()
{
    if (!m_weekDDbox || !m_monthDDbox || !m_yearDDbox) {
        Logger::instance().error("Cannot populate week dropdown - UI elements not initialized");
        return;
    }

    // Clear the week dropdown
    m_weekDDbox->clear();
    m_weekDDbox->addItem("");

    // Get selected year and month
    QString yearStr = m_yearDDbox->currentText();
    QString monthStr = m_monthDDbox->currentText();

    if (yearStr.isEmpty() || monthStr.isEmpty()) {
        return;
    }

    int year = yearStr.toInt();
    int month = monthStr.toInt();

    // Get all Wednesdays in this month
    QDate date(year, month, 1);
    QList<int> wednesdayDates;

    // Find first Wednesday
    while (date.dayOfWeek() != 3) { // Qt::Wednesday = 3
        date = date.addDays(1);
        if (date.month() != month) {
            // No Wednesdays in this month (should never happen)
            return;
        }
    }

    // Add all Wednesdays in this month
    while (date.month() == month) {
        wednesdayDates.append(date.day());
        date = date.addDays(7);
    }

    // Add the Wednesday dates to the dropdown
    for (int day : wednesdayDates) {
        m_weekDDbox->addItem(QString::number(day));
    }
}

void TMWeeklyPCController::updateHtmlDisplay()
{
    if (!m_textBrowser) {
        return;
    }

    HtmlDisplayState newState = determineHtmlState();

    // Always update on first call or when state changes
    if (newState != m_currentHtmlState) {
        m_currentHtmlState = newState;

        QString resourcePath;
        switch (m_currentHtmlState) {
        case ProofState:
            resourcePath = ":/resources/tmweeklypc/proof.html";
            break;
        case PrintState:
            resourcePath = ":/resources/tmweeklypc/print.html";
            break;
        case DefaultState:
        default:
            resourcePath = ":/resources/tmweeklypc/default.html";
            break;
        }

        loadHtmlFile(resourcePath);

        // Save state if job is locked (has data)
        if (m_jobDataLocked) {
            saveJobState();
        }
    }
}

void TMWeeklyPCController::loadHtmlFile(const QString& resourcePath)
{
    if (!m_textBrowser) {
        return;
    }

    QFile file(resourcePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        QString content = stream.readAll();
        file.close();

        m_textBrowser->setHtml(content);
    } else {
        // Create fallback HTML content
        QString fallbackContent = QString(
                                      "<html><body style='font-family: Arial; padding: 20px;'>"
                                      "<h2>TM Weekly PC</h2>"
                                      "<p>HTML file could not be loaded: %1</p>"
                                      "<p>Current time: %2</p>"
                                      "</body></html>"
                                      ).arg(resourcePath, QDateTime::currentDateTime().toString());

        m_textBrowser->setHtml(fallbackContent);
    }
}

TMWeeklyPCController::HtmlDisplayState TMWeeklyPCController::determineHtmlState() const
{
    // If proof approval checkbox is checked, show print state
    if (m_proofApprovalCheckBox && m_proofApprovalCheckBox->isChecked()) {
        return PrintState;
    }

    // If job is locked (created), show proof state
    if (m_jobDataLocked) {
        return ProofState;
    }

    // Default state
    return DefaultState;
}

void TMWeeklyPCController::saveJobState()
{
    if (!m_jobDataLocked) {
        return; // Only save state for locked jobs
    }

    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString week = m_weekDDbox ? m_weekDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty() || week.isEmpty()) {
        return;
    }

    bool proofApprovalChecked = m_proofApprovalCheckBox ? m_proofApprovalCheckBox->isChecked() : false;
    int htmlDisplayState = static_cast<int>(m_currentHtmlState);

    // FIXED: Save postage lock state as well
    if (m_tmWeeklyPCDBManager->saveJobState(year, month, week, proofApprovalChecked,
                                            htmlDisplayState)) {
        outputToTerminal("Job state saved", Info);
    } else {
        outputToTerminal("Failed to save job state", Warning);
    }
}

void TMWeeklyPCController::loadJobState()
{
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString week = m_weekDDbox ? m_weekDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty() || week.isEmpty()) {
        return;
    }

    bool proofApprovalChecked;
    int htmlDisplayState;

    if (m_tmWeeklyPCDBManager->loadJobState(year, month, week, proofApprovalChecked,
                                            htmlDisplayState)) {
        // Restore proof approval checkbox
        if (m_proofApprovalCheckBox) {
            m_proofApprovalCheckBox->setChecked(proofApprovalChecked);
        }

        // Restore HTML display state
        m_currentHtmlState = static_cast<HtmlDisplayState>(htmlDisplayState);
        updateHtmlDisplay();

        outputToTerminal("Job state loaded", Info);
    }
}

void TMWeeklyPCController::onYearChanged(const QString& year)
{
    outputToTerminal("Year changed to: " + year);
}

void TMWeeklyPCController::onMonthChanged(const QString& month)
{
    outputToTerminal("Month changed to: " + month);

    // Update week dropdown with Wednesdays
    populateWeekDDbox();
}

void TMWeeklyPCController::onClassChanged(const QString& mailClass)
{
    // Auto-select permit 1662 if STANDARD is selected
    if (mailClass == "STANDARD" && m_permitDDbox) {
        m_permitDDbox->setCurrentText("1662");
    }
}

void TMWeeklyPCController::onProofApprovalChanged(bool checked)
{
    outputToTerminal(checked ? "Proof approval checked" : "Proof approval unchecked");

    // Update HTML display based on new checkbox state
    updateHtmlDisplay();
}

void TMWeeklyPCController::onLockButtonClicked()
{
    if (m_lockBtn->isChecked()) {
        if (m_editBtn->isChecked()) {
            outputToTerminal("Cannot lock while in edit mode. Use Edit button to make changes.");
            return;
        }
        // Commit changes after editing
        m_jobDataLocked = true;
        m_editBtn->setChecked(false);
        outputToTerminal("Job data updated and locked.");
        // Create folder for the job if it doesn't exist
        createJobFolder();
    } else {
        // Validate job data before locking
        if (!validateJobData()) {
            return;
        }
        // Lock job data
        m_jobDataLocked = true;
        outputToTerminal("Job data locked.");
        // Create folder for the job
        createJobFolder();
        // Save to database
        saveJobToDatabase();
    }

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
}

void TMWeeklyPCController::onEditButtonClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot edit job data until it is locked.");
        m_editBtn->setChecked(false);
        return;
    }

    if (m_editBtn->isChecked()) {
        outputToTerminal("Edit mode enabled. Make your changes and click Lock button to save.");
    } else {
        outputToTerminal("Edit mode disabled.");
    }

    updateControlStates();
}

void TMWeeklyPCController::onPostageLockButtonClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot lock postage data until job data is locked.");
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
        outputToTerminal("Postage data locked.");

        // Add log entry to tracker when postage is locked (like TMTermController does)
        addLogEntry();

        // FIXED: Save postage data persistently when locked
        savePostageData();
    } else {
        m_postageDataLocked = false;
        outputToTerminal("Postage data unlocked.");
    }

    // Save job state whenever postage lock button is clicked (includes lock state)
    saveJobState();
    updateControlStates();
}

void TMWeeklyPCController::savePostageData()
{
    if (!m_jobDataLocked) {
        return; // Only save for locked jobs
    }

    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString week = m_weekDDbox ? m_weekDDbox->currentText() : "";
    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";
    QString mailClass = m_classDDbox ? m_classDDbox->currentText() : "";
    QString permit = m_permitDDbox ? m_permitDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty() || week.isEmpty()) {
        return;
    }

    if (m_tmWeeklyPCDBManager->savePostageData(year, month, week, postage, count,
                                               mailClass, permit, m_postageDataLocked)) {
        outputToTerminal("Postage data saved persistently", Info);
    } else {
        outputToTerminal("Failed to save postage data", Warning);
    }
}

void TMWeeklyPCController::loadPostageData()
{
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString week = m_weekDDbox ? m_weekDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty() || week.isEmpty()) {
        return;
    }

    QString postage, count, mailClass, permit;
    bool postageDataLocked;

    if (m_tmWeeklyPCDBManager->loadPostageData(year, month, week, postage, count,
                                               mailClass, permit, postageDataLocked)) {
        // Load the data into UI fields
        if (m_postageBox) m_postageBox->setText(postage);
        if (m_countBox) m_countBox->setText(count);
        if (m_classDDbox) m_classDDbox->setCurrentText(mailClass);
        if (m_permitDDbox) m_permitDDbox->setCurrentText(permit);

        // Restore lock state
        m_postageDataLocked = postageDataLocked;
        if (m_postageLockBtn) m_postageLockBtn->setChecked(postageDataLocked);

        outputToTerminal("Postage data loaded from database", Info);
    }
}

void TMWeeklyPCController::onScriptStarted()
{
    outputToTerminal("Script execution started...");
}

void TMWeeklyPCController::onScriptOutput(const QString& output)
{
    // Parse output for special markers
    parseScriptOutput(output);

    // Display output in terminal
    outputToTerminal(output);
}

void TMWeeklyPCController::onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // Re-enable all buttons
    m_runInitialBtn->setEnabled(true);
    m_runProofDataBtn->setEnabled(true);
    m_runWeeklyMergedBtn->setEnabled(true);
    m_runPostPrintBtn->setEnabled(true);

    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        outputToTerminal("Script execution completed successfully.", Success);

        // FIXED: Only show NAS dialog for Post Print script
        // Do NOT add tracker entry here - that's handled in onPostageLockButtonClicked
        if (m_lastExecutedScript == "postprint" && !m_capturedNASPath.isEmpty()) {
            // Show NAS link dialog after a brief delay to let the terminal update
            QTimer::singleShot(500, this, &TMWeeklyPCController::showNASLinkDialog);
        }
    } else {
        outputToTerminal("Script execution failed with exit code: " + QString::number(exitCode), Error);
        // Clear captured path on failure
        m_capturedNASPath.clear();
    }

    // Reset script tracking
    m_lastExecutedScript.clear();
}

void TMWeeklyPCController::onRunInitialClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Please lock job data before running Initial script.", Warning);
        return;
    }

    outputToTerminal("Running Initial script...");

    // Disable the button while running
    m_runInitialBtn->setEnabled(false);

    // Get script path from file manager
    QString script = m_fileManager->getScriptPath("initial");

    // Run the script
    m_scriptRunner->runScript("python", QStringList() << script);

    // Manually call scriptStarted since we removed the signal
    onScriptStarted();
}

void TMWeeklyPCController::onOpenBulkMailerClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Please lock job data before opening Bulk Mailer.", Warning);
        return;
    }

    outputToTerminal("Opening Bulk Mailer application...");

    // Launch Bulk Mailer
    QProcess* process = new QProcess(this);
    process->startDetached("C:/Program Files (x86)/Satori Software/Bulk Mailer/BulkMailer.exe", QStringList());

    // Connect to handle process finish
    connect(process, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            process, &QProcess::deleteLater);
}

void TMWeeklyPCController::onRunProofDataClicked()
{
    if (!m_jobDataLocked || !m_postageDataLocked) {
        outputToTerminal("Please lock job data and postage data before running Proof Data script.", Warning);
        return;
    }

    outputToTerminal("Running Proof Data script...");

    // Disable the button while running
    m_runProofDataBtn->setEnabled(false);

    // Get script path from file manager
    QString script = m_fileManager->getScriptPath("proofdata");

    // Run the script
    m_scriptRunner->runScript("python", QStringList() << script);

    // Manually call scriptStarted since we removed the signal
    onScriptStarted();
}

void TMWeeklyPCController::onOpenProofFileClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Please lock job data before opening proof file.", Warning);
        return;
    }

    QString selection = m_proofDDbox->currentText();
    if (selection.isEmpty()) {
        outputToTerminal("Please select SORTED or UNSORTED from the dropdown.", Warning);
        return;
    }

    outputToTerminal("Opening " + selection + " proof file...");

    // Use file manager to open the appropriate file
    if (m_fileManager && m_fileManager->openProofFile(selection)) {
        outputToTerminal("Opened " + selection + " proof file successfully.", Success);
    } else {
        outputToTerminal("Failed to open " + selection + " proof file.", Error);
    }
}

void TMWeeklyPCController::onRunWeeklyMergedClicked()
{
    if (!m_jobDataLocked || !m_postageDataLocked) {
        outputToTerminal("Please lock job data and postage data before running Weekly Merged script.", Warning);
        return;
    }

    outputToTerminal("Running Weekly Merged script...");

    // Disable the button while running
    m_runWeeklyMergedBtn->setEnabled(false);

    // Get script path from file manager (probably "weeklymerged" or "03MERGE")
    QString script = m_fileManager->getScriptPath("weeklymerged");

    // Run the script - Python handles all the actual processing
    m_scriptRunner->runScript("python", QStringList() << script);

    // Manually call scriptStarted since we removed the signal
    onScriptStarted();
}

void TMWeeklyPCController::onOpenPrintFileClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Please lock job data before opening print file.", Warning);
        return;
    }

    QString selection = m_printDDbox->currentText();
    if (selection.isEmpty()) {
        outputToTerminal("Please select SORTED or UNSORTED from the dropdown.", Warning);
        return;
    }

    outputToTerminal("Opening " + selection + " print file...");

    // Use file manager to open the appropriate file
    if (m_fileManager && m_fileManager->openPrintFile(selection)) {
        outputToTerminal("Opened " + selection + " print file successfully.", Success);
    } else {
        outputToTerminal("Failed to open " + selection + " print file.", Error);
    }
}

void TMWeeklyPCController::onRunPostPrintClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Please lock job data before running Post Print script.", Warning);
        return;
    }

    if (!m_runPostPrintBtn->isEnabled()) {
        outputToTerminal("Post Print script is already running.", Warning);
        return;
    }

    // Disable button during execution
    m_runPostPrintBtn->setEnabled(false);

    QString jobNumber = m_jobNumberBox->text();
    QString month = m_monthDDbox->currentText();
    QString week = m_weekDDbox->currentText();
    QString year = m_yearDDbox->currentText();

    // Clear any previously captured NAS path
    m_capturedNASPath.clear();
    m_capturingNASPath = false;
    m_lastExecutedScript = "postprint";

    outputToTerminal(QString("Running Post Print script for job %1, week %2.%3, year %4...")
                         .arg(jobNumber, month, week, year));

    // FIXED: Do NOT add log entry here - post print only copies files to network
    // Tracker should only be updated when runWeeklyMergedTMWPC is clicked

    // Run the script
    QString scriptPath = QDir::currentPath() + "/scripts/04POSTPRINT.py";
    QStringList arguments;
    arguments << jobNumber << month << week << year;

    m_scriptRunner->runScript(scriptPath, arguments);
}

bool TMWeeklyPCController::validateJobData()
{
    // Check if required fields are filled
    if (m_jobNumberBox->text().isEmpty()) {
        QMessageBox::warning(nullptr, "Validation Error", "Job number cannot be empty.");
        return false;
    }

    if (m_yearDDbox->currentText().isEmpty()) {
        QMessageBox::warning(nullptr, "Validation Error", "Please select a year.");
        return false;
    }

    if (m_monthDDbox->currentText().isEmpty()) {
        QMessageBox::warning(nullptr, "Validation Error", "Please select a month.");
        return false;
    }

    if (m_weekDDbox->currentText().isEmpty()) {
        QMessageBox::warning(nullptr, "Validation Error", "Please select a week.");
        return false;
    }

    // Validate job number (5 digits)
    QString jobNumber = m_jobNumberBox->text();
    QRegularExpression jobNumberRegex("^\\d{5}$");
    if (!jobNumberRegex.match(jobNumber).hasMatch()) {
        QMessageBox::warning(nullptr, "Validation Error", "Job number must be exactly 5 digits.");
        return false;
    }

    return true;
}

bool TMWeeklyPCController::validatePostageData()
{
    // Check if required fields are filled
    if (m_postageBox->text().isEmpty()) {
        QMessageBox::warning(nullptr, "Validation Error", "Postage amount cannot be empty.");
        return false;
    }

    if (m_countBox->text().isEmpty()) {
        QMessageBox::warning(nullptr, "Validation Error", "Count cannot be empty.");
        return false;
    }

    if (m_classDDbox->currentText().isEmpty()) {
        QMessageBox::warning(nullptr, "Validation Error", "Please select a mail class.");
        return false;
    }

    if (m_permitDDbox->currentText().isEmpty()) {
        QMessageBox::warning(nullptr, "Validation Error", "Please select a permit number.");
        return false;
    }

    // Validate postage (numeric value) - remove dollar sign first
    QString postage = m_postageBox->text();
    postage.remove("$");  // Remove the dollar sign before validation
    bool ok;
    postage.toDouble(&ok);
    if (!ok) {
        QMessageBox::warning(nullptr, "Validation Error", "Postage must be a valid number.");
        return false;
    }

    // Validate count (numeric value)
    QString count = m_countBox->text();
    count.toInt(&ok);
    if (!ok) {
        QMessageBox::warning(nullptr, "Validation Error", "Count must be a valid integer.");
        return false;
    }

    return true;
}

void TMWeeklyPCController::formatPostageInput()
{
    QString text = m_postageBox->text().trimmed();
    if (text.isEmpty()) return;

    // Format as currency
    double value = text.toDouble();
    QString formatted = QString("$%1").arg(value, 0, 'f', 2);

    // Update the field
    m_postageBox->setText(formatted);
}

void TMWeeklyPCController::updateControlStates()
{
    // Job identification fields
    bool jobFieldsEditable = !m_jobDataLocked || m_editBtn->isChecked();
    m_jobNumberBox->setReadOnly(!jobFieldsEditable);
    m_yearDDbox->setEnabled(jobFieldsEditable);
    m_monthDDbox->setEnabled(jobFieldsEditable);
    m_weekDDbox->setEnabled(jobFieldsEditable);

    // Postage fields
    bool postageFieldsEditable = (!m_postageDataLocked || !m_postageLockBtn->isChecked()) && m_jobDataLocked;
    m_postageBox->setReadOnly(!postageFieldsEditable);
    m_countBox->setReadOnly(!postageFieldsEditable);
    m_classDDbox->setEnabled(postageFieldsEditable);
    m_permitDDbox->setEnabled(postageFieldsEditable);

    // Buttons
    m_editBtn->setEnabled(m_jobDataLocked);
    m_postageLockBtn->setEnabled(m_jobDataLocked);

    // Workflow buttons
    m_runInitialBtn->setEnabled(m_jobDataLocked);
    m_openBulkMailerBtn->setEnabled(m_jobDataLocked);
    m_runProofDataBtn->setEnabled(m_jobDataLocked && m_postageDataLocked);
    m_openProofFileBtn->setEnabled(m_jobDataLocked);
    m_proofDDbox->setEnabled(m_jobDataLocked);
    m_runWeeklyMergedBtn->setEnabled(m_jobDataLocked && m_postageDataLocked);
    m_openPrintFileBtn->setEnabled(m_jobDataLocked);
    m_printDDbox->setEnabled(m_jobDataLocked);
    m_runPostPrintBtn->setEnabled(m_jobDataLocked);

    // Proof approval checkbox
    m_proofApprovalCheckBox->setEnabled(m_jobDataLocked);
}

void TMWeeklyPCController::outputToTerminal(const QString& message, MessageType type)
{
    if (m_terminalWindow) {
        QString formattedMessage;

        switch (type) {
        case Error:
            formattedMessage = QString("<span style='color:#ff5555;'>%1</span>").arg(message);
            break;
        case Warning:
            formattedMessage = QString("<span style='color:#ffff55;'>%1</span>").arg(message);
            break;
        case Success:
            formattedMessage = QString("<span style='color:#55ff55;'>%1</span>").arg(message);
            break;
        case Info:
        default:
            formattedMessage = message; // Default white color
            break;
        }

        m_terminalWindow->append(formattedMessage);
        m_terminalWindow->ensureCursorVisible();
    }

    // Also log to the logger
    Logger::instance().info(message);
}

void TMWeeklyPCController::createBaseDirectories()
{
    if (m_fileManager) {
        m_fileManager->createBaseDirectories();
    }
}

void TMWeeklyPCController::createJobFolder()
{
    QString month = m_monthDDbox->currentText();
    QString week = m_weekDDbox->currentText();

    if (month.isEmpty() || week.isEmpty()) {
        outputToTerminal("Cannot create job folder: month or week is empty", Warning);
        return;
    }

    if (m_fileManager && m_fileManager->createJobFolder(month, week)) {
        outputToTerminal("Created job folder: " + m_fileManager->getJobFolderPath(month, week), Success);
    } else {
        outputToTerminal("Failed to create job folder", Error);
    }
}

void TMWeeklyPCController::saveJobToDatabase()
{
    QString jobNumber = m_jobNumberBox->text();
    QString year = m_yearDDbox->currentText();
    QString month = m_monthDDbox->currentText();
    QString week = m_weekDDbox->currentText();

    if (m_tmWeeklyPCDBManager->saveJob(jobNumber, year, month, week)) {
        outputToTerminal("Job saved to database successfully", Success);
    } else {
        outputToTerminal("Failed to save job to database", Error);
    }
}

bool TMWeeklyPCController::loadJob(const QString& year, const QString& month, const QString& week)
{
    QString jobNumber;
    if (!m_tmWeeklyPCDBManager->loadJob(year, month, week, jobNumber)) {
        outputToTerminal("Job not found in database", Warning);
        return false;
    }

    // Load job data into UI
    m_jobNumberBox->setText(jobNumber);
    m_yearDDbox->setCurrentText(year);
    m_monthDDbox->setCurrentText(month);
    populateWeekDDbox();
    m_weekDDbox->setCurrentText(week);

    // Set job as locked
    m_jobDataLocked = true;
    m_lockBtn->setChecked(true);

    // Load job state (checkbox and HTML display state)
    loadJobState();

    // FIXED: Also load postage data when loading job
    loadPostageData();

    // Update control states
    updateControlStates();

    outputToTerminal("Loaded job: " + jobNumber + " for " + year + "-" + month + "-" + week, Success);
    return true;
}

void TMWeeklyPCController::addLogEntry()
{
    // Get values from UI
    QString jobNumber = m_jobNumberBox->text();
    QString description = "TM WEEKLY " + m_monthDDbox->currentText() + "." + m_weekDDbox->currentText();
    QString postage = m_postageBox->text();
    QString count = m_countBox->text();
    QString mailClass = m_classDDbox->currentText();
    QString permit = m_permitDDbox->currentText();

    // Format count with comma if >= 1000
    int countValue = count.toInt();
    QString formattedCount = (countValue >= 1000) ?
                                 QString("%L1").arg(countValue) : QString::number(countValue);

    // Calculate per piece rate with exactly 3 decimal places and leading zero
    double postageAmount = postage.remove("$").toDouble();
    double perPiece = (countValue > 0) ? (postageAmount / countValue) : 0.0;
    QString perPieceStr = QString("0.%1").arg(QString::number(perPiece * 1000, 'f', 0).rightJustified(3, '0'));

    // Convert CLASS to abbreviated form
    QString classAbbrev = (mailClass == "STANDARD") ? "STD" :
                              (mailClass == "FIRST CLASS") ? "FC" : mailClass;

    // Convert PERMIT to shortened form
    QString permitShort = (permit == "METER") ? "METER" : permit;  // Already correct

    // Static shape value
    QString shape = "LTR";

    // Get current date
    QDateTime now = QDateTime::currentDateTime();
    QString date = now.toString("M/d/yyyy");

    // Add to database using the tab-specific manager
    if (m_tmWeeklyPCDBManager->addLogEntry(jobNumber, description, postage, formattedCount,
                                           perPieceStr, classAbbrev, shape, permitShort, date)) {
        outputToTerminal("Added log entry to database", Success);

        // Force refresh the table view
        refreshTrackerTable();
    } else {
        outputToTerminal("Failed to add log entry to database", Error);
    }
}

void TMWeeklyPCController::refreshTrackerTable()
{
    if (m_trackerModel) {
        m_trackerModel->select();
        outputToTerminal("Tracker table refreshed", Info);
    }
}

// Replace your copyFormattedRow() function with this simple, reliable version
QString TMWeeklyPCController::copyFormattedRow()
{
    if (!m_tracker) {
        return "Table view not available";
    }

    QModelIndex index = m_tracker->currentIndex();
    if (!index.isValid()) {
        return "No row selected";
    }

    // Get the row number
    int row = index.row();

    // Define the visible columns we want to copy (excluding hidden ones)
    QList<int> visibleColumns = {1, 2, 3, 4, 5, 6, 7, 8}; // Skip column 0 (ID) and any after 8
    QStringList headers = {"JOB", "DESCRIPTION", "POSTAGE", "COUNT", "AVG RATE", "CLASS", "SHAPE", "PERMIT"};

    // Collect data from the selected row
    QStringList rowData;
    for (int i = 0; i < visibleColumns.size(); i++) {
        int sourceCol = visibleColumns[i];
        QString cellData = m_trackerModel->data(m_trackerModel->index(row, sourceCol)).toString();

        // Format POSTAGE column to include $ symbol if it doesn't have one
        if (i == 2 && !cellData.isEmpty() && !cellData.startsWith("$")) { // POSTAGE column
            cellData = "$" + cellData;
        }

        rowData.append(cellData);
    }

    // Create Excel file using PowerShell and copy it
    if (createExcelAndCopy(headers, rowData)) {
        outputToTerminal("Copied row to clipboard with Excel formatting", Success);
        return "Row copied to clipboard";
    } else {
        outputToTerminal("Failed to copy row with Excel formatting", Error);
        return "Copy failed";
    }
}

// Simple method that creates Excel file and copies it in one step
bool TMWeeklyPCController::createExcelAndCopy(const QStringList& headers, const QStringList& rowData)
{
#ifdef Q_OS_WIN
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString tempFileName = QDir(tempDir).filePath("goji_temp_copy.xlsx");
    QString scriptPath = QDir(tempDir).filePath("goji_excel_script.ps1");

    // Remove existing files
    QFile::remove(tempFileName);
    QFile::remove(scriptPath);

    // Create PowerShell script that creates Excel file and opens it for copying
    QString script = "try {\n";
    script += "  $excel = New-Object -ComObject Excel.Application\n";
    script += "  $excel.Visible = $false\n";
    script += "  $excel.DisplayAlerts = $false\n";
    script += "  $workbook = $excel.Workbooks.Add()\n";
    script += "  $sheet = $workbook.ActiveSheet\n";

    // Add headers with styling
    for (int i = 0; i < headers.size(); i++) {
        script += QString("  $sheet.Cells(%1,%2) = '%3'\n").arg(1).arg(i + 1).arg(headers[i]);
        script += QString("  $sheet.Cells(%1,%2).Font.Bold = $true\n").arg(1).arg(i + 1);
        script += QString("  $sheet.Cells(%1,%2).Interior.Color = 14737632\n").arg(1).arg(i + 1);
    }

    // Add data with formatting
    for (int i = 0; i < rowData.size(); i++) {
        QString cellValue = rowData[i];
        cellValue.replace("'", "''"); // Escape single quotes
        script += QString("  $sheet.Cells(%1,%2) = '%3'\n").arg(2).arg(i + 1).arg(cellValue);

        // Format specific columns
        if (i == 2) { // POSTAGE - currency format
            script += QString("  $sheet.Cells(%1,%2).NumberFormat = '$#,##0.00'\n").arg(2).arg(i + 1);
            script += QString("  $sheet.Cells(%1,%2).HorizontalAlignment = -4152\n").arg(2).arg(i + 1);
        } else if (i == 3) { // COUNT - number format
            script += QString("  $sheet.Cells(%1,%2).NumberFormat = '#,##0'\n").arg(2).arg(i + 1);
            script += QString("  $sheet.Cells(%1,%2).HorizontalAlignment = -4152\n").arg(2).arg(i + 1);
        } else if (i == 4) { // AVG RATE - decimal format
            script += QString("  $sheet.Cells(%1,%2).NumberFormat = '0.000'\n").arg(2).arg(i + 1);
            script += QString("  $sheet.Cells(%1,%2).HorizontalAlignment = -4152\n").arg(2).arg(i + 1);
        }
    }

    // Add borders and formatting
    script += QString("  $range = $sheet.Range('A1:%1%2')\n").arg(QChar('A' + (char)(headers.size() - 1))).arg(2);
    script += "  $range.Borders.LineStyle = 1\n";
    script += "  $range.Borders.Weight = 2\n";
    script += "  $range.Borders.Color = 0\n";

    // Set column widths
    script += "  $sheet.Columns.Item(1).ColumnWidth = 8\n";   // JOB
    script += "  $sheet.Columns.Item(2).ColumnWidth = 20\n";  // DESCRIPTION
    script += "  $sheet.Columns.Item(3).ColumnWidth = 10\n";  // POSTAGE
    script += "  $sheet.Columns.Item(4).ColumnWidth = 8\n";   // COUNT
    script += "  $sheet.Columns.Item(5).ColumnWidth = 10\n";  // AVG RATE
    script += "  $sheet.Columns.Item(6).ColumnWidth = 6\n";   // CLASS
    script += "  $sheet.Columns.Item(7).ColumnWidth = 6\n";   // SHAPE
    script += "  $sheet.Columns.Item(8).ColumnWidth = 8\n";   // PERMIT

    // Save file and make Excel visible for copying
    script += QString("  $workbook.SaveAs('%1')\n").arg(tempFileName.replace('/', '\\'));
    script += "  $range.Select()\n";
    script += "  $range.Copy()\n";

    // Important: Keep Excel open briefly so clipboard data persists
    script += "  Start-Sleep -Seconds 1\n";
    script += "  $workbook.Close($false)\n";
    script += "  $excel.Quit()\n";

    // Clean up COM objects
    script += "  [System.Runtime.Interopservices.Marshal]::ReleaseComObject($range) | Out-Null\n";
    script += "  [System.Runtime.Interopservices.Marshal]::ReleaseComObject($sheet) | Out-Null\n";
    script += "  [System.Runtime.Interopservices.Marshal]::ReleaseComObject($workbook) | Out-Null\n";
    script += "  [System.Runtime.Interopservices.Marshal]::ReleaseComObject($excel) | Out-Null\n";
    script += "  [System.GC]::Collect()\n";

    script += "  Write-Output 'SUCCESS'\n";
    script += "} catch {\n";
    script += "  Write-Output \"ERROR: $_\"\n";
    script += "}\n";

    // Write script to file
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&scriptFile);
    out << script;
    scriptFile.close();

    // Execute PowerShell script
    QProcess process;
    QStringList arguments;
    arguments << "-ExecutionPolicy" << "Bypass" << "-NoProfile" << "-File" << scriptPath;

    process.start("powershell.exe", arguments);
    process.waitForFinished(15000); // 15 second timeout

    QString output = process.readAllStandardOutput();
    QString errorOutput = process.readAllStandardError();

    // Cleanup script file
    QFile::remove(scriptPath);

    // Cleanup Excel file after a delay
    QTimer::singleShot(5000, [tempFileName]() {
        QFile::remove(tempFileName);
    });

    if (output.contains("SUCCESS")) {
        return true;
    } else {
        outputToTerminal(QString("PowerShell error: %1 %2").arg(output).arg(errorOutput), Error);
        return false;
    }
#else
    // Fallback for non-Windows: create simple TSV
    QString tsv = headers.join("\t") + "\n" + rowData.join("\t");
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(tsv);
    return true;
#endif
}

void TMWeeklyPCController::showTableContextMenu(const QPoint& pos)
{
    QMenu menu(m_tracker);
    QAction* copyAction = menu.addAction("Copy Selected Row");
    QAction* selectedAction = menu.exec(m_tracker->mapToGlobal(pos));
    if (selectedAction == copyAction) {
        copyFormattedRow();
    }
}

void TMWeeklyPCController::parseScriptOutput(const QString& output)
{
    // Look for output path markers from the Post Print script
    if (output.contains("=== OUTPUT_PATH ===")) {
        m_capturingNASPath = true;
        return;
    }

    if (output.contains("=== END_OUTPUT_PATH ===")) {
        m_capturingNASPath = false;
        return;
    }

    // Capture the NAS path
    if (m_capturingNASPath && !output.trimmed().isEmpty()) {
        m_capturedNASPath = output.trimmed();
        outputToTerminal("Captured NAS path: " + m_capturedNASPath, Success);
    }
}

void TMWeeklyPCController::setTextBrowser(QTextBrowser* textBrowser)
{
    m_textBrowser = textBrowser;

    if (m_textBrowser) {
        // Force initial load by setting current state to an invalid value
        m_currentHtmlState = static_cast<HtmlDisplayState>(-1);
        // Load default HTML immediately
        updateHtmlDisplay();
    }
}

void TMWeeklyPCController::showNASLinkDialog()
{
    if (m_capturedNASPath.isEmpty()) {
        outputToTerminal("No NAS path captured - cannot display location dialog", Warning);
        return;
    }

    outputToTerminal("Opening print file location dialog...", Info);

    // Create and show the dialog with custom text for Post Print
    // Use nullptr as parent since TMWeeklyPCController is not a QWidget
    NASLinkDialog* dialog = new NASLinkDialog(
        "Print File Location",           // Window title
        "Print file located below",      // Description text
        m_capturedNASPath,              // Network path
        nullptr                         // Parent (changed from 'this' to nullptr)
        );
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void TMWeeklyPCController::calculateMeterPostage()
{
    // Only calculate if both class and permit are set to the required values
    if (m_classDDbox->currentText() != "FIRST CLASS" || m_permitDDbox->currentText() != "METER") {
        return; // Exit if conditions aren't met
    }

    // Get count value
    QString countText = m_countBox->text();
    if (countText.isEmpty()) {
        return; // Exit if no count entered
    }

    bool ok;
    int count = countText.toInt(&ok);
    if (!ok || count <= 0) {
        return; // Exit if invalid count
    }

    // Get current meter rate from database
    double meterRate = getMeterRateFromDatabase();
    if (meterRate <= 0) {
        meterRate = 0.69; // Fallback to default rate
    }

    // Calculate postage: count * meter rate
    double totalPostage = count * meterRate;

    // Format as currency and update the postage box
    QString formattedPostage = QString("$%1").arg(totalPostage, 0, 'f', 2);
    m_postageBox->setText(formattedPostage);
}

double TMWeeklyPCController::getMeterRateFromDatabase()
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        return 0.69; // Return default if database not available
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT rate_value FROM meter_rates ORDER BY created_at DESC LIMIT 1");

    if (m_dbManager->executeQuery(query) && query.next()) {
        return query.value("rate_value").toDouble();
    }

    return 0.69; // Return default if no rate found in database
}

void TMWeeklyPCController::resetToDefaults()
{
    // Reset all internal state variables
    m_jobDataLocked = false;
    m_postageDataLocked = false;
    m_currentHtmlState = DefaultState;
    m_capturedNASPath.clear();
    m_capturingNASPath = false;

    // Clear all form fields
    if (m_jobNumberBox) m_jobNumberBox->clear();
    if (m_postageBox) m_postageBox->clear();
    if (m_countBox) m_countBox->clear();

    // Reset all dropdowns to index 0 (empty)
    if (m_yearDDbox) m_yearDDbox->setCurrentIndex(0);
    if (m_monthDDbox) m_monthDDbox->setCurrentIndex(0);
    if (m_weekDDbox) m_weekDDbox->setCurrentIndex(0);
    if (m_proofDDbox) m_proofDDbox->setCurrentIndex(0);
    if (m_printDDbox) m_printDDbox->setCurrentIndex(0);
    if (m_classDDbox) m_classDDbox->setCurrentIndex(0);
    if (m_permitDDbox) m_permitDDbox->setCurrentIndex(0);

    // Reset checkboxes
    if (m_proofApprovalCheckBox) m_proofApprovalCheckBox->setChecked(false);

    // Reset all lock buttons to unchecked
    if (m_lockBtn) m_lockBtn->setChecked(false);
    if (m_editBtn) m_editBtn->setChecked(false);
    if (m_postageLockBtn) m_postageLockBtn->setChecked(false);

    // Clear terminal window
    if (m_terminalWindow) m_terminalWindow->clear();

    // Update control states and HTML display
    updateControlStates();
    updateHtmlDisplay();

    // Emit signal to stop auto-save timer since no job is open
    emit jobClosed();
    outputToTerminal("Job state reset to defaults", Info);
    outputToTerminal("Auto-save timer stopped - no job open", Info);
}
