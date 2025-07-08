#include "tmtarragoncontroller.h"

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
#include <QStandardPaths>
#include <QClipboard>
#include <QApplication>
#include <QTimer>
#include <QRegularExpression>
#include <QToolButton>

#include "logger.h"

TMTarragonController::TMTarragonController(QObject *parent)
    : BaseTrackerController(parent),
    m_dbManager(nullptr),
    m_fileManager(nullptr),
    m_tmTarragonDBManager(nullptr),
    m_scriptRunner(nullptr),
    m_openBulkMailerBtn(nullptr),
    m_runInitialBtn(nullptr),
    m_finalStepBtn(nullptr),
    m_lockBtn(nullptr),
    m_editBtn(nullptr),
    m_postageLockBtn(nullptr),
    m_yearDDbox(nullptr),
    m_monthDDbox(nullptr),
    m_dropNumberDDbox(nullptr),
    m_jobNumberBox(nullptr),
    m_postageBox(nullptr),
    m_countBox(nullptr),
    m_terminalWindow(nullptr),
    m_tracker(nullptr),
    m_textBrowser(nullptr),
    m_jobDataLocked(false),
    m_postageDataLocked(false),
    m_currentHtmlState(UninitializedState),
    m_capturedNASPath(),
    m_capturingNASPath(false),
    m_lastExecutedScript(),
    m_trackerModel(nullptr)
{
    Logger::instance().info("Initializing TMTarragonController...");

    // Get the database managers
    m_dbManager = DatabaseManager::instance();
    if (!m_dbManager) {
        Logger::instance().error("Failed to get DatabaseManager instance");
    }

    m_tmTarragonDBManager = TMTarragonDBManager::instance();
    if (!m_tmTarragonDBManager) {
        Logger::instance().error("Failed to get TMTarragonDBManager instance");
    }

    // Create a script runner
    m_scriptRunner = new ScriptRunner(this);

    // Create file manager
    m_fileManager = new TMTarragonFileManager(new QSettings(QSettings::IniFormat, QSettings::UserScope, "GojiApp", "Goji"));

    // Setup the model for the tracker table
    m_trackerModel = new QSqlTableModel(this, m_dbManager->getDatabase());
    m_trackerModel->setTable("tm_tarragon_log");
    m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_trackerModel->select();

    // Create base directories if they don't exist
    createBaseDirectories();

    Logger::instance().info("TMTarragonController initialization complete");
}

TMTarragonController::~TMTarragonController()
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

    Logger::instance().info("TMTarragonController destroyed");
}

void TMTarragonController::initializeUI(
    QPushButton* openBulkMailerBtn, QPushButton* runInitialBtn,
    QPushButton* finalStepBtn, QToolButton* lockBtn, QToolButton* editBtn,
    QToolButton* postageLockBtn, QComboBox* yearDDbox, QComboBox* monthDDbox,
    QComboBox* dropNumberDDbox, QLineEdit* jobNumberBox, QLineEdit* postageBox,
    QLineEdit* countBox, QTextEdit* terminalWindow, QTableView* tracker,
    QTextBrowser* textBrowser)
{
    Logger::instance().info("Initializing TM TARRAGON UI elements");

    // Store UI element pointers
    m_openBulkMailerBtn = openBulkMailerBtn;
    m_runInitialBtn = runInitialBtn;
    m_finalStepBtn = finalStepBtn;
    m_lockBtn = lockBtn;
    m_editBtn = editBtn;
    m_postageLockBtn = postageLockBtn;
    m_yearDDbox = yearDDbox;
    m_monthDDbox = monthDDbox;
    m_dropNumberDDbox = dropNumberDDbox;
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
                &TMTarragonController::showTableContextMenu);
    }

    // Connect UI signals to slots
    connectSignals();

    // Setup initial UI state
    setupInitialUIState();

    // Populate dropdowns
    populateDropdowns();

    // Initialize HTML display with default state
    updateHtmlDisplay();

    Logger::instance().info("TM TARRAGON UI initialization complete");
}

void TMTarragonController::connectSignals()
{
    // Connect buttons
    connect(m_openBulkMailerBtn, &QPushButton::clicked, this, &TMTarragonController::onOpenBulkMailerClicked);
    connect(m_runInitialBtn, &QPushButton::clicked, this, &TMTarragonController::onRunInitialClicked);
    connect(m_finalStepBtn, &QPushButton::clicked, this, &TMTarragonController::onFinalStepClicked);

    // Connect toggle buttons
    connect(m_lockBtn, &QToolButton::clicked, this, &TMTarragonController::onLockButtonClicked);
    connect(m_editBtn, &QToolButton::clicked, this, &TMTarragonController::onEditButtonClicked);
    connect(m_postageLockBtn, &QToolButton::clicked, this, &TMTarragonController::onPostageLockButtonClicked);

    // Connect dropdowns
    connect(m_yearDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &TMTarragonController::onYearChanged);
    connect(m_monthDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &TMTarragonController::onMonthChanged);
    connect(m_dropNumberDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &TMTarragonController::onDropNumberChanged);

    // Connect postage formatting
    if (m_postageBox) {
        connect(m_postageBox, &QLineEdit::textChanged, this, &TMTarragonController::formatPostageInput);
    }

    // Connect script runner signals
    if (m_scriptRunner) {
        connect(m_scriptRunner, &ScriptRunner::scriptOutput, this, &TMTarragonController::onScriptOutput);
        connect(m_scriptRunner, &ScriptRunner::scriptFinished, this, &TMTarragonController::onScriptFinished);
    }

    Logger::instance().info("TM TARRAGON signal connections complete");
}

void TMTarragonController::setupInitialUIState()
{
    Logger::instance().info("Setting up initial TM TARRAGON UI state...");

    // Initial lock states - all unlocked
    m_jobDataLocked = false;
    m_postageDataLocked = false;

    // Update control states
    updateControlStates();

    Logger::instance().info("Initial TM TARRAGON UI state setup complete");
}

void TMTarragonController::populateDropdowns()
{
    Logger::instance().info("Populating TM TARRAGON dropdowns...");

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

    // Populate drop number dropdown: 1-9
    if (m_dropNumberDDbox) {
        m_dropNumberDDbox->clear();
        m_dropNumberDDbox->addItem(""); // Blank default

        for (int i = 1; i <= 9; i++) {
            m_dropNumberDDbox->addItem(QString::number(i));
        }
    }

    Logger::instance().info("TM TARRAGON dropdown population complete");
}

void TMTarragonController::setupOptimizedTableLayout()
{
    if (!m_tracker) return;

    // Calculate optimal font size and column widths
    const int tableWidth = 615; // Fixed widget width from UI
    const int borderWidth = 2;   // Account for table borders
    const int availableWidth = tableWidth - borderWidth;

    // Define maximum content widths for TARRAGON (same columns as TMWPC/TMTERM)
    struct ColumnSpec {
        QString header;
        QString maxContent;
        int minWidth;
    };

    QList<ColumnSpec> columns = {
        {"JOB", "88888", 45},                    // 5 digits
        {"DESCRIPTION", "TM TARRAGON HOMES D9", 140}, // TM TARRAGON HOMES D[drop] format
        {"POSTAGE", "$888.88", 55},             // Max $XXX.XX
        {"COUNT", "8,888", 45},                 // Max 1,XXX with comma
        {"AVG RATE", "0.888", 45},              // 0.XXX format
        {"CLASS", "STD", 35},                   // Always STD for TARRAGON
        {"SHAPE", "LTR", 35},                   // Always LTR
        {"PERMIT", "1165", 45}                  // Always 1165 for TARRAGON
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

// BaseTrackerController implementation methods
void TMTarragonController::outputToTerminal(const QString& message, MessageType type)
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

QTableView* TMTarragonController::getTrackerWidget() const
{
    return m_tracker;
}

QSqlTableModel* TMTarragonController::getTrackerModel() const
{
    return m_trackerModel;
}

QStringList TMTarragonController::getTrackerHeaders() const
{
    return {"JOB", "DESCRIPTION", "POSTAGE", "COUNT", "AVG RATE", "CLASS", "SHAPE", "PERMIT"};
}

QList<int> TMTarragonController::getVisibleColumns() const
{
    return {1, 2, 3, 4, 5, 6, 7, 8}; // Skip column 0 (ID)
}

QString TMTarragonController::formatCellData(int columnIndex, const QString& cellData) const
{
    // Format POSTAGE column to include $ symbol if it doesn't have one
    if (columnIndex == 2 && !cellData.isEmpty() && !cellData.startsWith("$")) {
        return "$" + cellData;
    }
    return cellData;
}

QString TMTarragonController::copyFormattedRow()
{
    QString result = BaseTrackerController::copyFormattedRow(); // Call inherited method
    return result;
}

void TMTarragonController::showTableContextMenu(const QPoint& pos)
{
    QMenu menu(m_tracker);
    QAction* copyAction = menu.addAction("Copy Selected Row");
    QAction* selectedAction = menu.exec(m_tracker->mapToGlobal(pos));
    if (selectedAction == copyAction) {
        copyFormattedRow();
    }
}

void TMTarragonController::createBaseDirectories()
{
    if (!m_fileManager) return;

    QString basePath = m_fileManager->getBasePath();
    QDir baseDir(basePath);

    if (!baseDir.exists()) {
        if (baseDir.mkpath(".")) {
            outputToTerminal("Created base directory: " + basePath, Success);
        } else {
            outputToTerminal("Failed to create base directory: " + basePath, Error);
        }
    }

    // Ensure all subdirectories exist
    m_fileManager->ensureDirectoriesExist();
}

void TMTarragonController::createJobFolder()
{
    if (!m_fileManager) return;

    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString dropNumber = m_dropNumberDDbox ? m_dropNumberDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty() || dropNumber.isEmpty()) {
        outputToTerminal("Cannot create job folder: year, month, or drop number not selected", Warning);
        return;
    }

    QString basePath = "C:/Goji/TRACHMAR/TARRAGON";
    QString jobFolder = basePath + "/ARCHIVE/" + month + "." + dropNumber;
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

void TMTarragonController::onOpenBulkMailerClicked()
{
    outputToTerminal("Opening Bulk Mailer application...", Info);

    QString bulkMailerPath = "C:/Program Files (x86)/BCC Software/Bulk Mailer/BulkMailer.exe";

    QFileInfo fileInfo(bulkMailerPath);
    if (!fileInfo.exists()) {
        outputToTerminal("Bulk Mailer not found at: " + bulkMailerPath, Error);
        return;
    }

    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(bulkMailerPath));

    if (success) {
        outputToTerminal("Bulk Mailer launched successfully", Success);
    } else {
        outputToTerminal("Failed to launch Bulk Mailer", Error);
    }
}

void TMTarragonController::onRunInitialClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Please lock job data before running Initial script.", Warning);
        return;
    }

    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString dropNumber = m_dropNumberDDbox ? m_dropNumberDDbox->currentText() : "";
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (jobNumber.isEmpty() || dropNumber.isEmpty() || year.isEmpty() || month.isEmpty()) {
        outputToTerminal("Error: Missing required job data", Error);
        return;
    }

    outputToTerminal("Starting initial processing script...", Info);
    outputToTerminal(QString("Job: %1, Drop: %2, Year: %3, Month: %4").arg(jobNumber, dropNumber, year, month), Info);
    m_lastExecutedScript = "01INITIAL";

    QString scriptPath = m_fileManager->getScriptPath("01INITIAL");
    QStringList arguments;
    arguments << jobNumber << dropNumber << year << month;

    m_scriptRunner->runScript("python", QStringList() << scriptPath << arguments);
}

void TMTarragonController::onFinalStepClicked()
{
    if (!m_jobDataLocked || !m_postageDataLocked) {
        outputToTerminal("Please lock job data and postage data before running Final Step script.", Warning);
        return;
    }

    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString dropNumber = m_dropNumberDDbox ? m_dropNumberDDbox->currentText() : "";
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (jobNumber.isEmpty() || dropNumber.isEmpty() || year.isEmpty() || month.isEmpty()) {
        outputToTerminal("Error: Missing required job data", Error);
        return;
    }

    outputToTerminal("Starting final step script...", Info);
    outputToTerminal(QString("Job: %1, Drop: %2, Year: %3, Month: %4").arg(jobNumber, dropNumber, year, month), Info);
    m_lastExecutedScript = "02FINALSTEP";

    QString scriptPath = m_fileManager->getScriptPath("02FINALSTEP");
    QStringList arguments;
    arguments << jobNumber << dropNumber << year << month;

    m_scriptRunner->runScript("python", QStringList() << scriptPath << arguments);
}

void TMTarragonController::onLockButtonClicked()
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

        // Create folder for the job
        createJobFolder();

        // Save job to database
        saveJobToDatabase();

        // Copy files from HOME folder to DATA folder when opening
        copyFilesFromHomeFolder();

        // Create job folder
        createJobFolder();

        // Add log entry
        addLogEntry();

        // Update HTML display to show instructions
        updateHtmlDisplay();

        // Emit signal for auto-save timer
        emit jobOpened();
    } else {
        // User is unlocking the job
        m_jobDataLocked = false;
        outputToTerminal("Job data unlocked.", Info);

        // Emit signal to stop auto-save timer
        emit jobClosed();
    }

    // Update control states
    updateControlStates();
}

void TMTarragonController::onEditButtonClicked()
{
    if (m_editBtn->isChecked()) {
        // User clicked edit button - unlock job data for editing
        m_lockBtn->setChecked(false);
        m_jobDataLocked = false;
        outputToTerminal("Job data unlocked for editing.", Info);
    } else {
        // User finished editing - this shouldn't happen as edit button auto-unchecks
        outputToTerminal("Edit mode disabled.", Info);
    }

    // Update control states
    updateControlStates();
}

void TMTarragonController::onPostageLockButtonClicked()
{
    if (m_postageLockBtn->isChecked()) {
        // User is trying to lock postage data
        if (!validatePostageData()) {
            m_postageLockBtn->setChecked(false);
            outputToTerminal("Cannot lock postage: Please correct the validation errors above.", Error);
            return;
        }

        // Lock postage data
        m_postageDataLocked = true;
        outputToTerminal("Postage data locked.", Success);

        // Save postage data to database
        QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
        QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
        QString dropNumber = m_dropNumberDDbox ? m_dropNumberDDbox->currentText() : "";
        QString postage = m_postageBox ? m_postageBox->text() : "";
        QString count = m_countBox ? m_countBox->text() : "";

        if (m_tmTarragonDBManager->savePostageData(year, month, dropNumber, postage, count, true)) {
            outputToTerminal("Postage data saved to database", Success);
        } else {
            outputToTerminal("Failed to save postage data to database", Error);
        }
    } else {
        // User is unlocking postage data
        m_postageDataLocked = false;
        outputToTerminal("Postage data unlocked.", Info);

        // Update database to reflect unlocked state
        QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
        QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
        QString dropNumber = m_dropNumberDDbox ? m_dropNumberDDbox->currentText() : "";
        QString postage = m_postageBox ? m_postageBox->text() : "";
        QString count = m_countBox ? m_countBox->text() : "";

        m_tmTarragonDBManager->savePostageData(year, month, dropNumber, postage, count, false);
    }

    // Update control states
    updateControlStates();
}

void TMTarragonController::onYearChanged(const QString& year)
{
    Q_UNUSED(year)
    // Update HTML display based on whether we have complete job data
    updateHtmlDisplay();

    // Save job state if we have complete data
    if (hasJobData()) {
        saveJobState();
    }
}

void TMTarragonController::onMonthChanged(const QString& month)
{
    Q_UNUSED(month)
    // Update HTML display based on whether we have complete job data
    updateHtmlDisplay();

    // Save job state if we have complete data
    if (hasJobData()) {
        saveJobState();
    }
}

void TMTarragonController::onDropNumberChanged(const QString& dropNumber)
{
    Q_UNUSED(dropNumber)
    // Update HTML display based on whether we have complete job data
    updateHtmlDisplay();

    // Save job state if we have complete data
    if (hasJobData()) {
        saveJobState();
    }
}

void TMTarragonController::onScriptOutput(const QString& output)
{
    // Parse script output for special markers
    parseScriptOutput(output);

    // Output to terminal
    outputToTerminal(output, Info);
}

void TMTarragonController::onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::CrashExit) {
        outputToTerminal("Script crashed unexpectedly", Error);
        return;
    }

    if (exitCode == 0) {
        outputToTerminal("Script completed successfully", Success);

        // Handle script-specific post-processing
        if (m_lastExecutedScript == "01INITIAL") {
            outputToTerminal("Initial processing complete. Postage data can now be entered and locked.", Info);
        } else if (m_lastExecutedScript == "02FINALSTEP") {
            // Show NAS link dialog if we captured a path
            if (!m_capturedNASPath.isEmpty()) {
                showNASLinkDialog(m_capturedNASPath);
            }

            outputToTerminal("Final step complete. Files have been processed and archived.", Success);
        }

        // Refresh tracker
        if (m_trackerModel) {
            m_trackerModel->select();
        }
    } else {
        outputToTerminal(QString("Script failed with exit code: %1").arg(exitCode), Error);
    }
}

bool TMTarragonController::validateJobData()
{
    bool isValid = true;

    // Check job number
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    if (!validateJobNumber(jobNumber)) {
        outputToTerminal("Invalid job number. Must be exactly 5 digits.", Error);
        isValid = false;
    }

    // Check drop number
    QString dropNumber = m_dropNumberDDbox ? m_dropNumberDDbox->currentText() : "";
    if (!validateDropNumber(dropNumber)) {
        outputToTerminal("Invalid drop number. Must be a single digit (1-9).", Error);
        isValid = false;
    }

    // Check year
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    if (year.isEmpty()) {
        outputToTerminal("Year must be selected.", Error);
        isValid = false;
    }

    // Check month
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    if (!validateMonthSelection(month)) {
        outputToTerminal("Month must be selected.", Error);
        isValid = false;
    }

    return isValid;
}

bool TMTarragonController::validatePostageData()
{
    bool isValid = true;

    // Check postage
    QString postage = m_postageBox ? m_postageBox->text() : "";
    if (postage.isEmpty() || postage == "$") {
        outputToTerminal("Postage amount is required.", Error);
        isValid = false;
    } else {
        // Validate postage format
        QString cleanPostage = postage;
        cleanPostage.remove("$");
        bool ok;
        double postageValue = cleanPostage.toDouble(&ok);
        if (!ok || postageValue <= 0) {
            outputToTerminal("Invalid postage amount.", Error);
            isValid = false;
        }
    }

    // Check count
    QString count = m_countBox ? m_countBox->text() : "";
    if (count.isEmpty()) {
        outputToTerminal("Count is required.", Error);
        isValid = false;
    } else {
        bool ok;
        int countValue = count.toInt(&ok);
        if (!ok || countValue <= 0) {
            outputToTerminal("Invalid count. Must be a positive integer.", Error);
            isValid = false;
        }
    }

    return isValid;
}

bool TMTarragonController::validateJobNumber(const QString& jobNumber)
{
    if (jobNumber.isEmpty()) return false;

    bool ok;
    int jobNum = jobNumber.toInt(&ok);
    return ok && jobNumber.length() == 5 && jobNum > 0;
}

bool TMTarragonController::validateDropNumber(const QString& dropNumber)
{
    if (dropNumber.isEmpty()) return false;

    bool ok;
    int dropNum = dropNumber.toInt(&ok);
    return ok && dropNum >= 1 && dropNum <= 9;
}

bool TMTarragonController::validateMonthSelection(const QString& month)
{
    if (month.isEmpty()) return false;

    bool ok;
    int monthNum = month.toInt(&ok);
    return ok && monthNum >= 1 && monthNum <= 12;
}

QString TMTarragonController::convertMonthToAbbreviation(const QString& monthNumber) const
{
    static const QStringList monthAbbrevs = {
        "", "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
    };

    bool ok;
    int month = monthNumber.toInt(&ok);
    if (ok && month >= 1 && month <= 12) {
        return monthAbbrevs[month];
    }

    return monthNumber; // Return as-is if conversion fails
}

QString TMTarragonController::getJobDescription() const
{
    QString dropNumber = m_dropNumberDDbox ? m_dropNumberDDbox->currentText() : "";
    return QString("TM TARRAGON HOMES D%1").arg(dropNumber);
}

bool TMTarragonController::hasJobData() const
{
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString dropNumber = m_dropNumberDDbox ? m_dropNumberDDbox->currentText() : "";

    return !jobNumber.isEmpty() && !year.isEmpty() && !month.isEmpty() && !dropNumber.isEmpty();
}

void TMTarragonController::updateControlStates()
{
    // Job data fields - enabled when not locked
    bool jobFieldsEnabled = !m_jobDataLocked;
    if (m_jobNumberBox) m_jobNumberBox->setEnabled(jobFieldsEnabled);
    if (m_yearDDbox) m_yearDDbox->setEnabled(jobFieldsEnabled);
    if (m_monthDDbox) m_monthDDbox->setEnabled(jobFieldsEnabled);
    if (m_dropNumberDDbox) m_dropNumberDDbox->setEnabled(jobFieldsEnabled);

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

void TMTarragonController::updateHtmlDisplay()
{
    if (!m_textBrowser) return;

    HtmlDisplayState targetState = determineHtmlState();
    if (m_currentHtmlState == UninitializedState || m_currentHtmlState != targetState) {
        m_currentHtmlState = targetState;

        // Load appropriate HTML file based on state
        if (targetState == InstructionsState) {
            loadHtmlFile(":/resources/tmtarragon/instructions.html");
        } else {
            loadHtmlFile(":/resources/tmtarragon/default.html");
        }
    }
}

void TMTarragonController::loadHtmlFile(const QString& resourcePath)
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

TMTarragonController::HtmlDisplayState TMTarragonController::determineHtmlState() const
{
    // Show instructions when job data is locked AND initial script has been run
    if (m_jobDataLocked && !m_lastExecutedScript.isEmpty()) {
        return InstructionsState;  // Show instructions.html when job is locked and script run
    } else {
        return DefaultState;       // Show default.html otherwise
    }
}

void TMTarragonController::formatPostageInput(const QString& text)
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

    // Format with dollar sign if there's content
    if (!cleanText.isEmpty() && cleanText != ".") {
        QString formatted = "$" + cleanText;

        // Block signals to prevent infinite recursion
        QSignalBlocker blocker(m_postageBox);
        m_postageBox->setText(formatted);

        // Move cursor to end
        m_postageBox->setCursorPosition(formatted.length());
    } else if (cleanText.isEmpty()) {
        QSignalBlocker blocker(m_postageBox);
        m_postageBox->clear();
    }
}

void TMTarragonController::parseScriptOutput(const QString& output)
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

void TMTarragonController::showNASLinkDialog(const QString& nasPath)
{
    if (nasPath.isEmpty()) {
        outputToTerminal("No NAS path provided - cannot display location dialog", Warning);
        return;
    }

    outputToTerminal("Opening file location dialog...", Info);

    // Create and show the dialog with custom text for TARRAGON
    NASLinkDialog* dialog = new NASLinkDialog(
        "File Location",                 // Window title
        "Data file located below",       // Description text
        nasPath,                         // Network path
        nullptr                          // Parent
        );
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void TMTarragonController::setTextBrowser(QTextBrowser* textBrowser)
{
    m_textBrowser = textBrowser;
    if (m_textBrowser) {
        // Force initial HTML load by resetting state
        m_currentHtmlState = UninitializedState;
        updateHtmlDisplay();
    }
}

void TMTarragonController::saveJobState()
{
    if (!m_tmTarragonDBManager) return;

    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString dropNumber = m_dropNumberDDbox ? m_dropNumberDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty() || dropNumber.isEmpty()) return;

    // Get postage data for persistence
    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";

    // Save job state including lock states, postage data, and script execution state
    m_tmTarragonDBManager->saveJobState(year, month, dropNumber,
                                        static_cast<int>(m_currentHtmlState),
                                        m_jobDataLocked, m_postageDataLocked,
                                        postage, count, m_lastExecutedScript);
}

void TMTarragonController::loadJobState()
{
    if (!m_tmTarragonDBManager) return;

    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString dropNumber = m_dropNumberDDbox ? m_dropNumberDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty() || dropNumber.isEmpty()) return;

    int htmlState;
    bool jobLocked, postageLocked;
    QString postage, count, lastExecutedScript;

    if (m_tmTarragonDBManager->loadJobState(year, month, dropNumber, htmlState, jobLocked, postageLocked, postage, count, lastExecutedScript)) {
        m_currentHtmlState = static_cast<HtmlDisplayState>(htmlState);
        m_jobDataLocked = jobLocked;
        m_postageDataLocked = postageLocked;
        m_lastExecutedScript = lastExecutedScript; // Restore script execution state

        // Restore postage data to UI
        if (m_postageBox) m_postageBox->setText(postage);
        if (m_countBox) m_countBox->setText(count);

        updateControlStates();
        updateHtmlDisplay();
    }
}

void TMTarragonController::saveJobToDatabase()
{
    if (!m_tmTarragonDBManager) return;

    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString dropNumber = m_dropNumberDDbox ? m_dropNumberDDbox->currentText() : "";

    if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty() || dropNumber.isEmpty()) {
        outputToTerminal("Cannot save job: missing required data", Warning);
        return;
    }

    if (m_tmTarragonDBManager->saveJob(jobNumber, year, month, dropNumber)) {
        outputToTerminal("Job saved to database", Success);
    } else {
        outputToTerminal("Failed to save job to database", Error);
    }
}


bool TMTarragonController::loadJob(const QString& year, const QString& month, const QString& dropNumber)
{
    if (!m_tmTarragonDBManager) return false;

    QString jobNumber;
    if (m_tmTarragonDBManager->loadJob(year, month, dropNumber, jobNumber)) {
        // Populate UI with loaded data
        if (m_yearDDbox) m_yearDDbox->setCurrentText(year);
        if (m_monthDDbox) m_monthDDbox->setCurrentText(month);
        if (m_dropNumberDDbox) m_dropNumberDDbox->setCurrentText(dropNumber);
        if (m_jobNumberBox) m_jobNumberBox->setText(jobNumber);

        // Load job state (locks, etc.)
        loadJobState();

        // NEW: If job data was locked when saved, copy files back to DATA folder
        if (m_jobDataLocked) {
            copyFilesFromHomeFolder();
            outputToTerminal("Files copied from ARCHIVE to DATA folder", Info);

            // Start auto-save timer since job is locked/open
            emit jobOpened();
            outputToTerminal("Auto-save timer started (15 minutes)", Info);
        }

        outputToTerminal("Job loaded: " + jobNumber, Success);
        return true;
    }

    outputToTerminal("Failed to load job for " + year + "/" + month + "/D" + dropNumber, Error);
    return false;
}

void TMTarragonController::addLogEntry()
{
    if (!m_tmTarragonDBManager) return;

    // Get values from UI
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString dropNumber = m_dropNumberDDbox ? m_dropNumberDDbox->currentText() : "";
    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";

    // Validate required data
    if (jobNumber.isEmpty() || dropNumber.isEmpty()) {
        outputToTerminal("Cannot add log entry: missing job number or drop number", Warning);
        return;
    }

    // Create description
    QString description = getJobDescription(); // "TM TARRAGON HOMES D[dropNumber]"

    // Format count as integer (no decimals)
    int countValue = count.isEmpty() ? 0 : count.toInt();
    QString formattedCount = QString::number(countValue);

    // Ensure postage has $ symbol and proper formatting
    QString formattedPostage = postage;
    if (!formattedPostage.startsWith("$")) {
        formattedPostage = "$" + formattedPostage;
    }
    // Ensure 2 decimal places
    double postageAmount = formattedPostage.remove("$").toDouble();
    formattedPostage = QString("$%1").arg(postageAmount, 0, 'f', 2);

    // Calculate per piece rate (X.XXX format - always first digit + decimal + 3 places)
    double perPiece = (countValue > 0) ? (postageAmount / countValue) : 0.0;
    QString perPieceStr = QString("%1").arg(perPiece, 0, 'f', 3);

    // Static values for TARRAGON
    QString mailClass = "STD";
    QString shape = "LTR";
    QString permit = "1165";

    // Get current date
    QDateTime now = QDateTime::currentDateTime();
    QString date = now.toString("M/d/yyyy");

    // Add to database using the standardized 8-column format
    if (m_tmTarragonDBManager->addLogEntry(jobNumber, description, formattedPostage, formattedCount,
                                           perPieceStr, mailClass, shape, permit, date)) {
        outputToTerminal("Added log entry to database", Success);

        // Force refresh the table view
        if (m_trackerModel) {
            m_trackerModel->select();
            outputToTerminal("Tracker table refreshed", Info);
        }
    } else {
        outputToTerminal("Failed to add log entry to database", Error);
    }
}

bool TMTarragonController::createExcelAndCopy(const QStringList& headers, const QStringList& rowData)
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
    script += "  $sheet.Columns.Item(2).ColumnWidth = 25\n";  // DESCRIPTION (wider for TARRAGON)
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
    QTimer::singleShot(5000, this, [tempFileName]() {
        QFile::remove(tempFileName);
    });

    if (output.contains("SUCCESS")) {
        return true;
    } else {
        outputToTerminal(QString("PowerShell error: %1 %2").arg(output, errorOutput), Error);
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

void TMTarragonController::resetToDefaults()
{
    // CRITICAL FIX: Save current job state to database BEFORE resetting
    // This ensures lock states are preserved when job is reopened
    saveJobState();

    // CRITICAL FIX: Move files to HOME folder BEFORE clearing UI fields
    // This ensures we have access to job number, year, month, and drop number when moving files
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
    if (m_dropNumberDDbox) m_dropNumberDDbox->setCurrentIndex(0);

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
    loadHtmlFile(":/resources/tmtarragon/default.html");

    // Emit signal to stop auto-save timer since no job is open
    emit jobClosed();
    outputToTerminal("Job state reset to defaults", Info);
    outputToTerminal("Auto-save timer stopped - no job open", Info);
}

bool TMTarragonController::moveFilesToHomeFolder()
{
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString dropNumber = m_dropNumberDDbox ? m_dropNumberDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty() || dropNumber.isEmpty()) {
        return false;
    }

    QString basePath = "C:/Goji/TRACHMAR/TARRAGON";
    QString homeFolder = month + "." + dropNumber; // TARRAGON uses "MM.DD" format
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

bool TMTarragonController::copyFilesFromHomeFolder()
{
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString dropNumber = m_dropNumberDDbox ? m_dropNumberDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty() || dropNumber.isEmpty()) {
        return false;
    }

    QString basePath = "C:/Goji/TRACHMAR/TARRAGON";
    QString homeFolder = month + "." + dropNumber; // TARRAGON uses "MM.DD" format
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
