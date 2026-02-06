#include "fhcontroller.h"
#include "logger.h"
#include "dropwindow.h"
#include <QDesktopServices>
#include <QUrl>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QHeaderView>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QAbstractItemView>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QRegularExpression>
#include <QPointer>

class FormattedSqlModel : public QSqlTableModel {
public:
    FormattedSqlModel(QObject *parent, QSqlDatabase db, FHController *ctrl)
        : QSqlTableModel(parent, db), controller(ctrl) {}
    QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole) {
            QVariant val = QSqlTableModel::data(idx, role);
            return controller->formatCellData(idx.column(), val.toString());
        }
        return QSqlTableModel::data(idx, role);
    }
private:
    FHController *controller;
};

FHController::FHController(QObject *parent)
    : BaseTrackerController(parent)
    , m_fileManager(nullptr)
    , m_fhDBManager(nullptr)
    , m_scriptRunner(nullptr)
    , m_jobNumberBox(nullptr)
    , m_yearDDbox(nullptr)
    , m_monthDDbox(nullptr)
    , m_dropNumberComboBox(nullptr)
    , m_postageBox(nullptr)
    , m_countBox(nullptr)
    , m_textBrowser(nullptr)
    , m_jobDataLockBtn(nullptr)
    , m_postageLockBtn(nullptr)
    , m_editBtn(nullptr)
    , m_runInitialBtn(nullptr)
    , m_finalStepBtn(nullptr)
    , m_terminalWindow(nullptr)
    , m_tracker(nullptr)
    , m_dropWindow(nullptr)
    , m_jobDataLocked(false)
    , m_postageDataLocked(false)
    , m_currentHtmlState(UninitializedState)
    , m_trackerModel(nullptr)
    , m_currentYear("")
    , m_currentMonth("")
    , m_cachedJobNumber("")
    , m_initializing(true)
{
    initializeComponents();
    // Note: connectSignals() and setupInitialState() are now called from initializeUI()
}

FHController::~FHController()
{
    // Note: UI widgets are managed by Qt's parent-child system
    // File manager and script runner will be cleaned up automatically
}

void FHController::initializeUI(
    QPushButton* runInitialBtn,
    QPushButton* finalStepBtn,
    QToolButton* lockBtn,
    QToolButton* editBtn,
    QToolButton* postageLockBtn,
    QComboBox* yearDropdown,
    QComboBox* monthDropdown,
    QComboBox* dropNumberDropdown,
    QLineEdit* jobNumberBox,
    QLineEdit* postageBox,
    QLineEdit* countBox,
    QTextEdit* terminalWindow,
    QTableView* tracker,
    QTextBrowser* textBrowser,
    QWidget* dropWindow)
{
    Logger::instance().info("Initializing FOUR HANDS UI elements");

    // Store UI element pointers
    m_runInitialBtn = runInitialBtn;
    m_finalStepBtn = finalStepBtn;
    m_jobDataLockBtn = lockBtn;
    m_editBtn = editBtn;
    m_postageLockBtn = postageLockBtn;
    m_yearDDbox = yearDropdown;
    m_monthDDbox = monthDropdown;
    m_dropNumberComboBox = dropNumberDropdown;
    m_jobNumberBox = jobNumberBox;
    m_postageBox = postageBox;
    m_countBox = countBox;
    m_terminalWindow = terminalWindow;
    m_tracker = tracker;
    m_textBrowser = textBrowser;
    m_dropWindow = qobject_cast<DropWindow*>(dropWindow);

    // 🟢 Initialize the tracker model before using it — with DB availability check
    if (m_tracker && !m_trackerModel) {
        QSqlDatabase db = DatabaseManager::instance()->getDatabase();

        if (!db.isOpen()) {
            Logger::instance().warning("Database not open — delaying tracker model initialization until DB is ready.");
        } else {
            m_trackerModel = new FormattedSqlModel(this, db, this);
            m_trackerModel->setTable("fh_log");
            m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
            m_trackerModel->select();
            m_tracker->setModel(m_trackerModel);
            setupOptimizedTableLayout();
            Logger::instance().info("Tracker model initialized successfully.");
        }
    }

    // Setup drop window if provided
    if (m_dropWindow) {
        setupDropWindow();
    }

    connectSignals();
    setupInitialState();
    updateHtmlDisplay();

    Logger::instance().info("FOUR HANDS UI initialization complete");
}

void FHController::initializeComponents()
{
    // Initialize file manager
    m_fileManager = new FHFileManager(nullptr);

    // Initialize database manager
    m_fhDBManager = FHDBManager::instance();

    // Initialize script runner
    m_scriptRunner = new ScriptRunner(this);

    Logger::instance().info("FOUR HANDS controller components initialized");
}

void FHController::initializeAfterConstruction()
{
    // Safe point to perform actions that might call virtuals / logging
    createBaseDirectories();
}

void FHController::createBaseDirectories()
{
    if (m_fileManager) {
        if (m_fileManager->createBaseDirectories()) {
            outputToTerminal("Base directories created successfully", Success);
        } else {
            outputToTerminal("Failed to create some base directories", Warning);
        }
    } else {
        outputToTerminal("File manager not initialized - cannot create directories", Error);
    }
}

void FHController::connectSignals()
{
    // Connect script runner signals
    if (m_scriptRunner) {
        connect(m_scriptRunner, &ScriptRunner::scriptOutput, this, &FHController::onScriptOutput);
        connect(m_scriptRunner, &ScriptRunner::scriptFinished, this, &FHController::onScriptFinished);
    }

    // Connect job number box
    if (m_jobNumberBox) {
        connect(m_jobNumberBox, &QLineEdit::editingFinished, this, [this]() {
            if (m_initializing) return;
            const QString newNum = m_jobNumberBox->text().trimmed();
            if (newNum.isEmpty() || !validateJobNumber(newNum)) return;
            if (newNum != m_cachedJobNumber) {
                saveJobState();
                FHDBManager::instance()->updateLogJobNumber(m_cachedJobNumber, newNum);
                m_cachedJobNumber = newNum;
                refreshTrackerTable();
            }
        });
    }

    // Connect year dropdown
    if (m_yearDDbox) {
        connect(m_yearDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
                this, &FHController::onYearChanged);
    }

    // Connect month dropdown
    if (m_monthDDbox) {
        connect(m_monthDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
                this, &FHController::onMonthChanged);
    }

    // Connect drop number dropdown
    if (m_dropNumberComboBox) {
        connect(m_dropNumberComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int /*index*/) {
                    if (m_initializing) return;
                    QString dropNumber = m_dropNumberComboBox->currentText();
                    onDropNumberChanged(dropNumber);
                });
    }

    // Connect postage box
    if (m_postageBox) {
        QRegularExpressionValidator* validator = new QRegularExpressionValidator(QRegularExpression("[0-9]*\\.?[0-9]*\\$?"), this);
        m_postageBox->setValidator(validator);
        connect(m_postageBox, &QLineEdit::editingFinished, this, &FHController::formatPostageInput);
        connect(m_postageBox, &QLineEdit::textChanged, this, [this]() {
            if (m_initializing) return;
            if (m_jobDataLocked) {
                saveJobState();
            }
        });
    }

    // Connect count box
    if (m_countBox) {
        connect(m_countBox, &QLineEdit::textChanged, this, &FHController::formatCountInput);
        connect(m_countBox, &QLineEdit::textChanged, this, [this]() {
            if (m_initializing) return;
            if (m_jobDataLocked) {
                saveJobState();
            }
        });
    }

    // Connect lock buttons
    if (m_jobDataLockBtn) {
        connect(m_jobDataLockBtn, &QToolButton::clicked, this, &FHController::onJobDataLockClicked);
    }
    if (m_postageLockBtn) {
        connect(m_postageLockBtn, &QToolButton::clicked, this, &FHController::onPostageLockClicked);
    }
    if (m_editBtn) {
        connect(m_editBtn, &QToolButton::clicked, this, &FHController::onEditButtonClicked);
    }

    // Connect script buttons
    if (m_runInitialBtn) {
        connect(m_runInitialBtn, &QPushButton::clicked, this, &FHController::onRunInitialClicked);
    }
    if (m_finalStepBtn) {
        connect(m_finalStepBtn, &QPushButton::clicked, this, &FHController::onFinalStepClicked);
    }

    // Connect drop window
    if (m_dropWindow) {
        setupDropWindow();
    }

    // Connect tracker context menu
    if (m_tracker) {
        m_tracker->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_tracker, &QTableView::customContextMenuRequested,
                this, &FHController::showTableContextMenu);
    }
}

void FHController::setupInitialState()
{
    // Initialize states
    m_jobDataLocked = false;
    m_postageDataLocked = false;
    m_currentHtmlState = DefaultState;

    // Initialize with current date
    QDate currentDate = QDate::currentDate();
    m_currentYear = QString::number(currentDate.year());
    m_currentMonth = QString("%1").arg(currentDate.month(), 2, 10, QChar('0'));

    // Populate dropdowns
    m_initializing = true;
    
    if (m_yearDDbox) {
        populateYearDropdown();
        m_yearDDbox->setCurrentIndex(0);  // Leave on blank
    }
    
    if (m_monthDDbox) {
        populateMonthDropdown();
        m_monthDDbox->setCurrentIndex(0);  // Leave on blank
    }
    
    if (m_dropNumberComboBox) {
        m_dropNumberComboBox->clear();
        m_dropNumberComboBox->addItem("");  // Blank option
        m_dropNumberComboBox->addItem("1");
        m_dropNumberComboBox->addItem("2");
        m_dropNumberComboBox->addItem("3");
        m_dropNumberComboBox->addItem("4");
    }
    
    m_initializing = false;

    // Update UI states
    updateLockStates();
    updateButtonStates();

    Logger::instance().info("FOUR HANDS controller initialization complete");
    qDebug() << "FHController setup complete — m_initializing=false, current date:" << m_currentYear << "/" << m_currentMonth;
}

// Public getters (use cached values instead of UI widgets)
QString FHController::getJobNumber() const
{
    return m_cachedJobNumber;
}

QString FHController::getYear() const
{
    return m_currentYear;
}

QString FHController::getMonth() const
{
    return m_currentMonth;
}

bool FHController::isJobDataLocked() const
{
    return m_jobDataLocked;
}

bool FHController::isPostageDataLocked() const
{
    return m_postageDataLocked;
}

bool FHController::hasJobData() const
{
    return !m_cachedJobNumber.isEmpty() && !m_currentYear.isEmpty() && !m_currentMonth.isEmpty();
}

QString FHController::convertMonthToAbbreviation(const QString& monthNumber) const
{
    QMap<QString, QString> monthMap = {
        {"01", "JAN"}, {"02", "FEB"}, {"03", "MAR"}, {"04", "APR"},
        {"05", "MAY"}, {"06", "JUN"}, {"07", "JUL"}, {"08", "AUG"},
        {"09", "SEP"}, {"10", "OCT"}, {"11", "NOV"}, {"12", "DEC"}
    };
    return monthMap.value(monthNumber, monthNumber);
}

// BaseTrackerController implementation
void FHController::outputToTerminal(const QString& message, MessageType type)
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

    QTextCursor cursor = m_terminalWindow->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_terminalWindow->setTextCursor(cursor);
}

QTableView* FHController::getTrackerWidget() const
{
    return m_tracker;
}

QSqlTableModel* FHController::getTrackerModel() const
{
    return m_trackerModel;
}

QStringList FHController::getTrackerHeaders() const
{
    return {"JOB", "DESCRIPTION", "POSTAGE", "COUNT", "AVG RATE", "CLASS", "SHAPE", "PERMIT"};
}

QList<int> FHController::getVisibleColumns() const
{
    return {1, 2, 3, 4, 5, 6, 7, 8};
}

QString FHController::formatCellData(int columnIndex, const QString& cellData) const
{
    if (columnIndex == 3) { // POSTAGE
        QString clean = cellData;
        if (clean.startsWith("$")) clean.remove(0, 1);
        bool ok;
        double val = clean.toDouble(&ok);
        if (ok) {
            return QString("$%L1").arg(val, 0, 'f', 2);
        } else {
            return cellData;
        }
    }
    if (columnIndex == 4) { // COUNT
        bool ok;
        qlonglong val = cellData.toLongLong(&ok);
        if (ok) {
            return QString("%L1").arg(val);
        } else {
            return cellData;
        }
    }
    return cellData;
}

QString FHController::formatCellDataForCopy(int columnIndex, const QString& cellData) const
{
    if (columnIndex == 2) { // POSTAGE (visible column position)
        QString clean = cellData;
        if (clean.startsWith("$")) clean.remove(0, 1);
        bool ok;
        double val = clean.toDouble(&ok);
        if (ok) {
            return QString("$%L1").arg(val, 0, 'f', 2);
        } else {
            return cellData;
        }
    }
    if (columnIndex == 3) { // COUNT (visible column position)
        QString cleanData = cellData;
        cleanData.remove(',');
        bool ok;
        qlonglong val = cleanData.toLongLong(&ok);
        if (ok) {
            return QString::number(val);
        } else {
            return cellData;
        }
    }
    return cellData;
}

// Lock button handlers
void FHController::onJobDataLockClicked()
{
    if (m_jobDataLockBtn->isChecked()) {
        // Validate job data before locking
        if (m_cachedJobNumber.isEmpty() || !validateJobNumber(m_cachedJobNumber)) {
            outputToTerminal("Job number is required. Please enter a 5-digit job number before locking.", Warning);
            m_jobDataLockBtn->setChecked(false);
            return;
        }
        
        if (m_currentYear.isEmpty() || m_currentMonth.isEmpty()) {
            outputToTerminal("Year and month must be selected before locking.", Warning);
            m_jobDataLockBtn->setChecked(false);
            return;
        }

        if (!validateJobData()) {
            m_jobDataLockBtn->setChecked(false);
            outputToTerminal("Cannot lock job: Please correct the validation errors above.", Error);
            return;
        }

        m_jobDataLocked = true;
        outputToTerminal("Job data locked.", Success);

        // --- FIX: read and validate the current job number from the live field ---
        QString liveJobNumber = m_jobNumberBox ? m_jobNumberBox->text().trimmed() : m_cachedJobNumber;
        if (liveJobNumber.isEmpty() || !validateJobNumber(liveJobNumber)) {
            m_jobDataLockBtn->setChecked(false);
            outputToTerminal("Cannot lock/save: enter a valid 5-digit job number first.", Warning);
            return;
        }
        // keep cache synchronized
        m_cachedJobNumber = liveJobNumber;

        // Normalize drop number (default to "1")
        QString dropNumber = m_dropNumberComboBox ? m_dropNumberComboBox->currentText().trimmed() : m_currentDropNumber;
        if (dropNumber.isEmpty()) dropNumber = "1";
        m_currentDropNumber = dropNumber;

        // Create archive folder BEFORE DB save
        if (m_fileManager) {
            if (!m_fileManager->createJobFolder(m_cachedJobNumber, m_currentDropNumber, m_currentYear, m_currentMonth)) {
                outputToTerminal("Failed to create FOUR HANDS archive folder", Error);
                m_jobDataLockBtn->setChecked(false);
                m_jobDataLocked = false;
                return;
            }
        }

        // Save with validated job number
        if (m_fhDBManager->saveJob(m_cachedJobNumber, m_currentDropNumber, m_currentYear, m_currentMonth)) {
            outputToTerminal("Job saved to database", Success);
        } else {
            outputToTerminal("Failed to save job to database", Error);
            return;
        }

        saveJobState();
        updateLockStates();
        updateButtonStates();
        m_currentHtmlState = DefaultState;
        updateHtmlDisplay();

        emit jobOpened();
        outputToTerminal("Auto-save timer started (15 minutes)", Info);
    } else {
        m_jobDataLockBtn->setChecked(true);
    }
}

void FHController::onEditButtonClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Job data is already unlocked", Info);
        return;
    }

    // Unlock job data
    m_jobDataLocked = false;
    outputToTerminal("Job data unlocked for editing", Success);

    // Update UI states
    updateLockStates();
    updateButtonStates();
    
    // Save the state
    saveJobState();
    updateHtmlDisplay();
}

void FHController::onPostageLockClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot lock postage data: Job data must be locked first", Error);
        m_postageLockBtn->setChecked(false);
        return;
    }

    if (m_postageLockBtn->isChecked()) {
        if (!validatePostageData()) {
            m_postageLockBtn->setChecked(false);
            return;
        }
        
        m_postageDataLocked = true;
        outputToTerminal("Postage data locked", Success);
        
        addLogEntry();
        saveJobState();
    } else {
        m_postageDataLocked = false;
        outputToTerminal("Postage data unlocked", Info);
        saveJobState();
    }

    updateLockStates();
    updateButtonStates();
}

// Script execution handlers
void FHController::onRunInitialClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot run initial script: Job data must be locked first", Error);
        return;
    }

    executeScript("01 INITIAL");
}

void FHController::onFinalStepClicked()
{
    if (!m_postageDataLocked) {
        outputToTerminal("Cannot run final step: Postage data must be locked first", Error);
        return;
    }

    executeScript("02 FINAL PROCESS");
}

void FHController::executeScript(const QString& scriptName)
{
    if (!validateScriptExecution(scriptName)) {
        return;
    }

    QString scriptPath = m_fileManager->getScriptPath(scriptName);

    if (!QFile::exists(scriptPath)) {
        outputToTerminal(QString("Script file not found: %1").arg(scriptPath), Error);
        outputToTerminal("Please ensure scripts are installed in the correct location", Warning);
        return;
    }

    m_lastExecutedScript = scriptName;

    outputToTerminal(QString("Executing script: %1").arg(scriptName), Info);
    outputToTerminal(QString("Script path: %1").arg(scriptPath), Info);

    QStringList args;
    args << m_cachedJobNumber << m_currentYear << m_currentMonth;

    outputToTerminal(QString("Arguments: Job=%1, Year=%2, Month=%3").arg(m_cachedJobNumber, m_currentYear, m_currentMonth), Info);

    m_scriptRunner->runScript(scriptPath, args);
}

void FHController::onScriptOutput(const QString &output)
{
    outputToTerminal(output, Info);
    parseScriptOutput(output);
}

void FHController::onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::CrashExit) {
        outputToTerminal("Script crashed unexpectedly", Error);
        return;
    }

    if (exitCode == 0) {
        outputToTerminal("Script completed successfully", Success);

        if (m_lastExecutedScript == "02 FINAL PROCESS") {
            if (m_trackerModel) {
                m_trackerModel->select();
            }
        }
    } else {
        outputToTerminal(QString("Script failed with exit code: %1").arg(exitCode), Error);
    }
}

void FHController::parseScriptOutput(const QString& output)
{
    Q_UNUSED(output);
}

// State management methods
void FHController::updateLockStates()
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

void FHController::updateButtonStates()
{
    bool jobFieldsEnabled = !m_jobDataLocked;
    if (m_jobNumberBox) m_jobNumberBox->setEnabled(jobFieldsEnabled);
    if (m_yearDDbox) m_yearDDbox->setEnabled(jobFieldsEnabled);
    if (m_monthDDbox) m_monthDDbox->setEnabled(jobFieldsEnabled);
    if (m_dropNumberComboBox) m_dropNumberComboBox->setEnabled(jobFieldsEnabled);

    if (m_dropWindow) {
        m_dropWindow->setEnabled(!m_jobDataLocked);
        m_dropWindow->setAcceptDrops(!m_jobDataLocked);
    }
    
    if (m_postageBox) m_postageBox->setEnabled(!m_postageDataLocked);
    if (m_countBox) m_countBox->setEnabled(!m_postageDataLocked);

    if (m_jobDataLockBtn) m_jobDataLockBtn->setChecked(m_jobDataLocked);
    if (m_postageLockBtn) m_postageLockBtn->setChecked(m_postageDataLocked);

    if (m_postageLockBtn) m_postageLockBtn->setEnabled(m_jobDataLocked);
    if (m_editBtn) m_editBtn->setEnabled(m_jobDataLocked);

    if (m_runInitialBtn) m_runInitialBtn->setEnabled(m_jobDataLocked);
    if (m_finalStepBtn) m_finalStepBtn->setEnabled(m_postageDataLocked);
}

// Validation methods
bool FHController::validateJobData()
{
    if (m_cachedJobNumber.isEmpty() || m_cachedJobNumber.length() != 5) {
        outputToTerminal("Error: Job number must be exactly 5 digits", Error);
        return false;
    }

    if (m_currentYear.isEmpty()) {
        outputToTerminal("Error: Year must be selected", Error);
        return false;
    }

    if (m_currentMonth.isEmpty()) {
        outputToTerminal("Error: Month must be selected", Error);
        return false;
    }

    bool ok;
    m_cachedJobNumber.toInt(&ok);
    if (!ok) {
        outputToTerminal("Error: Job number must contain only digits", Error);
        return false;
    }

    return true;
}

bool FHController::validatePostageData()
{
    if (!m_postageBox || !m_countBox) {
        return true;
    }

    bool isValid = true;

    QString postage = m_postageBox->text();
    if (postage.isEmpty() || postage == "$") {
        outputToTerminal("Postage amount is required.", Error);
        isValid = false;
    } else {
        QString cleanPostage = postage;
        cleanPostage.remove("$");
        cleanPostage.remove(",");
        bool ok;
        double postageValue = cleanPostage.toDouble(&ok);
        if (!ok || postageValue <= 0) {
            outputToTerminal("Invalid postage amount.", Error);
            isValid = false;
        }
    }

    QString count = m_countBox->text();
    if (count.isEmpty()) {
        outputToTerminal("Count is required.", Error);
        isValid = false;
    } else {
        QString cleanCount = count;
        cleanCount.remove(',').remove(' ');
        bool ok;
        int countValue = cleanCount.toInt(&ok);
        if (!ok || countValue <= 0) {
            outputToTerminal("Invalid count. Must be a positive integer.", Error);
            isValid = false;
        }
    }

    return isValid;
}

void FHController::formatPostageInput()
{
    if (!m_postageBox) return;

    QString text = m_postageBox->text().trimmed();
    if (text.isEmpty()) return;

    QString cleanText = text;
    static const QRegularExpression nonNumericRegex("[^0-9.]");
    cleanText.remove(nonNumericRegex);

    int decimalPos = cleanText.indexOf('.');
    if (decimalPos != -1) {
        QString beforeDecimal = cleanText.left(decimalPos + 1);
        QString afterDecimal = cleanText.mid(decimalPos + 1).remove('.');
        cleanText = beforeDecimal + afterDecimal;
    }

    QString formatted;
    if (!cleanText.isEmpty() && cleanText != ".") {
        bool ok;
        double value = cleanText.toDouble(&ok);
        if (ok) {
            formatted = QString("$%L1").arg(value, 0, 'f', 2);
        } else {
            formatted = "$" + cleanText;
        }
    }

    m_postageBox->setText(formatted);
}

void FHController::formatCountInput(const QString& text)
{
    if (!m_countBox) return;

    QString cleanText = text;
    static const QRegularExpression nonDigitRegex("[^0-9]");
    cleanText.remove(nonDigitRegex);

    QString formatted;
    if (!cleanText.isEmpty()) {
        bool ok;
        int number = cleanText.toInt(&ok);
        if (ok) {
            formatted = QString("%L1").arg(number);
        } else {
            formatted = cleanText;
        }
    }

    if (m_countBox->text() != formatted) {
        m_countBox->blockSignals(true);
        m_countBox->setText(formatted);
        m_countBox->blockSignals(false);
    }
}

bool FHController::validateScriptExecution(const QString& scriptName) const
{
    if (scriptName.isEmpty()) {
        return false;
    }

    if (!m_fileManager) {
        return false;
    }

    if (!m_scriptRunner) {
        return false;
    }

    return true;
}

// Job management methods
bool FHController::loadJob(const QString& jobNumber, const QString& dropNumber)
{
    if (!m_fhDBManager) {
        outputToTerminal("Database manager not initialized", Error);
        return false;
    }

    QString normalizedJobNumber = jobNumber.trimmed();
    QString normalizedDropNumber = dropNumber.trimmed();
    if (normalizedDropNumber.isEmpty()) {
        normalizedDropNumber = "1";
    }

    QString year;
    QString month;
    if (!m_fhDBManager->loadJob(normalizedJobNumber, normalizedDropNumber, year, month)) {
        outputToTerminal(QString("No job found for %1 (Drop %2)").arg(normalizedJobNumber, normalizedDropNumber), Warning);
        return false;
    }

    // Cache identity
    m_cachedJobNumber = normalizedJobNumber;
    m_currentDropNumber = normalizedDropNumber;
    m_currentYear = year;
    m_currentMonth = month;

    // Update UI (guard signals to prevent loops)
    m_initializing = true;
    if (m_jobNumberBox) m_jobNumberBox->setText(m_cachedJobNumber);
    if (m_yearDDbox) m_yearDDbox->setCurrentText(m_currentYear);
    if (m_monthDDbox) m_monthDDbox->setCurrentText(m_currentMonth);
    if (m_dropNumberComboBox) m_dropNumberComboBox->setCurrentText(m_currentDropNumber);
    m_initializing = false;

    QCoreApplication::processEvents();
    loadJobState();

    copyFilesFromHomeFolder();

    if (m_jobDataLockBtn) m_jobDataLockBtn->setChecked(m_jobDataLocked);
    if (m_jobDataLocked) {
        emit jobOpened();
        outputToTerminal("Auto-save timer started (15 minutes)", Info);
    }

    updateLockStates();
    updateButtonStates();
    m_currentHtmlState = UninitializedState;
    updateHtmlDisplay();

    outputToTerminal(QString("Job loaded: %1 (Drop %2) %3/%4")
                         .arg(m_cachedJobNumber, m_currentDropNumber, m_currentYear, m_currentMonth), Success);
    return true;
}

void FHController::resetToDefaults()
{
    saveJobState();

    m_initializing = true;

    if (m_jobNumberBox) m_jobNumberBox->clear();
    if (m_postageBox) m_postageBox->clear();
    if (m_countBox) m_countBox->clear();
    if (m_yearDDbox) m_yearDDbox->setCurrentIndex(0);
    if (m_monthDDbox) m_monthDDbox->setCurrentIndex(0);

    m_jobDataLocked = false;
    m_postageDataLocked = false;
    m_lastExecutedScript.clear();

    // Reset cached values to current date
    QDate currentDate = QDate::currentDate();
    m_currentYear = QString::number(currentDate.year());
    m_currentMonth = QString("%1").arg(currentDate.month(), 2, 10, QChar('0'));
    m_cachedJobNumber.clear();

    if (m_jobDataLockBtn) m_jobDataLockBtn->setChecked(false);
    if (m_postageLockBtn) m_postageLockBtn->setChecked(false);

    if (m_terminalWindow) m_terminalWindow->clear();

    m_initializing = false;

    updateLockStates();
    updateButtonStates();

    emit jobClosed();
    outputToTerminal("Job state reset to defaults", Info);
    outputToTerminal("Auto-save timer stopped - no job open", Info);
}

void FHController::saveJobState()
{
    if (!m_fhDBManager) return;

    QString jobNumber = m_cachedJobNumber;
    if (jobNumber.isEmpty() && m_jobNumberBox) {
        jobNumber = m_jobNumberBox->text().trimmed();
        if (!jobNumber.isEmpty() && validateJobNumber(jobNumber)) {
            m_cachedJobNumber = jobNumber;
        }
    }
    if (m_cachedJobNumber.isEmpty()) {
        outputToTerminal("Cannot save job: Job number is empty — please enter a 5-digit job number.", Warning);
        return;
    }
    if (m_currentYear.isEmpty()) {
        outputToTerminal("Cannot save job: Year not selected.", Warning);
        return;
    }
    if (m_currentMonth.isEmpty()) {
        outputToTerminal("Cannot save job: Month not selected.", Warning);
        return;
    }

    // Normalize drop number (default to "1")
    QString dropNumber = !m_currentDropNumber.trimmed().isEmpty() ? m_currentDropNumber.trimmed()
                                                                  : (m_dropNumberComboBox ? m_dropNumberComboBox->currentText().trimmed() : QString());
    if (dropNumber.isEmpty()) {
        dropNumber = "1";
    }
    m_currentDropNumber = dropNumber;

    // Save job with cached values
    if (m_fhDBManager->saveJob(m_cachedJobNumber, m_currentDropNumber, m_currentYear, m_currentMonth)) {
        outputToTerminal("Job saved to database", Success);
    } else {
        outputToTerminal("Failed to save job to database", Error);
    }

    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";

    if (m_fhDBManager->saveJobState(m_cachedJobNumber, m_currentDropNumber, m_currentYear, m_currentMonth,
                                   static_cast<int>(m_currentHtmlState),
                                   m_jobDataLocked, m_postageDataLocked,
                                   postage, count, m_lastExecutedScript)) {
        outputToTerminal(QString("Job state saved to database: postage=%1, count=%2, postage_locked=%3")
                             .arg(postage, count, m_postageDataLocked ? "true" : "false"), Success);
    } else {
        outputToTerminal("Failed to save job state to database", Error);
    }
}

void FHController::loadJobState()
{
    if (!m_fhDBManager) return;

    if (m_currentYear.isEmpty() || m_currentMonth.isEmpty()) return;

    if (m_cachedJobNumber.isEmpty()) return;
    if (m_currentDropNumber.trimmed().isEmpty()) {
        m_currentDropNumber = "1";
    }

    if (m_cachedJobNumber.isEmpty()) return;
    if (m_currentDropNumber.isEmpty()) m_currentDropNumber = "1";

    int htmlState;
    bool jobLocked, postageLocked;
    QString postage, count, lastExecutedScript;

    if (m_fhDBManager->loadJobState(m_cachedJobNumber, m_currentDropNumber, m_currentYear, m_currentMonth,
                                   htmlState, jobLocked, postageLocked,
                                   postage, count, lastExecutedScript)) {
        m_initializing = true;
        
        m_currentHtmlState = static_cast<HtmlDisplayState>(htmlState);
        m_jobDataLocked = jobLocked;
        m_postageDataLocked = postageLocked;
        m_lastExecutedScript = lastExecutedScript;

        if (m_postageBox && !postage.isEmpty()) {
            m_postageBox->setText(postage);
        }
        if (m_countBox && !count.isEmpty()) {
            m_countBox->setText(count);
        }
        m_initializing = false;

        m_currentHtmlState = m_jobDataLocked ? InstructionsState : DefaultState;
        updateLockStates();
        updateButtonStates();
        updateHtmlDisplay();
        
        outputToTerminal(QString("Job state loaded: postage=%1, count=%2, postage_locked=%3")
                             .arg(postage, count, m_postageDataLocked ? "true" : "false"), Info);
    } else {
        m_jobDataLocked = false;
        m_postageDataLocked = false;
        m_currentHtmlState = UninitializedState;
        m_lastExecutedScript = "";
        m_currentHtmlState = m_jobDataLocked ? InstructionsState : DefaultState;
        updateLockStates();
        updateButtonStates();
        updateHtmlDisplay();
        outputToTerminal("No saved job state found, using defaults", Info);
    }
}

// Tracker operations
void FHController::onAddToTracker()
{
    if (!validateJobData()) {
        outputToTerminal("Cannot add to tracker: Invalid job data", Error);
        return;
    }

    outputToTerminal("Add to tracker functionality ready", Info);
}

void FHController::addLogEntry()
{
    if (!m_fhDBManager) {
        outputToTerminal("Database manager not available for log entry", Error);
        return;
    }

    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";

    if (m_cachedJobNumber.isEmpty() || m_currentMonth.isEmpty() || postage.isEmpty() || count.isEmpty()) {
        outputToTerminal(QString("Cannot add log entry: missing required data. Job: '%1', Month: '%2', Postage: '%3', Count: '%4'")
                             .arg(m_cachedJobNumber, m_currentMonth, postage, count), Warning);
        return;
    }

    QString monthAbbrev = convertMonthToAbbreviation(m_currentMonth);
    QString description = QString("FH %1").arg(monthAbbrev);

    QString cleanCount = count;
    cleanCount.remove(',').remove(' ');
    int countValue = cleanCount.toInt();
    QString formattedCount = QString::number(countValue);

    QString formattedPostage = postage;
    if (!formattedPostage.startsWith("$")) {
        formattedPostage = "$" + formattedPostage;
    }
    double postageAmount = formattedPostage.remove("$").toDouble();
    formattedPostage = QString("$%1").arg(postageAmount, 0, 'f', 2);

    double avgRate = (countValue > 0) ? (postageAmount / countValue) : 0.0;
    QString formattedAvgRate = QString("%1").arg(avgRate, 0, 'f', 3);

    QString mailClass = "STD";
    QString shape = "LTR";
    QString permit = "1662";
    QString date = QDate::currentDate().toString("MM/dd/yyyy");

    if (m_fhDBManager->updateLogEntryForJob(m_cachedJobNumber, description, formattedPostage, formattedCount,
                                            formattedAvgRate, mailClass, shape, permit, date)) {
        outputToTerminal(QString("Log entry updated for job %1: %2 pieces at %3 (%4 avg rate)")
                             .arg(m_cachedJobNumber, formattedCount, formattedPostage, formattedAvgRate), Success);
    } else {
        if (m_fhDBManager->addLogEntry(m_cachedJobNumber, description, formattedPostage, formattedCount,
                                       formattedAvgRate, mailClass, shape, permit, date)) {
            outputToTerminal(QString("Log entry added for job %1: %2 pieces at %3 (%4 avg rate)")
                                 .arg(m_cachedJobNumber, formattedCount, formattedPostage, formattedAvgRate), Success);
        } else {
            outputToTerminal("Failed to add/update log entry", Error);
            return;
        }
    }
    
    if (m_trackerModel) {
        m_trackerModel->select();
    }
}

void FHController::onCopyRowClicked()
{
    QString result = copyFormattedRow();
    outputToTerminal(result, result.contains("success") ? Success : Error);
}

void FHController::refreshTrackerTable()
{
    if (m_trackerModel) {
        m_trackerModel->select();
        applyTrackerHeaders();
        outputToTerminal("Tracker table refreshed", Info);
    }
}

void FHController::setupTrackerModel()
{
    if (!m_tracker || !m_fhDBManager) return;

    m_trackerModel = new FormattedSqlModel(this, DatabaseManager::instance()->getDatabase(), this);
    m_trackerModel->setTable("fh_log");
    m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_trackerModel->select();
    applyTrackerHeaders();

    if (m_trackerModel) {
        m_tracker->setModel(m_trackerModel);

        QList<int> visibleColumns = getVisibleColumns();
        for (int i = 0; i < m_trackerModel->columnCount(); ++i) {
            m_tracker->setColumnHidden(i, !visibleColumns.contains(i));
        }

        m_tracker->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_tracker->setSelectionMode(QAbstractItemView::SingleSelection);

        outputToTerminal("Tracker model initialized successfully", Success);
        setupOptimizedTableLayout();

        if (m_tracker) {
            m_tracker->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(m_tracker, &QTableView::customContextMenuRequested,
                    this, &FHController::showTableContextMenu);
        }
    } else {
        outputToTerminal("Failed to initialize tracker model", Error);
    }
}

void FHController::setupOptimizedTableLayout()
{
    if (!m_tracker || !m_trackerModel) {
        qCritical() << "setupOptimizedTableLayout() called with null tracker or model!";
        return;
    }

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
        {"DESCRIPTION", "FH DEC", 140},
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
    for (int fontSize = 11; fontSize >= 7; fontSize--) {
        testFont.setPointSize(fontSize);
        fm = QFontMetrics(testFont);

        int totalWidth = 0;
        bool fits = true;

        for (auto it = columns.cbegin(); it != columns.cend(); ++it) {
            const auto& col = *it;
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

    m_trackerModel->setSort(0, Qt::DescendingOrder);
    m_trackerModel->select();

    m_trackerModel->setHeaderData(1, Qt::Horizontal, tr("JOB"));
    m_trackerModel->setHeaderData(2, Qt::Horizontal, tr("DESCRIPTION"));
    m_trackerModel->setHeaderData(3, Qt::Horizontal, tr("POSTAGE"));
    m_trackerModel->setHeaderData(4, Qt::Horizontal, tr("COUNT"));
    m_trackerModel->setHeaderData(5, Qt::Horizontal, tr("AVG RATE"));
    m_trackerModel->setHeaderData(6, Qt::Horizontal, tr("CLASS"));
    m_trackerModel->setHeaderData(7, Qt::Horizontal, tr("SHAPE"));
    m_trackerModel->setHeaderData(8, Qt::Horizontal, tr("PERMIT"));

    m_tracker->setColumnHidden(0, true);

    int totalCols = m_trackerModel->columnCount();
    for (int i = 9; i < totalCols; i++) {
        m_tracker->setColumnHidden(i, true);
    }

    fm = QFontMetrics(tableFont);
    for (int i = 0; i < columns.size(); i++) {
        const auto& col = columns[i];
        int headerWidth = fm.horizontalAdvance(col.header) + 12;
        int contentWidth = fm.horizontalAdvance(col.maxContent) + 12;
        int colWidth = qMax(headerWidth, qMax(contentWidth, col.minWidth));

        m_tracker->setColumnWidth(i + 1, colWidth);
    }

    m_tracker->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_tracker->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tracker->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

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
        "   font-family: 'Blender Pro Bold';"
        "}"
        "QTableView::item {"
        "   padding: 3px;"
        "   border-right: 1px solid #cccccc;"
        "}"
        );

    m_tracker->setAlternatingRowColors(true);
}

// Dropdown population methods
void FHController::populateYearDropdown()
{
    if (!m_yearDDbox) return;
    
    m_yearDDbox->clear();
    m_yearDDbox->addItem("");
    
    QDate currentDate = QDate::currentDate();
    int currentYear = currentDate.year();
    
    m_yearDDbox->addItem(QString::number(currentYear - 1));
    m_yearDDbox->addItem(QString::number(currentYear));
    m_yearDDbox->addItem(QString::number(currentYear + 1));
}

void FHController::populateMonthDropdown()
{
    if (!m_monthDDbox) return;
    
    m_monthDDbox->clear();
    m_monthDDbox->addItem("");
    
    for (int i = 1; i <= 12; i++) {
        m_monthDDbox->addItem(QString("%1").arg(i, 2, 10, QChar('0')));
    }
}

// Dropdown change handlers
void FHController::onYearChanged(const QString& year)
{
    if (m_initializing) return;
    
    // Update cached value only - do NOT load job state automatically
    m_currentYear = year;
}

void FHController::onMonthChanged(const QString& month)
{
    if (m_initializing) return;
    
    // Update cached value only - do NOT load job state automatically
    m_currentMonth = month;
}

void FHController::onDropNumberChanged(const QString& dropNumber)
{
    if (m_initializing) return;
    
    m_currentDropNumber = dropNumber;
    qDebug() << "FOUR HANDS Drop Number changed to:" << dropNumber;
    outputToTerminal(QString("Drop Number set to: %1").arg(dropNumber.isEmpty() ? "(none)" : dropNumber), Info);
}

// Directory management
void FHController::setupDropWindow()
{
    if (!m_dropWindow) {
        return;
    }

    Logger::instance().info("Setting up FOUR HANDS drop window...");

    QString targetDirectory = "C:/Goji/AUTOMATION/FOUR HANDS/ORIGINAL";
    m_dropWindow->setTargetDirectory(targetDirectory);
    m_dropWindow->setSupportedExtensions({"xlsx", "xls", "csv", "zip"});

    connect(m_dropWindow, &DropWindow::filesDropped,
            this, &FHController::onFilesDropped);
    connect(m_dropWindow, &DropWindow::fileDropError,
            this, &FHController::onFileDropError);

    m_dropWindow->clearFiles();

    outputToTerminal(QString("Drop window configured for directory: %1").arg(targetDirectory), Info);
    Logger::instance().info("FOUR HANDS drop window setup complete");
}

void FHController::onFileSystemChanged()
{
    outputToTerminal("File system changed", Info);
}

bool FHController::moveFilesToHomeFolder()
{
    if (!m_fileManager) {
        return false;
    }
    if (m_cachedJobNumber.isEmpty() || m_currentYear.isEmpty() || m_currentMonth.isEmpty()) {
        return false;
    }
    if (m_currentDropNumber.trimmed().isEmpty()) {
        m_currentDropNumber = "1";
    }

    const QString jobBase = m_fileManager->getJobFolderPath(m_cachedJobNumber, m_currentDropNumber, m_currentYear, m_currentMonth);
    if (jobBase.isEmpty()) {
        return false;
    }

    struct FolderPair { QString src; QString dst; QString label; };
    const QList<FolderPair> folders = {
        { m_fileManager->getInputPath(),    jobBase + "/INPUT",    "INPUT" },
        { m_fileManager->getOriginalPath(), jobBase + "/ORIGINAL", "ORIGINAL" },
        { m_fileManager->getOutputPath(),   jobBase + "/OUTPUT",   "OUTPUT" }
    };

    auto copyAndDeleteAllFiles = [this](const QString& srcDirPath, const QString& dstDirPath) -> bool {
        QDir srcDir(srcDirPath);
        if (!srcDir.exists()) {
            return true; // nothing to do
        }
        QDir().mkpath(dstDirPath);
        const QFileInfoList files = srcDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo& fi : files) {
            const QString srcFile = fi.absoluteFilePath();
            const QString dstFile = QDir(dstDirPath).filePath(fi.fileName());

            if (QFile::exists(dstFile)) {
                QFile::remove(dstFile);
            }

            if (!QFile::copy(srcFile, dstFile)) {
                outputToTerminal(QString("Failed to archive file: %1").arg(fi.fileName()), Error);
                return false;
            }
        }

        // Copy succeeded for all files; now delete working copies.
        for (const QFileInfo& fi : files) {
            QFile::remove(fi.absoluteFilePath());
        }
        return true;
    };

    bool ok = true;
    for (const FolderPair& fp : folders) {
        if (!copyAndDeleteAllFiles(fp.src, fp.dst)) {
            ok = false;
            outputToTerminal(QString("Failed to archive %1 folder files.").arg(fp.label), Error);
        }
    }
    return ok;
}

bool FHController::copyFilesFromHomeFolder()
{
    if (!m_fileManager) {
        return false;
    }
    if (m_cachedJobNumber.isEmpty() || m_currentYear.isEmpty() || m_currentMonth.isEmpty()) {
        return false;
    }
    if (m_currentDropNumber.trimmed().isEmpty()) {
        m_currentDropNumber = "1";
    }

    const QString jobBase = m_fileManager->getJobFolderPath(m_cachedJobNumber, m_currentDropNumber, m_currentYear, m_currentMonth);
    if (jobBase.isEmpty()) {
        return false;
    }

    struct FolderPair { QString src; QString dst; QString label; };
    const QList<FolderPair> folders = {
        { jobBase + "/INPUT",    m_fileManager->getInputPath(),    "INPUT" },
        { jobBase + "/ORIGINAL", m_fileManager->getOriginalPath(), "ORIGINAL" },
        { jobBase + "/OUTPUT",   m_fileManager->getOutputPath(),   "OUTPUT" }
    };

    auto copyAllFiles = [this](const QString& srcDirPath, const QString& dstDirPath) -> bool {
        QDir srcDir(srcDirPath);
        if (!srcDir.exists()) {
            return true; // nothing to copy
        }
        QDir().mkpath(dstDirPath);
        const QFileInfoList files = srcDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo& fi : files) {
            const QString srcFile = fi.absoluteFilePath();
            const QString dstFile = QDir(dstDirPath).filePath(fi.fileName());

            if (QFile::exists(dstFile)) {
                QFile::remove(dstFile);
            }

            if (!QFile::copy(srcFile, dstFile)) {
                outputToTerminal(QString("Failed to restore file: %1").arg(fi.fileName()), Error);
                return false;
            }
        }
        return true;
    };

    bool ok = true;
    for (const FolderPair& fp : folders) {
        if (!copyAllFiles(fp.src, fp.dst)) {
            ok = false;
            outputToTerminal(QString("Failed to restore %1 folder files.").arg(fp.label), Error);
        }
    }
    return ok;
}

// Drop window handlers
void FHController::onFilesDropped(const QStringList& filePaths)
{
    outputToTerminal(QString("Files received: %1 file(s) dropped").arg(filePaths.size()), Success);
    
    for (auto it = filePaths.cbegin(); it != filePaths.cend(); ++it) {
        const QString& filePath = *it;
        const QFileInfo fileInfo(filePath);
        const QString fileName = fileInfo.fileName();
        outputToTerminal(QString("  - %1").arg(fileName), Info);
    }
    
    outputToTerminal("Files are ready for processing in INPUT folder", Info);
}

void FHController::onFileDropError(const QString& errorMessage)
{
    outputToTerminal(QString("File drop error: %1").arg(errorMessage), Warning);
}

void FHController::showTableContextMenu(const QPoint& pos)
{
    if (!m_tracker)
        return;

    QMenu menu(m_tracker);
    QAction* copyAction = menu.addAction("Copy Selected Row");

    QAction* selectedAction = menu.exec(m_tracker->mapToGlobal(pos));
    if (selectedAction == copyAction) {
        QString result = copyFormattedRow();
        if (result == "Row copied to clipboard") {
            outputToTerminal("Row copied to clipboard with formatting", Success);
        } else {
            outputToTerminal(result, Warning);
        }
    }
}

bool FHController::validateJobNumber(const QString& jobNumber) const {
    if (jobNumber.length() != 5) return false;
    for (const QChar ch : jobNumber) if (!ch.isDigit()) return false;
    return true;
}

FHController::HtmlDisplayState FHController::determineHtmlState() const
{
    // Determine which HTML file to display based on current job lock state
    if (m_jobDataLocked) {
        return InstructionsState;  // Show instructions.html when job is locked
    } else {
        return DefaultState;       // Show default.html when job is unlocked
    }
}

void FHController::updateHtmlDisplay()
{
    if (!m_textBrowser) {
        outputToTerminal("DEBUG: No text browser available!", Error);
        return;
    }

    HtmlDisplayState targetState = determineHtmlState();

    if (m_textBrowser->document()->isEmpty() || m_currentHtmlState == UninitializedState) {
        if (targetState == InstructionsState) {
            loadHtmlFile(":/resources/fourhands/instructions.html");
        } else {
            loadHtmlFile(":/resources/fourhands/default.html");
        }
        m_currentHtmlState = targetState;
        return;
    }

    if (m_currentHtmlState != targetState) {
        m_currentHtmlState = targetState;
        if (targetState == InstructionsState) {
            loadHtmlFile(":/resources/fourhands/instructions.html");
        } else {
            loadHtmlFile(":/resources/fourhands/default.html");
        }
    } else {
        outputToTerminal("DEBUG: HTML state unchanged, not loading new file", Info);
    }
}

void FHController::loadHtmlFile(const QString& resourcePath)
{
    if (!m_textBrowser) {
        outputToTerminal("DEBUG: Cannot load HTML — text browser not set.", Error);
        Logger::instance().error("FH loadHtmlFile: text browser pointer is null");
        return;
    }

    QFile file(resourcePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        QString htmlContent = stream.readAll();
        file.close();
        m_textBrowser->setHtml(htmlContent);
        Logger::instance().info("FH: Loaded HTML file: " + resourcePath);
        outputToTerminal(QString("DEBUG: HTML loaded from %1").arg(resourcePath), Info);
    } else {
        Logger::instance().warning("FH: Failed to load HTML file: " + resourcePath);
        outputToTerminal(QString("DEBUG: Failed to open HTML file: %1").arg(resourcePath), Error);

        QString fallbackContent = QString(
            "<html><body style='font-family: Arial; padding: 20px;'>"
            "<h2>FOUR HANDS</h2>"
            "<p>Instructions not available</p>"
            "<p>Please check that HTML resources are properly installed.</p>"
            "<p>Attempted path: %1</p>"
            "</body></html>"
        ).arg(resourcePath);
        m_textBrowser->setHtml(fallbackContent);
    }
}

void FHController::autoSaveAndCloseCurrentJob()
{
    if (m_jobDataLocked) {
        if (!m_cachedJobNumber.isEmpty() && !m_currentYear.isEmpty() && !m_currentMonth.isEmpty()) {
            if (m_currentDropNumber.trimmed().isEmpty()) {
                m_currentDropNumber = "1";
            }
            outputToTerminal(QString("Auto-saving current job %1 (%2-%3) before opening new job")
                                 .arg(m_cachedJobNumber, m_currentYear, m_currentMonth), Info);

            FHDBManager* dbManager = FHDBManager::instance();
            if (dbManager) {
                if (dbManager->saveJob(m_cachedJobNumber, m_currentDropNumber, m_currentYear, m_currentMonth)) {
                    outputToTerminal("Job saved to database", Success);
                } else {
                    outputToTerminal("Failed to save job to database", Error);
                }

                QString postage = m_postageBox ? m_postageBox->text() : "";
                QString count = m_countBox ? m_countBox->text() : "";
                if (dbManager->saveJobState(m_cachedJobNumber, m_currentDropNumber, m_currentYear, m_currentMonth,
                                            static_cast<int>(m_currentHtmlState),
                                            m_jobDataLocked, m_postageDataLocked,
                                            postage, count, m_lastExecutedScript)) {
                    outputToTerminal(QString("Job state saved to database: postage=%1, count=%2, postage_locked=%3")
                                         .arg(postage, count, m_postageDataLocked ? "true" : "false"), Success);
                } else {
                    outputToTerminal("Failed to save job state to database", Error);
                }

                moveFilesToHomeFolder();

                m_jobDataLocked = false;
                m_postageDataLocked = false;
                m_currentHtmlState = UninitializedState;

                updateLockStates();
                updateButtonStates();
                emit jobClosed();

                outputToTerminal("Current job auto-saved and closed", Success);
            } else {
                outputToTerminal("Database manager not initialized", Error);
            }
        }
    }
}

void FHController::applyTrackerHeaders()
{
    if (!m_trackerModel) return;

    const int idxJob         = m_trackerModel->fieldIndex(QStringLiteral("job_number"));
    const int idxDescription = m_trackerModel->fieldIndex(QStringLiteral("description"));
    const int idxPostage     = m_trackerModel->fieldIndex(QStringLiteral("postage"));
    const int idxCount       = m_trackerModel->fieldIndex(QStringLiteral("count"));
    const int idxAvgRate     = m_trackerModel->fieldIndex(QStringLiteral("per_piece"));
    const int idxMailClass   = m_trackerModel->fieldIndex(QStringLiteral("class"));
    const int idxShape       = m_trackerModel->fieldIndex(QStringLiteral("shape"));
    const int idxPermit      = m_trackerModel->fieldIndex(QStringLiteral("permit"));

    if (idxJob         >= 0) m_trackerModel->setHeaderData(idxJob,         Qt::Horizontal, tr("JOB"),         Qt::DisplayRole);
    if (idxDescription >= 0) m_trackerModel->setHeaderData(idxDescription, Qt::Horizontal, tr("DESCRIPTION"), Qt::DisplayRole);
    if (idxPostage     >= 0) m_trackerModel->setHeaderData(idxPostage,     Qt::Horizontal, tr("POSTAGE"),     Qt::DisplayRole);
    if (idxCount       >= 0) m_trackerModel->setHeaderData(idxCount,       Qt::Horizontal, tr("COUNT"),       Qt::DisplayRole);
    if (idxAvgRate     >= 0) m_trackerModel->setHeaderData(idxAvgRate,     Qt::Horizontal, tr("AVG RATE"),    Qt::DisplayRole);
    if (idxMailClass   >= 0) m_trackerModel->setHeaderData(idxMailClass,   Qt::Horizontal, tr("CLASS"),       Qt::DisplayRole);
    if (idxShape       >= 0) m_trackerModel->setHeaderData(idxShape,       Qt::Horizontal, tr("SHAPE"),       Qt::DisplayRole);
    if (idxPermit      >= 0) m_trackerModel->setHeaderData(idxPermit,      Qt::Horizontal, tr("PERMIT"),      Qt::DisplayRole);
}
