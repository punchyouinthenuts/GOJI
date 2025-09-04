#include "tmfarmcontroller.h"
#include "logger.h"
#include "tmfarmemaildialog.h"
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
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QLocale>

// Model wrapper to format tracker data
class FormattedSqlModel : public QSqlTableModel {
public:
    FormattedSqlModel(QObject *parent, QSqlDatabase db, TMFarmController *ctrl)
        : QSqlTableModel(parent, db), controller(ctrl) {}
    QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole) {
            QVariant val = QSqlTableModel::data(idx, role);
            return controller->formatCellData(idx.column(), val.toString());
        }
        return QSqlTableModel::data(idx, role);
    }
private:
    TMFarmController *controller;
};

TMFarmController::TMFarmController(QObject *parent)
    : BaseTrackerController(parent)
    , m_dbManager(DatabaseManager::instance())
    , m_fileManager(nullptr)
    , m_tmFarmDBManager(TMFarmDBManager::instance())
    , m_scriptRunner(nullptr)
    , m_openBulkMailerBtn(nullptr)
    , m_runInitialBtn(nullptr)
    , m_finalStepBtn(nullptr)
    , m_lockBtn(nullptr)
    , m_editBtn(nullptr)
    , m_postageLockBtn(nullptr)
    , m_yearDDbox(nullptr)
    , m_quarterDDbox(nullptr)
    , m_jobNumberBox(nullptr)
    , m_postageBox(nullptr)
    , m_countBox(nullptr)
    , m_terminalWindow(nullptr)
    , m_tracker(nullptr)
    , m_textBrowser(nullptr)
    , m_jobDataLocked(false)
    , m_postageDataLocked(false)
    , m_currentHtmlState(UninitializedState)
    , m_lastExecutedScript("")
    , m_capturedNASPath("")
    , m_capturingNASPath(false)
    , m_trackerModel(nullptr)
{
    // Initialize file manager
    QSettings* settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "GojiApp", "Goji");
    m_fileManager = new TMFarmFileManager(settings);
    m_scriptRunner = new ScriptRunner(this);
    m_finalNASPath.clear();

    if (m_dbManager && m_dbManager->isInitialized()) {
        m_trackerModel = new FormattedSqlModel(this, m_dbManager->getDatabase(), this);
        m_trackerModel->setTable("tm_farm_log");
        m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
        m_trackerModel->select();
    } else {
        Logger::instance().warning("Cannot setup tracker model - database not available");
        m_trackerModel = nullptr;
    }
}

TMFarmController::~TMFarmController()
{
    Logger::instance().info("TMFarmController destroyed");
}

void TMFarmController::initializeUI(QPushButton* openBulkMailerBtn, QPushButton* runInitialBtn,
                                    QPushButton* finalStepBtn, QToolButton* lockBtn, QToolButton* editBtn,
                                    QToolButton* postageLockBtn, QComboBox* yearDDbox, QComboBox* quarterDDbox,
                                    QLineEdit* jobNumberBox, QLineEdit* postageBox, QLineEdit* countBox,
                                    QTextEdit* terminalWindow, QTableView* tracker, QTextBrowser* textBrowser)
{
    Logger::instance().info("Initializing TM FARMWORKERS UI elements");

    m_openBulkMailerBtn = openBulkMailerBtn;
    m_runInitialBtn = runInitialBtn;
    m_finalStepBtn = finalStepBtn;
    m_lockBtn = lockBtn;
    m_editBtn = editBtn;
    m_postageLockBtn = postageLockBtn;
    m_yearDDbox = yearDDbox;
    m_quarterDDbox = quarterDDbox;
    m_jobNumberBox = jobNumberBox;
    m_postageBox = postageBox;
    m_countBox = countBox;
    m_terminalWindow = terminalWindow;
    m_tracker = tracker;
    m_textBrowser = textBrowser;

    if (m_tracker) {
        m_tracker->setModel(m_trackerModel);
        m_tracker->setEditTriggers(QAbstractItemView::NoEditTriggers);
        setupOptimizedTableLayout();
        m_tracker->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_tracker, &QTableView::customContextMenuRequested, this,
                &TMFarmController::showTableContextMenu);
    }

    connectSignals();
    setupInitialUIState();
    populateDropdowns();
    updateHtmlDisplay();

    Logger::instance().info("TM FARMWORKERS UI initialization complete");
}

void TMFarmController::connectSignals()
{
    if (m_openBulkMailerBtn)
        connect(m_openBulkMailerBtn, &QPushButton::clicked, this, &TMFarmController::onOpenBulkMailerClicked);
    if (m_runInitialBtn)
        connect(m_runInitialBtn, &QPushButton::clicked, this, &TMFarmController::onRunInitialClicked);
    if (m_finalStepBtn)
        connect(m_finalStepBtn, &QPushButton::clicked, this, &TMFarmController::onFinalStepClicked);

    if (m_lockBtn)
        connect(m_lockBtn, &QToolButton::clicked, this, &TMFarmController::onLockButtonClicked);
    if (m_editBtn)
        connect(m_editBtn, &QToolButton::clicked, this, &TMFarmController::onEditButtonClicked);
    if (m_postageLockBtn)
        connect(m_postageLockBtn, &QToolButton::clicked, this, &TMFarmController::onPostageLockButtonClicked);

    if (m_yearDDbox)
        connect(m_yearDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
                this, &TMFarmController::onYearChanged);
    if (m_quarterDDbox)
        connect(m_quarterDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
                this, &TMFarmController::onMonthChanged);

    if (m_postageBox) {
        auto *validator = new QRegularExpressionValidator(QRegularExpression(R"(^\s*\$?\s*[0-9,]*(?:\.[0-9]{0,2})?\s*$)"), this);
        m_postageBox->setValidator(validator);
        connect(m_postageBox, &QLineEdit::editingFinished, this, &TMFarmController::formatPostageInput);
        connect(m_postageBox, &QLineEdit::textChanged, this, [this]() { if (m_jobDataLocked) saveJobState(); });
    }
    if (m_countBox) {
        connect(m_countBox, &QLineEdit::textChanged, this, &TMFarmController::formatCountInput);
        connect(m_countBox, &QLineEdit::textChanged, this, [this]() { if (m_jobDataLocked) saveJobState(); });
    }

    if (m_jobNumberBox) {
        connect(m_jobNumberBox, &QLineEdit::editingFinished, this, [this]() {
            const QString newNum = m_jobNumberBox->text().trimmed();
            if (newNum.isEmpty() || !validateJobNumber(newNum)) return;
            const QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
            const QString quarter = m_quarterDDbox ? m_quarterDDbox->currentText() : "";
            if (year.isEmpty() || quarter.isEmpty()) return;
            if (newNum != m_cachedJobNumber) {
                saveJobState();
                if (m_tmFarmDBManager) m_tmFarmDBManager->updateLogJobNumber(m_cachedJobNumber, newNum);
                m_cachedJobNumber = newNum;
                refreshTrackerTable();
            }
        });
    }

    if (m_scriptRunner) {
        connect(m_scriptRunner, &ScriptRunner::scriptOutput, this, &TMFarmController::onScriptOutput);
        connect(m_scriptRunner, &ScriptRunner::scriptFinished, this, &TMFarmController::onScriptFinished);
    }

    Logger::instance().info("TM FARMWORKERS signal connections complete");
}

void TMFarmController::setupInitialUIState()
{
    Logger::instance().info("Setting up initial TM FARMWORKERS UI state...");
    m_jobDataLocked = false;
    m_postageDataLocked = false;
    updateControlStates();
    Logger::instance().info("Initial TM FARMWORKERS UI state setup complete");
}

void TMFarmController::populateDropdowns()
{
    Logger::instance().info("Populating TM FARMWORKERS dropdowns...");
    if (m_yearDDbox) {
        m_yearDDbox->clear();
        m_yearDDbox->addItem("");
        int currentYear = QDate::currentDate().year();
        m_yearDDbox->addItem(QString::number(currentYear - 1));
        m_yearDDbox->addItem(QString::number(currentYear));
        m_yearDDbox->addItem(QString::number(currentYear + 1));
    }
    if (m_quarterDDbox) {
        m_quarterDDbox->clear();
        m_quarterDDbox->addItem("");
        m_quarterDDbox->addItems({"1ST","2ND","3RD","4TH"});
    }
    Logger::instance().info("TM FARMWORKERS dropdown population complete");
}

void TMFarmController::onRunInitialClicked()
{
    if (!m_jobDataLocked) { outputToTerminal("Error: Job must be locked first", Error); return; }
    QString scriptPath = m_fileManager->getScriptPath("01 INITIAL");
    if (!QFile::exists(scriptPath)) {
        outputToTerminal("Error: Initial script not found: " + scriptPath, Error);
        return;
    }
    outputToTerminal("Running 01 INITIAL.py ...", Info);
    m_lastExecutedScript = "01 INITIAL";
    m_scriptRunner->runScript(scriptPath, {});
}

void TMFarmController::onFinalStepClicked()
{
    if (!m_postageDataLocked) { outputToTerminal("Error: Postage must be locked first", Error); return; }
    QString scriptPath = m_fileManager->getScriptPath("02 POST PROCESS");
    if (!QFile::exists(scriptPath)) {
        outputToTerminal("Error: Final script not found: " + scriptPath, Error);
        return;
    }
    QString job = m_jobNumberBox ? m_jobNumberBox->text() : "";
    QString quarter = m_quarterDDbox ? m_quarterDDbox->currentText() : "";
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    if (job.isEmpty() || quarter.isEmpty() || year.isEmpty()) {
        outputToTerminal("Error: Missing job/year/quarter", Error);
        return;
    }
    outputToTerminal(QString("Running 02 POST PROCESS.py for job %1_%2%3").arg(job,quarter,year), Info);
    m_lastExecutedScript = "02 POST PROCESS";
    addLogEntry();
    m_scriptRunner->runScript(scriptPath, {job, quarter, year});
}

// (Rest of controller methods unchanged except TERM->FARMWORKERS, quarter model, etc.)
