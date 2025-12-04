#include "tmfarmcontroller.h"
#include "tmfarmdbmanager.h"
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

TMFarmController::TMFarmController(QObject *parent)
    : QObject(parent)
{
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

    // Mirror TERM widget behavior
    initYearDropdown();         // year list
    setupTextBrowserInitial();  // initial default HTML
    wireFormattingForInputs();  // currency + thousands

    // Dynamic HTML refresh, identical trigger points as TERM
    if (m_lockButton)        connect(m_lockButton,        &QAbstractButton::toggled, this, &TMFarmController::updateHtmlDisplay);
    if (m_postageLockButton) connect(m_postageLockButton, &QAbstractButton::toggled, this, &TMFarmController::updateHtmlDisplay);
    if (m_yearDD)            connect(m_yearDD,            &QComboBox::currentTextChanged, this, &TMFarmController::updateHtmlDisplay);
    if (m_quarterDD)         connect(m_quarterDD,         &QComboBox::currentTextChanged, this, &TMFarmController::updateHtmlDisplay);

    // Tracker (visuals preserved)
    setupTrackerModel();
    setupOptimizedTableLayout();

    // Initial HTML refresh based on lock state
    updateHtmlDisplay();

    updateControlStates();
}

void TMFarmController::setupTrackerModel()
{
    if (!m_trackerView) return;

    m_trackerModel = std::make_unique<QSqlTableModel>(this, TMFarmDBManager::instance()->getDatabase());
    m_trackerModel->setTable(QStringLiteral("tm_farm_log"));
    m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_trackerModel->select();
    m_trackerView->setModel(m_trackerModel.get());
}

int TMFarmController::computeOptimalFontSize() const
{
    // Heuristic widths consistent with TERM look
    const QStringList labels = {
        QObject::tr("JOB"),
        QObject::tr("DESCRIPTION"),
        QObject::tr("POSTAGE"),
        QObject::tr("COUNT"),
        QObject::tr("AVG RATE"),
        QObject::tr("CLASS"),
        QObject::tr("SHAPE"),
        QObject::tr("PERMIT")
    };
    const QList<int> maxWidths = {56, 140, 29, 45, 45, 60, 33, 36};

    for (int pt = 11; pt >= 7; --pt) {
        QFont f(QStringLiteral("Blender Pro Bold"), pt);
        QFontMetrics fm(f);
        bool ok = true;
        for (int i = 0; i < labels.size(); ++i) {
            if (fm.horizontalAdvance(labels[i]) > maxWidths[i]) {
                ok = false; break;
            }
        }
        if (ok) return pt;
    }
    return 7;
}

void TMFarmController::applyHeaderLabels()
{
    if (!m_trackerModel) return;

    const int idxJob         = m_trackerModel->fieldIndex(QStringLiteral("job"));
    const int idxDescription = m_trackerModel->fieldIndex(QStringLiteral("description"));
    const int idxPostage     = m_trackerModel->fieldIndex(QStringLiteral("postage"));
    const int idxCount       = m_trackerModel->fieldIndex(QStringLiteral("count"));
    const int idxAvgRate     = m_trackerModel->fieldIndex(QStringLiteral("avg_rate"));
    const int idxMailClass   = m_trackerModel->fieldIndex(QStringLiteral("mail_class"));
    const int idxShape       = m_trackerModel->fieldIndex(QStringLiteral("shape"));
    const int idxPermit      = m_trackerModel->fieldIndex(QStringLiteral("permit"));

    if (idxJob >= 0)         m_trackerModel->setHeaderData(idxJob,         Qt::Horizontal, QObject::tr("JOB"));
    if (idxDescription >= 0) m_trackerModel->setHeaderData(idxDescription, Qt::Horizontal, QObject::tr("DESCRIPTION"));
    if (idxPostage >= 0)     m_trackerModel->setHeaderData(idxPostage,     Qt::Horizontal, QObject::tr("POSTAGE"));
    if (idxCount >= 0)       m_trackerModel->setHeaderData(idxCount,       Qt::Horizontal, QObject::tr("COUNT"));
    if (idxAvgRate >= 0)     m_trackerModel->setHeaderData(idxAvgRate,     Qt::Horizontal, QObject::tr("AVG RATE"));
    if (idxMailClass >= 0)   m_trackerModel->setHeaderData(idxMailClass,   Qt::Horizontal, QObject::tr("CLASS"));
    if (idxShape >= 0)       m_trackerModel->setHeaderData(idxShape,       Qt::Horizontal, QObject::tr("SHAPE"));
    if (idxPermit >= 0)      m_trackerModel->setHeaderData(idxPermit,      Qt::Horizontal, QObject::tr("PERMIT"));
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
    if (!m_trackerModel || !m_trackerView) return;

    const int tableWidth = 611; // matches UI geometry in this project
    const int borderWidth = 2;
    const int availableWidth = tableWidth - borderWidth;

    struct ColumnSpec { QString header; QString maxContent; int minWidth; };
    QList<ColumnSpec> columns = {
        {"JOB", "88888", 56},
        {"DESCRIPTION", "TM FARMWORKERS", 140},
        {"POSTAGE", "$888,888.88", 29},
        {"COUNT", "88,888", 45},
        {"AVG RATE", "0.888", 45},
        {"CLASS", "STD", 60},
        {"SHAPE", "LTR", 33},
        {"PERMIT", "NKLN", 36}
    };

    int optimalFontSize = computeOptimalFontSize();
    QFont tableFont(QStringLiteral("Blender Pro Bold"), optimalFontSize);
    m_trackerView->setFont(tableFont);

    QFontMetrics fm(tableFont);
    for (int i = 0; i < columns.size(); i++) {
        const auto& col = columns[i];
        int headerWidth  = fm.horizontalAdvance(col.header) + 12;
        int contentWidth = fm.horizontalAdvance(col.maxContent) + 12;
        int colWidth = qMax(headerWidth, qMax(contentWidth, col.minWidth));
        Q_UNUSED(availableWidth);
        m_trackerView->setColumnWidth(i + 1, colWidth); // we hide column 0
    }

    m_trackerView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_trackerView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_trackerView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void TMFarmController::setupOptimizedTableLayout()
{
    if (!m_trackerModel || !m_trackerView) return;

    m_trackerModel->setSort(0, Qt::DescendingOrder);
    m_trackerModel->select();

    applyHeaderLabels();
    applyFixedColumnWidths();
    enforceVisibilityMask();

    m_trackerView->horizontalHeader()->setVisible(true);
    m_trackerView->verticalHeader()->setVisible(false);
    m_trackerView->setAlternatingRowColors(true);
    m_trackerView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_trackerView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_trackerView->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void TMFarmController::refreshTracker(const QString &jobNumber)
{
    if (!m_trackerModel) return;

    m_trackerModel->setFilter(QStringLiteral("job='%1'").arg(jobNumber));
    m_trackerModel->setSort(0, Qt::DescendingOrder);
    m_trackerModel->select();

    applyHeaderLabels();
    applyFixedColumnWidths();
    enforceVisibilityMask();
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
    formatPostageBoxDisplay();
}

void TMFarmController::onCountEditingFinished()
{
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
    const bool jobLocked = (m_lockButton && m_lockButton->isChecked());
    const bool postageLocked = (m_postageLockButton && m_postageLockButton->isChecked());
    if (jobLocked && !postageLocked) return 1;
    return 0;
}

void TMFarmController::updateHtmlDisplay()
{
    const int state = determineHtmlState();
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

// ================================ Buttons ===================================

void TMFarmController::onRunInitialClicked()
{
    const QString scriptPath = QStringLiteral("C:/Goji/scripts/TRACHMAR/FARMWORKERS/01 INITIAL.py");
    if (!QFile::exists(scriptPath)) {
        if (m_terminalWindow) m_terminalWindow->append("[ERROR] Initial script not found: " + scriptPath);
        return;
    }
    if (m_terminalWindow) m_terminalWindow->append("[FARMWORKERS] Starting initial script...");
    m_scriptRunner->runScript(scriptPath, QStringList());
}

void TMFarmController::onOpenBulkMailerClicked()
{
    const QString bulkMailerPath = QStringLiteral("C:/Program Files (x86)/Satori Software/Bulk Mailer/BulkMailer.exe");
    if (!QFile::exists(bulkMailerPath)) {
        if (m_terminalWindow) m_terminalWindow->append("[ERROR] Bulk Mailer not found at: " + bulkMailerPath);
        return;
    }
    if (!QProcess::startDetached(bulkMailerPath, QStringList())) {
        if (m_terminalWindow) m_terminalWindow->append("[ERROR] Failed to launch Bulk Mailer");
    } else {
        if (m_terminalWindow) m_terminalWindow->append("[FARMWORKERS] Bulk Mailer launched");
    }
}

void TMFarmController::onFinalStepClicked()
{
    const QString jobNumber = m_jobNumberBox ? m_jobNumberBox->text().trimmed() : QString();
    const QString quarter = m_quarterDD ? m_quarterDD->currentText().trimmed() : QString();
    const QString year = m_yearDD ? m_yearDD->currentText().trimmed() : QString();

    if (jobNumber.isEmpty() || quarter.isEmpty() || year.isEmpty()) {
        if (m_terminalWindow) m_terminalWindow->append("[ERROR] Job number, quarter, and year are required");
        return;
    }

    QString scriptPath = QStringLiteral("C:/Goji/scripts/TRACHMAR/FARMWORKERS/02 POST PROCESS.py");
    if (!QFile::exists(scriptPath)) {
        if (m_terminalWindow) m_terminalWindow->append("[ERROR] Post-process script not found: " + scriptPath);
        return;
    }

    // reset capture
    m_capturedNASPath.clear();
    m_capturingNASPath = false;

    if (m_terminalWindow) {
        m_terminalWindow->append("[FARMWORKERS] Starting prearchive phase...");
        m_terminalWindow->append(QString("Job: %1, Quarter: %2, Year: %3").arg(jobNumber, quarter, year));
    }

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
        if (m_terminalWindow) m_terminalWindow->append(trimmed);
    }

    parseScriptOutputLine(trimmed);
}

void TMFarmController::onScriptError(const QString& line)
{
    if (m_terminalWindow) m_terminalWindow->append("[ERROR] " + line);
}

void TMFarmController::onScriptFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status);
    if (m_terminalWindow) {
        if (exitCode == 0) {
            m_terminalWindow->append("[FARMWORKERS] Prearchive phase completed");
        } else {
            m_terminalWindow->append(QString("[FARMWORKERS] Prearchive phase failed (exit code: %1)").arg(exitCode));
        }
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
        if (m_terminalWindow) m_terminalWindow->append("[ERROR] Cannot start archive phase: missing job data");
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

    if (m_terminalWindow) m_terminalWindow->append("[FARMWORKERS] Starting archive phase...");

    QProcess* archiveProcess = new QProcess(this);

    connect(archiveProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TMFarmController::onArchiveFinished);

    connect(archiveProcess, &QProcess::readyReadStandardOutput, this, [this, archiveProcess]() {
        const QString out = QString::fromUtf8(archiveProcess->readAllStandardOutput());
        if (m_terminalWindow && !out.trimmed().isEmpty()) m_terminalWindow->append(out.trimmed());
    });
    connect(archiveProcess, &QProcess::readyReadStandardError, this, [this, archiveProcess]() {
        const QString err = QString::fromUtf8(archiveProcess->readAllStandardError());
        if (m_terminalWindow && !err.trimmed().isEmpty()) m_terminalWindow->append("[ERROR] " + err.trimmed());
    });

    archiveProcess->start(QStringLiteral("python"), args);
}

void TMFarmController::onArchiveFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status);
    if (m_terminalWindow) {
        if (exitCode == 0) {
            m_terminalWindow->append("[FARMWORKERS] Archive phase completed successfully");
        } else {
            m_terminalWindow->append(QString("[ERROR] Archive phase failed (exit code: %1)").arg(exitCode));
        }
    }
    QObject* proc = sender();
    if (proc) proc->deleteLater();
}

// ================================ Misc ======================================

void TMFarmController::updateControlStates()
{
    // Add enable/disable rules here if you later mirror TERM lock gating
}

void TMFarmController::triggerArchivePhase()
{
    runArchivePhase();
}
