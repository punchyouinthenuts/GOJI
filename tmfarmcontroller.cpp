#include "tmfarmcontroller.h"
#include "tmfarmdbmanager.h"
#include "tmfarmfilemanager.h"
#include "scriptrunner.h"
#include "tmfarmemaildialog.h"

#include <QSqlRecord>
#include <QSqlError>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QFont>
#include <QFontMetrics>
#include <QDate>
#include <QLocale>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QDebug>
#include <QSettings>
#include <QCoreApplication>
#include <QApplication>
#include <QMessageBox>

TMFarmController::TMFarmController(QObject *parent)
    : QObject(parent)
{
    // Initialize file manager
    QSettings* settings = new QSettings(QSettings::IniFormat, QSettings::UserScope,
                                        QCoreApplication::organizationName(),
                                        QCoreApplication::applicationName(), this);
    m_fileManager = new TMFarmFileManager(settings);

    // Initialize database manager
    m_dbManager = TMFarmDBManager::instance();

    // Create base directories
    m_fileManager->createBaseDirectories();
}

TMFarmController::~TMFarmController() = default;

void TMFarmController::setTextBrowser(QTextBrowser *browser)
{
    m_textBrowser = browser;
}

void TMFarmController::initializeUI(
    QAbstractButton *openBulkMailerBtn,
    QAbstractButton *runInitialBtn,
    QAbstractButton *finalStepBtn,
    QAbstractButton *lockButton,
    QAbstractButton *editButton,
    QAbstractButton *postageLockButton,
    QComboBox  *yearDD,
    QComboBox  *quarterDD,
    QLineEdit  *jobNumberBox,
    QLineEdit  *postageBox,
    QLineEdit  *countBox,
    QTextEdit  *terminalWindow,
    QTableView *trackerView,
    QTextBrowser *textBrowser)
{
    // Set initialization guard to prevent spurious signal handling during UI setup
    m_initializing = true;

    m_openBulkMailerBtn = openBulkMailerBtn;
    m_runInitialBtn     = runInitialBtn;
    m_finalStepBtn      = finalStepBtn;
    m_lockButton        = lockButton;
    m_editButton        = editButton;
    m_postageLockButton = postageLockButton;

    m_yearDD            = yearDD;
    m_quarterDD         = quarterDD;

    m_jobNumberBox      = jobNumberBox;
    m_postageBox        = postageBox;
    m_countBox          = countBox;

    m_terminalWindow    = terminalWindow;
    m_trackerView       = trackerView;
    m_textBrowser       = textBrowser;

    // ScriptRunner for prearchive
    m_scriptRunner = new ScriptRunner(this);
    connect(m_scriptRunner, &ScriptRunner::scriptOutput,  this, &TMFarmController::onScriptOutput);
    connect(m_scriptRunner, &ScriptRunner::scriptError,   this, &TMFarmController::onScriptError);
    connect(m_scriptRunner, &ScriptRunner::scriptFinished,this, &TMFarmController::onScriptFinished);

    // Buttons
    if (m_runInitialBtn)     connect(m_runInitialBtn,     &QAbstractButton::clicked, this, &TMFarmController::onRunInitialClicked);
    if (m_finalStepBtn)      connect(m_finalStepBtn,      &QAbstractButton::clicked, this, &TMFarmController::onFinalStepClicked);
    if (m_openBulkMailerBtn) connect(m_openBulkMailerBtn, &QAbstractButton::clicked, this, &TMFarmController::onOpenBulkMailerClicked);

    // Lock/Edit/Postage workflow (TRACHMAR pattern)
    if (m_lockButton)        connect(m_lockButton,        &QAbstractButton::clicked, this, &TMFarmController::onLockButtonClicked);
    if (m_editButton)        connect(m_editButton,        &QAbstractButton::clicked, this, &TMFarmController::onEditButtonClicked);
    if (m_postageLockButton) connect(m_postageLockButton, &QAbstractButton::clicked, this, &TMFarmController::onPostageLockButtonClicked);

    // Year/Quarter change handlers for state loading
    if (m_yearDD)    connect(m_yearDD,    &QComboBox::currentTextChanged, this, &TMFarmController::onYearChanged);
    if (m_quarterDD) connect(m_quarterDD, &QComboBox::currentTextChanged, this, &TMFarmController::onQuarterChanged);

    // Mirror TERM widget behavior
    initYearDropdown();         // year list
    setupTextBrowserInitial();  // initial default HTML
    wireFormattingForInputs();  // currency + thousands

    // Tracker (visuals preserved)
    setupTrackerModel();
    setupOptimizedTableLayout();

    // Initial HTML refresh based on lock state
    updateHtmlDisplay();

    updateControlStates();

    // Clear initialization guard after all setup is complete
    m_initializing = false;

    outputToTerminal("FARMWORKERS controller initialized", Info);
}

// ============================= Tracker Setup ================================

void TMFarmController::setupTrackerModel()
{
    if (!m_trackerView || !m_dbManager) return;

    m_trackerModel = std::make_unique<QSqlTableModel>(this, m_dbManager->getDatabase());
    m_trackerModel->setTable(QStringLiteral("tm_farm_log"));
    m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_trackerModel->select();
    m_trackerView->setModel(m_trackerModel.get());
}

int TMFarmController::computeOptimalFontSize() const
{
    // This function is no longer used - layout logic moved to applyFixedColumnWidths
    // Kept for API compatibility
    return 7;
}

void TMFarmController::applyHeaderLabels()
{
    if (!m_trackerModel) return;

    // Set custom headers - SAME AS TMHB (with TMFarm content)
    m_trackerModel->setHeaderData(1, Qt::Horizontal, tr("JOB"));
    m_trackerModel->setHeaderData(2, Qt::Horizontal, tr("DESCRIPTION"));
    m_trackerModel->setHeaderData(3, Qt::Horizontal, tr("POSTAGE"));
    m_trackerModel->setHeaderData(4, Qt::Horizontal, tr("COUNT"));
    m_trackerModel->setHeaderData(5, Qt::Horizontal, tr("AVG RATE"));
    m_trackerModel->setHeaderData(6, Qt::Horizontal, tr("CLASS"));
    m_trackerModel->setHeaderData(7, Qt::Horizontal, tr("SHAPE"));
    m_trackerModel->setHeaderData(8, Qt::Horizontal, tr("PERMIT"));
}

void TMFarmController::enforceVisibilityMask()
{
    if (!m_trackerModel || !m_trackerView) return;

    // Show ONLY columns 1..8
    const int total = m_trackerModel->columnCount();
    for (int c = 0; c < total; ++c) {
        const bool shouldShow = (c >= 1 && c <= 8);
        m_trackerView->setColumnHidden(c, !shouldShow);
    }

    // Hide DATE by name if present
    const int idxDate = m_trackerModel->fieldIndex(QStringLiteral("date"));
    if (idxDate >= 0) m_trackerView->setColumnHidden(idxDate, true);
}

void TMFarmController::applyFixedColumnWidths()
{
    if (!m_trackerView) return;

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
        {"DESCRIPTION", "TM FARMWORKERS", 140},
        {"POSTAGE", "$888,888.88", 29},
        {"COUNT", "88,888", 45},
        {"AVG RATE", "0.888", 45},
        {"CLASS", "STD", 60},
        {"SHAPE", "LTR", 33},
        {"PERMIT", "1662", 36}
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
    m_trackerView->setFont(tableFont);

    // Hide ALL unwanted columns (assuming columns 0, 9, 10 are id, date, created_at)
    m_trackerView->setColumnHidden(0, true);  // Hide ID column

    // Check total column count and hide extra columns
    int totalCols = m_trackerModel->columnCount();
    for (int i = 9; i < totalCols; i++) {
        m_trackerView->setColumnHidden(i, true);  // Hide date, created_at, etc.
    }

    fm = QFontMetrics(tableFont);
    for (int i = 0; i < columns.size(); i++) {
        const auto& col = columns[i];
        int headerWidth = fm.horizontalAdvance(col.header) + 12;
        int contentWidth = fm.horizontalAdvance(col.maxContent) + 12;
        int colWidth = qMax(headerWidth, qMax(contentWidth, col.minWidth));

        m_trackerView->setColumnWidth(i + 1, colWidth); // +1 because we hide column 0
    }

    m_trackerView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    // Enable only vertical scrolling
    m_trackerView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_trackerView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_trackerView->setStyleSheet(
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
    m_trackerView->setAlternatingRowColors(true);
}

void TMFarmController::setupOptimizedTableLayout()
{
    if (!m_trackerView) return;

    // Set up the model with proper ordering (newest first)
    m_trackerModel->setSort(0, Qt::DescendingOrder);
    m_trackerModel->select();
    if (m_trackerView) {
        m_trackerView->setSortingEnabled(true);
        m_trackerView->sortByColumn(0, Qt::DescendingOrder);
    }

    applyHeaderLabels();
    applyFixedColumnWidths();
}

void TMFarmController::refreshTracker(const QString &jobNumber)
{
    if (!m_trackerModel) return;

    m_trackerModel->setFilter(QStringLiteral("job='%1'").arg(jobNumber));
    m_trackerModel->setSort(0, Qt::DescendingOrder);
    m_trackerModel->select();
}

// ============================= Widget Behavior ==============================

void TMFarmController::initYearDropdown()
{
    if (!m_yearDD) return;

    m_yearDD->clear();
    const int y = QDate::currentDate().year();
    m_yearDD->addItem(QString());             // blank first
    m_yearDD->addItem(QString::number(y - 1));
    m_yearDD->addItem(QString::number(y));
    m_yearDD->addItem(QString::number(y + 1));

    // Default to blank (top entry)
    m_yearDD->setCurrentIndex(0);
}

void TMFarmController::setupTextBrowserInitial()
{
    if (!m_textBrowser) return;
    m_textBrowser->setSource(QUrl(QStringLiteral("qrc:/resources/tmfarmworkers/default.html")));
}

void TMFarmController::wireFormattingForInputs()
{
    if (m_postageBox)
        connect(m_postageBox, &QLineEdit::editingFinished, this, &TMFarmController::onPostageEditingFinished);
    if (m_countBox)
        connect(m_countBox, &QLineEdit::editingFinished, this, &TMFarmController::onCountEditingFinished);
}

void TMFarmController::onPostageEditingFinished()
{
    if (m_initializing) return; // Don't format during initialization
    formatPostageBoxDisplay();
}

void TMFarmController::onCountEditingFinished()
{
    if (m_initializing) return; // Don't format during initialization
    formatCountBoxDisplay();
}

void TMFarmController::formatPostageBoxDisplay()
{
    if (!m_postageBox) return;

    QString s = m_postageBox->text().trimmed();
    // keep digits and one dot
    QString digits; digits.reserve(s.size());
    bool dotSeen = false;
    for (const QChar& ch : s) {
        if (ch.isDigit()) { digits.append(ch); continue; }
        if (ch == '.' && !dotSeen) { digits.append('.'); dotSeen = true; }
    }
    bool ok = false;
    const double val = digits.toDouble(&ok);
    if (!ok) { m_postageBox->setText(QString()); return; }

    QLocale us(QLocale::English, QLocale::UnitedStates);
    const QString money = us.toCurrencyString(val, "$"); // $ + thousands + 2 decimals
    m_postageBox->setText(money);
}

void TMFarmController::formatCountBoxDisplay()
{
    if (!m_countBox) return;

    QString s = m_countBox->text().trimmed();
    QString digits; digits.reserve(s.size());
    for (const QChar& ch : s) {
        if (ch.isDigit()) digits.append(ch);
    }
    bool ok = false;
    const qint64 v = digits.toLongLong(&ok);
    if (!ok) { m_countBox->setText(QString()); return; }

    QLocale us(QLocale::English, QLocale::UnitedStates);
    m_countBox->setText(us.toString(v)); // thousands separators
}

// ============================ Dynamic HTML (TERM) ===========================

int TMFarmController::determineHtmlState() const
{
    // 0 = default, 1 = instructions
    // Show instructions whenever job data is locked, regardless of postage lock
    return m_jobDataLocked ? 1 : 0;
}

void TMFarmController::updateHtmlDisplay()
{
    const int state = determineHtmlState();
    m_currentHtmlState = static_cast<HtmlDisplayState>(state);
    
    const QString resourcePath = (state == 1)
        ? QStringLiteral("qrc:/resources/tmfarmworkers/instructions.html")
        : QStringLiteral("qrc:/resources/tmfarmworkers/default.html");
    loadHtmlFile(resourcePath);
}

void TMFarmController::loadHtmlFile(const QString& resourcePath)
{
    if (!m_textBrowser) return;
    m_textBrowser->setSource(QUrl(resourcePath));
}

// ====================== Lock/Edit/Postage Workflow ==========================

void TMFarmController::onLockButtonClicked()
{
    if (m_lockButton->isChecked()) {
        // User is trying to lock the job
        if (!validateJobData()) {
            m_lockButton->setChecked(false);
            outputToTerminal("Cannot lock job: Please correct the validation errors above.", Error);
            return;
        }

        // Lock job data
        m_jobDataLocked = true;
        if (m_editButton) m_editButton->setChecked(false); // Auto-uncheck edit button
        outputToTerminal("Job data locked.", Success);

        // Create folder for the job
        createJobFolder();

        // Copy files from HOME (ARCHIVE) folder to DATA folder when opening
        copyFilesFromHomeFolder();

        // Save to database
        saveJobToDatabase();

        // Save job state
        saveJobState();

        // Update control states and HTML display
        updateControlStates();
        updateHtmlDisplay();

        // Emit signal to MainWindow to start auto-save timer
        emit jobOpened();
        outputToTerminal("Auto-save timer started (15 minutes)", Info);
    } else {
        // User unchecked lock button - this shouldn't happen in normal flow
        m_lockButton->setChecked(true); // Force it back to checked
    }
}

void TMFarmController::onEditButtonClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot edit job data until it is locked.", Error);
        m_editButton->setChecked(false);
        return;
    }

    if (m_editButton->isChecked()) {
        // Edit button was just checked - unlock job data for editing
        m_jobDataLocked = false;
        if (m_lockButton) m_lockButton->setChecked(false); // Unlock the lock button

        outputToTerminal("Job data unlocked for editing.", Info);
        updateControlStates();
        updateHtmlDisplay(); // This will switch back to default.html since job is no longer locked
    }
    // If edit button is unchecked, do nothing (ignore the click)
}

void TMFarmController::onPostageLockButtonClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot lock postage data until job data is locked.", Error);
        m_postageLockButton->setChecked(false);
        return;
    }

    if (m_postageLockButton->isChecked()) {
        // Validate postage data before locking
        if (!validatePostageData()) {
            m_postageDataLocked = false;
            m_postageLockButton->setChecked(false);
            return;
        }

        m_postageDataLocked = true;
        outputToTerminal("Postage data locked and saved.", Success);

        // Add or update log entry when postage is locked
        addLogEntry();

        // Save postage data to database
        saveJobState();
    } else {
        m_postageDataLocked = false;
        outputToTerminal("Postage data unlocked.", Info);

        // Save unlocked state to database
        saveJobState();
    }

    updateControlStates();
    updateHtmlDisplay();
}

// ========================= Year/Quarter Change Handlers =====================

void TMFarmController::onYearChanged(const QString& year)
{
    if (m_initializing) return;
    
    Q_UNUSED(year);
    loadJobState(); // Load state when year changes
    updateHtmlDisplay(); // Update HTML based on loaded state
}

void TMFarmController::onQuarterChanged(const QString& quarter)
{
    if (m_initializing) return;
    
    Q_UNUSED(quarter);
    loadJobState(); // Load state when quarter changes
    updateHtmlDisplay(); // Update HTML based on loaded state
}

// ========================= Database Operations ==============================

bool TMFarmController::validateJobData()
{
    bool isValid = true;

    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text().trimmed() : QString();
    QString quarter = m_quarterDD ? m_quarterDD->currentText().trimmed() : QString();
    QString year = m_yearDD ? m_yearDD->currentText().trimmed() : QString();

    if (jobNumber.isEmpty()) {
        outputToTerminal("Validation Error: Job number is required", Error);
        isValid = false;
    } else if (jobNumber.length() != 5 || !jobNumber.toInt()) {
        outputToTerminal("Validation Error: Job number must be a 5-digit number", Error);
        isValid = false;
    }

    if (quarter.isEmpty()) {
        outputToTerminal("Validation Error: Quarter is required", Error);
        isValid = false;
    }

    if (year.isEmpty()) {
        outputToTerminal("Validation Error: Year is required", Error);
        isValid = false;
    } else if (year.length() != 4 || year.toInt() < 2000 || year.toInt() > 2100) {
        outputToTerminal("Validation Error: Year must be a valid 4-digit year", Error);
        isValid = false;
    }

    return isValid;
}

bool TMFarmController::validatePostageData()
{
    bool isValid = true;

    QString postage = m_postageBox ? m_postageBox->text().trimmed() : QString();
    QString count = m_countBox ? m_countBox->text().trimmed() : QString();

    if (postage.isEmpty()) {
        outputToTerminal("Validation Error: Postage amount is required", Error);
        isValid = false;
    }

    if (count.isEmpty()) {
        outputToTerminal("Validation Error: Count is required", Error);
        isValid = false;
    }

    return isValid;
}

void TMFarmController::saveJobToDatabase()
{
    if (!m_dbManager) {
        outputToTerminal("Database manager not initialized", Error);
        return;
    }

    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text().trimmed() : QString();
    QString quarter = m_quarterDD ? m_quarterDD->currentText().trimmed() : QString();
    QString year = m_yearDD ? m_yearDD->currentText().trimmed() : QString();

    if (jobNumber.isEmpty() || quarter.isEmpty() || year.isEmpty()) {
        outputToTerminal("Cannot save job: Missing required data", Warning);
        return;
    }

    if (m_dbManager->saveJob(jobNumber, year, quarter)) {
        outputToTerminal("Job saved to database", Success);
    } else {
        outputToTerminal("Failed to save job to database", Error);
    }
}

void TMFarmController::saveJobState()
{
    if (!m_dbManager) return;

    QString quarter = m_quarterDD ? m_quarterDD->currentText().trimmed() : QString();
    QString year = m_yearDD ? m_yearDD->currentText().trimmed() : QString();

    if (quarter.isEmpty() || year.isEmpty()) {
        return; // No state to save without year/quarter
    }

    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";

    if (m_dbManager->saveJobState(year, quarter,
                                   static_cast<int>(m_currentHtmlState),
                                   m_jobDataLocked, m_postageDataLocked,
                                   postage, count, m_lastExecutedScript)) {
        // Success - no need to spam terminal
    } else {
        outputToTerminal("Failed to save job state to database", Error);
    }
}

void TMFarmController::loadJobState()
{
    if (!m_dbManager) return;

    QString quarter = m_quarterDD ? m_quarterDD->currentText().trimmed() : QString();
    QString year = m_yearDD ? m_yearDD->currentText().trimmed() : QString();

    if (quarter.isEmpty() || year.isEmpty()) {
        return; // No state to load without year/quarter
    }

    m_initializing = true;

    int htmlState;
    bool jobLocked, postageLocked;
    QString postage, count, lastScript;

    if (m_dbManager->loadJobState(year, quarter, htmlState, jobLocked, postageLocked,
                                   postage, count, lastScript)) {
        m_currentHtmlState = static_cast<HtmlDisplayState>(htmlState);
        m_jobDataLocked = jobLocked;
        m_postageDataLocked = postageLocked;
        m_lastExecutedScript = lastScript;

        // Restore UI state
        if (m_postageBox && !postage.isEmpty()) {
            m_postageBox->setText(postage);
        }
        if (m_countBox && !count.isEmpty()) {
            m_countBox->setText(count);
        }

        // Restore lock button states
        if (m_lockButton) m_lockButton->setChecked(m_jobDataLocked);
        if (m_postageLockButton) m_postageLockButton->setChecked(m_postageDataLocked);

        updateControlStates();
        updateHtmlDisplay();

        outputToTerminal(QString("Job state loaded: postage=%1, count=%2, locked=%3")
                             .arg(postage, count, m_jobDataLocked ? "Yes" : "No"), Info);
    }

    m_initializing = false;
}

void TMFarmController::addLogEntry()
{
    if (!m_dbManager) return;

    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text().trimmed() : QString();
    QString postage = m_postageBox ? m_postageBox->text().trimmed() : QString();
    QString count = m_countBox ? m_countBox->text().trimmed() : QString();
    QString quarter = m_quarterDD ? m_quarterDD->currentText().trimmed() : QString();
    QString year = m_yearDD ? m_yearDD->currentText().trimmed() : QString();

    if (jobNumber.isEmpty() || postage.isEmpty() || count.isEmpty() || quarter.isEmpty() || year.isEmpty()) {
        return; // Cannot add/update log entry without required data
    }

    // Calculate avg rate (POSTAGE / COUNT, formatted to 3 decimal places)
    QString postageClean = postage;
    postageClean.remove('$').remove(',');
    bool ok;
    double postageVal = postageClean.toDouble(&ok);
    if (!ok) return;

    QString countClean = count;
    countClean.remove(',');
    qint64 countVal = countClean.toLongLong(&ok);
    if (!ok || countVal == 0) return;

    double avgRate = postageVal / countVal;
    QString avgRateStr = QString::number(avgRate, 'f', 3);

    // Get current date
    QString currentDate = QDateTime::currentDateTime().toString("yyyy-MM-dd");

    bool updated = m_dbManager->updateLogEntryForJob(jobNumber, "TM FARMWORKERS", postage, count, avgRateStr,
                                                      "STD", "LTR", "1662", currentDate, year, quarter);

    if (updated) {
        outputToTerminal(QString("Log entry updated: %1 per piece").arg(avgRateStr), Success);
    } else {
        // No existing entry - insert new one
        if (m_dbManager->addLogEntry(jobNumber, "TM FARMWORKERS", postage, count, avgRateStr,
                                      "STD", "LTR", "1662", currentDate, year, quarter)) {
            outputToTerminal(QString("Log entry added: %1 per piece").arg(avgRateStr), Success);
        } else {
            outputToTerminal("Failed to add log entry", Error);
            return;
        }
    }

    // Refresh tracker to show updated/new entry
    refreshTracker(jobNumber);
}

bool TMFarmController::loadJob(const QString& year, const QString& quarter)
{
    if (!m_dbManager) {
        outputToTerminal("Database manager not initialized", Error);
        return false;
    }

    // Set the year and quarter dropdowns
    m_initializing = true;
    if (m_yearDD) {
        int yearIndex = m_yearDD->findText(year);
        if (yearIndex >= 0) {
            m_yearDD->setCurrentIndex(yearIndex);
        }
    }
    if (m_quarterDD) {
        int quarterIndex = m_quarterDD->findText(quarter);
        if (quarterIndex >= 0) {
            m_quarterDD->setCurrentIndex(quarterIndex);
        }
    }
    m_initializing = false;

    // Load job number from database
    QString jobNumber;
    if (m_dbManager->loadJob(year, quarter, jobNumber)) {
        if (m_jobNumberBox) {
            m_jobNumberBox->setText(jobNumber);
        }
        outputToTerminal(QString("Loaded job %1 for %2-%3").arg(jobNumber, year, quarter), Success);
    } else {
        outputToTerminal(QString("No job found for %1-%2").arg(year, quarter), Warning);
        return false;
    }

    // Load job state (postage, count, lock states, etc.)
    loadJobState();

    // If job data is locked, restore files from ARCHIVE to DATA and start auto-save
    if (m_jobDataLocked) {
        copyFilesFromHomeFolder();
        outputToTerminal("Files copied from FARMWORKERS ARCHIVE to DATA folder", Info);

        emit jobOpened();
        outputToTerminal("Auto-save timer started (15 minutes)", Info);
    }

    // Refresh tracker for this job
    if (!jobNumber.isEmpty()) {
        refreshTracker(jobNumber);
    }

    return true;
}

// ========================= File Operations ==================================

void TMFarmController::createJobFolder()
{
    if (!m_fileManager) return;

    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text().trimmed() : QString();
    QString quarter = m_quarterDD ? m_quarterDD->currentText().trimmed() : QString();
    QString year = m_yearDD ? m_yearDD->currentText().trimmed() : QString();

    if (jobNumber.isEmpty() || quarter.isEmpty() || year.isEmpty()) {
        return;
    }

    if (m_fileManager->createJobFolder(jobNumber, year, quarter)) {
        outputToTerminal("Job folder created successfully", Success);
    } else {
        outputToTerminal("Failed to create job folder", Warning);
    }
}

void TMFarmController::copyFilesFromHomeFolder()
{
    if (!m_fileManager) return;

    QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text().trimmed() : QString();
    QString quarter = m_quarterDD ? m_quarterDD->currentText().trimmed() : QString();
    QString year = m_yearDD ? m_yearDD->currentText().trimmed() : QString();

    if (jobNumber.isEmpty() || quarter.isEmpty() || year.isEmpty()) {
        return;
    }

    if (m_fileManager->copyFilesFromArchive(jobNumber, year, quarter)) {
        outputToTerminal("Files copied from archive to DATA", Success);
    } else {
        // This is not necessarily an error - job folder might not exist yet
        outputToTerminal("No files found in archive (new job)", Info);
    }
}

void TMFarmController::moveFilesToHomeFolder()
{
    if (!m_fileManager) return;

    QString jobNumber = m_cachedJobNumber.isEmpty() ? 
        (m_jobNumberBox ? m_jobNumberBox->text().trimmed() : QString()) : m_cachedJobNumber;
    QString quarter = m_cachedQuarter.isEmpty() ?
        (m_quarterDD ? m_quarterDD->currentText().trimmed() : QString()) : m_cachedQuarter;
    QString year = m_cachedYear.isEmpty() ?
        (m_yearDD ? m_yearDD->currentText().trimmed() : QString()) : m_cachedYear;

    if (jobNumber.isEmpty() || quarter.isEmpty() || year.isEmpty()) {
        return;
    }

    if (m_fileManager->moveFilesToArchive(jobNumber, year, quarter)) {
        outputToTerminal("Files moved to archive", Success);
    } else {
        outputToTerminal("Failed to move files to archive", Warning);
    }
}

// ========================= Job Management ===================================

void TMFarmController::autoSaveAndCloseCurrentJob()
{
    if (!m_jobDataLocked) {
        return; // No job is open
    }

    outputToTerminal("Auto-saving and closing job...", Info);

    // Cache current values before resetting
    m_cachedJobNumber = m_jobNumberBox ? m_jobNumberBox->text().trimmed() : QString();
    m_cachedQuarter = m_quarterDD ? m_quarterDD->currentText().trimmed() : QString();
    m_cachedYear = m_yearDD ? m_yearDD->currentText().trimmed() : QString();

    // Save job state to database
    saveJobState();

    // Move files back to archive (HOME folder)
    moveFilesToHomeFolder();

    // Reset to defaults
    resetToDefaults();
}

void TMFarmController::resetToDefaults()
{
    // Save current job state to database BEFORE resetting
    saveJobState();

    // Move files to HOME folder BEFORE clearing UI fields
    moveFilesToHomeFolder();

    // Reset all internal state variables
    m_jobDataLocked = false;
    m_postageDataLocked = false;
    m_currentHtmlState = DefaultState;
    m_capturedNASPath.clear();
    m_capturingNASPath = false;
    m_lastExecutedScript.clear();

    // Clear cached values
    m_cachedJobNumber.clear();
    m_cachedQuarter.clear();
    m_cachedYear.clear();

    // Clear all form fields
    if (m_jobNumberBox) m_jobNumberBox->clear();
    if (m_postageBox) m_postageBox->clear();
    if (m_countBox) m_countBox->clear();

    // Reset all dropdowns to index 0 (empty)
    if (m_yearDD) m_yearDD->setCurrentIndex(0);
    if (m_quarterDD) m_quarterDD->setCurrentIndex(0);

    // Reset all lock buttons to unchecked
    if (m_lockButton) m_lockButton->setChecked(false);
    if (m_editButton) m_editButton->setChecked(false);
    if (m_postageLockButton) m_postageLockButton->setChecked(false);

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

// ================================ Buttons ===================================

void TMFarmController::onRunInitialClicked()
{
    const QString scriptPath = QStringLiteral("C:/Goji/scripts/TRACHMAR/FARMWORKERS/01 INITIAL.py");
    if (!QFile::exists(scriptPath)) {
        outputToTerminal("Initial script not found: " + scriptPath, Error);
        return;
    }
    outputToTerminal("Starting initial script...", Info);
    m_scriptRunner->runScript(scriptPath, QStringList());
}

void TMFarmController::onOpenBulkMailerClicked()
{
    const QString bulkMailerPath = QStringLiteral("C:/Program Files (x86)/Satori Software/Bulk Mailer/BulkMailer.exe");
    if (!QFile::exists(bulkMailerPath)) {
        outputToTerminal("Bulk Mailer not found at: " + bulkMailerPath, Error);
        return;
    }
    if (!QProcess::startDetached(bulkMailerPath, QStringList())) {
        outputToTerminal("Failed to launch Bulk Mailer", Error);
    } else {
        outputToTerminal("Bulk Mailer launched", Success);
    }
}

void TMFarmController::onFinalStepClicked()
{
    const QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text().trimmed() : QString();
    const QString quarter = m_quarterDD ? m_quarterDD->currentText().trimmed() : QString();
    const QString year = m_yearDD ? m_yearDD->currentText().trimmed() : QString();

    if (jobNumber.isEmpty() || quarter.isEmpty() || year.isEmpty()) {
        outputToTerminal("Job number, quarter, and year are required", Error);
        return;
    }

    QString scriptPath = QStringLiteral("C:/Goji/scripts/TRACHMAR/FARMWORKERS/02 POST PROCESS.py");
    if (!QFile::exists(scriptPath)) {
        outputToTerminal("Post-process script not found: " + scriptPath, Error);
        return;
    }

    // reset capture
    m_capturedNASPath.clear();
    m_capturingNASPath = false;

    outputToTerminal("Starting prearchive phase...", Info);
    outputToTerminal(QString("Job: %1, Quarter: %2, Year: %3").arg(jobNumber, quarter, year), Info);

    QStringList args;
    // NOTE: TERM order parity: positional triplet + flags
    args << jobNumber << quarter << year
         << "--mode" << "prearchive"
         << "--work-dir"     << "C:/Goji/TRACHMAR/FARMWORKERS/DATA"
         << "--archive-root" << "C:/Goji/TRACHMAR/FARMWORKERS/ARCHIVE"
         << "--backup-dir"   << "C:/Goji/TRACHMAR/FARMWORKERS/DATA/_BACKUP"
         << "--network-base" << (QStringLiteral("\\\\NAS1069D9\\AMPrintData\\") + year + QStringLiteral("_SrcFiles\\T\\Trachmar"));

    m_scriptRunner->runScript(scriptPath, args);
}

// ====================== ScriptRunner (Prearchive Phase) =====================

void TMFarmController::onScriptOutput(const QString& line)
{
    const QString trimmed = line.trimmed();

    // Show non-marker lines in terminal
    if (!trimmed.startsWith(QStringLiteral("==="))) {
        outputToTerminal(trimmed, Info);
    }

    parseScriptOutputLine(trimmed);
}

void TMFarmController::onScriptError(const QString& line)
{
    outputToTerminal(line, Error);
}

void TMFarmController::onScriptFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status);
    if (exitCode == 0) {
        outputToTerminal("Prearchive phase completed", Success);
    } else {
        outputToTerminal(QString("Prearchive phase failed (exit code: %1)").arg(exitCode), Error);
    }
}

void TMFarmController::parseScriptOutputLine(const QString& line)
{
    if (line == QStringLiteral("=== NAS_FOLDER_PATH ===")) {
        m_capturingNASPath = true;
        m_capturedNASPath.clear();
        return;
    }

    if (line == QStringLiteral("=== END_NAS_FOLDER_PATH ===")) {
        m_capturingNASPath = false;

        if (!m_capturedNASPath.isEmpty()) {
            const QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text().trimmed() : QString();

            // Modal email dialog; user drags *_MERGED.csv then CLOSE → proceed
            TMFarmEmailDialog dialog(m_capturedNASPath, jobNumber);
            dialog.exec();

            runArchivePhase();
        }
        return;
    }

    if (m_capturingNASPath && !line.isEmpty() && !line.startsWith(QStringLiteral("==="))) {
        m_capturedNASPath = line;
    }
}

// =============================== Archive Phase ==============================

void TMFarmController::runArchivePhase()
{
    const QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text().trimmed() : QString();
    const QString quarter = m_quarterDD ? m_quarterDD->currentText().trimmed() : QString();
    const QString year = m_yearDD ? m_yearDD->currentText().trimmed() : QString();

    if (jobNumber.isEmpty() || quarter.isEmpty() || year.isEmpty()) {
        outputToTerminal("Cannot start archive phase: missing job data", Error);
        return;
    }

    const QString scriptPath = QStringLiteral("C:/Goji/scripts/TRACHMAR/FARMWORKERS/02 POST PROCESS.py");

    QStringList args;
    args << scriptPath
         << jobNumber << quarter << year
         << "--mode" << "archive"
         << "--work-dir"     << "C:/Goji/TRACHMAR/FARMWORKERS/DATA"
         << "--archive-root" << "C:/Goji/TRACHMAR/FARMWORKERS/ARCHIVE"
         << "--backup-dir"   << "C:/Goji/TRACHMAR/FARMWORKERS/DATA/_BACKUP"
         << "--network-base" << (QStringLiteral("\\\\NAS1069D9\\AMPrintData\\") + year + QStringLiteral("_SrcFiles\\T\\Trachmar"));

    outputToTerminal("Starting archive phase...", Info);

    QProcess* archiveProcess = new QProcess(this);

    connect(archiveProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TMFarmController::onArchiveFinished);

    connect(archiveProcess, &QProcess::readyReadStandardOutput, this, [this, archiveProcess]() {
        const QString out = QString::fromUtf8(archiveProcess->readAllStandardOutput());
        if (!out.trimmed().isEmpty()) outputToTerminal(out.trimmed(), Info);
    });
    connect(archiveProcess, &QProcess::readyReadStandardError, this, [this, archiveProcess]() {
        const QString err = QString::fromUtf8(archiveProcess->readAllStandardError());
        if (!err.trimmed().isEmpty()) outputToTerminal(err.trimmed(), Error);
    });

    archiveProcess->start(QStringLiteral("python"), args);
}

void TMFarmController::onArchiveFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status);
    if (exitCode == 0) {
        outputToTerminal("Archive phase completed successfully", Success);
    } else {
        outputToTerminal(QString("Archive phase failed (exit code: %1)").arg(exitCode), Error);
    }
    
    QObject* proc = sender();
    if (proc) proc->deleteLater();
}

// ================================ Misc ======================================

void TMFarmController::updateControlStates()
{
    // Mirror TERM's enable/disable rules based on lock states
    if (!m_jobDataLocked) {
        // Job not locked - allow entry, disable processing buttons and postage lock
        if (m_lockButton) m_lockButton->setEnabled(true);
        if (m_editButton) m_editButton->setEnabled(false);
        if (m_postageLockButton) m_postageLockButton->setEnabled(false);
        
        // Processing buttons disabled when job not locked
        if (m_runInitialBtn) m_runInitialBtn->setEnabled(false);
        if (m_finalStepBtn) m_finalStepBtn->setEnabled(false);
        
        // Allow field editing
        if (m_jobNumberBox) m_jobNumberBox->setEnabled(true);
        if (m_yearDD) m_yearDD->setEnabled(true);
        if (m_quarterDD) m_quarterDD->setEnabled(true);
    } else if (!m_postageDataLocked) {
        // Job locked, postage not locked - allow postage entry and runInitial
        if (m_lockButton) m_lockButton->setEnabled(true);
        if (m_editButton) m_editButton->setEnabled(true);
        if (m_postageLockButton) m_postageLockButton->setEnabled(true);
        
        // runInitial enabled when job data locked, finalStep disabled until postage locked
        if (m_runInitialBtn) m_runInitialBtn->setEnabled(true);
        if (m_finalStepBtn) m_finalStepBtn->setEnabled(false);
        
        // Lock job fields, allow postage/count editing
        if (m_jobNumberBox) m_jobNumberBox->setEnabled(false);
        if (m_yearDD) m_yearDD->setEnabled(false);
        if (m_quarterDD) m_quarterDD->setEnabled(false);
        if (m_postageBox) m_postageBox->setEnabled(true);
        if (m_countBox) m_countBox->setEnabled(true);
    } else {
        // Everything locked - enable both processing buttons
        if (m_lockButton) m_lockButton->setEnabled(true);
        if (m_editButton) m_editButton->setEnabled(true);
        if (m_postageLockButton) m_postageLockButton->setEnabled(true);
        
        // Both processing buttons enabled when fully locked
        if (m_runInitialBtn) m_runInitialBtn->setEnabled(true);
        if (m_finalStepBtn) m_finalStepBtn->setEnabled(true);
        
        // Lock all fields
        if (m_jobNumberBox) m_jobNumberBox->setEnabled(false);
        if (m_yearDD) m_yearDD->setEnabled(false);
        if (m_quarterDD) m_quarterDD->setEnabled(false);
        if (m_postageBox) m_postageBox->setEnabled(false);
        if (m_countBox) m_countBox->setEnabled(false);
    }
}

void TMFarmController::triggerArchivePhase()
{
    runArchivePhase();
}

// ========================= Terminal Output Helper ===========================

void TMFarmController::outputToTerminal(const QString& message, OutputType type)
{
    if (!m_terminalWindow) return;

    QString prefix;
    switch (type) {
        case Success:  prefix = "[FARMWORKERS] "; break;
        case Warning:  prefix = "[WARNING] "; break;
        case Error:    prefix = "[ERROR] "; break;
        case Info:
        default:       prefix = "[FARMWORKERS] "; break;
    }

    m_terminalWindow->append(prefix + message);
}
