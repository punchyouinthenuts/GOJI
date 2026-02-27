#include "tmcacontroller.h"
#include "tmcaemaildialog.h"

#include "databasemanager.h"
#include "logger.h"
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
#include <QSqlQuery>
#include <QSqlError>
#include <QPointer>
#include <QTextCursor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QRegularExpression>
#include <cmath>

// ============================================================
// Constants (Part 1, Section 7)
// ============================================================
static const QString TMCA_SCRIPT_PATH   = "C:/Goji/scripts/TRACHMAR/CA/TMCA.py";
static const QString TMCA_BA_INPUT      = "C:/Goji/TRACHMAR/CA/BA/INPUT";
static const QString TMCA_EDR_INPUT     = "C:/Goji/TRACHMAR/CA/EDR/INPUT";
static const QString TMCA_W_DEST        = "W:/";
static const QString TMCA_W_FALLBACK    = "C:/Users/JCox/Desktop/MOVE TO BUSKRO";
static const QString TMCA_NAS_BASE      = "\\\\NAS1069D9\\AMPrintData";
static const double  TMCA_DEFAULT_RATE  = 0.69;
static const QString TMCA_CLASS         = "FC";
static const QString TMCA_SHAPE         = "LTR";
static const QString TMCA_PERMIT        = "METER";

static const QString JSON_BEGIN_MARKER  = "=== TMCA_RESULT_BEGIN ===";
static const QString JSON_END_MARKER    = "=== TMCA_RESULT_END ===";

// ============================================================
// Constructor / Destructor
// ============================================================

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
    , m_lastExecutedScript()
    , m_trackerModel(nullptr)
    , m_currentPhase(PhaseNone)
    , m_pendingJobType()
    , m_pendingJobNumber()
    , m_pendingYear()
    , m_pendingLaValidCount(0)
    , m_pendingSaValidCount(0)
    , m_pendingLaBlankCount(0)
    , m_pendingSaBlankCount(0)
    , m_pendingLaPostage(0.0)
    , m_pendingSaPostage(0.0)
    , m_pendingRate(TMCA_DEFAULT_RATE)
    , m_pendingNasDest()
    , m_pendingWDest()
    , m_pendingMergedFiles()
    , m_capturingJson(false)
    , m_jsonAccumulator()
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

// ============================================================
// Initialization
// ============================================================

void TMCAController::initializeComponents()
{
    m_fileManager    = new TMCAFileManager(nullptr);
    m_tmcaDBManager  = TMCADBManager::instance();

    m_scriptRunner = new ScriptRunner(this);
    // Disable input wrapper — TMCA.py is non-interactive
    m_scriptRunner->setInputWrapperEnabled(false);

    connect(m_scriptRunner, &ScriptRunner::scriptOutput,
            this, &TMCAController::onScriptOutput);
    connect(m_scriptRunner, &ScriptRunner::scriptError,
            this, &TMCAController::onScriptError);
    connect(m_scriptRunner, &ScriptRunner::scriptFinished,
            this, &TMCAController::onScriptFinished);

    createBaseDirectories();

    if (m_tmcaDBManager) {
        m_tmcaDBManager->initializeTables();
    }

    setupInitialState();
}

void TMCAController::initializeAfterConstruction()
{
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

void TMCAController::setupInitialState()
{
    m_jobDataLocked     = false;
    m_postageDataLocked = false;
    m_currentHtmlState  = UninitializedState;
    m_lastExecutedScript.clear();

    m_currentPhase          = PhaseNone;
    m_pendingJobType.clear();
    m_pendingJobNumber.clear();
    m_pendingYear.clear();
    m_pendingLaValidCount   = 0;
    m_pendingSaValidCount   = 0;
    m_pendingLaBlankCount   = 0;
    m_pendingSaBlankCount   = 0;
    m_pendingLaPostage      = 0.0;
    m_pendingSaPostage      = 0.0;
    m_pendingRate           = TMCA_DEFAULT_RATE;
    m_pendingNasDest.clear();
    m_pendingWDest.clear();
    m_pendingMergedFiles.clear();
    m_capturingJson         = false;
    m_jsonAccumulator.clear();
}

// ============================================================
// Widget setters
// ============================================================

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
        connect(m_jobDataLockBtn, &QToolButton::clicked,
                this, &TMCAController::onJobDataLockClicked);
    }
}

void TMCAController::setEditButton(QToolButton* button)
{
    m_editBtn = button;
    if (m_editBtn) {
        connect(m_editBtn, &QToolButton::clicked,
                this, &TMCAController::onEditButtonClicked);
    }
}

void TMCAController::setPostageLockButton(QToolButton* button)
{
    m_postageLockBtn = button;
    if (m_postageLockBtn) {
        connect(m_postageLockBtn, &QToolButton::clicked,
                this, &TMCAController::onPostageLockClicked);
    }
}

void TMCAController::setRunInitialButton(QPushButton* button)
{
    m_runInitialBtn = button;
    if (m_runInitialBtn) {
        connect(m_runInitialBtn, &QPushButton::clicked,
                this, &TMCAController::onRunInitialClicked);
    }
}

void TMCAController::setFinalStepButton(QPushButton* button)
{
    m_finalStepBtn = button;
    if (m_finalStepBtn) {
        connect(m_finalStepBtn, &QPushButton::clicked,
                this, &TMCAController::onFinalStepClicked);
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
    if (!m_tmcaDBManager) return;

    if (m_trackerModel) {
        m_trackerModel->deleteLater();
        m_trackerModel = nullptr;
    }

    m_trackerModel = new QSqlTableModel(this, DatabaseManager::instance()->getDatabase());
    m_trackerModel->setTable("tm_ca_log");
    m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);

    m_trackerModel->setHeaderData(1, Qt::Horizontal, tr("JOB"));
    m_trackerModel->setHeaderData(2, Qt::Horizontal, tr("DESCRIPTION"));
    m_trackerModel->setHeaderData(3, Qt::Horizontal, tr("POSTAGE"));
    m_trackerModel->setHeaderData(4, Qt::Horizontal, tr("COUNT"));
    m_trackerModel->setHeaderData(5, Qt::Horizontal, tr("AVG RATE"));
    m_trackerModel->setHeaderData(6, Qt::Horizontal, tr("CLASS"));
    m_trackerModel->setHeaderData(7, Qt::Horizontal, tr("SHAPE"));
    m_trackerModel->setHeaderData(8, Qt::Horizontal, tr("PERMIT"));

    m_trackerModel->setSort(0, Qt::DescendingOrder);
    m_trackerModel->select();

    m_tracker->setModel(m_trackerModel);

    // Hide non-visible columns (col 0 = id)
    QList<int> visible = getVisibleColumns();
    for (int i = 0; i < m_trackerModel->columnCount(); ++i) {
        m_tracker->setColumnHidden(i, !visible.contains(i));
    }
    m_tracker->setColumnHidden(0, true);

    m_tracker->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tracker->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tracker->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tracker->setAlternatingRowColors(true);
    m_tracker->verticalHeader()->setVisible(false);
    m_tracker->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    // Optimized layout (mirrors TMFLER)
    const int tableWidth    = 611;
    const int borderWidth   = 2;
    const int availableWidth = tableWidth - borderWidth;

    struct ColumnSpec {
        QString header;
        QString maxContent;
        int minWidth;
    };
    QList<ColumnSpec> columns = {
        {"JOB",         "88888",         56},
        {"DESCRIPTION", "TM CA EDR LA",  140},
        {"POSTAGE",     "$888,888.88",    90},
        {"COUNT",       "88,888",         55},
        {"AVG RATE",    "0.888",          55},
        {"CLASS",       "STD",            45},
        {"SHAPE",       "LTR",            40},
        {"PERMIT",      "METER",          50}
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
            int headerWidth  = fm.horizontalAdvance(col.header) + 12;
            int contentWidth = fm.horizontalAdvance(col.maxContent) + 12;
            int colWidth     = qMax(headerWidth, qMax(contentWidth, col.minWidth));
            totalWidth += colWidth;
            if (totalWidth > availableWidth) { fits = false; break; }
        }
        if (fits) { optimalFontSize = fontSize; break; }
    }

    QFont tableFont("Blender Pro Bold", optimalFontSize);
    m_tracker->setFont(tableFont);
    fm = QFontMetrics(tableFont);

    int col = 1;
    for (const ColumnSpec& spec : columns) {
        int headerW  = fm.horizontalAdvance(spec.header) + 12;
        int contentW = fm.horizontalAdvance(spec.maxContent) + 12;
        int w        = qMax(headerW, qMax(contentW, spec.minWidth));
        m_tracker->setColumnWidth(col++, w);
    }

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

    m_tracker->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tracker, &QTableView::customContextMenuRequested,
            this, &TMCAController::showTableContextMenu);

    outputToTerminal("Tracker model initialized successfully", Success);
}

void TMCAController::setDropWindow(DropWindow* dropWindow)
{
    m_dropWindow = dropWindow;
    if (m_dropWindow) {
        setupDropWindow();
    }
}

// ============================================================
// Tracker helpers
// ============================================================

void TMCAController::refreshTrackerTable()
{
    if (m_trackerModel) {
        m_trackerModel->select();
    }
}

// ============================================================
// Job management
// ============================================================

bool TMCAController::loadJob(const QString& jobNumber, const QString& year, const QString& month)
{
    if (!m_tmcaDBManager) return false;

    if (m_jobNumberBox) m_jobNumberBox->setText(jobNumber);
    if (m_yearDDbox)    m_yearDDbox->setCurrentText(year);
    if (m_monthDDbox)   m_monthDDbox->setCurrentText(month);

    loadJobState(jobNumber);

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
    if (m_postageBox)   m_postageBox->clear();
    if (m_countBox)     m_countBox->clear();

    m_jobDataLocked     = false;
    m_postageDataLocked = false;
    m_lastExecutedScript.clear();
    m_currentHtmlState  = UninitializedState;

    // Reset two-phase run state
    m_currentPhase = PhaseNone;
    m_pendingJobType.clear();
    m_pendingJobNumber.clear();
    m_pendingYear.clear();
    m_pendingLaValidCount = 0;
    m_pendingSaValidCount = 0;
    m_pendingLaBlankCount = 0;
    m_pendingSaBlankCount = 0;
    m_pendingLaPostage    = 0.0;
    m_pendingSaPostage    = 0.0;
    m_pendingRate         = TMCA_DEFAULT_RATE;
    m_pendingNasDest.clear();
    m_pendingWDest.clear();
    m_pendingMergedFiles.clear();
    m_capturingJson = false;
    m_jsonAccumulator.clear();

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

    const QString jobNumber = getJobNumber();
    const QString year      = getYear();
    const QString month     = getMonth();

    // Guard: all three keys required for UNIQUE(job_number, year, month)
    if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty()) return;

    const QString postage = m_postageBox ? m_postageBox->text().trimmed() : QString();
    const QString count   = m_countBox   ? m_countBox->text().trimmed()   : QString();

    m_tmcaDBManager->saveJobState(jobNumber, year, month,
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
    if (!m_tmcaDBManager) return;

    // Guard: all three keys required for UNIQUE(job_number, year, month)
    const QString year  = getYear();
    const QString month = getMonth();
    if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty()) return;

    int htmlState = 0;
    bool jobLocked = false;
    bool postageLocked = false;
    QString postage, count, lastScript;

    if (m_tmcaDBManager->loadJobState(jobNumber, year, month,
                                      htmlState, jobLocked, postageLocked,
                                      postage, count, lastScript)) {
        m_currentHtmlState  = static_cast<HtmlDisplayState>(htmlState);
        m_jobDataLocked     = jobLocked;
        m_postageDataLocked = postageLocked;
        m_lastExecutedScript = lastScript;
        if (m_postageBox) m_postageBox->setText(postage);
        if (m_countBox)   m_countBox->setText(count);
    } else {
        m_currentHtmlState  = UninitializedState;
        m_jobDataLocked     = false;
        m_postageDataLocked = false;
        m_lastExecutedScript.clear();
    }
}

void TMCAController::autoSaveAndCloseCurrentJob()
{
    if (m_jobDataLocked) {
        saveJobState();
    }
    resetToDefaults();
}

// ============================================================
// Public accessors
// ============================================================

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

// ============================================================
// BaseTrackerController implementation
// ============================================================

void TMCAController::outputToTerminal(const QString& message, MessageType type)
{
    if (!m_terminalWindow) return;

    const QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString colorClass;

    switch (type) {
    case Error:   colorClass = "error";   break;
    case Success: colorClass = "success"; break;
    case Warning: colorClass = "warning"; break;
    case Info:
    default:      colorClass = "";        break;
    }

    QString formattedMessage = QString("[%1] %2").arg(timestamp, message);
    if (!colorClass.isEmpty()) {
        formattedMessage = QString("<span class=\"%1\">%2</span>")
                           .arg(colorClass, formattedMessage);
    }

    m_terminalWindow->append(formattedMessage);

    QTextCursor cursor = m_terminalWindow->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_terminalWindow->setTextCursor(cursor);

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
    if (columnIndex == 3) {   // POSTAGE
        QString clean = cellData;
        if (clean.startsWith("$")) clean.remove(0, 1);
        bool ok;
        double val = clean.toDouble(&ok);
        if (ok) return QString("$%1").arg(val, 0, 'f', 2);
    }
    if (columnIndex == 4) {   // COUNT
        bool ok;
        qlonglong val = cellData.toLongLong(&ok);
        if (ok) return QString("%L1").arg(val);
    }
    return cellData;
}

QString TMCAController::formatCellDataForCopy(int columnIndex, const QString& cellData) const
{
    if (columnIndex == 2) {   // POSTAGE visible position
        QString clean = cellData;
        if (clean.startsWith("$")) clean.remove(0, 1);
        bool ok;
        double val = clean.toDouble(&ok);
        if (ok) return QString("$%1").arg(val, 0, 'f', 2);
    }
    if (columnIndex == 3) {   // COUNT visible position
        QString cleanData = cellData;
        cleanData.remove(',');
        bool ok;
        qlonglong val = cleanData.toLongLong(&ok);
        if (ok) return QString::number(val);
    }
    return cellData;
}

// ============================================================
// Signal wiring helpers
// ============================================================

void TMCAController::connectSignals()
{
    if (m_yearDDbox) {
        connect(m_yearDDbox, &QComboBox::currentTextChanged,
                this, &TMCAController::onYearChanged);
    }
    if (m_monthDDbox) {
        connect(m_monthDDbox, &QComboBox::currentTextChanged,
                this, &TMCAController::onMonthChanged);
    }
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

// ============================================================
// Lock state management
// ============================================================

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
    if (m_yearDDbox)    m_yearDDbox->setEnabled(jobFieldsEnabled);
    if (m_monthDDbox)   m_monthDDbox->setEnabled(jobFieldsEnabled);

    const bool postageFieldsEnabled = m_jobDataLocked && !m_postageDataLocked;
    if (m_postageBox) m_postageBox->setEnabled(postageFieldsEnabled);
    if (m_countBox)   m_countBox->setEnabled(postageFieldsEnabled);

    if (m_dropWindow) {
        const bool enabled = !m_jobDataLocked;
        m_dropWindow->setEnabled(enabled);
        m_dropWindow->setAcceptDrops(enabled);
    }

    if (m_editBtn)       m_editBtn->setEnabled(m_jobDataLocked);
    if (m_postageLockBtn) m_postageLockBtn->setEnabled(m_jobDataLocked);

    // RUN INITIAL enabled when job is locked and no script currently running
    const bool scriptRunning = m_scriptRunner && m_scriptRunner->isRunning();
    if (m_runInitialBtn) m_runInitialBtn->setEnabled(m_jobDataLocked && !scriptRunning);
    if (m_finalStepBtn)  m_finalStepBtn->setEnabled(m_postageDataLocked && !scriptRunning);
}

// ============================================================
// HTML display management
// ============================================================

void TMCAController::updateHtmlDisplay()
{
    if (!m_textBrowser) return;

    HtmlDisplayState target = determineHtmlState();
    if (m_currentHtmlState == UninitializedState || m_currentHtmlState != target) {
        m_currentHtmlState = target;
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
    return m_jobDataLocked ? InstructionsState : DefaultState;
}

// ============================================================
// Lock button handlers
// ============================================================

void TMCAController::onJobDataLockClicked()
{
    if (!m_jobDataLockBtn) return;

    if (m_jobDataLockBtn->isChecked()) {
        if (!validateJobData()) {
            outputToTerminal("Cannot lock: job number, year, and month are required.", Error);
            m_jobDataLockBtn->setChecked(false);
            return;
        }

        m_jobDataLocked = true;

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
        // Prevent direct unlock via button — unlock only via Edit
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

// ============================================================
// RUN INITIAL — preflight + Phase 1 (Part 3, Sections 2-4)
// ============================================================

void TMCAController::onRunInitialClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot run: job data must be locked first.", Error);
        return;
    }

    // Single-run guard (Part 6, Section 7)
    if (m_scriptRunner && m_scriptRunner->isRunning()) {
        outputToTerminal("A script is already running. Please wait for it to finish.", Warning);
        return;
    }

    // Preflight scan — validates job number and detects job type
    QString detectedJobType;
    if (!preflightScan(detectedJobType)) {
        // preflightScan already printed the error/warning
        return;
    }

    // Run Phase 1
    runPhase1(detectedJobType);
}

// ============================================================
// FINAL STEP — reserved slot (not the primary TMCA flow)
// ============================================================

void TMCAController::onFinalStepClicked()
{
    outputToTerminal("FINAL STEP is not used in the TMCA workflow. "
                     "Archive runs automatically after closing the email dialog.", Info);
}

// ============================================================
// Preflight scan (Part 3, Sections 1-2)
// ============================================================

bool TMCAController::validateJobNumber(const QString& jobNumber) const
{
    // Exactly 5 characters, each must be ASCII '0'..'9' (no Unicode digits)
    if (jobNumber.length() != 5) return false;
    for (const QChar& c : jobNumber) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

QStringList TMCAController::scanEligibleFiles(const QString& folderPath,
                                              const QStringList& tokens) const
{
    QStringList result;
    QDir dir(folderPath);
    if (!dir.exists()) return result;

    static const QStringList validExts = {"csv", "xls", "xlsx"};

    const QFileInfoList entries = dir.entryInfoList(QDir::Files);
    for (const QFileInfo& fi : entries) {
        if (!validExts.contains(fi.suffix().toLower())) continue;
        const QString nameUpper = fi.fileName().toUpper();
        for (const QString& token : tokens) {
            if (nameUpper.contains(token.toUpper())) {
                result.append(fi.absoluteFilePath());
                break;
            }
        }
    }
    return result;
}

void TMCAController::outputRedWarning(const QString& firstLine,
                                      const QStringList& bodyLines)
{
    // First line: "WARNING!!!" prefix, emitted as Error (red)
    outputToTerminal("WARNING!!! " + firstLine, Error);
    for (const QString& line : bodyLines) {
        outputToTerminal(line, Error);
    }
}

bool TMCAController::preflightScan(QString& detectedJobType)
{
    // 1. Validate job number
    const QString jobNumber = getJobNumber();
    if (!validateJobNumber(jobNumber)) {
        outputToTerminal(
            QString("Invalid job number \"%1\": must be exactly 5 digits.").arg(jobNumber),
            Error);
        return false;
    }

    // 2. Scan both input folders
    const QStringList baFiles  = scanEligibleFiles(TMCA_BA_INPUT,  {"LA_BA",  "SA_BA"});
    const QStringList edrFiles = scanEligibleFiles(TMCA_EDR_INPUT, {"LA_EDR", "SA_EDR"});

    const bool hasBa  = !baFiles.isEmpty();
    const bool hasEdr = !edrFiles.isEmpty();

    // 3. Dual-folder abort (Part 3, Section 2.2)
    if (hasBa && hasEdr) {
        outputRedWarning(
            "Both BA and EDR input folders contain eligible files.",
            {
                "Only ONE job type may be processed per run.",
                "BA/INPUT eligible files: " + QString::number(baFiles.size()),
                "EDR/INPUT eligible files: " + QString::number(edrFiles.size()),
                "Please clear one folder and try again.",
                "Run aborted."
            });
        return false;
    }

    // 4. Neither-folder abort (Part 3, Section 2.1)
    if (!hasBa && !hasEdr) {
        outputToTerminal(
            "No eligible files found in BA/INPUT or EDR/INPUT. "
            "Eligible files must have a .csv/.xls/.xlsx extension and contain "
            "LA_BA/SA_BA (for BA) or LA_EDR/SA_EDR (for EDR) in the filename. "
            "Run aborted.",
            Error);
        return false;
    }

    // 5. Exactly one folder
    detectedJobType = hasBa ? "BA" : "EDR";
    outputToTerminal(
        QString("Preflight: detected job type %1 (%2 eligible file(s)).")
            .arg(detectedJobType)
            .arg(hasBa ? baFiles.size() : edrFiles.size()),
        Info);
    return true;
}

// ============================================================
// Phase 1 invocation (Part 3, Section 4)
// ============================================================

void TMCAController::runPhase1(const QString& jobType)
{
    const QString scriptPath = TMCA_SCRIPT_PATH;
    if (!QFile::exists(scriptPath)) {
        outputToTerminal("TMCA script not found: " + scriptPath, Error);
        return;
    }

    // Cache run parameters for Phase 2 and handlePhase1Success()
    m_pendingJobType   = jobType;
    m_pendingJobNumber = getJobNumber();
    m_pendingYear      = getYear();

    // Reset JSON capture state
    m_capturingJson   = false;
    m_jsonAccumulator.clear();

    // Reset pending result fields
    m_pendingLaValidCount = 0;
    m_pendingSaValidCount = 0;
    m_pendingLaBlankCount = 0;
    m_pendingSaBlankCount = 0;
    m_pendingLaPostage    = 0.0;
    m_pendingSaPostage    = 0.0;
    m_pendingRate         = TMCA_DEFAULT_RATE;
    m_pendingNasDest.clear();
    m_pendingWDest.clear();
    m_pendingMergedFiles.clear();

    m_currentPhase = PhaseProcess;

    QStringList args;
    args << "--phase"     << "process"
         << "--job"       << m_pendingJobNumber
         << "--ba-input"  << TMCA_BA_INPUT
         << "--edr-input" << TMCA_EDR_INPUT
         << "--w-dest"    << TMCA_W_DEST
         << "--nas-base"  << TMCA_NAS_BASE
         << "--year"      << m_pendingYear;

    outputToTerminal("Starting TMCA Phase 1 (process) for job " + m_pendingJobNumber
                     + ", type " + jobType + " ...", Info);
    outputToTerminal("Script: " + scriptPath, Info);
    outputToTerminal("Args: " + args.join(" "), Info);

    m_lastExecutedScript = "TMCA_PHASE1";
    updateButtonStates();

    m_scriptRunner->runScript(scriptPath, args);
}

// ============================================================
// Phase 2 invocation (Part 3, Section 7)
// ============================================================

void TMCAController::runPhase2()
{
    const QString scriptPath = TMCA_SCRIPT_PATH;
    if (!QFile::exists(scriptPath)) {
        outputToTerminal("FATAL: TMCA script not found for archive phase: " + scriptPath, Error);
        outputToTerminal("Archive aborted. Operator intervention required.", Error);
        return;
    }

    m_currentPhase = PhaseArchive;

    QStringList args;
    args << "--phase"     << "archive"
         << "--job"       << m_pendingJobNumber
         << "--ba-input"  << TMCA_BA_INPUT
         << "--edr-input" << TMCA_EDR_INPUT;

    outputToTerminal("Starting TMCA Phase 2 (archive) for job " + m_pendingJobNumber + " ...", Info);
    outputToTerminal("Args: " + args.join(" "), Info);

    m_lastExecutedScript = "TMCA_PHASE2";
    updateButtonStates();

    m_scriptRunner->runScript(scriptPath, args);
}

// ============================================================
// triggerArchivePhase — slot connected to TMCAEmailDialog::dialogClosed
// ============================================================

void TMCAController::triggerArchivePhase()
{
    outputToTerminal("Email dialog closed. Triggering archive phase...", Info);
    runPhase2();
}

// ============================================================
// ScriptRunner output handler
// ============================================================

void TMCAController::onScriptOutput(const QString& output)
{
    const QString trimmed = output.trimmed();

    // JSON marker detection
    if (trimmed == JSON_BEGIN_MARKER) {
        m_capturingJson   = true;
        m_jsonAccumulator.clear();
        // Do not print the marker itself to terminal
        return;
    }

    if (trimmed == JSON_END_MARKER) {
        m_capturingJson = false;
        // Do not print the marker to terminal
        return;
    }

    if (m_capturingJson) {
        m_jsonAccumulator.append(output + "\n");
        // Do not echo JSON lines to terminal
        return;
    }

    // All non-marker, non-JSON lines go to terminal
    outputToTerminal(trimmed, Info);
}

void TMCAController::onScriptError(const QString& output)
{
    // stderr always displayed as Error (red) regardless of JSON capture state
    const QString trimmed = output.trimmed();
    if (!trimmed.isEmpty()) {
        outputToTerminal(trimmed, Error);
    }
}

// ============================================================
// ScriptRunner finish handler
// ============================================================

void TMCAController::onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    updateButtonStates();

    if (exitStatus == QProcess::CrashExit) {
        outputToTerminal("FATAL: Script crashed unexpectedly. Run aborted.", Error);
        m_currentPhase = PhaseNone;
        return;
    }

    if (m_currentPhase == PhaseProcess) {
        if (exitCode != 0) {
            outputToTerminal(
                QString("FATAL: Phase 1 script failed (exit code %1). "
                        "No DB rows inserted. No popup shown. Run aborted.")
                    .arg(exitCode),
                Error);
            m_currentPhase = PhaseNone;
            return;
        }

        // Phase 1 succeeded — parse JSON then handle
        QJsonObject jsonResult;
        if (!parseAndValidateJson(m_jsonAccumulator, jsonResult)) {
            // parseAndValidateJson already printed the fatal error
            m_currentPhase = PhaseNone;
            return;
        }

        // Populate pending state from JSON
        m_pendingJobType      = jsonResult["job_type"].toString();
        m_pendingLaValidCount = jsonResult["la_valid_count"].toInt();
        m_pendingSaValidCount = jsonResult["sa_valid_count"].toInt();
        m_pendingLaBlankCount = jsonResult["la_blank_count"].toInt();
        m_pendingSaBlankCount = jsonResult["sa_blank_count"].toInt();
        m_pendingNasDest      = jsonResult["nas_dest"].toString();
        m_pendingWDest        = jsonResult["w_dest"].toString();

        const QJsonArray mergedArr = jsonResult["merged_files"].toArray();
        m_pendingMergedFiles.clear();
        for (const QJsonValue& v : mergedArr) {
            m_pendingMergedFiles.append(v.toString());
        }

        // Report blank counts as info (non-fatal)
        if (m_pendingLaBlankCount > 0) {
            outputToTerminal(
                QString("Info: %1 blank-address row(s) excluded from LA side.")
                    .arg(m_pendingLaBlankCount),
                Info);
        }
        if (m_pendingSaBlankCount > 0) {
            outputToTerminal(
                QString("Info: %1 blank-address row(s) excluded from SA side.")
                    .arg(m_pendingSaBlankCount),
                Info);
        }

        // Execute strict post-Phase-1 sequence
        handlePhase1Success();

    } else if (m_currentPhase == PhaseArchive) {
        handlePhase2Result(exitCode == 0);
        m_currentPhase = PhaseNone;
    }
}

// ============================================================
// JSON parsing (Part 3, Section 5)
// ============================================================

bool TMCAController::parseAndValidateJson(const QString& rawJson, QJsonObject& out)
{
    if (rawJson.trimmed().isEmpty()) {
        outputToTerminal(
            "FATAL: No JSON result received from script (markers present but content empty). "
            "Run aborted. No DB rows inserted. No popup shown.",
            Error);
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(rawJson.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        outputToTerminal(
            QString("FATAL: Failed to parse JSON result: %1. "
                    "Run aborted. No DB rows inserted. No popup shown.")
                .arg(parseError.errorString()),
            Error);
        return false;
    }

    out = doc.object();

    // Validate job_type
    if (!out.contains("job_type") || !out["job_type"].isString()) {
        outputToTerminal("FATAL: JSON missing or invalid field 'job_type'. Run aborted.", Error);
        return false;
    }
    const QString jt = out["job_type"].toString();
    if (jt != "BA" && jt != "EDR") {
        outputToTerminal(
            QString("FATAL: JSON 'job_type' must be \"BA\" or \"EDR\", got \"%1\". Run aborted.")
                .arg(jt),
            Error);
        return false;
    }

    // Validate integer count fields — must exist, be numeric, non-negative, and whole
    const QStringList intFields = {
        "la_valid_count", "sa_valid_count",
        "la_blank_count", "sa_blank_count"
    };
    for (const QString& field : intFields) {
        if (!out.contains(field) || !out[field].isDouble()) {
            outputToTerminal(
                QString("FATAL: JSON missing or invalid field '%1'. Run aborted.").arg(field),
                Error);
            return false;
        }
        const double d = out[field].toDouble();
        if (d < 0.0 || std::floor(d) != d) {
            outputToTerminal(
                QString("FATAL: JSON field '%1' must be a non-negative integer (got %2). Run aborted.")
                    .arg(field).arg(d, 0, 'g', 10),
                Error);
            return false;
        }
    }

    // Validate string fields
    const QStringList strFields = {"nas_dest", "w_dest"};
    for (const QString& field : strFields) {
        if (!out.contains(field) || !out[field].isString()) {
            outputToTerminal(
                QString("FATAL: JSON missing or invalid field '%1'. Run aborted.").arg(field),
                Error);
            return false;
        }
    }

    // Validate merged_files — must be array, all elements must be strings
    if (!out.contains("merged_files") || !out["merged_files"].isArray()) {
        outputToTerminal("FATAL: JSON missing or invalid field 'merged_files'. Run aborted.", Error);
        return false;
    }
    {
        const QJsonArray mf = out["merged_files"].toArray();
        for (int i = 0; i < mf.size(); ++i) {
            if (!mf[i].isString()) {
                outputToTerminal(
                    QString("FATAL: JSON 'merged_files[%1]' is not a string. Run aborted.").arg(i),
                    Error);
                return false;
            }
        }
    }

    // Validate deliverables object with w_drive[] and nas[] arrays
    if (!out.contains("deliverables") || !out["deliverables"].isObject()) {
        outputToTerminal(
            "FATAL: JSON missing or invalid field 'deliverables' (expected object). Run aborted.",
            Error);
        return false;
    }
    {
        const QJsonObject deliverables = out["deliverables"].toObject();

        if (!deliverables.contains("w_drive") || !deliverables["w_drive"].isArray()) {
            outputToTerminal(
                "FATAL: JSON 'deliverables.w_drive' missing or not an array. Run aborted.",
                Error);
            return false;
        }
        {
            const QJsonArray wdArr = deliverables["w_drive"].toArray();
            for (int i = 0; i < wdArr.size(); ++i) {
                if (!wdArr[i].isString()) {
                    outputToTerminal(
                        QString("FATAL: JSON 'deliverables.w_drive[%1]' is not a string. Run aborted.").arg(i),
                        Error);
                    return false;
                }
            }
        }

        if (!deliverables.contains("nas") || !deliverables["nas"].isArray()) {
            outputToTerminal(
                "FATAL: JSON 'deliverables.nas' missing or not an array. Run aborted.",
                Error);
            return false;
        }
        {
            const QJsonArray nasArr = deliverables["nas"].toArray();
            for (int i = 0; i < nasArr.size(); ++i) {
                if (!nasArr[i].isString()) {
                    outputToTerminal(
                        QString("FATAL: JSON 'deliverables.nas[%1]' is not a string. Run aborted.").arg(i),
                        Error);
                    return false;
                }
            }
        }
    }

    return true;
}

// ============================================================
// Post-Phase-1 success handler (Part 3, Section 6 — strict order)
// ============================================================

void TMCAController::handlePhase1Success()
{
    outputToTerminal("Phase 1 complete. Processing results...", Success);

    // Step 1: Query meter rate
    m_pendingRate = queryMeterRate();
    outputToTerminal(
        QString("Meter rate: %1").arg(m_pendingRate, 0, 'f', 3),
        Info);

    // Step 2: Compute per-side postage
    m_pendingLaPostage = m_pendingLaValidCount * m_pendingRate;
    m_pendingSaPostage = m_pendingSaValidCount * m_pendingRate;

    // Step 3: Insert DB rows — fatal if failure
    if (!insertLogRows(m_pendingJobType,
                       m_pendingLaValidCount, m_pendingSaValidCount,
                       m_pendingLaPostage,    m_pendingSaPostage,
                       m_pendingRate,         m_pendingJobNumber)) {
        outputToTerminal(
            "FATAL: Database insertion failed. "
            "No popup shown. Archive will not run. "
            "Operator intervention required.",
            Error);
        m_currentPhase = PhaseNone;
        return;
    }

    // Step 4: Refresh tracker
    refreshTrackerTable();

    // Step 5: Update informational UI fields (total across both sides)
    const int    totalCount   = m_pendingLaValidCount + m_pendingSaValidCount;
    const double totalPostage = m_pendingLaPostage    + m_pendingSaPostage;

    if (m_countBox) {
        m_countBox->setText(QString::number(totalCount));
    }
    if (m_postageBox) {
        m_postageBox->setText(QString("$%1").arg(totalPostage, 0, 'f', 2));
    }

    // Persist informational totals to DB
    saveJobState();

    // Step 6: Launch modal popup — archive triggered on CLOSE
    outputToTerminal("Launching email dialog...", Info);

    QPointer<TMCAEmailDialog> dlg = new TMCAEmailDialog(
        m_pendingJobNumber,
        m_pendingJobType,
        m_pendingLaValidCount,
        m_pendingSaValidCount,
        m_pendingLaPostage,
        m_pendingSaPostage,
        m_pendingRate,
        m_pendingNasDest,
        m_pendingMergedFiles,
        nullptr   // top-level modal
    );

    connect(dlg, &TMCAEmailDialog::dialogClosed,
            this, &TMCAController::triggerArchivePhase);

    dlg->exec();   // blocks until user clicks CLOSE
}

// ============================================================
// Post-Phase-2 result handler (Part 6, Section 6.4)
// ============================================================

void TMCAController::handlePhase2Result(bool success)
{
    if (success) {
        outputToTerminal("Archive phase completed successfully. "
                         "INPUT, OUTPUT, and MERGED folders have been cleared. "
                         "ZIP archive created.",
                         Success);
    } else {
        outputToTerminal(
            "FATAL: Archive phase failed. "
            "Folders may not have been cleared. "
            "Do NOT re-run without verifying folder state. "
            "Operator intervention required.",
            Error);
    }
}

// ============================================================
// DB helpers
// ============================================================

double TMCAController::queryMeterRate() const
{
    QSqlQuery q(DatabaseManager::instance()->getDatabase());
    q.prepare("SELECT rate FROM meter_rates ORDER BY created_at DESC LIMIT 1");
    if (q.exec() && q.next()) {
        bool ok = false;
        double rate = q.value(0).toDouble(&ok);
        if (ok && rate > 0.0) return rate;
    }
    return TMCA_DEFAULT_RATE;
}

bool TMCAController::insertLogRows(const QString& jobType,
                                   int           laValidCount,
                                   int           saValidCount,
                                   double        laPostage,
                                   double        saPostage,
                                   double        rate,
                                   const QString& jobNumber)
{
    if (!m_tmcaDBManager) {
        outputToTerminal("FATAL: DB manager unavailable for log row insertion.", Error);
        return false;
    }

    // Format fields per spec (Part 2, Section 7)
    // postage: $X.XX   per_piece: 0.XXX   count: plain integer string
    const QString perPiece = QString("%1").arg(rate, 0, 'f', 3);
    const QString date     = QDate::currentDate().toString("yyyy-MM-dd");

    bool ok = true;

    if (laValidCount > 0) {
        const QString desc    = QString("TM CA %1 LA").arg(jobType);
        const QString postage = QString("$%1").arg(laPostage, 0, 'f', 2);
        const QString count   = QString::number(laValidCount);

        if (!m_tmcaDBManager->insertLogRow(jobNumber, desc, postage, count,
                                           perPiece, TMCA_CLASS, TMCA_SHAPE,
                                           TMCA_PERMIT, date,
                                           getYear(), getMonth())) {
            outputToTerminal(
                QString("FATAL: Failed to insert LA log row for job %1.").arg(jobNumber),
                Error);
            ok = false;
        } else {
            outputToTerminal(
                QString("DB: Inserted %1 — count=%2, postage=%3")
                    .arg(desc, count, postage),
                Success);
        }
    }

    if (saValidCount > 0) {
        const QString desc    = QString("TM CA %1 SA").arg(jobType);
        const QString postage = QString("$%1").arg(saPostage, 0, 'f', 2);
        const QString count   = QString::number(saValidCount);

        if (!m_tmcaDBManager->insertLogRow(jobNumber, desc, postage, count,
                                           perPiece, TMCA_CLASS, TMCA_SHAPE,
                                           TMCA_PERMIT, date,
                                           getYear(), getMonth())) {
            outputToTerminal(
                QString("FATAL: Failed to insert SA log row for job %1.").arg(jobNumber),
                Error);
            ok = false;
        } else {
            outputToTerminal(
                QString("DB: Inserted %1 — count=%2, postage=%3")
                    .arg(desc, count, postage),
                Success);
        }
    }

    if (laValidCount == 0 && saValidCount == 0) {
        outputToTerminal(
            "Info: Both LA and SA valid counts are 0. "
            "No log rows inserted (all input rows were blank-address).",
            Info);
    }

    return ok;
}

// ============================================================
// Validation utilities
// ============================================================

bool TMCAController::validateJobData() const
{
    return !getJobNumber().isEmpty()
        && !getYear().isEmpty()
        && !getMonth().isEmpty();
}

bool TMCAController::validatePostageData() const
{
    const QString postage = m_postageBox ? m_postageBox->text().trimmed() : QString();
    const QString count   = m_countBox   ? m_countBox->text().trimmed()   : QString();
    return !postage.isEmpty() && !count.isEmpty();
}

// ============================================================
// Drop window
// ============================================================

void TMCAController::setupDropWindow()
{
    if (!m_dropWindow || !m_fileManager) return;

    m_dropWindow->setTargetDirectory(m_fileManager->getDropPath());

    connect(m_dropWindow, &DropWindow::filesDropped,
            this, &TMCAController::onFilesDropped);
    connect(m_dropWindow, SIGNAL(dropError(QString)),
            this, SLOT(onFileDropError(QString)));
}

void TMCAController::onFilesDropped(const QStringList& filePaths)
{
    outputToTerminal(
        QString("Files received: %1 file(s) dropped.").arg(filePaths.size()),
        Success);
    for (const QString& path : filePaths) {
        QFileInfo fi(path);
        outputToTerminal("  - " + fi.fileName(), Info);
        routeDroppedFile(path);
    }
}

void TMCAController::onFileDropError(const QString& errorMessage)
{
    outputToTerminal("File drop error: " + errorMessage, Warning);
}

void TMCAController::routeDroppedFile(const QString& absoluteFilePath)
{
    if (!m_fileManager) return;

    QFileInfo fi(absoluteFilePath);
    if (!fi.exists()) {
        outputToTerminal("Routing skipped (missing file): " + absoluteFilePath, Warning);
        return;
    }

    const QString nameUpper = fi.fileName().toUpper();
    QString destinationDir;
    QString reason;

    if (nameUpper.contains("LA_BA") || nameUpper.contains("SA_BA")) {
        destinationDir = m_fileManager->getBAInputPath();
        reason = "BA token matched -> BA\\INPUT";
    } else if (nameUpper.contains("LA_EDR") || nameUpper.contains("SA_EDR")) {
        destinationDir = m_fileManager->getEDRInputPath();
        reason = "EDR token matched -> EDR\\INPUT";
    } else {
        outputToTerminal(
            QString("Routing: %1 — no BA or EDR token found, left in DROP.")
                .arg(fi.fileName()),
            Info);
        return;
    }

    QDir().mkpath(destinationDir);
    const QString destPath = QDir(destinationDir).filePath(fi.fileName());

    if (m_fileManager->moveFile(absoluteFilePath, destPath)) {
        outputToTerminal(
            QString("Routed: %1 -> %2 (%3)")
                .arg(fi.fileName(), destinationDir, reason),
            Success);
    } else {
        outputToTerminal(
            QString("Routing FAILED: %1 -> %2")
                .arg(fi.fileName(), destinationDir),
            Error);
    }
}

// ============================================================
// Tracker context menu
// ============================================================

void TMCAController::showTableContextMenu(const QPoint& pos)
{
    if (!m_tracker) return;

    QMenu menu(m_tracker);
    QAction* copyAction = menu.addAction("Copy Selected Row");
    QAction* selected   = menu.exec(m_tracker->mapToGlobal(pos));

    if (selected == copyAction) {
        copyFormattedRow();
    }
}
