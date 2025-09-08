#include "tmfarmcontroller.h"
#include "logger.h"
#include "databasemanager.h"

#include <QHeaderView>
#include <QMenu>
#include <QSqlQuery>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

TMFarmController::TMFarmController(QObject* parent)
    : BaseTrackerController(parent)
{
}

void TMFarmController::setTextBrowser(QTextBrowser* browser)
{
    m_textBrowser = browser;
    // Defer to end-of-event-loop so other startup writes won't overwrite it
    QTimer::singleShot(0, this, [this]() { updateHtmlDisplay(); });
}

void TMFarmController::initializeUI(
    QPushButton* openBulkMailerBtn,
    QPushButton* runInitialBtn,
    QPushButton* finalStepBtn,
    QToolButton* lockBtn,
    QToolButton* editBtn,
    QToolButton* postageLockBtn,
    QComboBox* yearCombo,
    QComboBox* monthOrQuarterCombo,
    QLineEdit* jobNumberEdit,
    QLineEdit* postageEdit,
    QLineEdit* countEdit,
    QTextEdit* terminalEdit,
    QTableView* trackerView,
    QTextBrowser* textBrowser)
{
    m_openBulkMailerBtn = openBulkMailerBtn;
    m_runInitialBtn = runInitialBtn;
    m_finalStepBtn = finalStepBtn;
    m_lockBtn = lockBtn;
    m_editBtn = editBtn;
    m_postageLockBtn = postageLockBtn;
    m_yearCombo = yearCombo;
    m_monthOrQuarterCombo = monthOrQuarterCombo;
    m_jobNumberEdit = jobNumberEdit;
    m_postageEdit = postageEdit;
    m_countEdit = countEdit;
    m_terminal = terminalEdit;
    m_tracker = trackerView;
    m_textBrowser = textBrowser;

    setupOptimizedTableLayout();
    connectSignals();
    QTimer::singleShot(0, this, [this]() { updateHtmlDisplay(); });
    updateControlStates();
}

bool TMFarmController::loadJob(const QString& year, const QString& monthOrQuarter)
{
    Q_UNUSED(year);
    Q_UNUSED(monthOrQuarter);
    updateHtmlDisplay();
    emit jobOpened();
    return true;
}

void TMFarmController::autoSaveAndCloseCurrentJob()
{
    saveJobState();
    m_jobDataLocked = false; // closing job returns to default state
    updateHtmlDisplay();
    emit jobClosed();
}

void TMFarmController::outputToTerminal(const QString& message, MessageType)
{
    if (m_terminal) {
        m_terminal->append(message);
        m_terminal->ensureCursorVisible();
    }
    Logger::instance().info(message);
}

QTableView* TMFarmController::getTrackerWidget() const
{
    return m_tracker;
}

QSqlTableModel* TMFarmController::getTrackerModel() const
{
    return m_trackerModel;
}

QStringList TMFarmController::getTrackerHeaders() const
{
    return {"DATE","DESCRIPTION","POSTAGE","COUNT","AVG RATE","CLASS","SHAPE","PERMIT"};
}

QList<int> TMFarmController::getVisibleColumns() const
{
    QList<int> cols;
    for (int i = 0; i < getTrackerHeaders().size(); ++i) cols << i;
    return cols;
}

QString TMFarmController::formatCellData(int, const QString& cellData) const
{
    return cellData;
}

QString TMFarmController::formatCellDataForCopy(int, const QString& cellData) const
{
    return cellData;
}

bool TMFarmController::isJobDataLocked() const
{
    return m_lockBtn ? m_lockBtn->isChecked() : m_jobDataLocked;
}

void TMFarmController::refreshTrackerTable()
{
    if (!m_tracker) return;
    if (!m_trackerModel) {
        QSqlDatabase db = DatabaseManager::instance()->getDatabase();
        m_trackerModel = new QSqlTableModel(const_cast<TMFarmController*>(this), db);
        m_trackerModel->setTable("tm_farm_log");
        m_trackerModel->select();
    } else {
        m_trackerModel->select();
    }
    m_tracker->setModel(m_trackerModel);
}

void TMFarmController::updateControlStates()
{
    const bool locked = isJobDataLocked();
    if (m_jobNumberEdit) m_jobNumberEdit->setReadOnly(locked);
    if (m_postageEdit)   m_postageEdit->setReadOnly(locked || (m_postageLockBtn && m_postageLockBtn->isChecked()));
    if (m_countEdit)     m_countEdit->setReadOnly(locked);
    if (m_runInitialBtn) m_runInitialBtn->setEnabled(!locked);
    if (m_finalStepBtn)  m_finalStepBtn->setEnabled(!locked);
}

void TMFarmController::onScriptFinished(int, QProcess::ExitStatus)
{
    outputToTerminal("Script finished.", BaseTrackerController::Info);
}

void TMFarmController::onScriptOutput(const QString& text)
{
    outputToTerminal(text, BaseTrackerController::Info);
}

void TMFarmController::onOpenBulkMailerClicked()
{
    outputToTerminal("Open Bulk Mailer clicked.", BaseTrackerController::Info);
}

void TMFarmController::onRunInitialClicked()
{
    outputToTerminal("Run Initial clicked.", BaseTrackerController::Info);
}

void TMFarmController::onFinalStepClicked()
{
    outputToTerminal("Final Step clicked.", BaseTrackerController::Info);
}

void TMFarmController::onLockButtonClicked()
{
    m_jobDataLocked = m_lockBtn && m_lockBtn->isChecked();
    updateControlStates();
    updateHtmlDisplay();
    saveJobState();
}

void TMFarmController::onEditButtonClicked()
{
    if (m_editBtn && m_editBtn->isChecked()) {
        m_jobDataLocked = false; // Unlock for editing
    }
    updateControlStates();
    updateHtmlDisplay();
    outputToTerminal("Edit toggled.", BaseTrackerController::Info);
}

void TMFarmController::onPostageLockButtonClicked()
{
    m_postageDataLocked = m_postageLockBtn && m_postageLockBtn->isChecked();
    updateControlStates();
    saveJobState();
}

void TMFarmController::onYearChanged(const QString&)
{
    saveJobState();
    updateHtmlDisplay();
}

void TMFarmController::onMonthChanged(const QString&)
{
    saveJobState();
    updateHtmlDisplay();
}

void TMFarmController::formatCountInput(const QString& text)
{
    QString digits;
    for (const QChar& c : text) if (c.isDigit()) digits.append(c);
    if (m_countEdit && m_countEdit->text() != digits) m_countEdit->setText(digits);
}

void TMFarmController::formatPostageInput()
{
    if (!m_postageEdit) return;
    bool ok = false;
    double v = m_postageEdit->text().toDouble(&ok);
    if (ok) m_postageEdit->setText(QString::number(v, 'f', 2));
}

void TMFarmController::connectSignals()
{
    if (m_openBulkMailerBtn)
        QObject::connect(m_openBulkMailerBtn, &QPushButton::clicked, this, &TMFarmController::onOpenBulkMailerClicked);
    if (m_runInitialBtn)
        QObject::connect(m_runInitialBtn, &QPushButton::clicked, this, &TMFarmController::onRunInitialClicked);
    if (m_finalStepBtn)
        QObject::connect(m_finalStepBtn, &QPushButton::clicked, this, &TMFarmController::onFinalStepClicked);

    if (m_lockBtn)
        QObject::connect(m_lockBtn, &QToolButton::clicked, this, &TMFarmController::onLockButtonClicked);
    if (m_editBtn)
        QObject::connect(m_editBtn, &QToolButton::clicked, this, &TMFarmController::onEditButtonClicked);
    if (m_postageLockBtn)
        QObject::connect(m_postageLockBtn, &QToolButton::clicked, this, &TMFarmController::onPostageLockButtonClicked);

    if (m_yearCombo)
        QObject::connect(m_yearCombo, &QComboBox::currentTextChanged, this, &TMFarmController::onYearChanged);
    if (m_monthOrQuarterCombo)
        QObject::connect(m_monthOrQuarterCombo, &QComboBox::currentTextChanged, this, &TMFarmController::onMonthChanged);

    if (m_countEdit)
        QObject::connect(m_countEdit, &QLineEdit::textChanged, this, &TMFarmController::formatCountInput);
    if (m_postageEdit)
        QObject::connect(m_postageEdit, &QLineEdit::editingFinished, this, &TMFarmController::formatPostageInput);

    if (m_tracker)
        QObject::connect(m_tracker, &QTableView::customContextMenuRequested, this, &TMFarmController::showTableContextMenu);
}

void TMFarmController::setupOptimizedTableLayout()
{
    if (!m_tracker) return;
    m_tracker->horizontalHeader()->setStretchLastSection(true);
    m_tracker->verticalHeader()->setVisible(false);
    m_tracker->setContextMenuPolicy(Qt::CustomContextMenu);
}

void TMFarmController::showTableContextMenu(const QPoint& pos)
{
    if (!m_tracker) return;
    QMenu menu(m_tracker);
    menu.addAction("Copy");
    menu.addAction("Export CSV");
    menu.exec(m_tracker->viewport()->mapToGlobal(pos));
}

void TMFarmController::updateHtmlDisplay()
{
    if (!m_textBrowser) return;

    // Match TMTERM: instructions when locked, default when unlocked
    const bool showInstructions = m_jobDataLocked || (m_lockBtn && m_lockBtn->isChecked());

    if (showInstructions) {
        m_textBrowser->setSource(QUrl(QStringLiteral("qrc:/resources/tmfarmworkers/instructions.html")));
        Logger::instance().info("TMFW HTML: instructions.html loaded");
    } else {
        m_textBrowser->setSource(QUrl(QStringLiteral("qrc:/resources/tmfarmworkers/default.html")));
        Logger::instance().info("TMFW HTML: default.html loaded");
    }
}

bool TMFarmController::saveJobState()
{
    return true;
}

bool TMFarmController::validateJobNumber(const QString& jobNumber) const
{
    return !jobNumber.trimmed().isEmpty();
}

void TMFarmController::addLogEntry()
{
    outputToTerminal("Log entry added (stub).", BaseTrackerController::Info);
}
