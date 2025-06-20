#include "tmtermcontroller.h"

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
#include <QHeaderView>
#include <QDateTime>
#include <QFontMetrics>
#include <QFile>
#include <QDesktopServices>
#include <QUrl>

#include "logger.h"
#include "validator.h"
#include "excelclipboard.h"

TMTermController::TMTermController(QObject *parent)
    : QObject(parent),
    m_dbManager(nullptr),
    m_tmTermDBManager(nullptr),
    m_scriptRunner(nullptr),
    m_fileManager(nullptr),
    m_jobDataLocked(false),
    m_postageDataLocked(false),
    m_currentHtmlState(DefaultState),
    m_capturedNASPath(),
    m_capturingNASPath(false)
{
    Logger::instance().info("Initializing TMTermController...");

    // Get the database managers
    m_dbManager = DatabaseManager::instance();
    if (!m_dbManager) {
        Logger::instance().error("Failed to get DatabaseManager instance");
    }

    m_tmTermDBManager = TMTermDBManager::instance();
    if (!m_tmTermDBManager) {
        Logger::instance().error("Failed to get TMTermDBManager instance");
    }

    // Create a script runner
    m_scriptRunner = new ScriptRunner(this);

    // Get file manager - direct creation instead of using the factory
    // Use a new QSettings instance since DatabaseManager doesn't provide getSettings
    m_fileManager = new TMTermFileManager(new QSettings(QSettings::IniFormat, QSettings::UserScope, "GojiApp", "Goji"));

    // Setup the model for the tracker table
    m_trackerModel = new QSqlTableModel(this, m_dbManager->getDatabase());
    m_trackerModel->setTable("tm_term_log");
    m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_trackerModel->select();

    // Create base directories if they don't exist
    createBaseDirectories();

    Logger::instance().info("TMTermController initialization complete");
}

TMTermController::~TMTermController()
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

    Logger::instance().info("TMTermController destroyed");
}

void TMTermController::setupOptimizedTableLayout()
{
    if (!m_tracker) return;

    // Calculate optimal font size and column widths
    const int tableWidth = 615; // Fixed widget width from UI
    const int borderWidth = 2;   // Account for table borders
    const int availableWidth = tableWidth - borderWidth;

    // Define maximum content widths for TERM (same columns as TMWPC)
    struct ColumnSpec {
        QString header;
        QString maxContent;
        int minWidth;
    };

    QList<ColumnSpec> columns = {
        {"JOB", "88888", 45},           // 5 digits
        {"DESCRIPTION", "TM DEC TERM", 95}, // TM [month] TERM format
        {"POSTAGE", "$888.88", 55},     // Max $XXX.XX
        {"COUNT", "8,888", 45},         // Max 1,XXX with comma
        {"AVG RATE", "0.888", 45},      // 0.XXX format
        {"CLASS", "STD", 35},           // Always STD for TERM
        {"SHAPE", "LTR", 35},           // Always LTR
        {"PERMIT", "1662", 45}          // Always 1662 for TERM
    };

    // Calculate optimal font size
    QFont testFont("Consolas", 8); // Start with monospace font
    QFontMetrics fm(testFont);

    // Find the largest font size that fits all columns
    int optimalFontSize = 8;
    for (int fontSize = 12; fontSize >= 6; fontSize--) {
        testFont.setPointSize(fontSize);
        fm = QFontMetrics(testFont);

        int totalWidth = 0;
        bool fits = true;

        for (const auto& col : columns) {
            int headerWidth = fm.horizontalAdvance(col.header) + 10; // padding
            int contentWidth = fm.horizontalAdvance(col.maxContent) + 10; // padding
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

    // Set custom headers
    m_trackerModel->setHeaderData(1, Qt::Horizontal, tr("JOB"));
    m_trackerModel->setHeaderData(2, Qt::Horizontal, tr("DESCRIPTION"));
    m_trackerModel->setHeaderData(3, Qt::Horizontal, tr("POSTAGE"));
    m_trackerModel->setHeaderData(4, Qt::Horizontal, tr("COUNT"));
    m_trackerModel->setHeaderData(5, Qt::Horizontal, tr("AVG RATE"));
    m_trackerModel->setHeaderData(6, Qt::Horizontal, tr("CLASS"));
    m_trackerModel->setHeaderData(7, Qt::Horizontal, tr("SHAPE"));
    m_trackerModel->setHeaderData(8, Qt::Horizontal, tr("PERMIT"));

    // Hide the ID column (column 0)
    m_tracker->setColumnHidden(0, true);

    // Calculate and set precise column widths
    fm = QFontMetrics(tableFont);
    for (int i = 0; i < columns.size(); i++) {
        const auto& col = columns[i];
        int headerWidth = fm.horizontalAdvance(col.header) + 10;
        int contentWidth = fm.horizontalAdvance(col.maxContent) + 10;
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
        "   font-family: 'Consolas';"
        "}"
        "QTableView::item {"
        "   padding: 2px;"
        "   border-right: 1px solid #cccccc;"
        "}"
        );

    // Enable alternating row colors
    m_tracker->setAlternatingRowColors(true);
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

void TMTermController::connectSignals()
{
    // Connect buttons
    connect(m_openBulkMailerBtn, &QPushButton::clicked, this, &TMTermController::onOpenBulkMailerClicked);
    connect(m_runInitialBtn, &QPushButton::clicked, this, &TMTermController::onRunInitialClicked);
    connect(m_finalStepBtn, &QPushButton::clicked, this, &TMTermController::onFinalStepClicked);

    // Connect toggle buttons
    connect(m_lockBtn, &QToolButton::clicked, this, &TMTermController::onLockButtonClicked);
    connect(m_editBtn, &QToolButton::clicked, this, &TMTermController::onEditButtonClicked);
    connect(m_postageLockBtn, &QToolButton::clicked, this, &TMTermController::onPostageLockButtonClicked);

    // Connect dropdowns
    connect(m_yearDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &TMTermController::onYearChanged);
    connect(m_monthDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &TMTermController::onMonthChanged);

    // Connect postage formatting
    if (m_postageBox) {
        connect(m_postageBox, &QLineEdit::textChanged, this, &TMTermController::formatPostageInput);
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

void TMTermController::formatPostageInput()
{
    if (!m_postageBox) return;

    QString text = m_postageBox->text();
    QString cleanText = text.remove(QRegularExpression("[^0-9.]"));

    // Prevent multiple decimal points
    int decimalPos = cleanText.indexOf('.');
    if (decimalPos != -1) {
        QString beforeDecimal = cleanText.left(decimalPos + 1);
        QString afterDecimal = cleanText.mid(decimalPos + 1).remove('.');
        cleanText = beforeDecimal + afterDecimal;
    }

    // Format with dollar sign if there's content
    if (!cleanText.isEmpty() && cleanText != ".") {
        if (!cleanText.startsWith('$')) {
            cleanText = "$" + cleanText;
        }
    }

    // Update text if it changed
    if (text != cleanText) {
        int cursorPos = m_postageBox->cursorPosition();
        m_postageBox->setText(cleanText);
        m_postageBox->setCursorPosition(qMin(cursorPos, cleanText.length()));
    }
}

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

void TMTermController::updateControlStates()
{
    // Job data fields
    if (m_jobNumberBox) m_jobNumberBox->setEnabled(!m_jobDataLocked);
    if (m_yearDDbox) m_yearDDbox->setEnabled(!m_jobDataLocked);
    if (m_monthDDbox) m_monthDDbox->setEnabled(!m_jobDataLocked);

    // Postage data fields
    if (m_postageBox) m_postageBox->setEnabled(!m_postageDataLocked);
    if (m_countBox) m_countBox->setEnabled(!m_postageDataLocked);

    // Lock button states
    if (m_lockBtn) m_lockBtn->setChecked(m_jobDataLocked);
    if (m_editBtn) m_editBtn->setChecked(false); // Edit button is always unchecked initially
    if (m_editBtn) m_editBtn->setEnabled(m_jobDataLocked); // Only enabled when job data is locked
    if (m_postageLockBtn) m_postageLockBtn->setChecked(m_postageDataLocked);

    // postageLockBtn can only be engaged if job data is locked
    if (m_postageLockBtn) m_postageLockBtn->setEnabled(m_jobDataLocked);

    // finalStepBtn is only enabled when postage is locked
    if (m_finalStepBtn) m_finalStepBtn->setEnabled(m_postageDataLocked);

    // Initial script can run when job data is locked
    if (m_runInitialBtn) m_runInitialBtn->setEnabled(m_jobDataLocked);
}

void TMTermController::updateHtmlDisplay()
{
    if (!m_textBrowser) return;

    HtmlDisplayState targetState = determineHtmlState();
    if (m_currentHtmlState == targetState) return;

    m_currentHtmlState = targetState;

    // Load appropriate HTML file based on state
    if (targetState == InstructionsState) {
        loadHtmlFile(":/resources/tmterm/instructions.html");
    } else {
        loadHtmlFile(":/resources/tmterm/default.html");
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
    // Check if we have job data loaded
    if (hasJobData()) {
        return InstructionsState;  // Show instructions.html when job is loaded
    } else {
        return DefaultState;       // Show default.html when no job is loaded
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

    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty()) {
        outputToTerminal("Cannot create job folder: year or month not selected", Warning);
        return;
    }

    QString jobFolder = m_fileManager->getJobFolderPath(year, month);
    QDir dir(jobFolder);

    if (!dir.exists()) {
        if (dir.mkpath(".")) {
            outputToTerminal("Created job folder: " + jobFolder, Success);
        } else {
            outputToTerminal("Failed to create job folder: " + jobFolder, Error);
        }
    }
}

QString TMTermController::convertMonthToAbbreviation(const QString& monthNumber) const
{
    static const QMap<QString, QString> monthMap = {
        {"01", "JAN"}, {"02", "FEB"}, {"03", "MAR"}, {"04", "APR"},
        {"05", "MAY"}, {"06", "JUN"}, {"07", "JUL"}, {"08", "AUG"},
        {"09", "SEP"}, {"10", "OCT"}, {"11", "NOV"}, {"12", "DEC"}
    };

    return monthMap.value(monthNumber, "");
}

bool TMTermController::validateJobNumber(const QString& jobNumber)
{
    if (jobNumber.length() != 5) return false;

    bool ok;
    jobNumber.toInt(&ok);
    return ok;
}

bool TMTermController::validateMonthSelection(const QString& month)
{
    if (month.isEmpty()) return false;

    bool ok;
    int monthInt = month.toInt(&ok);
    return ok && monthInt >= 1 && monthInt <= 12;
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

// Button handlers
void TMTermController::onLockButtonClicked()
{
    if (!validateJobData()) {
        if (m_lockBtn) m_lockBtn->setChecked(false);
        return;
    }

    m_jobDataLocked = true;
    updateControlStates();
    createJobFolder();
    outputToTerminal("Job data locked", Success);
}

void TMTermController::onEditButtonClicked()
{
    m_jobDataLocked = false;
    m_postageDataLocked = false; // Unlock postage data too when editing job data
    updateControlStates();
    outputToTerminal("Job data unlocked for editing", Info);
}

void TMTermController::onPostageLockButtonClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Error: Job data must be locked first", Error);
        if (m_postageLockBtn) m_postageLockBtn->setChecked(false);
        return;
    }

    if (!validatePostageData()) {
        if (m_postageLockBtn) m_postageLockBtn->setChecked(false);
        return;
    }

    m_postageDataLocked = !m_postageDataLocked;
    updateControlStates();

    if (m_postageDataLocked) {
        outputToTerminal("Postage data locked - Final Step enabled", Success);
        addLogEntry(); // Add entry to tracker when postage is locked
    } else {
        outputToTerminal("Postage data unlocked - Final Step disabled", Info);
    }
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

void TMTermController::onYearChanged(const QString& year)
{
    Q_UNUSED(year)
    // Update HTML display based on whether we have complete job data
    updateHtmlDisplay();

    // Save job state if we have complete data
    if (hasJobData()) {
        saveJobState();
    }
}

void TMTermController::onMonthChanged(const QString& month)
{
    Q_UNUSED(month)
    // Update HTML display based on whether we have complete job data
    updateHtmlDisplay();

    // Save job state if we have complete data
    if (hasJobData()) {
        saveJobState();
    }
}

void TMTermController::showTableContextMenu(const QPoint& pos)
{
    QMenu menu(m_tracker);
    QAction* copyAction = menu.addAction("Copy Selected Row");
    QAction* selectedAction = menu.exec(m_tracker->mapToGlobal(pos));
    if (selectedAction == copyAction) {
        copyFormattedRow();
    }
}

void TMTermController::onScriptOutput(const QString& output)
{
    // Parse script output for special markers
    parseScriptOutput(output);

    // Output to terminal
    outputToTerminal(output, Info);
}

void TMTermController::onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::CrashExit) {
        outputToTerminal("Script crashed unexpectedly", Error);
        return;
    }

    if (exitCode == 0) {
        outputToTerminal("Script completed successfully", Success);

        // Handle script-specific post-processing
        if (m_lastExecutedScript == "02TERMFINALSTEP") {
            // Open DATA folder
            QString dataPath = m_fileManager->getBasePath() + "/DATA";
            QDesktopServices::openUrl(QUrl::fromLocalFile(dataPath));
            outputToTerminal("Opened DATA folder: " + dataPath, Info);

            // Show NAS link dialog if we captured a path
            if (!m_capturedNASPath.isEmpty()) {
                showNASLinkDialog();
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

void TMTermController::parseScriptOutput(const QString& output)
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

void TMTermController::showNASLinkDialog()
{
    if (m_capturedNASPath.isEmpty()) {
        outputToTerminal("No NAS path captured - cannot display location dialog", Warning);
        return;
    }

    outputToTerminal("Opening print file location dialog...", Info);

    // Create and show the dialog with custom text for TERM
    // Use nullptr as parent since TMTermController is not a QWidget
    NASLinkDialog* dialog = new NASLinkDialog(
        "Print File Location",           // Window title
        "Print data file located below", // Description text
        m_capturedNASPath,              // Network path
        nullptr                         // Parent
        );
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void TMTermController::setTextBrowser(QTextBrowser* textBrowser)
{
    m_textBrowser = textBrowser;

    if (m_textBrowser) {
        // Force initial load by setting current state to an invalid value
        m_currentHtmlState = static_cast<HtmlDisplayState>(-1);
        // Load default HTML immediately
        updateHtmlDisplay();
    }
}

void TMTermController::saveJobState()
{
    if (!m_tmTermDBManager) return;

    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty()) return;

    // Save job state including lock states
    m_tmTermDBManager->saveJobState(year, month,
                                   static_cast<int>(m_currentHtmlState),
                                   m_jobDataLocked, m_postageDataLocked);
}

void TMTermController::loadJobState()
{
    if (!m_tmTermDBManager) return;

    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty()) return;

    int htmlState;
    bool jobLocked, postageLocked;

    if (m_tmTermDBManager->loadJobState(year, month, htmlState, jobLocked, postageLocked)) {
        m_currentHtmlState = static_cast<HtmlDisplayState>(htmlState);
        m_jobDataLocked = jobLocked;
        m_postageDataLocked = postageLocked;
        updateControlStates();
        updateHtmlDisplay();
    }
}

void TMTermController::saveJobToDatabase()
{
    if (!m_tmTermDBManager) return;

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
        // Populate UI with loaded data
        if (m_yearDDbox) m_yearDDbox->setCurrentText(year);
        if (m_monthDDbox) m_monthDDbox->setCurrentText(month);
        if (m_jobNumberBox) m_jobNumberBox->setText(jobNumber);

        // Load job state (locks, etc.)
        loadJobState();

        outputToTerminal("Job loaded: " + jobNumber, Success);
        return true;
    }

    outputToTerminal("Failed to load job for " + year + "/" + month, Error);
    return false;
}

void TMTermController::addLogEntry()
{
    if (!m_tmTermDBManager) return;

    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";

    if (jobNumber.isEmpty() || postage.isEmpty() || count.isEmpty()) {
        outputToTerminal("Cannot add log entry: missing required data", Warning);
        return;
    }

    // Format count with commas
    bool ok;
    int countValue = count.toInt(&ok);
    QString formattedCount = ok ? QLocale().toString(countValue) : count;

    // Calculate average rate (postage/count)
    QString postageClean = postage.remove('$').remove(',');
    double postageAmount = postageClean.toDouble();
    double perPiece = (countValue > 0) ? (postageAmount / countValue) : 0.0;
    QString perPieceStr = QString("0.%1").arg(QString::number(perPiece * 1000, 'f', 0).rightJustified(3, '0'));

    // Get description
    QString description = getJobDescription();

    // Static values for TERM
    QString mailClass = "STD";
    QString shape = "LTR";
    QString permit = "1662";

    // Get current date
    QDateTime now = QDateTime::currentDateTime();
    QString date = now.toString("M/d/yyyy");

    // Add to database
    if (m_tmTermDBManager->addLogEntry(jobNumber, description, postage, formattedCount,
                                      perPieceStr, mailClass, shape, permit, date)) {
        outputToTerminal("Added log entry to database", Success);

        // Refresh the table view
        if (m_trackerModel) {
            m_trackerModel->select();
        }
    } else {
        outputToTerminal("Failed to add log entry to database", Error);
    }
}

QString TMTermController::copyFormattedRow()
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

    // Create a temporary QTableWidget with the selected row data
    QTableWidget tempTable;
    tempTable.setColumnCount(m_trackerModel->columnCount());
    tempTable.setRowCount(1);

    // Set header labels
    QStringList headers;
    for (int col = 0; col < m_trackerModel->columnCount(); col++) {
        headers << m_trackerModel->headerData(col, Qt::Horizontal).toString();
    }
    tempTable.setHorizontalHeaderLabels(headers);

    // Populate the single row with data
    for (int col = 0; col < m_trackerModel->columnCount(); col++) {
        QTableWidgetItem* item = new QTableWidgetItem(
            m_trackerModel->data(m_trackerModel->index(row, col)).toString());
        tempTable.setItem(0, col, item);
    }

    // Use ExcelClipboard to copy with proper Excel formatting
    ExcelClipboard::copyTableToExcel(&tempTable);

    outputToTerminal("Copied row to clipboard with Excel formatting", Success);
    return "Row copied to clipboard";
}

void TMTermController::resetToDefaults()
{
    // Reset all internal state variables
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

    // Update control states and HTML display
    updateControlStates();
    updateHtmlDisplay();

    outputToTerminal("Job state reset to defaults", Info);
}
