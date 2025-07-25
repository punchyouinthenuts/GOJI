#include "tmweeklypccontroller.h"
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
#include <QTableWidgetItem>
#include <QMimeData>
#include <QClipboard>
#include <QApplication>
#include <QHeaderView>
#include <QDateTime>
#include <QFontMetrics>
#include <QFile>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include "logger.h"

class FormattedSqlModel : public QSqlTableModel {
public:
    FormattedSqlModel(QObject *parent, QSqlDatabase db, TMWeeklyPCController *ctrl)
        : QSqlTableModel(parent, db), controller(ctrl) {}
    QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole) {
            QVariant val = QSqlTableModel::data(idx, role);
            return controller->formatCellData(idx.column(), val.toString());
        }
        return QSqlTableModel::data(idx, role);
    }
private:
    TMWeeklyPCController *controller;
};

TMWeeklyPCController::TMWeeklyPCController(QObject *parent)
    : BaseTrackerController(parent),
    m_dbManager(nullptr),
    m_fileManager(nullptr),
    m_tmWeeklyPCDBManager(nullptr),
    m_scriptRunner(nullptr),
    m_runInitialBtn(nullptr),
    m_openBulkMailerBtn(nullptr),
    m_runProofDataBtn(nullptr),
    m_openProofFileBtn(nullptr),
    m_runWeeklyMergedBtn(nullptr),
    m_openPrintFileBtn(nullptr),
    m_runPostPrintBtn(nullptr),
    m_lockBtn(nullptr),
    m_editBtn(nullptr),
    m_postageLockBtn(nullptr),
    m_proofDDbox(nullptr),
    m_printDDbox(nullptr),
    m_yearDDbox(nullptr),
    m_monthDDbox(nullptr),
    m_weekDDbox(nullptr),
    m_classDDbox(nullptr),
    m_permitDDbox(nullptr),
    m_jobNumberBox(nullptr),
    m_postageBox(nullptr),
    m_countBox(nullptr),
    m_terminalWindow(nullptr),
    m_tracker(nullptr),
    m_textBrowser(nullptr),
    m_proofApprovalCheckBox(nullptr),
    m_jobDataLocked(false),
    m_postageDataLocked(false),
    m_currentHtmlState(UninitializedState),
    m_lastExecutedScript(),
    m_capturedNASPath(),
    m_capturingNASPath(false),
    m_trackerModel(nullptr)
{
    Logger::instance().info("Initializing TMWeeklyPCController...");

    // Get the database managers
    m_dbManager = DatabaseManager::instance();
    if (!m_dbManager) {
        Logger::instance().error("Failed to get DatabaseManager instance");
        return;
    }

    // Verify core database is initialized
    if (!m_dbManager->isInitialized()) {
        Logger::instance().error("Core DatabaseManager not initialized");
        return;
    }

    // Get TM Weekly PC database manager
    m_tmWeeklyPCDBManager = TMWeeklyPCDBManager::instance();
    if (!m_tmWeeklyPCDBManager) {
        Logger::instance().error("Failed to get TMWeeklyPCDBManager instance");
        return;
    }

    // Initialize the TM Weekly PC database manager
    if (!m_tmWeeklyPCDBManager->initialize()) {
        Logger::instance().error("Failed to initialize TMWeeklyPCDBManager");
        // Continue anyway - some functionality may still work
    } else {
        Logger::instance().info("TMWeeklyPCDBManager initialized successfully");
    }

    // Create a script runner
    m_scriptRunner = new ScriptRunner(this);

    // Get file manager - direct creation instead of using the factory
    // Use a new QSettings instance since DatabaseManager doesn't provide getSettings
    m_fileManager = new TMWeeklyPCFileManager(new QSettings(QSettings::IniFormat, QSettings::UserScope, "GojiApp", "Goji"));

    // Setup the model for the tracker table
    if (m_dbManager && m_dbManager->isInitialized()) {
        m_trackerModel = new FormattedSqlModel(this, m_dbManager->getDatabase(), this);
        m_trackerModel->setTable("tm_weekly_log");
        m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
        // CRITICAL FIX: Set sort order immediately to show newest entries at top
        m_trackerModel->setSort(0, Qt::DescendingOrder);
        m_trackerModel->select();
    } else {
        Logger::instance().warning("Cannot setup tracker model - database not available");
        m_trackerModel = nullptr;
    }

    // Create base directories if they don't exist
    createBaseDirectories();

    Logger::instance().info("TMWeeklyPCController initialization complete");
}

TMWeeklyPCController::~TMWeeklyPCController()
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

    Logger::instance().info("TMWeeklyPCController destroyed");
}

// (Unifies column widths to match TMTERM; reduces POSTAGE width if needed)
void TMWeeklyPCController::setupOptimizedTableLayout()
{
    if (!m_tracker) return;

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
        {"DESCRIPTION", "TM DEC TERM", 140},
        {"POSTAGE", "$888,888.88", 29},
        {"COUNT", "88,888", 45},
        {"AVG RATE", "0.888", 45},
        {"CLASS", "STD", 60},               // Header "CLASS" will dictate width
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
    m_tracker->setFont(tableFont);

    // CRITICAL FIX: Always sort by ID in descending order to show newest entries at top
    // This setting should persist across all operations and not be affected by application state
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

void TMWeeklyPCController::initializeUI(
    QPushButton* runInitialBtn, QPushButton* openBulkMailerBtn,
    QPushButton* runProofDataBtn, QPushButton* openProofFileBtn,
    QPushButton* runWeeklyMergedBtn, QPushButton* openPrintFileBtn,
    QPushButton* runPostPrintBtn, QToolButton* lockBtn, QToolButton* editBtn,
    QToolButton* postageLockBtn, QComboBox* proofDDbox, QComboBox* printDDbox,
    QComboBox* yearDDbox, QComboBox* monthDDbox, QComboBox* weekDDbox,
    QComboBox* classDDbox, QComboBox* permitDDbox, QLineEdit* jobNumberBox,
    QLineEdit* postageBox, QLineEdit* countBox, QTextEdit* terminalWindow,
    QTableView* tracker, QTextBrowser* textBrowser, QCheckBox* proofApprovalCheckBox)
{
    Logger::instance().info("Initializing TM WEEKLY PC UI elements");

    // Store UI element pointers
    m_runInitialBtn = runInitialBtn;
    m_openBulkMailerBtn = openBulkMailerBtn;
    m_runProofDataBtn = runProofDataBtn;
    m_openProofFileBtn = openProofFileBtn;
    m_runWeeklyMergedBtn = runWeeklyMergedBtn;
    m_openPrintFileBtn = openPrintFileBtn;
    m_runPostPrintBtn = runPostPrintBtn;
    m_lockBtn = lockBtn;
    m_editBtn = editBtn;
    m_postageLockBtn = postageLockBtn;
    m_proofDDbox = proofDDbox;
    m_printDDbox = printDDbox;
    m_yearDDbox = yearDDbox;
    m_monthDDbox = monthDDbox;
    m_weekDDbox = weekDDbox;
    m_classDDbox = classDDbox;
    m_permitDDbox = permitDDbox;
    m_jobNumberBox = jobNumberBox;
    m_postageBox = postageBox;
    m_countBox = countBox;
    m_terminalWindow = terminalWindow;
    m_tracker = tracker;
    m_textBrowser = textBrowser;
    m_proofApprovalCheckBox = proofApprovalCheckBox;

    // Setup tracker table with optimized layout
    if (m_tracker) {
        m_tracker->setModel(m_trackerModel);
        m_tracker->setEditTriggers(QAbstractItemView::NoEditTriggers); // Read-only
        
        // CRITICAL FIX: Force the table view to maintain descending sort order
        m_tracker->setSortingEnabled(true);
        m_tracker->sortByColumn(0, Qt::DescendingOrder);

        // Setup optimized table layout
        setupOptimizedTableLayout();

        // Connect contextual menu for copying
        m_tracker->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_tracker, &QTableView::customContextMenuRequested, this,
                &TMWeeklyPCController::showTableContextMenu);
    }

    // Connect UI signals to slots
    connectSignals();

    // Setup initial UI state
    setupInitialUIState();

    // Populate dropdowns
    populateDropdowns();

    // Initialize HTML display with default state
    updateHtmlDisplay();

    Logger::instance().info("TM WEEKLY PC UI initialization complete");
}

void TMWeeklyPCController::connectSignals()
{
    // Connect buttons with null pointer checks
    if (m_runInitialBtn) {
        connect(m_runInitialBtn, &QPushButton::clicked, this, &TMWeeklyPCController::onRunInitialClicked);
    }
    if (m_openBulkMailerBtn) {
        connect(m_openBulkMailerBtn, &QPushButton::clicked, this, &TMWeeklyPCController::onOpenBulkMailerClicked);
    }
    if (m_runProofDataBtn) {
        connect(m_runProofDataBtn, &QPushButton::clicked, this, &TMWeeklyPCController::onRunProofDataClicked);
    }
    if (m_openProofFileBtn) {
        connect(m_openProofFileBtn, &QPushButton::clicked, this, &TMWeeklyPCController::onOpenProofFileClicked);
    }
    if (m_runWeeklyMergedBtn) {
        connect(m_runWeeklyMergedBtn, &QPushButton::clicked, this, &TMWeeklyPCController::onRunWeeklyMergedClicked);
    }
    if (m_openPrintFileBtn) {
        connect(m_openPrintFileBtn, &QPushButton::clicked, this, &TMWeeklyPCController::onOpenPrintFileClicked);
    }
    if (m_runPostPrintBtn) {
        connect(m_runPostPrintBtn, &QPushButton::clicked, this, &TMWeeklyPCController::onRunPostPrintClicked);
    }

    // Connect toggle buttons with null pointer checks
    if (m_lockBtn) {
        connect(m_lockBtn, &QToolButton::clicked, this, &TMWeeklyPCController::onLockButtonClicked);
    }
    if (m_editBtn) {
        connect(m_editBtn, &QToolButton::clicked, this, &TMWeeklyPCController::onEditButtonClicked);
    }
    if (m_postageLockBtn) {
        connect(m_postageLockBtn, &QToolButton::clicked, this, &TMWeeklyPCController::onPostageLockButtonClicked);
    }

    // Connect checkbox with null pointer check
    if (m_proofApprovalCheckBox) {
        connect(m_proofApprovalCheckBox, &QCheckBox::toggled, this, &TMWeeklyPCController::onProofApprovalChanged);
    }

    // Connect dropdowns with null pointer checks
    if (m_yearDDbox) {
        connect(m_yearDDbox, &QComboBox::currentTextChanged, this, &TMWeeklyPCController::onYearChanged);
    }
    if (m_monthDDbox) {
        connect(m_monthDDbox, &QComboBox::currentTextChanged, this, &TMWeeklyPCController::onMonthChanged);
    }
    if (m_weekDDbox) {
        connect(m_weekDDbox, &QComboBox::currentTextChanged, this, &TMWeeklyPCController::onWeekChanged);
    }
    if (m_classDDbox) {
        connect(m_classDDbox, &QComboBox::currentTextChanged, this, &TMWeeklyPCController::onClassChanged);
    }

    // Connect fields for automatic meter postage calculation with null pointer checks
    if (m_countBox) {
        connect(m_countBox, &QLineEdit::textChanged, this, &TMWeeklyPCController::calculateMeterPostage);
    }
    if (m_classDDbox) {
        connect(m_classDDbox, &QComboBox::currentTextChanged, this, &TMWeeklyPCController::calculateMeterPostage);
    }
    if (m_permitDDbox) {
        connect(m_permitDDbox, &QComboBox::currentTextChanged, this, &TMWeeklyPCController::calculateMeterPostage);
    }

    // Connect script runner signals with null pointer check
    if (m_scriptRunner) {
        connect(m_scriptRunner, &ScriptRunner::scriptOutput, this, &TMWeeklyPCController::onScriptOutput);
        connect(m_scriptRunner, &ScriptRunner::scriptFinished, this, &TMWeeklyPCController::onScriptFinished);
    }

    // FIXED: Connect postage fields to auto-save with null pointer checks
    if (m_postageBox) {
        connect(m_postageBox, &QLineEdit::textChanged, this, [this]() {
            if (m_jobDataLocked) {
                savePostageData();
            }
        });
    }

    if (m_countBox) {
        connect(m_countBox, &QLineEdit::textChanged, this, [this]() {
            if (m_jobDataLocked) {
                savePostageData();
            }
        });
    }

    if (m_classDDbox) {
        connect(m_classDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
                this, [this]() {
                    if (m_jobDataLocked) {
                        savePostageData();
                    }
                });
    }

    if (m_permitDDbox) {
        connect(m_permitDDbox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
                this, [this]() {
                    if (m_jobDataLocked) {
                        savePostageData();
                    }
                });
    }
}

void TMWeeklyPCController::setupInitialUIState()
{
    // Initialize dropdown contents
    if (m_proofDDbox) {
        m_proofDDbox->clear();
        m_proofDDbox->addItem("");
        m_proofDDbox->addItem("SORTED");
        m_proofDDbox->addItem("UNSORTED");
    }

    if (m_printDDbox) {
        m_printDDbox->clear();
        m_printDDbox->addItem("");
        m_printDDbox->addItem("SORTED");
        m_printDDbox->addItem("UNSORTED");
    }

    if (m_classDDbox) {
        m_classDDbox->clear();
        m_classDDbox->addItem("");
        m_classDDbox->addItem("STANDARD");
        m_classDDbox->addItem("FIRST CLASS");
    }

    if (m_permitDDbox) {
        m_permitDDbox->clear();
        m_permitDDbox->addItem("");
        m_permitDDbox->addItem("1662");
        m_permitDDbox->addItem("METER");
    }

    // Clear all input fields to start fresh
    if (m_jobNumberBox) m_jobNumberBox->clear();
    if (m_postageBox) m_postageBox->clear();
    if (m_countBox) m_countBox->clear();

    // Set validators for input fields
    if (m_postageBox) {
        QRegularExpressionValidator* validator = new QRegularExpressionValidator(QRegularExpression("[0-9]*\\.?[0-9]*\\$?"), this);
        m_postageBox->setValidator(validator);
        connect(m_postageBox, &QLineEdit::editingFinished, this, &TMWeeklyPCController::formatPostageInput);
    }

    if (m_countBox) {
        connect(m_countBox, &QLineEdit::textChanged, this, &TMWeeklyPCController::formatCountInput);
    }

    // Set all control states based on current job
    updateControlStates();
}

void TMWeeklyPCController::populateDropdowns()
{
    // Populate year dropdown
    if (m_yearDDbox) {
        m_yearDDbox->clear();
        m_yearDDbox->addItem("");

        // Use QDateTime instead of QDate::currentDateTime
        const int currentYear = QDateTime::currentDateTime().date().year();
        m_yearDDbox->addItem(QString::number(currentYear - 1));
        m_yearDDbox->addItem(QString::number(currentYear));
        m_yearDDbox->addItem(QString::number(currentYear + 1));
    }

    // Populate month dropdown
    if (m_monthDDbox) {
        m_monthDDbox->clear();
        m_monthDDbox->addItem("");

        for (int i = 1; i <= 12; i++) {
            m_monthDDbox->addItem(QString("%1").arg(i, 2, 10, QChar('0')));
        }
    }

    // Week dropdown will be populated when month is selected
}

void TMWeeklyPCController::populateWeekDDbox()
{
    if (!m_weekDDbox || !m_monthDDbox || !m_yearDDbox) {
        Logger::instance().error("Cannot populate week dropdown - UI elements not initialized");
        return;
    }

    // Clear the week dropdown
    m_weekDDbox->clear();
    m_weekDDbox->addItem("");

    // Get selected year and month
    QString yearStr = m_yearDDbox->currentText();
    QString monthStr = m_monthDDbox->currentText();

    if (yearStr.isEmpty() || monthStr.isEmpty()) {
        return;
    }

    int year = yearStr.toInt();
    int month = monthStr.toInt();

    // Get all Wednesdays in this month
    QDate date(year, month, 1);
    QList<int> wednesdayDates;

    // Find first Wednesday
    while (date.dayOfWeek() != 3) { // Qt::Wednesday = 3
        date = date.addDays(1);
        if (date.month() != month) {
            // No Wednesdays in this month (should never happen)
            return;
        }
    }

    // Add all Wednesdays in this month
    while (date.month() == month) {
        wednesdayDates.append(date.day());
        date = date.addDays(7);
    }

    // Add the Wednesday dates to the dropdown with two-digit formatting
    for (int day : wednesdayDates) {
        m_weekDDbox->addItem(QString("%1").arg(day, 2, 10, QChar('0')));
    }
}

// FIXED: Enhanced updateHtmlDisplay to handle state transitions properly
void TMWeeklyPCController::updateHtmlDisplay()
{
    if (!m_textBrowser) {
        return;
    }

    HtmlDisplayState newState = determineHtmlState();

    // CRITICAL FIX: Always update HTML when state is Uninitialized or when state changes
    if (m_currentHtmlState == UninitializedState || newState != m_currentHtmlState) {
        m_currentHtmlState = newState;

        QString resourcePath;
        QString stateName;

        switch (m_currentHtmlState) {
        case ProofState:
            resourcePath = ":/resources/tmweeklypc/proof.html";
            stateName = "Proof";
            break;
        case PrintState:
            resourcePath = ":/resources/tmweeklypc/print.html";
            stateName = "Print";
            break;
        case DefaultState:
        default:
            resourcePath = ":/resources/tmweeklypc/default.html";
            stateName = "Default";
            break;
        }

        loadHtmlFile(resourcePath);
        Logger::instance().info(QString("TMWEEKLYPC HTML state changed to: %1 (%2)")
                                    .arg(m_currentHtmlState).arg(stateName));
        outputToTerminal(QString("HTML display updated to: %1").arg(stateName), Info);

        // Save state when HTML changes (if job is locked)
        if (m_jobDataLocked) {
            saveJobState();
        }
    }
}

void TMWeeklyPCController::loadHtmlFile(const QString& resourcePath)
{
    if (!m_textBrowser) {
        return;
    }

    QFile file(resourcePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        QString content = stream.readAll();
        file.close();

        m_textBrowser->setHtml(content);
    } else {
        // Create fallback HTML content
        QString fallbackContent = QString(
                                      "<html><body style='font-family: Arial; padding: 20px;'>"
                                      "<h2>TM Weekly PC</h2>"
                                      "<p>HTML file could not be loaded: %1</p>"
                                      "<p>Current time: %2</p>"
                                      "</body></html>"
                                      ).arg(resourcePath, QDateTime::currentDateTime().toString());

        m_textBrowser->setHtml(fallbackContent);
    }
}

TMWeeklyPCController::HtmlDisplayState TMWeeklyPCController::determineHtmlState() const
{
    // If proof approval checkbox is checked, show print state
    if (m_proofApprovalCheckBox && m_proofApprovalCheckBox->isChecked()) {
        return PrintState;
    }

    // If job is locked (created), show proof state
    if (m_jobDataLocked) {
        return ProofState;
    }

    // Default state
    return DefaultState;
}

// FIXED: Enhanced saveJobState to include postage data and lock states
void TMWeeklyPCController::saveJobState()
{
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString week = m_weekDDbox ? m_weekDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty() || week.isEmpty()) {
        return;
    }

    bool proofApprovalChecked = m_proofApprovalCheckBox ? m_proofApprovalCheckBox->isChecked() : false;
    int htmlDisplayState = static_cast<int>(m_currentHtmlState);
    
    // Get postage data
    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";
    QString mailClass = m_classDDbox ? m_classDDbox->currentText() : "";
    QString permit = m_permitDDbox ? m_permitDDbox->currentText() : "";

    if (m_tmWeeklyPCDBManager->saveJobState(year, month, week, proofApprovalChecked, htmlDisplayState,
                                            m_jobDataLocked, m_postageDataLocked,
                                            postage, count, mailClass, permit)) {
        outputToTerminal(QString("Job state saved - HTML: %1, Job locked: %2, Postage locked: %3")
                             .arg(htmlDisplayState).arg(m_jobDataLocked ? "Yes" : "No")
                             .arg(m_postageDataLocked ? "Yes" : "No"), Info);
    } else {
        outputToTerminal("Failed to save job state", Warning);
    }
}

// FIXED: Enhanced loadJobState to restore postage data and lock states
void TMWeeklyPCController::loadJobState()
{
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString week = m_weekDDbox ? m_weekDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty() || week.isEmpty()) {
        return;
    }

    bool proofApprovalChecked, jobDataLocked, postageDataLocked;
    int htmlDisplayState;
    QString postage, count, mailClass, permit;

    if (m_tmWeeklyPCDBManager->loadJobState(year, month, week, proofApprovalChecked, htmlDisplayState,
                                            jobDataLocked, postageDataLocked,
                                            postage, count, mailClass, permit)) {
        // Restore proof approval checkbox
        if (m_proofApprovalCheckBox) {
            m_proofApprovalCheckBox->setChecked(proofApprovalChecked);
        }

        // Restore lock states
        m_jobDataLocked = jobDataLocked;
        m_postageDataLocked = postageDataLocked;
        
        // CRITICAL DEBUG: Log exactly what postage data was loaded from job state
        outputToTerminal(QString("DEBUG JobState: About to populate widgets with - Postage: '%1', Count: '%2', Class: '%3', Permit: '%4'")
                           .arg(postage, count, mailClass, permit), Info);
        
        // Restore postage data to UI
        if (m_postageBox) {
            m_postageBox->setText(postage);
            outputToTerminal(QString("DEBUG JobState: Set postageBox to: '%1'").arg(postage), Info);
        }
        if (m_countBox) {
            m_countBox->setText(count);
            outputToTerminal(QString("DEBUG JobState: Set countBox to: '%1'").arg(count), Info);
        }
        if (m_classDDbox) {
            m_classDDbox->setCurrentText(mailClass);
            outputToTerminal(QString("DEBUG JobState: Set classDDbox to: '%1'").arg(mailClass), Info);
        }
        if (m_permitDDbox) {
            m_permitDDbox->setCurrentText(permit);
            outputToTerminal(QString("DEBUG JobState: Set permitDDbox to: '%1'").arg(permit), Info);
        }

        // Restore HTML display state
        m_currentHtmlState = static_cast<HtmlDisplayState>(htmlDisplayState);

        outputToTerminal(QString("Job state loaded - HTML: %1, Job locked: %2, Postage locked: %3")
                             .arg(htmlDisplayState).arg(m_jobDataLocked ? "Yes" : "No")
                             .arg(m_postageDataLocked ? "Yes" : "No"), Info);
    } else {
        // No saved state found, set defaults
        m_jobDataLocked = false;
        m_postageDataLocked = false;
        m_currentHtmlState = UninitializedState;
        outputToTerminal("No saved job state found, using defaults", Info);
    }
}

void TMWeeklyPCController::onYearChanged(const QString& year)
{
    outputToTerminal("Year changed to: " + year, Info);
    
    // CRITICAL FIX: Auto-save and close current job when opening a different one
    autoSaveAndCloseCurrentJob();
    
    // Load job state when year changes
    loadJobState();
    
    // CRITICAL FIX: Also load postage data to ensure all four postage widgets are populated
    loadPostageData();
    
    updateControlStates();
    updateHtmlDisplay();
}

void TMWeeklyPCController::onMonthChanged(const QString& month)
{
    outputToTerminal("Month changed to: " + month, Info);

    // CRITICAL FIX: Auto-save and close current job when opening a different one
    autoSaveAndCloseCurrentJob();

    // Update week dropdown with Wednesdays
    populateWeekDDbox();
    
    // Load job state when month changes
    loadJobState();
    
    // CRITICAL FIX: Also load postage data to ensure all four postage widgets are populated
    loadPostageData();
    
    updateControlStates();
    updateHtmlDisplay();
}

void TMWeeklyPCController::onWeekChanged(const QString& week)
{
    outputToTerminal("Week changed to: " + week, Info);
    
    // CRITICAL FIX: Auto-save and close current job when opening a different one
    autoSaveAndCloseCurrentJob();
    
    // Load job state when week changes
    loadJobState();
    
    // CRITICAL FIX: Also load postage data to ensure all four postage widgets are populated
    loadPostageData();
    
    updateControlStates();
    updateHtmlDisplay();
}

void TMWeeklyPCController::onClassChanged(const QString& mailClass)
{
    // Auto-select permit 1662 if STANDARD is selected
    if (mailClass == "STANDARD" && m_permitDDbox) {
        m_permitDDbox->setCurrentText("1662");
    }
}

// FIXED: Enhanced onProofApprovalChanged to properly handle HTML state
void TMWeeklyPCController::onProofApprovalChanged(bool checked)
{
    outputToTerminal(checked ? "Proof approval checked" : "Proof approval unchecked", Info);

    // CRITICAL FIX: Force HTML state update by resetting current state
    HtmlDisplayState oldState = m_currentHtmlState;
    m_currentHtmlState = UninitializedState; // Force state change
    updateHtmlDisplay(); // This will determine new state and load appropriate HTML

    outputToTerminal(QString("HTML state changed from %1 to %2 due to proof approval change")
                         .arg(oldState).arg(m_currentHtmlState), Info);

    // Save job state when checkbox changes (if job is locked)
    if (m_jobDataLocked) {
        saveJobState();
    }
}

// FIXED: Enhanced lock button handler to properly set HTML state and copy files
void TMWeeklyPCController::onLockButtonClicked()
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

        // CRITICAL FIX: Copy files from HOME folder to JOB folder when locking new job
        outputToTerminal("Copying files from HOME to JOB folder...", Info);
        if (copyFilesFromHomeFolder()) {
            outputToTerminal("Files copied successfully from HOME to JOB folder", Success);
        } else {
            outputToTerminal("No existing files to copy (normal for new jobs)", Info);
        }

        // Save to database
        saveJobToDatabase();

        // CRITICAL FIX: Force HTML state update after locking
        m_currentHtmlState = UninitializedState;
        updateHtmlDisplay(); // This will show proof.html since job is now locked

        // Update control states
        updateControlStates();

        // Start auto-save timer since job is now locked/open
        emit jobOpened();
        outputToTerminal("Auto-save timer started (15 minutes)", Info);
    } else {
        // User unchecked lock button - this shouldn't happen in normal flow
        m_lockBtn->setChecked(true); // Force it back to checked
    }
}

void TMWeeklyPCController::onEditButtonClicked()
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

void TMWeeklyPCController::onPostageLockButtonClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Cannot lock postage data until job data is locked.", Error);
        m_postageLockBtn->setChecked(false);
        return;
    }

    if (m_postageLockBtn->isChecked()) {
        // Validate postage data before locking
        if (!validatePostageData()) {
            m_postageLockBtn->setChecked(false);
            return;
        }

        m_postageDataLocked = true;
        outputToTerminal("Postage data locked.", Success);

        // Add log entry to tracker when postage is locked (like TMTermController does)
        addLogEntry();

        // FIXED: Save postage data persistently when locked
        savePostageData();
    } else {
        m_postageDataLocked = false;
        outputToTerminal("Postage data unlocked.", Info);

        // Save the unlocked state to database
        savePostageData();
    }

    // Save job state whenever postage lock button is clicked (includes lock state)
    saveJobState();
    updateControlStates();
}

void TMWeeklyPCController::savePostageData()
{
    if (!m_jobDataLocked) {
        return; // Only save for locked jobs
    }

    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString week = m_weekDDbox ? m_weekDDbox->currentText() : "";
    QString postage = m_postageBox ? m_postageBox->text() : "";
    QString count = m_countBox ? m_countBox->text() : "";
    QString mailClass = m_classDDbox ? m_classDDbox->currentText() : "";
    QString permit = m_permitDDbox ? m_permitDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty() || week.isEmpty()) {
        return;
    }

    if (m_tmWeeklyPCDBManager->savePostageData(year, month, week, postage, count,
                                               mailClass, permit, m_postageDataLocked)) {
        outputToTerminal("Postage data saved persistently", Info);
    } else {
        outputToTerminal("Failed to save postage data", Warning);
    }
}

void TMWeeklyPCController::loadPostageData(const QString& year, const QString& month, const QString& week)
{
    // Use parameters if provided, otherwise fall back to reading from UI
    QString actualYear = year.isEmpty() ? (m_yearDDbox ? m_yearDDbox->currentText() : "") : year;
    QString actualMonth = month.isEmpty() ? (m_monthDDbox ? m_monthDDbox->currentText() : "") : month;
    QString actualWeek = week.isEmpty() ? (m_weekDDbox ? m_weekDDbox->currentText() : "") : week;

    if (actualYear.isEmpty() || actualMonth.isEmpty() || actualWeek.isEmpty()) {
        return;
    }

    QString postage, count, mailClass, permit;
    bool postageDataLocked;

    if (m_tmWeeklyPCDBManager->loadPostageData(actualYear, actualMonth, actualWeek, postage, count,
                                               mailClass, permit, postageDataLocked)) {
        // CRITICAL DEBUG: Log exactly what data was loaded before setting widgets
        outputToTerminal(QString("DEBUG: About to populate widgets with - Postage: '%1', Count: '%2', Class: '%3', Permit: '%4'")
                           .arg(postage, count, mailClass, permit), Info);
        
        // Load the data into UI fields
        if (m_postageBox) {
            m_postageBox->setText(postage);
            outputToTerminal(QString("DEBUG: Set postageBox to: '%1'").arg(postage), Info);
        } else {
            outputToTerminal("DEBUG: postageBox is NULL!", Error);
        }
        
        if (m_countBox) {
            m_countBox->setText(count);
            outputToTerminal(QString("DEBUG: Set countBox to: '%1'").arg(count), Info);
        } else {
            outputToTerminal("DEBUG: countBox is NULL!", Error);
        }
        
        if (m_classDDbox) {
            m_classDDbox->setCurrentText(mailClass);
            outputToTerminal(QString("DEBUG: Set classDDbox to: '%1'").arg(mailClass), Info);
        } else {
            outputToTerminal("DEBUG: classDDbox is NULL!", Error);
        }
        
        if (m_permitDDbox) {
            m_permitDDbox->setCurrentText(permit);
            outputToTerminal(QString("DEBUG: Set permitDDbox to: '%1'").arg(permit), Info);
        } else {
            outputToTerminal("DEBUG: permitDDbox is NULL!", Error);
        }

        // Restore lock state
        m_postageDataLocked = postageDataLocked;
        if (m_postageLockBtn) m_postageLockBtn->setChecked(postageDataLocked);

        outputToTerminal("Postage data loaded from database", Info);
        
        // CRITICAL DEBUG: Verify what the widgets actually contain after setting
        outputToTerminal(QString("DEBUG: After setting - postageBox: '%1', countBox: '%2', classDDbox: '%3', permitDDbox: '%4'")
                           .arg(m_postageBox ? m_postageBox->text() : "NULL",
                                m_countBox ? m_countBox->text() : "NULL",
                                m_classDDbox ? m_classDDbox->currentText() : "NULL",
                                m_permitDDbox ? m_permitDDbox->currentText() : "NULL"), Info);
    } else {
        // FALLBACK: Try to load postage data from log table if postage table lookup failed
        outputToTerminal("Primary postage data not found, trying fallback from log table...", Warning);
        
        QString fallbackPostage, fallbackCount, fallbackMailClass, fallbackPermit;
        if (m_tmWeeklyPCDBManager->loadPostageDataFromLog(actualYear, actualMonth, actualWeek,
                                                          fallbackPostage, fallbackCount,
                                                          fallbackMailClass, fallbackPermit)) {
            // CRITICAL DEBUG: Log what fallback data was found
            outputToTerminal(QString("DEBUG FALLBACK: Found data - Postage: '%1', Count: '%2', Class: '%3', Permit: '%4'")
                               .arg(fallbackPostage, fallbackCount, fallbackMailClass, fallbackPermit), Info);
            
            // Load the fallback data into UI fields
            if (m_postageBox) {
                m_postageBox->setText(fallbackPostage);
                outputToTerminal(QString("DEBUG FALLBACK: Set postageBox to: '%1'").arg(fallbackPostage), Info);
            }
            
            if (m_countBox) {
                m_countBox->setText(fallbackCount);
                outputToTerminal(QString("DEBUG FALLBACK: Set countBox to: '%1'").arg(fallbackCount), Info);
            }
            
            if (m_classDDbox) {
                m_classDDbox->setCurrentText(fallbackMailClass);
                outputToTerminal(QString("DEBUG FALLBACK: Set classDDbox to: '%1'").arg(fallbackMailClass), Info);
            }
            
            if (m_permitDDbox) {
                m_permitDDbox->setCurrentText(fallbackPermit);
                outputToTerminal(QString("DEBUG FALLBACK: Set permitDDbox to: '%1'").arg(fallbackPermit), Info);
            }
            
            outputToTerminal("Postage data loaded from log table (fallback method)", Success);
            
            // CRITICAL DEBUG: Verify what the widgets actually contain after fallback
            outputToTerminal(QString("DEBUG FALLBACK: After setting - postageBox: '%1', countBox: '%2', classDDbox: '%3', permitDDbox: '%4'")
                               .arg(m_postageBox ? m_postageBox->text() : "NULL",
                                    m_countBox ? m_countBox->text() : "NULL",
                                    m_classDDbox ? m_classDDbox->currentText() : "NULL",
                                    m_permitDDbox ? m_permitDDbox->currentText() : "NULL"), Info);
        } else {
            outputToTerminal("No postage data found in either postage table or log table", Warning);
        }
    }
}

void TMWeeklyPCController::onScriptStarted()
{
    outputToTerminal("Script execution started...", Info);
}

void TMWeeklyPCController::onScriptOutput(const QString& output)
{
    // Parse output for special markers
    parseScriptOutput(output);

    // Display output in terminal
    outputToTerminal(output, Info);
}

void TMWeeklyPCController::onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // Re-enable all buttons
    m_runInitialBtn->setEnabled(true);
    m_runProofDataBtn->setEnabled(true);
    m_runWeeklyMergedBtn->setEnabled(true);
    m_runPostPrintBtn->setEnabled(true);

    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        outputToTerminal("Script execution completed successfully.", Success);

        // FIXED: Only show NAS dialog for Post Print script
        // Do NOT add tracker entry here - that's handled in onPostageLockButtonClicked
        if (m_lastExecutedScript == "postprint" && !m_capturedNASPath.isEmpty()) {
            // Show NAS link dialog after a brief delay to let the terminal update
            QTimer::singleShot(500, this, &TMWeeklyPCController::showNASLinkDialog);
        }
    } else {
        outputToTerminal("Script execution failed with exit code: " + QString::number(exitCode), Error);
        // Clear captured path on failure
        m_capturedNASPath.clear();
    }

    // Reset script tracking
    m_lastExecutedScript.clear();
}

void TMWeeklyPCController::onRunInitialClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Please lock job data before running Initial script.", Warning);
        return;
    }

    outputToTerminal("Running Initial script...", Info);

    // Disable the button while running
    m_runInitialBtn->setEnabled(false);

    // Get script path from file manager
    QString script = m_fileManager->getScriptPath("initial");

    // Run the script
    m_scriptRunner->runScript("python", QStringList() << script);

    // Manually call scriptStarted since we removed the signal
    onScriptStarted();
}

void TMWeeklyPCController::onOpenBulkMailerClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Please lock job data before opening Bulk Mailer.", Warning);
        return;
    }

    outputToTerminal("Opening Bulk Mailer application...", Info);

    // Launch Bulk Mailer
    QProcess* process = new QProcess(this);
    process->startDetached("C:/Program Files (x86)/Satori Software/Bulk Mailer/BulkMailer.exe", QStringList());

    // Connect to handle process finish
    connect(process, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            process, &QProcess::deleteLater);
}

void TMWeeklyPCController::onRunProofDataClicked()
{
    if (!m_jobDataLocked || !m_postageDataLocked) {
        outputToTerminal("Please lock job data and postage data before running Proof Data script.", Warning);
        return;
    }

    outputToTerminal("Running Proof Data script...", Info);

    // Disable the button while running
    m_runProofDataBtn->setEnabled(false);

    // Get script path from file manager
    QString script = m_fileManager->getScriptPath("proofdata");

    // Run the script
    m_scriptRunner->runScript("python", QStringList() << script);

    // Manually call scriptStarted since we removed the signal
    onScriptStarted();
}

void TMWeeklyPCController::onOpenProofFileClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Please lock job data before opening proof file.", Warning);
        return;
    }

    QString selection = m_proofDDbox->currentText();
    if (selection.isEmpty()) {
        outputToTerminal("Please select SORTED or UNSORTED from the dropdown.", Warning);
        return;
    }

    outputToTerminal("Opening " + selection + " proof file...", Info);

    // Use file manager to open the appropriate file
    if (m_fileManager && m_fileManager->openProofFile(selection)) {
        outputToTerminal("Opened " + selection + " proof file successfully.", Success);
    } else {
        outputToTerminal("Failed to open " + selection + " proof file.", Error);
    }
}

void TMWeeklyPCController::onRunWeeklyMergedClicked()
{
    if (!m_jobDataLocked || !m_postageDataLocked) {
        outputToTerminal("Please lock job data and postage data before running Weekly Merged script.", Warning);
        return;
    }

    outputToTerminal("Running Weekly Merged script...", Info);

    // Disable the button while running
    m_runWeeklyMergedBtn->setEnabled(false);

    // Get the required parameters from the UI
    QString jobNumber = m_jobNumberBox->text();
    QString month = m_monthDDbox->currentText();
    QString week = m_weekDDbox->currentText();

    // Validate that we have all required parameters
    if (jobNumber.isEmpty() || month.isEmpty() || week.isEmpty()) {
        outputToTerminal("Error: Missing required job data (job number, month, or week)", Error);
        m_runWeeklyMergedBtn->setEnabled(true);  // Re-enable button
        return;
    }

    outputToTerminal(QString("Running Weekly Merged script for job %1, week %2.%3...")
                         .arg(jobNumber, month, week), Info);

    // Get script path from file manager
    QString scriptPath = m_fileManager->getScriptPath("weeklymerged");

    // Prepare arguments for the script
    QStringList arguments;
    arguments << jobNumber << month << week;

    // Run the script with the required parameters
    m_scriptRunner->runScript(scriptPath, arguments);

    // Manually call scriptStarted since we removed the signal
    onScriptStarted();
}

void TMWeeklyPCController::onOpenPrintFileClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Please lock job data before opening print file.", Warning);
        return;
    }

    QString selection = m_printDDbox->currentText();
    if (selection.isEmpty()) {
        outputToTerminal("Please select SORTED or UNSORTED from the dropdown.", Warning);
        return;
    }

    outputToTerminal("Opening " + selection + " print file...", Info);

    // Use file manager to open the appropriate file
    if (m_fileManager && m_fileManager->openPrintFile(selection)) {
        outputToTerminal("Opened " + selection + " print file successfully.", Success);
    } else {
        outputToTerminal("Failed to open " + selection + " print file.", Error);
    }
}

void TMWeeklyPCController::onRunPostPrintClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal("Please lock job data before running Post Print script.", Warning);
        return;
    }

    if (!m_runPostPrintBtn->isEnabled()) {
        outputToTerminal("Post Print script is already running.", Warning);
        return;
    }

    // Disable button during execution
    m_runPostPrintBtn->setEnabled(false);

    QString jobNumber = m_jobNumberBox->text();
    QString month = m_monthDDbox->currentText();
    QString week = m_weekDDbox->currentText();
    QString year = m_yearDDbox->currentText();

    // Clear any previously captured NAS path
    m_capturedNASPath.clear();
    m_capturingNASPath = false;
    m_lastExecutedScript = "postprint";

    outputToTerminal(QString("Running Post Print script for job %1, week %2.%3, year %4...")
                         .arg(jobNumber, month, week, year), Info);

    // FIXED: Do NOT add log entry here - post print only copies files to network
    // Tracker should only be updated when runWeeklyMergedTMWPC is clicked

    // Run the script
    QString scriptPath = m_fileManager->getScriptPath("postprint");
    QStringList arguments;
    arguments << jobNumber << month << week << year;

    m_scriptRunner->runScript(scriptPath, arguments);
}

bool TMWeeklyPCController::validateJobData()
{
    // Check if required fields are filled
    if (m_jobNumberBox->text().isEmpty()) {
        QMessageBox::warning(nullptr, "Validation Error", "Job number cannot be empty.");
        return false;
    }

    if (m_yearDDbox->currentText().isEmpty()) {
        QMessageBox::warning(nullptr, "Validation Error", "Please select a year.");
        return false;
    }

    if (m_monthDDbox->currentText().isEmpty()) {
        QMessageBox::warning(nullptr, "Validation Error", "Please select a month.");
        return false;
    }

    if (m_weekDDbox->currentText().isEmpty()) {
        QMessageBox::warning(nullptr, "Validation Error", "Please select a week.");
        return false;
    }

    // Validate job number (5 digits)
    QString jobNumber = m_jobNumberBox->text();
    static const QRegularExpression jobNumberRegex("^\\d{5}$");
    if (!jobNumberRegex.match(jobNumber).hasMatch()) {
        QMessageBox::warning(nullptr, "Validation Error", "Job number must be exactly 5 digits.");
        return false;
    }

    return true;
}

bool TMWeeklyPCController::validatePostageData()
{
    // Check if required fields are filled
    if (m_postageBox->text().isEmpty()) {
        QMessageBox::warning(nullptr, "Validation Error", "Postage amount cannot be empty.");
        return false;
    }

    if (m_countBox->text().isEmpty()) {
        QMessageBox::warning(nullptr, "Validation Error", "Count cannot be empty.");
        return false;
    }

    if (m_classDDbox->currentText().isEmpty()) {
        QMessageBox::warning(nullptr, "Validation Error", "Please select a mail class.");
        return false;
    }

    if (m_permitDDbox->currentText().isEmpty()) {
        QMessageBox::warning(nullptr, "Validation Error", "Please select a permit number.");
        return false;
    }

    // Validate postage (numeric value) - remove dollar sign first
    QString postage = m_postageBox->text();
    postage.remove("$");  // Remove the dollar sign before validation
    bool ok;
    postage.toDouble(&ok);
    if (!ok) {
        QMessageBox::warning(nullptr, "Validation Error", "Postage must be a valid number.");
        return false;
    }

    // Validate count (numeric value)
    QString count = m_countBox->text();
    count.toInt(&ok);
    if (!ok) {
        QMessageBox::warning(nullptr, "Validation Error", "Count must be a valid integer.");
        return false;
    }

    return true;
}

void TMWeeklyPCController::formatPostageInput()
{
    if (!m_postageBox) return;
    
    QString text = m_postageBox->text().trimmed();
    if (text.isEmpty()) return;

    // Remove any non-numeric characters except decimal point
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

    // Format with dollar sign and thousand separators if there's content
    QString formatted;
    if (!cleanText.isEmpty() && cleanText != ".") {
        // Parse the number to add thousand separators
        bool ok;
        double value = cleanText.toDouble(&ok);
        if (ok) {
            // Format with thousand separators and 2 decimal places
            formatted = QString("$%L1").arg(value, 0, 'f', 2);
        } else {
            // Fallback if parsing fails
            formatted = "$" + cleanText;
        }
    }

    // Prevent infinite loop by checking if update is needed
    if (m_postageBox->text() != formatted) {
        m_postageBox->blockSignals(true);
        m_postageBox->setText(formatted);
        m_postageBox->blockSignals(false);
    }
}

void TMWeeklyPCController::formatCountInput(const QString& text)
{
    if (!m_countBox) return;

    QString cleanText = text;
    static const QRegularExpression nonDigitRegex("[^0-9]");
    cleanText.remove(nonDigitRegex);

    QString formatted;
    if (!cleanText.isEmpty()) {
        bool ok;
        qlonglong number = cleanText.toLongLong(&ok);
        if (ok) {
            formatted = QString("%L1").arg(number);
        } else {
            formatted = cleanText;
        }
    }

    if (m_countBox->text() != formatted) {
        QSignalBlocker blocker(m_countBox);
        m_countBox->setText(formatted);
    }
}

void TMWeeklyPCController::updateControlStates()
{
    // Job data fields - enabled when not locked
    bool jobFieldsEnabled = !m_jobDataLocked;
    if (m_jobNumberBox) m_jobNumberBox->setEnabled(jobFieldsEnabled);
    if (m_yearDDbox) m_yearDDbox->setEnabled(jobFieldsEnabled);
    if (m_monthDDbox) m_monthDDbox->setEnabled(jobFieldsEnabled);
    if (m_weekDDbox) m_weekDDbox->setEnabled(jobFieldsEnabled);

    // Postage data fields - enabled when postage not locked
    if (m_postageBox) m_postageBox->setEnabled(!m_postageDataLocked);
    if (m_countBox) m_countBox->setEnabled(!m_postageDataLocked);
    if (m_classDDbox) m_classDDbox->setEnabled(!m_postageDataLocked);
    if (m_permitDDbox) m_permitDDbox->setEnabled(!m_postageDataLocked);

    // Lock button states
    if (m_lockBtn) m_lockBtn->setChecked(m_jobDataLocked);
    if (m_postageLockBtn) m_postageLockBtn->setChecked(m_postageDataLocked);

    // Edit button only enabled when job data is locked
    if (m_editBtn) m_editBtn->setEnabled(m_jobDataLocked);

    // Postage lock can only be engaged if job data is locked
    if (m_postageLockBtn) m_postageLockBtn->setEnabled(m_jobDataLocked);

    // Other buttons enabled based on job state
    if (m_runInitialBtn) m_runInitialBtn->setEnabled(m_jobDataLocked);
    if (m_runProofDataBtn) m_runProofDataBtn->setEnabled(m_jobDataLocked);
    if (m_runWeeklyMergedBtn) m_runWeeklyMergedBtn->setEnabled(m_jobDataLocked);
    if (m_runPostPrintBtn) m_runPostPrintBtn->setEnabled(m_jobDataLocked);
    if (m_openProofFileBtn) m_openProofFileBtn->setEnabled(m_jobDataLocked);
    if (m_openPrintFileBtn) m_openPrintFileBtn->setEnabled(m_jobDataLocked);
}

// BaseTrackerController implementation methods
void TMWeeklyPCController::outputToTerminal(const QString& message, MessageType type)
{
    if (m_terminalWindow) {
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

    // Also log to the logger
    Logger::instance().info(message);
}

QTableView* TMWeeklyPCController::getTrackerWidget() const
{
    return m_tracker;
}

QSqlTableModel* TMWeeklyPCController::getTrackerModel() const
{
    return m_trackerModel;
}

QStringList TMWeeklyPCController::getTrackerHeaders() const
{
    return {"JOB", "DESCRIPTION", "POSTAGE", "COUNT", "AVG RATE", "CLASS", "SHAPE", "PERMIT"};
}

QList<int> TMWeeklyPCController::getVisibleColumns() const
{
    return {1, 2, 3, 4, 5, 6, 7, 8}; // Skip column 0 (ID)
}

// (Fixes incorrect column indices for formatting; POSTAGE is model column 3, COUNT is 4)
QString TMWeeklyPCController::formatCellData(int columnIndex, const QString& cellData) const
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

void TMWeeklyPCController::createBaseDirectories()
{
    if (m_fileManager) {
        m_fileManager->createBaseDirectories();
    }
}

void TMWeeklyPCController::createJobFolder()
{
    QString month = m_monthDDbox->currentText();
    QString week = m_weekDDbox->currentText();

    if (month.isEmpty() || week.isEmpty()) {
        outputToTerminal("Cannot create job folder: month or week is empty", Warning);
        return;
    }

    if (m_fileManager && m_fileManager->createJobFolder(month, week)) {
        outputToTerminal("Created job folder: " + m_fileManager->getJobFolderPath(month, week), Success);
    } else {
        outputToTerminal("Failed to create job folder", Error);
    }
}

void TMWeeklyPCController::saveJobToDatabase()
{
    QString jobNumber = m_jobNumberBox->text();
    QString year = m_yearDDbox->currentText();
    QString month = m_monthDDbox->currentText();
    QString week = m_weekDDbox->currentText();

    if (m_tmWeeklyPCDBManager->saveJob(jobNumber, year, month, week)) {
        outputToTerminal("Job saved to database successfully", Success);
    } else {
        outputToTerminal("Failed to save job to database", Error);
    }
}

// FIXED: Enhanced loadJob to restore complete job state AND copy files
bool TMWeeklyPCController::loadJob(const QString& year, const QString& month, const QString& week)
{
    if (!m_tmWeeklyPCDBManager) {
        outputToTerminal("Database manager not available", Error);
        return false;
    }

    QString jobNumber;
    if (m_tmWeeklyPCDBManager->loadJob(year, month, week, jobNumber)) {
        outputToTerminal(QString("Loading job: %1 for %2-%3-%4").arg(jobNumber, year, month, week), Info);

        // CRITICAL DEBUG: Show what's actually in the database for this year/month
        m_tmWeeklyPCDBManager->debugDatabaseContents(year, month);

        // CRITICAL FIX: Block ALL dropdown signals to prevent cascade of events that
        // would cause loadJobState() to be called prematurely and clear widgets
        {
            QSignalBlocker yearBlocker(m_yearDDbox);
            QSignalBlocker monthBlocker(m_monthDDbox);
            QSignalBlocker weekBlocker(m_weekDDbox);

            // Set year and month first
            if (m_yearDDbox) m_yearDDbox->setCurrentText(year);
            if (m_monthDDbox) m_monthDDbox->setCurrentText(month);

            // CRITICAL FIX: Manually populate week dropdown since month signal is blocked
            populateWeekDDbox();

            // Now set the week after dropdown is populated
            if (m_weekDDbox) m_weekDDbox->setCurrentText(week);

            // Set the job number
            if (m_jobNumberBox) m_jobNumberBox->setText(jobNumber);
        } // Signal blockers automatically released here

        // CRITICAL FIX: Load complete job state INCLUDING postage data and lock states
        // Now that all dropdowns are properly set, load the job state
        loadJobState();
        
        // CRITICAL FIX: Also load postage data separately to ensure all four postage widgets are populated
        // This is needed because postage data is stored in a separate table from job state
        loadPostageData(year, month, week);

        // If job wasn't locked when saved, set as locked since it exists in database
        if (!m_jobDataLocked) {
            m_jobDataLocked = true;
        }
        if (m_lockBtn) m_lockBtn->setChecked(m_jobDataLocked);

        // CRITICAL FIX: Copy files from home folder to JOB folder when opening job
        outputToTerminal("Copying files from HOME to JOB folder...", Info);
        if (copyFilesFromHomeFolder()) {
            outputToTerminal("Files copied successfully from HOME to JOB folder", Success);
        } else {
            outputToTerminal("Some files may not have been copied (this is normal for new jobs)", Warning);
        }

        // Update control states AFTER loading job state
        updateControlStates();

        // CRITICAL FIX: Force HTML display update after loading complete state
        // Set to uninitialized first to force refresh
        m_currentHtmlState = UninitializedState;
        updateHtmlDisplay(); // This will restore the correct HTML state

        // Start auto-save timer since job is locked/open
        emit jobOpened();
        outputToTerminal("Auto-save timer started (15 minutes)", Info);

        outputToTerminal(QString("Successfully loaded TM Weekly PC job for %1-%2-%3").arg(year, month, week), Success);
        return true;
    } else {
        outputToTerminal(QString("No job found for %1/%2/%3").arg(year, month, week), Warning);
        return false;
    }
}

void TMWeeklyPCController::addLogEntry()
{
    // Get values from UI
    QString jobNumber = m_jobNumberBox->text();
    QString description = "TM WEEKLY " + m_monthDDbox->currentText() + "." + m_weekDDbox->currentText();
    QString postage = m_postageBox->text();
    QString count = m_countBox->text();
    QString mailClass = m_classDDbox->currentText();
    QString permit = m_permitDDbox->currentText();

    // Format count with comma if >= 1000
    int countValue = count.toInt();
    QString formattedCount = (countValue >= 1000) ?
                                 QString("%L1").arg(countValue) : QString::number(countValue);

    // Calculate per piece rate with exactly 3 decimal places and leading zero
    double postageAmount = postage.remove("$").toDouble();
    double perPiece = (countValue > 0) ? (postageAmount / countValue) : 0.0;
    QString perPieceStr = QString("0.%1").arg(QString::number(perPiece * 1000, 'f', 0).rightJustified(3, '0'));

    // Convert CLASS to abbreviated form
    QString classAbbrev = (mailClass == "STANDARD") ? "STD" :
                              (mailClass == "FIRST CLASS") ? "FC" : mailClass;

    // Convert PERMIT to shortened form
    QString permitShort = (permit == "METER") ? "METER" : permit;  // Already correct

    // Static shape value
    QString shape = "LTR";

    // Get current date
    QDateTime now = QDateTime::currentDateTime();
    QString date = now.toString("M/d/yyyy");

    // Add to database using the tab-specific manager
    if (m_tmWeeklyPCDBManager->addLogEntry(jobNumber, description, postage, formattedCount,
                                           perPieceStr, classAbbrev, shape, permitShort, date)) {
        outputToTerminal("Added log entry to database", Success);

        // Force refresh the table view
        refreshTrackerTable();
    } else {
        outputToTerminal("Failed to add log entry to database", Error);
    }
}

void TMWeeklyPCController::refreshTrackerTable()
{
    if (m_trackerModel) {
        // CRITICAL FIX: Always ensure newest entries appear at top after refresh
        // This should be independent of any application state
        m_trackerModel->setSort(0, Qt::DescendingOrder);
        m_trackerModel->select();
        outputToTerminal("Tracker table refreshed with newest entries at top", Info);
    }
}

QString TMWeeklyPCController::copyFormattedRow()
{
    QString result = BaseTrackerController::copyFormattedRow(); // Call inherited method
    return result;
}

bool TMWeeklyPCController::createExcelAndCopy(const QStringList& headers, const QStringList& rowData)
{
    // Use the inherited BaseTrackerController implementation
    return BaseTrackerController::createExcelAndCopy(headers, rowData);
}

void TMWeeklyPCController::showTableContextMenu(const QPoint& pos)
{
    QMenu menu(m_tracker);
    QAction* copyAction = menu.addAction("Copy Selected Row");
    QAction* selectedAction = menu.exec(m_tracker->mapToGlobal(pos));
    if (selectedAction == copyAction) {
        copyFormattedRow();
    }
}

void TMWeeklyPCController::parseScriptOutput(const QString& output)
{
    // Look for output path markers from the Post Print script
    if (output.contains("=== OUTPUT_PATH ===")) {
        m_capturingNASPath = true;
        return;
    }

    if (output.contains("=== END_OUTPUT_PATH ===")) {
        m_capturingNASPath = false;
        return;
    }

    // Capture the NAS path
    if (m_capturingNASPath && !output.trimmed().isEmpty()) {
        m_capturedNASPath = output.trimmed();
        outputToTerminal("Captured NAS path: " + m_capturedNASPath, Success);
    }
}

void TMWeeklyPCController::setTextBrowser(QTextBrowser* textBrowser)
{
    m_textBrowser = textBrowser;
    if (m_textBrowser) {
        // Force initial HTML load by resetting state
        m_currentHtmlState = UninitializedState;
        updateHtmlDisplay();
    }
}

void TMWeeklyPCController::showNASLinkDialog()
{
    if (m_capturedNASPath.isEmpty()) {
        outputToTerminal("No NAS path captured - cannot display location dialog", Warning);
        return;
    }

    outputToTerminal("Opening print file location dialog...", Info);

    // Create and show the dialog with custom text for Post Print
    // Use nullptr as parent since TMWeeklyPCController is not a QWidget
    NASLinkDialog* dialog = new NASLinkDialog(
        "Print File Location",           // Window title
        "Print file located below",      // Description text
        m_capturedNASPath,              // Network path
        nullptr                         // Parent (changed from 'this' to nullptr)
        );
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void TMWeeklyPCController::calculateMeterPostage()
{
    // Only calculate if both class and permit are set to the required values
    if (m_classDDbox->currentText() != "FIRST CLASS" || m_permitDDbox->currentText() != "METER") {
        return; // Exit if conditions aren't met
    }

    // Get count value
    QString countText = m_countBox->text();
    if (countText.isEmpty()) {
        return; // Exit if no count entered
    }

    bool ok;
    int count = countText.toInt(&ok);
    if (!ok || count <= 0) {
        return; // Exit if invalid count
    }

    // Get current meter rate from database
    double meterRate = getMeterRateFromDatabase();
    if (meterRate <= 0) {
        meterRate = 0.69; // Fallback to default rate
    }

    // Calculate postage: count * meter rate
    double totalPostage = count * meterRate;

    // Format as currency and update the postage box
    QString formattedPostage = QString("$%1").arg(totalPostage, 0, 'f', 2);
    m_postageBox->setText(formattedPostage);
}

double TMWeeklyPCController::getMeterRateFromDatabase()
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        return 0.69; // Return default if database not available
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT rate_value FROM meter_rates ORDER BY created_at DESC LIMIT 1");

    if (m_dbManager->executeQuery(query) && query.next()) {
        return query.value("rate_value").toDouble();
    }

    return 0.69; // Return default if no rate found in database
}

// FIXED: Enhanced resetToDefaults to properly save state before reset and not force default.html
void TMWeeklyPCController::resetToDefaults()
{
    // CRITICAL FIX: Save current job state BEFORE resetting if job was locked
    if (m_jobDataLocked) {
        outputToTerminal("Saving job state before reset...", Info);
        saveJobState();
    }

    // Move files to home folder before resetting
    outputToTerminal("Moving files to HOME folder...", Info);
    moveFilesToHomeFolder();

    // Reset internal state variables
    m_jobDataLocked = false;
    m_postageDataLocked = false;
    m_capturedNASPath.clear();
    m_capturingNASPath = false;
    m_lastExecutedScript.clear();

    // Clear all form fields
    if (m_jobNumberBox) m_jobNumberBox->clear();
    if (m_postageBox) {
        m_postageBox->clear();
        m_postageBox->setText(""); // Ensure it's completely cleared
    }
    if (m_countBox) m_countBox->clear();

    // Reset all dropdowns to index 0 (empty)
    if (m_yearDDbox) m_yearDDbox->setCurrentIndex(0);
    if (m_monthDDbox) m_monthDDbox->setCurrentIndex(0);
    if (m_weekDDbox) m_weekDDbox->setCurrentIndex(0);
    if (m_proofDDbox) m_proofDDbox->setCurrentIndex(0);
    if (m_printDDbox) m_printDDbox->setCurrentIndex(0);
    if (m_classDDbox) m_classDDbox->setCurrentIndex(0);
    if (m_permitDDbox) m_permitDDbox->setCurrentIndex(0);

    // Reset checkboxes
    if (m_proofApprovalCheckBox) m_proofApprovalCheckBox->setChecked(false);

    // Reset all lock buttons to unchecked
    if (m_lockBtn) m_lockBtn->setChecked(false);
    if (m_editBtn) m_editBtn->setChecked(false);
    if (m_postageLockBtn) m_postageLockBtn->setChecked(false);

    // Clear terminal window
    if (m_terminalWindow) m_terminalWindow->clear();

    // CRITICAL FIX: Reset HTML state to Uninitialized, then let updateHtmlDisplay determine correct state
    m_currentHtmlState = UninitializedState;
    updateControlStates();
    updateHtmlDisplay(); // This will call determineHtmlState() and load appropriate HTML (default.html)

    // Emit signal to stop auto-save timer since no job is open
    emit jobClosed();
    outputToTerminal("Job state reset to defaults", Info);
    outputToTerminal("Auto-save timer stopped - no job open", Info);
}

// FIXED: Enhanced moveFilesToHomeFolder with better reporting
bool TMWeeklyPCController::moveFilesToHomeFolder()
{
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString week = m_weekDDbox ? m_weekDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty() || week.isEmpty()) {
        outputToTerminal("Cannot move files: missing year, month, or week data", Warning);
        return false;
    }

    QString basePath = "C:/Goji/TRACHMAR/WEEKLY PC";
    QString homeFolder = month + "." + week;
    QString jobFolder = basePath + "/JOB";
    QString homeFolderPath = basePath + "/" + homeFolder;

    // Create home folder structure if it doesn't exist
    QDir homeDir(homeFolderPath);
    if (!homeDir.exists()) {
        if (!homeDir.mkpath(".")) {
            outputToTerminal("Failed to create HOME folder: " + homeFolderPath, Error);
            return false;
        }
        outputToTerminal("Created HOME folder: " + homeFolderPath, Info);
    }

    // Create subdirectories in HOME folder
    QStringList subDirs = {"INPUT", "OUTPUT", "PRINT", "PROOF"};
    for (const QString& subDir : subDirs) {
        QString subDirPath = homeFolderPath + "/" + subDir;
        QDir dir(subDirPath);
        if (!dir.exists() && !dir.mkpath(".")) {
            outputToTerminal("Failed to create subdirectory: " + subDirPath, Error);
            return false;
        }
    }

    // Move files from JOB subfolders to HOME subfolders
    bool allMoved = true;
    int totalFilesMoved = 0;

    for (const QString& subDir : subDirs) {
        QString jobSubDir = jobFolder + "/" + subDir;
        QString homeSubDir = homeFolderPath + "/" + subDir;

        QDir sourceDir(jobSubDir);
        if (sourceDir.exists()) {
            QStringList files = sourceDir.entryList(QDir::Files);
            for (const QString& fileName : files) {
                QString sourcePath = jobSubDir + "/" + fileName;
                QString destPath = homeSubDir + "/" + fileName;

                // Remove existing file in destination if it exists
                if (QFile::exists(destPath)) {
                    if (!QFile::remove(destPath)) {
                        outputToTerminal("Failed to remove existing file: " + destPath, Warning);
                    }
                }

                // Move file (rename)
                if (!QFile::rename(sourcePath, destPath)) {
                    outputToTerminal("Failed to move file: " + sourcePath, Error);
                    allMoved = false;
                } else {
                    outputToTerminal("Moved: " + fileName + " (" + subDir + ")", Info);
                    totalFilesMoved++;
                }
            }
        }
    }

    outputToTerminal(QString("File move completed: %1 files moved to HOME folder").arg(totalFilesMoved),
                     totalFilesMoved > 0 ? Success : Info);
    return allMoved;
}

// FIXED: Enhanced copyFilesFromHomeFolder with better error reporting and file counting
bool TMWeeklyPCController::copyFilesFromHomeFolder()
{
    QString year = m_yearDDbox ? m_yearDDbox->currentText() : "";
    QString month = m_monthDDbox ? m_monthDDbox->currentText() : "";
    QString week = m_weekDDbox ? m_weekDDbox->currentText() : "";

    if (year.isEmpty() || month.isEmpty() || week.isEmpty()) {
        outputToTerminal("Cannot copy files: missing year, month, or week data", Warning);
        return false;
    }

    QString basePath = "C:/Goji/TRACHMAR/WEEKLY PC";
    QString homeFolder = month + "." + week;
    QString jobFolder = basePath + "/JOB";
    QString homeFolderPath = basePath + "/" + homeFolder;

    // Check if home folder exists
    QDir homeDir(homeFolderPath);
    if (!homeDir.exists()) {
        outputToTerminal("HOME folder does not exist: " + homeFolderPath, Info);
        outputToTerminal("This is normal for new jobs - no files to copy", Info);
        return true; // Not an error if no previous job exists
    }

    // Create JOB subdirectories if they don't exist
    QStringList subDirs = {"INPUT", "OUTPUT", "PRINT", "PROOF"};
    for (const QString& subDir : subDirs) {
        QString subDirPath = jobFolder + "/" + subDir;
        QDir dir(subDirPath);
        if (!dir.exists() && !dir.mkpath(".")) {
            outputToTerminal("Failed to create JOB subdirectory: " + subDirPath, Error);
            return false;
        }
    }

    // Copy files from HOME subfolders to JOB subfolders
    bool allCopied = true;
    int totalFilesCopied = 0;

    for (const QString& subDir : subDirs) {
        QString homeSubDir = homeFolderPath + "/" + subDir;
        QString jobSubDir = jobFolder + "/" + subDir;

        QDir sourceDir(homeSubDir);
        if (sourceDir.exists()) {
            QStringList files = sourceDir.entryList(QDir::Files);
            for (const QString& fileName : files) {
                QString sourcePath = homeSubDir + "/" + fileName;
                QString destPath = jobSubDir + "/" + fileName;

                // Remove existing file in destination if it exists
                if (QFile::exists(destPath)) {
                    if (!QFile::remove(destPath)) {
                        outputToTerminal("Failed to remove existing file: " + destPath, Warning);
                    }
                }

                // Copy file
                if (!QFile::copy(sourcePath, destPath)) {
                    outputToTerminal("Failed to copy file: " + sourcePath, Error);
                    allCopied = false;
                } else {
                    outputToTerminal("Copied: " + fileName + " (" + subDir + ")", Info);
                    totalFilesCopied++;
                }
            }
        }
    }

    outputToTerminal(QString("File copy completed: %1 files copied").arg(totalFilesCopied),
                     totalFilesCopied > 0 ? Success : Info);
    return allCopied;
}

// CRITICAL FIX: Auto-save and close current job before opening a new one
void TMWeeklyPCController::autoSaveAndCloseCurrentJob()
{
    // Check if we have a job currently open (locked)
    if (m_jobDataLocked) {
        QString currentJobNumber = m_jobNumberBox ? m_jobNumberBox->text() : "";
        QString currentYear = m_yearDDbox ? m_yearDDbox->currentText() : "";
        QString currentMonth = m_monthDDbox ? m_monthDDbox->currentText() : "";
        QString currentWeek = m_weekDDbox ? m_weekDDbox->currentText() : "";
        
        if (!currentJobNumber.isEmpty() && !currentYear.isEmpty() && !currentMonth.isEmpty() && !currentWeek.isEmpty()) {
            outputToTerminal(QString("Auto-saving current job %1 (%2-%3-%4) before opening new job")
                           .arg(currentJobNumber, currentYear, currentMonth, currentWeek), Info);
            
            // Save current job state
            saveJobState();
            
            // Save to database
            saveJobToDatabase();
            
            // Clear current job state
            m_jobDataLocked = false;
            m_postageDataLocked = false;
            m_currentHtmlState = UninitializedState;
            
            // Update UI to reflect cleared state
            updateControlStates();
            
            // Signal that job is closed (stops auto-save timer)
            emit jobClosed();
            
            outputToTerminal("Current job auto-saved and closed", Success);
        }
    }
}
