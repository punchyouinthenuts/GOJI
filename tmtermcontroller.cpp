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
#include <QStandardPaths>
#include <QToolButton>
#include <QDebug>
#include <QCoreApplication>

#include "logger.h"
#include "databasemanager.h"
#include "naslinkdialog.h"

TMTermController::TMTermController(QObject *parent)
    : BaseTrackerController(parent),
    m_dbManager(nullptr),
    m_fileManager(nullptr),
    m_tmTermDBManager(nullptr),
    m_scriptRunner(nullptr),
    m_openBulkMailerBtn(nullptr),
    m_runInitialBtn(nullptr),
    m_finalStepBtn(nullptr),
    m_lockBtn(nullptr),
    m_editBtn(nullptr),
    m_postageLockBtn(nullptr),
    m_yearDDbox(nullptr),
    m_monthDDbox(nullptr),
    m_jobNumberBox(nullptr),
    m_postageBox(nullptr),
    m_countBox(nullptr),
    m_terminalWindow(nullptr),
    m_textBrowser(nullptr),
    m_tracker(nullptr),
    m_jobDataLocked(false),
    m_postageDataLocked(false),
    m_currentHtmlState(UninitializedState),
    m_lastExecutedScript(),
    m_capturedNASPath(),
    m_capturingNASPath(false),
    m_trackerModel(nullptr)
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

void TMTermController::loadJobState()
{
    if (!m_tmTermDBManager) return;

    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    outputToTerminal(QString("DEBUG: loadJobState called with year='%1', month='%2'").arg(year, month), Info);

    if (year.isEmpty() || month.isEmpty()) {
        outputToTerminal("DEBUG: Year or month is empty, cannot load job state", Warning);
        return;
    }

    int htmlState;
    bool jobLocked, postageLocked;
    QString postage, count, lastExecutedScript;

    if (m_tmTermDBManager->loadJobState(year, month, htmlState, jobLocked, postageLocked, postage, count, lastExecutedScript)) {
        m_currentHtmlState = static_cast<HtmlDisplayState>(htmlState);
        m_jobDataLocked = jobLocked;
        m_postageDataLocked = postageLocked;
        m_lastExecutedScript = lastExecutedScript;

        // Restore postage data to UI
        if (m_postageBox) m_postageBox->setText(postage);
        if (m_countBox) m_countBox->setText(count);

        // Restore lock button states
        if (m_lockBtn) m_lockBtn->setChecked(m_jobDataLocked);
        if (m_postageLockBtn) m_postageLockBtn->setChecked(m_postageDataLocked);

        updateControlStates();
        updateHtmlDisplay();

        outputToTerminal(QString("Job state loaded - Postage: %1, Count: %2, Postage Locked: %3")
                             .arg(postage, count, m_postageDataLocked ? "Yes" : "No"), Success);
    } else {
        outputToTerminal("No saved job state found", Warning);
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

        // CRITICAL FIX: ALWAYS load postage data separately like TMWeeklyPC
        loadPostageData();

        // Update UI to reflect the lock state
        if (m_lockBtn) m_lockBtn->setChecked(m_jobDataLocked);

        // If job data is locked, handle file operations and auto-save
        if (m_jobDataLocked) {
            copyFilesFromHomeFolder();
            outputToTerminal("Files copied from ARCHIVE to DATA folder", Info);

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

    outputToTerminal("Failed to load job for " + year + "/" + month, Error);
    return false;
}

// CRITICAL FIX: Updated addLogEntry() to properly handle count from countBoxTMTERM and fix AVG rate calculation
void TMTermController::addLogEntry()
{
    if (!m_tmTermDBManager) return;

    // Get values from UI
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString postage = m_postageBox ? m_postageBox->text() : "";

    // CRITICAL FIX: Get count from the correct count box (countBoxTMTERM)
    QString count = m_countBox ? m_countBox->text() : "";

    // DEBUG: Output what we're getting from the count box
    outputToTerminal(QString("DEBUG - Count from countBox: '%1'").arg(count), Info);

    // Validate required data
    if (jobNumber.isEmpty() || month.isEmpty() || postage.isEmpty() || count.isEmpty()) {
        outputToTerminal(QString("Cannot add log entry: missing required data. Job: '%1', Month: '%2', Postage: '%3', Count: '%4'")
                             .arg(jobNumber, month, postage, count), Warning);
        return;
    }

    // Convert month to abbreviation for description
    QString monthAbbrev = convertMonthToAbbreviation(month);
    QString description = QString("TM %1 TERM").arg(monthAbbrev);

    // CRITICAL FIX: Remove commas and format count as integer
    QString cleanCount = count;
    cleanCount.remove(','); // Remove commas from formatted numbers
    int countValue = cleanCount.toInt();
    QString formattedCount = QString::number(countValue);

    // DEBUG: Output the cleaned count value
    outputToTerminal(QString("DEBUG - Cleaned count: '%1', Integer value: %2")
                         .arg(cleanCount).arg(countValue), Info);

    // Ensure postage has $ symbol and proper formatting
    QString formattedPostage = postage;
    if (!formattedPostage.startsWith("$")) {
        formattedPostage = "$" + formattedPostage;
    }
    // Ensure 2 decimal places
    double postageAmount = formattedPostage.remove("$").toDouble();
    formattedPostage = QString("$%1").arg(postageAmount, 0, 'f', 2);

    // CRITICAL FIX: Calculate per piece rate with correct format (X.XXX format)
    double perPiece = (countValue > 0) ? (postageAmount / countValue) : 0.0;
    QString perPieceStr = QString("%1").arg(perPiece, 0, 'f', 3);

    // DEBUG: Output the calculation
    outputToTerminal(QString("DEBUG - Postage: %1, Count: %2, Rate: %3")
                         .arg(postageAmount).arg(countValue).arg(perPieceStr), Info);

    // Static values for TERM
    QString mailClass = "STD";
    QString shape = "LTR";
    QString permit = "1662";

    // Get current date
    QDateTime now = QDateTime::currentDateTime();
    QString date = now.toString("M/d/yyyy");

    // Add to database using the standardized 8-column format
    if (m_tmTermDBManager->addLogEntry(jobNumber, description, formattedPostage, formattedCount,
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

void TMTermController::resetToDefaults()
{
    // CRITICAL FIX: Save current job state to database BEFORE resetting
    // This ensures lock states are preserved when job is reopened
    saveJobState();

    // CRITICAL FIX: Move files to HOME folder BEFORE clearing UI fields
    // This ensures we have access to job number, year, and month when moving files
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
    loadHtmlFile(":/resources/tmterm/default.html");

    // Emit signal to stop auto-save timer since no job is open
    emit jobClosed();
    outputToTerminal("Job state reset to defaults", Info);
    outputToTerminal("Auto-save timer stopped - no job open", Info);
}

// BaseTrackerController implementation methods
QTableView* TMTermController::getTrackerWidget() const
{
    return m_tracker;
}

QSqlTableModel* TMTermController::getTrackerModel() const
{
    return m_trackerModel;
}

QStringList TMTermController::getTrackerHeaders() const
{
    return {"JOB", "DESCRIPTION", "POSTAGE", "COUNT", "AVG RATE", "CLASS", "SHAPE", "PERMIT"};
}

QList<int> TMTermController::getVisibleColumns() const
{
    return {1, 2, 3, 4, 5, 6, 7, 8}; // Skip column 0 (ID)
}

QString TMTermController::formatCellData(int columnIndex, const QString& cellData) const
{
    // Format POSTAGE column (index 3) to include $ symbol and comma formatting
    if (columnIndex == 3 && !cellData.isEmpty()) {
        // Remove existing $ sign if present
        QString cleanData = cellData;
        cleanData.remove("$");

        // Convert to double and format with $ and comma
        bool ok;
        double amount = cleanData.toDouble(&ok);
        if (ok) {
            return QString("$%L1").arg(amount, 0, 'f', 2);
        }

        // Fallback: just add $ if conversion failed
        if (!cellData.startsWith("$")) {
            return "$" + cellData;
        }
    }

    // Format COUNT column (index 4) to include comma for thousands
    if (columnIndex == 4 && !cellData.isEmpty()) {
        bool ok;
        int count = cellData.toInt(&ok);
        if (ok && count >= 1000) {
            return QString("%L1").arg(count);
        }
    }

    return cellData;
}

bool TMTermController::moveFilesToHomeFolder()
{
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty()) {
        return false;
    }

    // If no job number, still try to move files to a basic folder structure
    if (jobNumber.isEmpty()) {
        outputToTerminal("Warning: No job number available for folder naming", Warning);
        return moveFilesToBasicHomeFolder(year, month);
    }

    QString basePath = "C:/Goji/TRACHMAR/TERM";
    QString monthAbbrev = convertMonthToAbbreviation(month);

    // CRITICAL FIX: Use format "JOBNUM MONTHABBREV YEAR" (e.g., "37580 JUL 2025")
    QString homeFolder = jobNumber + " " + monthAbbrev + " " + year;
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
                outputToTerminal("Moved file: " + fileName + " to ARCHIVE/" + homeFolder, Info);
            }
        }
        return allMoved;
    }

    return true;
}

// ==== NEW HELPER METHOD for fallback when no job number ====
bool TMTermController::moveFilesToBasicHomeFolder(const QString& year, const QString& month)
{
    QString basePath = "C:/Goji/TRACHMAR/TERM";
    QString monthAbbrev = convertMonthToAbbreviation(month);

    // Fallback format: "00000 MONTHABBREV YEAR"
    QString homeFolder = "00000 " + monthAbbrev + " " + year;
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
                outputToTerminal("Moved file: " + fileName + " to ARCHIVE/" + homeFolder, Info);
            }
        }
        return allMoved;
    }

    return true;
}

bool TMTermController::copyFilesFromHomeFolder()
{
    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty()) {
        return false;
    }

    QString basePath = "C:/Goji/TRACHMAR/TERM";
    QString monthAbbrev = convertMonthToAbbreviation(month);
    QString jobFolder = basePath + "/DATA";
    QString homeFolderPath;

    // Try to find the correct archive folder
    if (!jobNumber.isEmpty()) {
        // Try with job number first
        QString homeFolder = jobNumber + " " + monthAbbrev + " " + year;
        homeFolderPath = basePath + "/ARCHIVE/" + homeFolder;
    }

    // Check if home folder exists
    QDir homeDir(homeFolderPath);
    if (!homeDir.exists()) {
        // Try fallback format
        QString homeFolder = "00000 " + monthAbbrev + " " + year;
        homeFolderPath = basePath + "/ARCHIVE/" + homeFolder;
        homeDir.setPath(homeFolderPath);

        if (!homeDir.exists()) {
            outputToTerminal("HOME folder does not exist: " + homeFolderPath, Warning);
            return true; // Not an error if no previous job exists
        }
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

void TMTermController::setupOptimizedTableLayout()
{
    if (!m_tracker) return;

    // Calculate optimal font size and column widths
    const int tableWidth = 615; // Fixed widget width from UI
    const int borderWidth = 2;   // Account for table borders
    const int availableWidth = tableWidth - borderWidth;

    // Define maximum content widths for TERM - UPDATED with much wider Job and Postage
    struct ColumnSpec {
        QString header;
        QString maxContent;
        int minWidth;
    };

    QList<ColumnSpec> columns = {
        {"JOB", "88888", 50},           // Reduced from 55
        {"DESCRIPTION", "TM DEC TERM", 95}, // Reduced from 105
        {"POSTAGE", "$8888.88", 75},    // Reduced from 85
        {"COUNT", "88,888", 45},        // Reduced from 50
        {"AVG RATE", "0.888", 32},      // Reduced from 35
        {"CLASS", "STD", 28},           // Reduced from 30
        {"SHAPE", "LTR", 28},           // Reduced from 30
        {"PERMIT", "1662", 35}          // Reduced from 38
    };

    // Calculate optimal font size - INCREASED slightly
    QFont testFont("Consolas", 8); // Increased from 7
    QFontMetrics fm(testFont);

    // Find the largest font size that fits all columns
    int optimalFontSize = 8;
    for (int fontSize = 11; fontSize >= 7; fontSize--) { // Increased minimum from 6 to 7
        testFont.setPointSize(fontSize);
        fm = QFontMetrics(testFont);

        int totalWidth = 0;
        bool fits = true;

        for (const auto& col : columns) {
            int headerWidth = fm.horizontalAdvance(col.header) + 8; // Reduced padding
            int contentWidth = fm.horizontalAdvance(col.maxContent) + 8; // Reduced padding
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

    // Hide the ID column (column 0) and the last two columns (date and created_at)
    m_tracker->setColumnHidden(0, true);  // Hide ID

    // CRITICAL FIX: Hide the final two columns (date and created_at)
    int totalCols = m_trackerModel->columnCount();
    if (totalCols > 9) m_tracker->setColumnHidden(9, true);   // Hide date column
    if (totalCols > 10) m_tracker->setColumnHidden(10, true); // Hide created_at column

    // Calculate and set precise column widths to reach the edge
    fm = QFontMetrics(tableFont);
    int totalCalculatedWidth = 0;
    QList<int> calculatedWidths;

    // First pass: calculate base widths
    for (int i = 0; i < columns.size(); i++) {
        const auto& col = columns[i];
        int headerWidth = fm.horizontalAdvance(col.header) + 8;
        int contentWidth = fm.horizontalAdvance(col.maxContent) + 8;
        int colWidth = qMax(headerWidth, qMax(contentWidth, col.minWidth));
        calculatedWidths.append(colWidth);
        totalCalculatedWidth += colWidth;
    }

    // Second pass: distribute any remaining space proportionally
    int remainingSpace = availableWidth - totalCalculatedWidth;
    if (remainingSpace > 0) {
        // Distribute the remaining space proportionally
        for (int i = 0; i < calculatedWidths.size(); i++) {
            int additionalWidth = (remainingSpace * calculatedWidths[i]) / totalCalculatedWidth;
            calculatedWidths[i] += additionalWidth;
        }
    }

    // Apply the calculated widths
    for (int i = 0; i < calculatedWidths.size(); i++) {
        m_tracker->setColumnWidth(i + 1, calculatedWidths[i]); // +1 because we hide column 0
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

    // Connect input formatting
    if (m_postageBox) {
        connect(m_postageBox, &QLineEdit::textChanged, this, &TMTermController::formatPostageInput);
    }

    if (m_countBox) {
        connect(m_countBox, &QLineEdit::textChanged, this, &TMTermController::formatCountInput);
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

void TMTermController::formatPostageInput(const QString& text)
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
    QString formatted;
    if (!cleanText.isEmpty() && cleanText != ".") {
        formatted = "$" + cleanText;
    } else {
        formatted = cleanText;
    }

    // Update text if it changed
    if (text != formatted) {
        // Block signals to prevent infinite recursion
        QSignalBlocker blocker(m_postageBox);

        // Calculate cursor position relative to the end
        int cursorPos = m_postageBox->cursorPosition();
        int oldLength = text.length();
        int newLength = formatted.length();
        int lengthDiff = newLength - oldLength;

        // Set the formatted text
        m_postageBox->setText(formatted);

        // Adjust cursor position
        int newCursorPos = cursorPos + lengthDiff;
        m_postageBox->setCursorPosition(qMax(0, qMin(newCursorPos, formatted.length())));
    }
}

void TMTermController::formatCountInput(const QString& text)
{
    if (!m_countBox) return;

    QString cleanText = text;
    static const QRegularExpression nonNumericRegex("[^0-9]");
    cleanText.remove(nonNumericRegex);

    if (cleanText.isEmpty()) {
        return; // Don't format empty text
    }

    // Convert to integer and back to remove leading zeros
    bool ok;
    int value = cleanText.toInt(&ok);
    if (!ok) return;

    // CRITICAL FIX: Format with comma if >= 1000 for display, but ensure we can parse it back
    QString formatted;
    if (value >= 1000) {
        formatted = QString("%L1").arg(value);
    } else {
        formatted = QString::number(value);
    }

    // Update text if it changed
    if (text != formatted) {
        // Block signals to prevent infinite recursion
        QSignalBlocker blocker(m_countBox);

        // Calculate cursor position
        int cursorPos = m_countBox->cursorPosition();
        int oldLength = text.length();
        int newLength = formatted.length();
        int lengthDiff = newLength - oldLength;

        // Set the formatted text
        m_countBox->setText(formatted);

        // Adjust cursor position
        int newCursorPos = cursorPos + lengthDiff;
        m_countBox->setCursorPosition(qMax(0, qMin(newCursorPos, formatted.length())));
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

void TMTermController::updateHtmlDisplay()
{
    if (!m_textBrowser) {
        outputToTerminal("DEBUG: No text browser available!", Error);
        return;
    }

    HtmlDisplayState targetState = determineHtmlState();

    // DEBUG OUTPUT
    outputToTerminal(QString("DEBUG: Job locked = %1").arg(m_jobDataLocked ? "TRUE" : "FALSE"), Info);
    outputToTerminal(QString("DEBUG: Current HTML state = %1").arg(m_currentHtmlState), Info);
    outputToTerminal(QString("DEBUG: Target HTML state = %1").arg(targetState), Info);
    outputToTerminal(QString("DEBUG: Target state name = %1").arg(targetState == InstructionsState ? "INSTRUCTIONS" : "DEFAULT"), Info);

    if (m_currentHtmlState == UninitializedState || m_currentHtmlState != targetState) {
        m_currentHtmlState = targetState;

        // Load appropriate HTML file based on state
        if (targetState == InstructionsState) {
            outputToTerminal("DEBUG: Loading instructions.html", Info);
            loadHtmlFile(":/resources/tmterm/instructions.html");
        } else {
            outputToTerminal("DEBUG: Loading default.html", Info);
            loadHtmlFile(":/resources/tmterm/default.html");
        }
    } else {
        outputToTerminal("DEBUG: HTML state unchanged, not loading new file", Info);
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
    // DEBUG OUTPUT
    qDebug() << "DEBUG determineHtmlState: m_jobDataLocked =" << (m_jobDataLocked ? "TRUE" : "FALSE");

    // Show instructions when job data is locked
    if (m_jobDataLocked) {
        qDebug() << "DEBUG determineHtmlState: Returning InstructionsState";
        return InstructionsState;  // Show instructions.html when job is locked
    } else {
        qDebug() << "DEBUG determineHtmlState: Returning DefaultState";
        return DefaultState;       // Show default.html otherwise
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

    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty()) {
        outputToTerminal("Cannot create job folder: year or month not selected", Warning);
        return;
    }

    if (jobNumber.isEmpty()) {
        outputToTerminal("Cannot create job folder: job number not entered", Warning);
        return;
    }

    // Use the 3-parameter version that includes job number
    QString jobFolder = m_fileManager->getJobFolderPath(jobNumber, year, month);
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

QString TMTermController::convertMonthToAbbreviation(const QString& monthNumber) const
{
    static const QMap<QString, QString> monthMap = {
        {"01", "JAN"}, {"02", "FEB"}, {"03", "MAR"}, {"04", "APR"},
        {"05", "MAY"}, {"06", "JUN"}, {"07", "JUL"}, {"08", "AUG"},
        {"09", "SEP"}, {"10", "OCT"}, {"11", "NOV"}, {"12", "DEC"}
    };

    return monthMap.value(monthNumber, "");
}

void TMTermController::debugCheckTables()
{
    if (!m_tmTermDBManager || !m_dbManager) return;

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT name FROM sqlite_master WHERE type='table' AND name='tm_term_postage'");

    if (query.exec() && query.next()) {
        outputToTerminal("DEBUG: tm_term_postage table EXISTS", Success);
    } else {
        outputToTerminal("DEBUG: tm_term_postage table DOES NOT EXIST", Error);
    }
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
    if (m_lockBtn->isChecked()) {
        // User is trying to lock the job
        if (!validateJobData()) {
            m_lockBtn->setChecked(false);
            outputToTerminal("Cannot lock job: Please correct the validation errors above.", Error);
            // Edit button stays checked so user can fix the data
            return;
        }

        // Lock job data
        m_jobDataLocked = true;
        if (m_editBtn) m_editBtn->setChecked(false); // Auto-uncheck edit button
        outputToTerminal("Job data locked.", Success);

        // Create folder for the job
        createJobFolder();

        // Copy files from HOME folder to DATA folder when opening
        copyFilesFromHomeFolder();

        // Save to database
        saveJobToDatabase();

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
    } else {
        // User unchecked lock button - this shouldn't happen in normal flow
        m_lockBtn->setChecked(true); // Force it back to checked
    }
}

void TMTermController::onEditButtonClicked()
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
    }
    // If edit button is unchecked, do nothing (ignore the click)
}

void TMTermController::onPostageLockButtonClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot lock postage data until job data is locked.", Error);
        m_postageLockBtn->setChecked(false);
        return;
    }

    if (m_postageLockBtn->isChecked()) {
        // User is locking postage data
        if (!validatePostageData()) {
            m_postageLockBtn->setChecked(false);
            outputToTerminal("Cannot lock postage: Please correct the validation errors above.", Error);
            return;
        }

        m_postageDataLocked = true;
        outputToTerminal("Postage data locked.", Success);

        // Add log entry to tracker when postage is locked
        addLogEntry();

        // CRITICAL FIX: Save postage data persistently when locked
        savePostageData();

    } else {
        // User is unlocking postage data
        m_postageDataLocked = false;
        outputToTerminal("Postage data unlocked.", Info);

        // Save the unlocked state to database
        savePostageData();
    }

    // Save job state whenever postage lock button is clicked (includes lock state)
    saveJobState();
    updateControlStates();
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

    // Only save job state if job is already locked (not during loading)
    if (m_jobDataLocked && hasJobData()) {
        saveJobState();
    }
}

void TMTermController::onMonthChanged(const QString& month)
{
    Q_UNUSED(month)
    // Update HTML display based on whether we have complete job data
    updateHtmlDisplay();

    // Only save job state if job is already locked (not during loading)
    if (m_jobDataLocked && hasJobData()) {
        saveJobState();
    }
}

// CRITICAL FIX: Use the working BaseTrackerController copy method instead of old HTML approach
void TMTermController::showTableContextMenu(const QPoint& pos)
{
    QMenu menu(m_tracker);
    QAction* copyAction = menu.addAction("Copy Selected Row");
    QAction* selectedAction = menu.exec(m_tracker->mapToGlobal(pos));
    if (selectedAction == copyAction) {
        // CRITICAL FIX: Use the inherited BaseTrackerController method
        // This uses PowerShell Excel method that works in TMWEEKLYPC
        BaseTrackerController::copyFormattedRow();
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

void TMTermController::showNASLinkDialog(const QString& nasPath)
{
    if (nasPath.isEmpty()) {
        outputToTerminal("No NAS path provided - cannot display location dialog", Warning);
        return;
    }

    outputToTerminal("Opening print file location dialog...", Info);

    // Create and show the dialog with custom text for TERM
    NASLinkDialog* dialog = new NASLinkDialog(
        "Print File Location",           // Window title
        "Print data file located below", // Description text
        nasPath,                        // Network path (use the parameter)
        nullptr                         // Parent
        );
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void TMTermController::setTextBrowser(QTextBrowser* textBrowser)
{
    m_textBrowser = textBrowser;
    if (m_textBrowser) {
        // Force initial HTML load by resetting state
        m_currentHtmlState = UninitializedState;
        updateHtmlDisplay();
    }
}

void TMTermController::saveJobState()
{
    if (!m_tmTermDBManager) return;

    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty()) return;

    // Get postage data for persistence
    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";

    // Save job state including lock states, postage data, and script execution state
    m_tmTermDBManager->saveJobState(year, month,
                                    static_cast<int>(m_currentHtmlState),
                                    m_jobDataLocked, m_postageDataLocked,
                                    postage, count, m_lastExecutedScript);
}

void TMTermController::savePostageData()
{
    if (!m_jobDataLocked) {
        return; // Only save for locked jobs
    }

    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";

    if (year.isEmpty() || month.isEmpty()) {
        return;
    }

    if (m_tmTermDBManager->savePostageData(year, month, postage, count, m_postageDataLocked)) {
        outputToTerminal("Postage data saved persistently", Info);
    } else {
        outputToTerminal("Failed to save postage data", Warning);
    }
}

void TMTermController::loadPostageData()
{
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty()) {
        return;
    }

    // CRITICAL FIX: Load postage data from jobs table using loadJobState
    // This eliminates the dual-table conflict
    int htmlState;
    bool jobLocked, postageLocked;
    QString postage, count, lastExecutedScript;

    if (m_tmTermDBManager && m_tmTermDBManager->loadJobState(year, month, htmlState, jobLocked, postageLocked, postage, count, lastExecutedScript)) {
        // Only restore postage data to UI, don't override other state variables
        if (m_postageBox && !postage.isEmpty()) m_postageBox->setText(postage);
        if (m_countBox && !count.isEmpty()) m_countBox->setText(count);

        // Only restore lock state if it's different from current
        if (postageLocked != m_postageDataLocked) {
            m_postageDataLocked = postageLocked;
            if (m_postageLockBtn) m_postageLockBtn->setChecked(postageLocked);
        }

        outputToTerminal("Postage data loaded from jobs table", Info);
    }
}
