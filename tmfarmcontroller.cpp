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
    // Defer to end-of-event-loop to avoid getting overwritten by later init code
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
    // Also defer here in case only initializeUI is used
    QTimer::singleShot(0, this, [this]() { updateHtmlDisplay(); });
    updateControlStates();
}

bool TMFarmController::loadJob(const QString& year, const QString& monthOrQuarter)
{
    Q_UNUSED(year);
    Q_UNUSED(monthOrQuarter);
    emit jobOpened();
    return true;
}

void TMFarmController::autoSaveAndCloseCurrentJob()
{
    saveJobState();
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
    saveJobState();
}

void TMFarmController::onEditButtonClicked()
{
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
}

void TMFarmController::onMonthChanged(const QString&)
{
    saveJobState();
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

// Ensure default.html is shown on app launch / when no job is open
void TMFarmController::updateHtmlDisplay()
{
    if (!m_textBrowser) return;

    // The qrc shows prefix "/" and file "resources/tmfarmworkers/default.html"
    const QString path = QStringLiteral(":/resources/tmfarmworkers/default.html");

    if (QFile::exists(path)) {
        // Defer to avoid overwrites during startup (e.g., other init code writing "Ready")
        QTimer::singleShot(0, this, [this, path]() {
            m_textBrowser->setSource(QUrl(path));  // better base-url handling than setHtml
            Logger::instance().info("TMFW loaded startup page: " + path);
        });
    } else {
        Logger::instance().warning("TMFW startup page missing in qrc at: " + path);
        m_textBrowser->setHtml(QStringLiteral("<html><body><h3>TM Farm Workers</h3><p>Instructions not available. Add resources/tmfarmworkers/default.html to resources.qrc.</p></body></html>"));
    }
}

void TMFarmController::loadHtmlFile(const QString& resourcePath)
{
    if (!m_textBrowser) return;

    // Prefer setSource, but keep this as a utility when we already know the path exists
    if (QFile::exists(resourcePath)) {
        m_textBrowser->setSource(QUrl(resourcePath));
        Logger::instance().info("Loaded HTML file: " + resourcePath);
        return;
    }

    QFile file(resourcePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        const QString htmlContent = stream.readAll();
        m_textBrowser->setHtml(htmlContent);
        file.close();
        Logger::instance().info("Loaded HTML content (stream): " + resourcePath);
    } else {
        Logger::instance().warning("Failed to load HTML file: " + resourcePath);
        m_textBrowser->setHtml(QStringLiteral("<html><body><p>Instructions not available.</p></body></html>"));
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
