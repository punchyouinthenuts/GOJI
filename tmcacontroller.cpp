#include "tmcacontroller.h"

#include "databasemanager.h"
#include "logger.h"
#include "naslinkdialog.h"
#include "dropwindow.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QAction>
#include <QHeaderView>
#include <QSqlTableModel>
#include <QPointer>
#include <QTextCursor>

TMCAController::TMCAController(QObject *parent)
    : BaseTrackerController(parent)
    , m_fileManager(nullptr)
    , m_tmcaDBManager(nullptr)
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
    , m_trackerModel(nullptr)
{
    initializeComponents();
}

TMCAController::~TMCAController()
{
    if (m_trackerModel) {
        m_trackerModel->deleteLater();
        m_trackerModel = nullptr;
    }

    if (m_scriptRunner) {
        m_scriptRunner->deleteLater();
        m_scriptRunner = nullptr;
    }

    if (m_fileManager) {
        delete m_fileManager;
        m_fileManager = nullptr;
    }
}

void TMCAController::initializeComponents()
{
    // Managers (match TMFLER pattern)
    m_fileManager = new TMCAFileManager(nullptr);
    m_tmcaDBManager = TMCADBManager::instance();

    m_scriptRunner = new ScriptRunner(this);
    connect(m_scriptRunner, &ScriptRunner::scriptOutput, this, &TMCAController::onScriptOutput);
    connect(m_scriptRunner, &ScriptRunner::scriptError, this, &TMCAController::onScriptOutput);
    connect(m_scriptRunner, &ScriptRunner::scriptFinished, this, &TMCAController::onScriptFinished);

    // Ensure base dirs exist early
    createBaseDirectories();

    if (m_tmcaDBManager) {
        m_tmcaDBManager->initializeTables();
    }

    setupInitialState();
}

void TMCAController::initializeAfterConstruction()
{
    // Used by MainWindow after it wires widgets.
    connectSignals();
    populateDropdowns();
    setupDropWindow();
    updateLockStates();
    updateButtonStates();
    updateHtmlDisplay();
}

void TMCAController::createBaseDirectories()
{
    if (m_fileManager) {
        m_fileManager->createBaseDirectories();
    }
}

void TMCAController::refreshTrackerTable()
{
    if (m_trackerModel) {
        m_trackerModel->select();
    }
}

// ---- Widget setters ----
void TMCAController::setJobNumberBox(QLineEdit* lineEdit)
{
    m_jobNumberBox = lineEdit;
}

void TMCAController::setYearDropdown(QComboBox* comboBox)
{
    m_yearDDbox = comboBox;
}

void TMCAController::setMonthDropdown(QComboBox* comboBox)
{
    m_monthDDbox = comboBox;
}

void TMCAController::setPostageBox(QLineEdit* lineEdit)
{
    m_postageBox = lineEdit;
}

void TMCAController::setCountBox(QLineEdit* lineEdit)
{
    m_countBox = lineEdit;
}

void TMCAController::setJobDataLockButton(QToolButton* button)
{
    m_jobDataLockBtn = button;
    if (m_jobDataLockBtn) {
        connect(m_jobDataLockBtn, &QToolButton::clicked, this, &TMCAController::onJobDataLockClicked);
    }
}

void TMCAController::setEditButton(QToolButton* button)
{
    m_editBtn = button;
    if (m_editBtn) {
        connect(m_editBtn, &QToolButton::clicked, this, &TMCAController::onEditButtonClicked);
    }
}

void TMCAController::setPostageLockButton(QToolButton* button)
{
    m_postageLockBtn = button;
    if (m_postageLockBtn) {
        connect(m_postageLockBtn, &QToolButton::clicked, this, &TMCAController::onPostageLockClicked);
    }
}

void TMCAController::setRunInitialButton(QPushButton* button)
{
    m_runInitialBtn = button;
    if (m_runInitialBtn) {
        connect(m_runInitialBtn, &QPushButton::clicked, this, &TMCAController::onRunInitialClicked);
    }
}

void TMCAController::setFinalStepButton(QPushButton* button)
{
    m_finalStepBtn = button;
    if (m_finalStepBtn) {
        connect(m_finalStepBtn, &QPushButton::clicked, this, &TMCAController::onFinalStepClicked);
    }
}

void TMCAController::setTerminalWindow(QTextEdit* textEdit)
{
    m_terminalWindow = textEdit;
}

void TMCAController::setTextBrowser(QTextBrowser* textBrowser)
{
    m_textBrowser = textBrowser;
    updateHtmlDisplay();
}

void TMCAController::setTracker(QTableView* tableView)
{
    m_tracker = tableView;
    if (!m_tracker) return;

    // Mirror TMFLER's tracker model setup (using QSqlTableModel)
    if (!m_tmcaDBManager) return;

    if (m_trackerModel) {
        m_trackerModel->deleteLater();
        m_trackerModel = nullptr;
    }

    m_trackerModel = new QSqlTableModel(this, DatabaseManager::instance()->getDatabase());
    m_trackerModel->setTable("tm_ca_log");
    m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_trackerModel->select();

    // Apply headers via DisplayRole (matches TMFLER style)
    m_trackerModel->setHeaderData(1, Qt::Horizontal, tr("JOB"));
    m_trackerModel->setHeaderData(2, Qt::Horizontal, tr("DESCRIPTION"));
    m_trackerModel->setHeaderData(3, Qt::Horizontal, tr("POSTAGE"));
    m_trackerModel->setHeaderData(4, Qt::Horizontal, tr("COUNT"));
    m_trackerModel->setHeaderData(5, Qt::Horizontal, tr("AVG RATE"));
    m_trackerModel->setHeaderData(6, Qt::Horizontal, tr("CLASS"));
    m_trackerModel->setHeaderData(7, Qt::Horizontal, tr("SHAPE"));
    m_trackerModel->setHeaderData(8, Qt::Horizontal, tr("PERMIT"));

    m_tracker->setModel(m_trackerModel);

    // Hide all non-visible cols (column 0 = id)
    QList<int> visible = getVisibleColumns();
    for (int i = 0; i < m_trackerModel->columnCount(); ++i) {
        m_tracker->setColumnHidden(i, !visible.contains(i));
    }
    m_tracker->setColumnHidden(0, true);

    // Match TMFLER selection behavior
    m_tracker->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tracker->setSelectionMode(QAbstractItemView::SingleSelection);

    // Match TMFLER optimized layout (font/width calculations)
    // NOTE: This block is copied from TMFLERController::setupOptimizedTableLayout
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
        {"DESCRIPTION", "TM CA EDR BA", 140},
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
    for (int fontSize = 11; fontSize >= 7; --fontSize) {
        testFont.setPointSize(fontSize);
        fm = QFontMetrics(testFont);

        int totalWidth = 0;
        bool fits = true;

        for (const ColumnSpec& col : columns) {
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

    QFont tableFont("Blender Pro Bold", optimalFontSize);
    m_tracker->setFont(tableFont);

    // Set ordering newest-first
    m_trackerModel->setSort(0, Qt::DescendingOrder);
    m_trackerModel->select();

    // Fix resize behavior (matches TMFLER)
    m_tracker->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_tracker->verticalHeader()->setVisible(false);
    m_tracker->setAlternatingRowColors(true);

    // Apply widths similar to TMFLER expected proportions
    // (Use the same columns list to set widths deterministically)
    int runningWidth = 0;
    QList<int> colWidths;
    for (const ColumnSpec& col : columns) {
        const int headerWidth  = fm.horizontalAdvance(col.header) + 12;
        const int contentWidth = fm.horizontalAdvance(col.maxContent) + 12;
        const int colWidth     = qMax(headerWidth, qMax(contentWidth, col.minWidth));
        colWidths.append(colWidth);
        runningWidth += colWidth;
    }
    // Map widths to visible db columns 1..8
    for (int i = 0; i < colWidths.size() && (i + 1) < m_trackerModel->columnCount(); ++i) {
        m_tracker->setColumnWidth(i + 1, colWidths[i]);
    }

    m_tracker->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_tracker->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tracker, &QTableView::customContextMenuRequested,
            this, &TMCAController::showTableContextMenu);

    m_tracker->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tracker->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_tracker->setStyleSheet(QString(
        "QTableView {"
        "  border: 1px solid black;"
        "  selection-background-color: #d0d0ff;"
        "  alternate-background-color: #f8f8f8;"
        "  gridline-color: #cccccc;"
        "}"
        "QHeaderView::section {"
        "  background-color: #e0e0e0;"
        "  padding: 4px;"
        "  border: 1px solid black;"
        "  font-weight: bold;"
        "  font-family: 'Blender Pro Bold';"
        "  font-size: %1pt;"
        "}"
        "QTableView::item {"
        "  padding: 3px;"
        "  border-right: 1px solid #cccccc;"
        "}"
    ).arg(optimalFontSize));

    outputToTerminal("Tracker model initialized successfully", Success);
}

void TMCAController::setDropWindow(DropWindow* dropWindow)
{
    m_dropWindow = dropWindow;
    if (m_dropWindow) {
        setupDropWindow();
    }
}

// ---- Job mgmt ----
bool TMCAController::loadJob(const QString& jobNumber, const QString& year, const QString& month)
{
    if (!m_tmcaDBManager) return false;

    // Set UI
    if (m_jobNumberBox) m_jobNumberBox->setText(jobNumber);
    if (m_yearDDbox) m_yearDDbox->setCurrentText(year);
    if (m_monthDDbox) m_monthDDbox->setCurrentText(month);

    // Load job state (locks/postage/count/html/last script)
    loadJobState(jobNumber);

    // Force UI refresh
    m_currentHtmlState = UninitializedState;
    updateLockStates();
    updateButtonStates();
    updateHtmlDisplay();

    emit jobOpened();
    return true;
}

void TMCAController::resetToDefaults()
{
    if (m_jobNumberBox) m_jobNumberBox->clear();
    if (m_postageBox) m_postageBox->clear();
    if (m_countBox) m_countBox->clear();

    m_jobDataLocked = false;
    m_postageDataLocked = false;
    m_lastExecutedScript.clear();
    m_currentHtmlState = UninitializedState;

    updateLockStates();
    updateButtonStates();
    updateHtmlDisplay();

    if (m_dropWindow) {
        m_dropWindow->clearFiles();
    }

    emit jobClosed();
}

void TMCAController::saveJobState()
{
    if (!m_tmcaDBManager) return;

    const QString year = getYear();
    const QString month = getMonth();

    const QString postage = m_postageBox ? m_postageBox->text().trimmed() : QString();
    const QString count = m_countBox ? m_countBox->text().trimmed() : QString();

    m_tmcaDBManager->saveJobState(year, month,
                                 static_cast<int>(m_currentHtmlState),
                                 m_jobDataLocked, m_postageDataLocked,
                                 postage, count,
                                 m_lastExecutedScript);
}

void TMCAController::loadJobState()
{
    loadJobState(getJobNumber());
}

void TMCAController::loadJobState(const QString& jobNumber)
{
    Q_UNUSED(jobNumber);

    if (!m_tmcaDBManager) return;

    int htmlState = 0;
    bool jobLocked = false;
    bool postageLocked = false;
    QString postage;
    QString count;
    QString lastScript;

    const QString year = getYear();
    const QString month = getMonth();

    if (m_tmcaDBManager->loadJobState(year, month, htmlState, jobLocked, postageLocked, postage, count, lastScript)) {
        m_currentHtmlState = static_cast<HtmlDisplayState>(htmlState);
        m_jobDataLocked = jobLocked;
        m_postageDataLocked = postageLocked;
        m_lastExecutedScript = lastScript;

        if (m_postageBox) m_postageBox->setText(postage);
        if (m_countBox) m_countBox->setText(count);
    } else {
        m_currentHtmlState = UninitializedState;
        m_jobDataLocked = false;
        m_postageDataLocked = false;
        m_lastExecutedScript.clear();
    }
}

QString TMCAController::getJobNumber() const
{
    return m_jobNumberBox ? m_jobNumberBox->text().trimmed() : QString();
}

QString TMCAController::getYear() const
{
    return m_yearDDbox ? m_yearDDbox->currentText().trimmed() : QString();
}

QString TMCAController::getMonth() const
{
    return m_monthDDbox ? m_monthDDbox->currentText().trimmed() : QString();
}

bool TMCAController::isJobDataLocked() const
{
    return m_jobDataLocked;
}

bool TMCAController::isPostageDataLocked() const
{
    return m_postageDataLocked;
}

bool TMCAController::hasJobData() const
{
    return !getJobNumber().isEmpty() && !getYear().isEmpty() && !getMonth().isEmpty();
}

void TMCAController::autoSaveAndCloseCurrentJob()
{
    if (m_jobDataLocked) {
        saveJobState();
    }
    resetToDefaults();
}

// ---- BaseTrackerController ----
void TMCAController::outputToTerminal(const QString& message, MessageType type)
{
    if (!m_terminalWindow) return;

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString colorClass;

    switch (type) {
    case Error:   colorClass = "error"; break;
    case Success: colorClass = "success"; break;
    case Warning: colorClass = "warning"; break;
    case Info:
    default:      colorClass = ""; break;
    }

    QString formattedMessage = QString("[%1] %2").arg(timestamp, message);
    if (!colorClass.isEmpty()) {
        formattedMessage = QString("<span class=\"%1\">%2</span>").arg(colorClass, formattedMessage);
    }

    m_terminalWindow->append(formattedMessage);

    QTextCursor cursor = m_terminalWindow->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_terminalWindow->setTextCursor(cursor);

    // Persist terminal log per period (matches other TM modules)
    if (m_tmcaDBManager && !getYear().isEmpty() && !getMonth().isEmpty()) {
        m_tmcaDBManager->saveTerminalLog(getYear(), getMonth(), message);
    }
}

QTableView* TMCAController::getTrackerWidget() const
{
    return m_tracker;
}

QSqlTableModel* TMCAController::getTrackerModel() const
{
    return m_trackerModel;
}

QStringList TMCAController::getTrackerHeaders() const
{
    return {"JOB", "DESCRIPTION", "POSTAGE", "COUNT", "AVG RATE", "CLASS", "SHAPE", "PERMIT"};
}

QList<int> TMCAController::getVisibleColumns() const
{
    return {1, 2, 3, 4, 5, 6, 7, 8};
}

QString TMCAController::formatCellData(int columnIndex, const QString& cellData) const
{
    // Same formatting as TMFLER (POSTAGE col 3, COUNT col 4)
    if (columnIndex == 3) {
        QString clean = cellData;
        if (clean.startsWith("$")) clean.remove(0, 1);
        bool ok;
        double val = clean.toDouble(&ok);
        if (ok) return QString("$%L1").arg(val, 0, 'f', 2);
    }
    if (columnIndex == 4) {
        bool ok;
        qlonglong val = cellData.toLongLong(&ok);
        if (ok) return QString("%L1").arg(val);
    }
    return cellData;
}

QString TMCAController::formatCellDataForCopy(int columnIndex, const QString& cellData) const
{
    // Same formatting as TMFLER copy: visible column positions
    if (columnIndex == 2) { // POSTAGE visible position
        QString clean = cellData;
        if (clean.startsWith("$")) clean.remove(0, 1);
        bool ok;
        double val = clean.toDouble(&ok);
        if (ok) return QString("$%L1").arg(val, 0, 'f', 2);
    }
    if (columnIndex == 3) { // COUNT visible position
        QString cleanData = cellData;
        cleanData.remove(',');
        bool ok;
        qlonglong val = cleanData.toLongLong(&ok);
        if (ok) return QString::number(val);
    }
    return cellData;
}

// ---- Wiring helpers ----
void TMCAController::populateDropdowns()
{
    if (m_yearDDbox) {
        m_yearDDbox->clear();
        const int currentYear = QDate::currentDate().year();
        m_yearDDbox->addItem(QString());
        m_yearDDbox->addItem(QString::number(currentYear - 1));
        m_yearDDbox->addItem(QString::number(currentYear));
        m_yearDDbox->addItem(QString::number(currentYear + 1));
    }

    if (m_monthDDbox) {
        m_monthDDbox->clear();
        m_monthDDbox->addItem(QString());
        for (int i = 1; i <= 12; ++i) {
            m_monthDDbox->addItem(QString("%1").arg(i, 2, 10, QChar('0')));
        }
    }
}

void TMCAController::onYearChanged(const QString&)
{
    updateHtmlDisplay();
}

void TMCAController::onMonthChanged(const QString&)
{
    updateHtmlDisplay();
}

void TMCAController::connectSignals()
{
    // Dominant behavior: prevent direct unlock by unchecking lock; unlock happens via Edit
    if (m_jobDataLockBtn) {
        connect(m_jobDataLockBtn, &QToolButton::clicked, this, &TMCAController::onJobDataLockClicked);
    }
    if (m_editBtn) {
        connect(m_editBtn, &QToolButton::clicked, this, &TMCAController::onEditButtonClicked);
    }
    if (m_postageLockBtn) {
        connect(m_postageLockBtn, &QToolButton::clicked, this, &TMCAController::onPostageLockClicked);
    }

    if (m_yearDDbox) {
        connect(m_yearDDbox, &QComboBox::currentTextChanged, this, &TMCAController::onYearChanged);
    }
    if (m_monthDDbox) {
        connect(m_monthDDbox, &QComboBox::currentTextChanged, this, &TMCAController::onMonthChanged);
    }

    // Auto-save on postage/count changes when job is locked
    if (m_postageBox) {
        connect(m_postageBox, &QLineEdit::textChanged, this, [this]() {
            if (m_jobDataLocked) saveJobState();
        });
    }
    if (m_countBox) {
        connect(m_countBox, &QLineEdit::textChanged, this, [this]() {
            if (m_jobDataLocked) saveJobState();
        });
    }
}

void TMCAController::setupInitialState()
{
    m_jobDataLocked = false;
    m_postageDataLocked = false;
    m_currentHtmlState = UninitializedState;
    m_lastExecutedScript.clear();
}

void TMCAController::updateLockStates()
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

void TMCAController::updateButtonStates()
{
    const bool jobFieldsEnabled = !m_jobDataLocked;

    if (m_jobNumberBox) m_jobNumberBox->setEnabled(jobFieldsEnabled);
    if (m_yearDDbox) m_yearDDbox->setEnabled(jobFieldsEnabled);
    if (m_monthDDbox) m_monthDDbox->setEnabled(jobFieldsEnabled);

    // Postage fields enabled only if job locked and postage not locked
    const bool postageFieldsEnabled = m_jobDataLocked && !m_postageDataLocked;
    if (m_postageBox) m_postageBox->setEnabled(postageFieldsEnabled);
    if (m_countBox) m_countBox->setEnabled(postageFieldsEnabled);

    // DropWindow disabled when job is locked (matches TMFLER)
    if (m_dropWindow) {
        const bool enabled = !m_jobDataLocked;
        m_dropWindow->setEnabled(enabled);
        m_dropWindow->setAcceptDrops(enabled);
    }

    if (m_editBtn) m_editBtn->setEnabled(m_jobDataLocked);

    // Postage lock enabled only when job locked
    if (m_postageLockBtn) m_postageLockBtn->setEnabled(m_jobDataLocked);

    // Script buttons
    if (m_runInitialBtn) m_runInitialBtn->setEnabled(m_jobDataLocked);
    if (m_finalStepBtn) m_finalStepBtn->setEnabled(m_postageDataLocked);
}

void TMCAController::updateHtmlDisplay()
{
    if (!m_textBrowser) return;

    HtmlDisplayState target = determineHtmlState();
    if (m_currentHtmlState == UninitializedState || m_currentHtmlState != target) {
        m_currentHtmlState = target;

        // TMCA baseline: only one HTML resource page exists; map both states to it.
        loadHtmlFile(":/resources/tmca/default.html");
    }
}

void TMCAController::loadHtmlFile(const QString& resourcePath)
{
    if (!m_textBrowser) return;

    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        outputToTerminal("Failed to load HTML resource: " + resourcePath, Warning);
        return;
    }

    const QString html = QString::fromUtf8(file.readAll());
    file.close();
    m_textBrowser->setHtml(html);
}

TMCAController::HtmlDisplayState TMCAController::determineHtmlState() const
{
    // Dominant pattern: instructions state shown when job locked (some modules flip this).
    // For TMCA baseline, keep consistent with TMFLER: show instructions when job locked.
    return m_jobDataLocked ? InstructionsState : DefaultState;
}

bool TMCAController::validateJobData() const
{
    if (getJobNumber().isEmpty() || getYear().isEmpty() || getMonth().isEmpty()) {
        return false;
    }
    return true;
}

bool TMCAController::validatePostageData() const
{
    // Minimal validation (match other controllers' baseline): require non-empty
    const QString postage = m_postageBox ? m_postageBox->text().trimmed() : QString();
    const QString count = m_countBox ? m_countBox->text().trimmed() : QString();
    return !postage.isEmpty() && !count.isEmpty();
}

// ---- Lock handlers ----
void TMCAController::onJobDataLockClicked()
{
    if (!m_jobDataLockBtn) return;

    if (m_jobDataLockBtn->isChecked()) {
        // Attempt to lock
        if (!validateJobData()) {
            outputToTerminal("Cannot lock: job number, year, and month are required.", Error);
            m_jobDataLockBtn->setChecked(false);
            return;
        }

        m_jobDataLocked = true;

        // Save job record (one job per year/month)
        if (m_tmcaDBManager) {
            m_tmcaDBManager->saveJob(getJobNumber(), getYear(), getMonth());
        }

        saveJobState();
        outputToTerminal("Job data locked.", Success);

        updateLockStates();
        updateButtonStates();
        m_currentHtmlState = UninitializedState;
        updateHtmlDisplay();

        emit jobOpened();
    } else {
        // Prevent direct unlock by unchecking lock button (dominant behavior)
        m_jobDataLockBtn->setChecked(true);
    }
}

void TMCAController::onEditButtonClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot edit until job data is locked.", Error);
        if (m_editBtn) m_editBtn->setChecked(false);
        return;
    }

    if (m_editBtn && m_editBtn->isChecked()) {
        // Edit returns to UNLOCKED state (two-state model)
        m_jobDataLocked = false;
        if (m_jobDataLockBtn) m_jobDataLockBtn->setChecked(false);

        outputToTerminal("Job data unlocked for editing.", Info);

        updateLockStates();
        updateButtonStates();
        m_currentHtmlState = UninitializedState;
        updateHtmlDisplay();
    }
}

void TMCAController::onPostageLockClicked()
{
    if (!m_postageLockBtn) return;

    if (!m_jobDataLocked) {
        outputToTerminal("Cannot lock postage: job data must be locked first.", Error);
        m_postageLockBtn->setChecked(false);
        return;
    }

    if (m_postageLockBtn->isChecked()) {
        if (!validatePostageData()) {
            outputToTerminal("Cannot lock postage: postage and count are required.", Error);
            m_postageLockBtn->setChecked(false);
            return;
        }

        m_postageDataLocked = true;
        outputToTerminal("Postage data locked.", Success);

        saveJobState();
    } else {
        m_postageDataLocked = false;
        outputToTerminal("Postage data unlocked.", Info);

        saveJobState();
    }

    updateLockStates();
    updateButtonStates();
}

// ---- Script handlers ----
void TMCAController::onRunInitialClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot run initial: job data must be locked first.", Error);
        return;
    }

    executeScript("01 INITIAL");
}

void TMCAController::onFinalStepClicked()
{
    if (!m_postageDataLocked) {
        outputToTerminal("Cannot run final step: postage must be locked first.", Error);
        return;
    }

    // Two-phase: run prearchive first
    executeScript("02 FINAL PROCESS::prearchive");
}

void TMCAController::executeScript(const QString& scriptName)
{
    if (!m_fileManager || !m_scriptRunner) return;

    if (m_scriptRunner->isRunning()) {
        outputToTerminal("A script is already running. Please wait for it to finish.", Warning);
        return;
    }

    QString resolvedName = scriptName;
    QString mode;

    // Support "script::mode" pattern without adding new members
    if (resolvedName.contains("::")) {
        const QStringList parts = resolvedName.split("::");
        resolvedName = parts.value(0);
        mode = parts.value(1);
    }

    const QString scriptPath = m_fileManager->getScriptPath(resolvedName);
    if (!QFile::exists(scriptPath)) {
        outputToTerminal("Script not found: " + scriptPath, Error);
        return;
    }

    m_lastExecutedScript = scriptName;

    QStringList args;
    const QString jobNumber = getJobNumber();
    const QString year = getYear();
    const QString month = getMonth();

    if (!mode.isEmpty()) {
        args << jobNumber << year << month << "--mode" << mode;
    } else {
        args << jobNumber << year << month;
    }

    outputToTerminal(QString("Executing script: %1").arg(resolvedName), Info);
    outputToTerminal(QString("Script path: %1").arg(scriptPath), Info);
    outputToTerminal(QString("Arguments: %1").arg(args.join(" ")), Info);

    m_scriptRunner->runScript(scriptPath, args);
}

void TMCAController::onScriptOutput(const QString& output)
{
    outputToTerminal(output, Info);

    // Marker-driven pause popup (mirrors HEALTHY/TMFLER behavior but using a generic dialog)
    if (output.contains("=== PAUSE_FOR_EMAIL ===")) {
        outputToTerminal("Detected PAUSE_FOR_EMAIL. Showing TMCA popup...", Info);

        // Show a generic location dialog; for baseline use CA base path
        const QString location = m_fileManager ? m_fileManager->getBasePath() : QString("C:/Goji/TRACHMAR/CA");

        QPointer<NASLinkDialog> dlg = new NASLinkDialog(
            "TMCA Action Required",
            "Processing paused. Use the path below as needed, then close this window to continue.",
            location,
            nullptr
        );
        dlg->setAttribute(Qt::WA_DeleteOnClose);

        // When user closes dialog, resume the prearchive script if it is waiting
        connect(dlg, &QDialog::finished, this, [this](int) {
            if (m_scriptRunner && m_scriptRunner->isRunning()) {
                m_scriptRunner->writeToScript("\n");
            }
        });

        // Modal pause (matches TMHealthy pause/resume behavior)
        dlg->exec();
        return;
    }

    if (output.contains("=== RESUME_PROCESSING ===")) {
        outputToTerminal("Script resumed processing...", Info);
        return;
    }
}

void TMCAController::onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::CrashExit) {
        outputToTerminal("Script crashed unexpectedly.", Error);
        return;
    }

    if (exitCode == 0) {
        outputToTerminal("Script completed successfully.", Success);

        // If we just completed prearchive, trigger archive phase (TMFLER pattern)
        if (m_lastExecutedScript == "02 FINAL PROCESS::prearchive") {
            triggerArchivePhase();
        }

        if (m_trackerModel) {
            m_trackerModel->select();
        }
    } else {
        outputToTerminal(QString("Script failed with exit code: %1").arg(exitCode), Error);
    }

    saveJobState();
    updateHtmlDisplay();
}

void TMCAController::triggerArchivePhase()
{
    if (!m_fileManager || !m_scriptRunner) {
        outputToTerminal("Error: Missing file manager or script runner.", Error);
        return;
    }

    const QString scriptPath = m_fileManager->getScriptPath("02 FINAL PROCESS");
    if (!QFile::exists(scriptPath)) {
        outputToTerminal("Archive script not found: " + scriptPath, Error);
        return;
    }

    QStringList args;
    args << getJobNumber() << getYear() << getMonth() << "--mode" << "archive";

    outputToTerminal("Starting TMCA archive phase...", Info);
    m_scriptRunner->runScript(scriptPath, args);
}

// ---- DropWindow ----
void TMCAController::setupDropWindow()
{
    if (!m_dropWindow || !m_fileManager) return;

    // Target directory: C:\Goji\TRACHMAR\CA\DROP
    m_dropWindow->setTargetDirectory(m_fileManager->getDropPath());

    // Match DropWindow patterns: let it handle copy and then we route by token
    connect(m_dropWindow, &DropWindow::filesDropped, this, &TMCAController::onFilesDropped);

    // Use string-based connect to avoid compile errors if DropWindow's error signal name varies
    // across Qt versions / project revisions. If the signal exists, the connection will succeed.
    connect(m_dropWindow, SIGNAL(dropError(QString)), this, SLOT(onFileDropError(QString)));
}

void TMCAController::onFilesDropped(const QStringList& filePaths)
{
    outputToTerminal(QString("Files received: %1 file(s) dropped").arg(filePaths.size()), Success);

    for (const QString& path : filePaths) {
        QFileInfo fi(path);
        outputToTerminal(QString("  - %1").arg(fi.fileName()), Info);
        routeDroppedFile(path);
    }
}

void TMCAController::onFileDropError(const QString& errorMessage)
{
    outputToTerminal("File drop error: " + errorMessage, Warning);
}

void TMCAController::showTableContextMenu(const QPoint& pos)
{
    if (!m_tracker) return;

    QMenu menu(m_tracker);
    QAction* copyAction = menu.addAction("Copy Selected Row");
    QAction* selectedAction = menu.exec(m_tracker->mapToGlobal(pos));

    if (selectedAction == copyAction) {
        copyFormattedRow();
    }
}

void TMCAController::routeDroppedFile(const QString& absoluteFilePath)
{
    if (!m_fileManager) return;

    QFileInfo fi(absoluteFilePath);
    if (!fi.exists()) {
        outputToTerminal("Routing skipped (missing file): " + absoluteFilePath, Warning);
        return;
    }

    const QString fileNameUpper = fi.fileName().toUpper();

    QString destinationDir;
    QString reason;

    // First match wins: BA before EDR
    if (fileNameUpper.contains("_BA_")) {
        destinationDir = m_fileManager->getBAInputPath();
        reason = "Matched _BA_ -> BA\\INPUT";
    } else if (fileNameUpper.contains("_EDR_")) {
        destinationDir = m_fileManager->getEDRInputPath();
        reason = "Matched _EDR_ -> EDR\\INPUT";
    } else {
        outputToTerminal(QString("Routing: %1 (no match, left in DROP)").arg(fi.fileName()), Info);
        return;
    }

    QDir().mkpath(destinationDir);

    const QString destPath = QDir(destinationDir).filePath(fi.fileName());

    // Use BaseFileSystemManager moveFile to preserve patterns/logging
    if (m_fileManager->moveFile(absoluteFilePath, destPath)) {
        outputToTerminal(QString("Routing: %1 -> %2 (%3)")
                         .arg(fi.fileName(), destinationDir, reason), Success);
    } else {
        outputToTerminal(QString("Routing FAILED: %1 -> %2").arg(fi.fileName(), destinationDir), Error);
    }
}
