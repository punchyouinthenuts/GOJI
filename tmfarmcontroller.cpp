#include <QDebug>
#include "tmfarmcontroller.h"

#include <QComboBox>
#include <QDate>
#include <QLineEdit>
#include <QAbstractButton>
#include <QPushButton>
#include <QToolButton>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTableView>
#include <QTextBrowser>
#include <QTextEdit>
#include <QHeaderView>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QUrl>
#include <QLocale>
#include <QSignalBlocker>

static const char *TABLE_JOBS  = "tm_farm_jobs";
static const char *TABLE_STATE = "tm_farm_state";
static const char *TABLE_LOG   = "tm_farm_log";

TMFarmController::TMFarmController(QObject *parent)
    : QObject(parent)
{
    m_jobLocked = false;
    m_postageLocked = false;
}

void TMFarmController::attachWidgets(QComboBox *yearDD,
                                     QComboBox *quarterDD,
                                     QLineEdit *jobNumberBox,
                                     QLineEdit *postageBox,
                                     QLineEdit *countBox,
                                     QAbstractButton *lockButton,
                                     QAbstractButton *editButton,
                                     QAbstractButton *postageLockButton,
                                     QTableView *trackerView,
                                     QTextBrowser *htmlView)
{
    m_yearDD = yearDD;
    m_quarterDD = quarterDD;
    m_jobNumberBox = jobNumberBox;
    m_postageBox = postageBox;
    m_countBox = countBox;
    m_lockButton = lockButton;
    m_editButton = editButton;
    m_postageLockButton = postageLockButton;
    m_trackerView = trackerView;
    m_htmlView = htmlView;
}

void TMFarmController::initialize()
{
    ensureTables();
    populateYearCombo();
    setupTrackerModel();
    connectSignals();

    updateControlStates();
    updateHtmlDisplay();
    refreshTracker();
}

void TMFarmController::setTextBrowser(QTextBrowser *htmlView)
{
    m_htmlView = htmlView;
}

void TMFarmController::initializeUI(QComboBox *yearDD,
                                    QComboBox *quarterDD,
                                    QLineEdit *jobNumberBox,
                                    QLineEdit *postageBox,
                                    QLineEdit *countBox,
                                    QPushButton *lockButton,
                                    QPushButton *editButton,
                                    QPushButton *postageLockButton,
                                    QTableView *trackerView)
{
    attachWidgets(yearDD, quarterDD, jobNumberBox, postageBox, countBox,
                  lockButton, editButton, postageLockButton, trackerView, m_htmlView);
    initialize();
}

void TMFarmController::initializeUI(QPushButton *openBulkMailerBtn,
                                    QPushButton *runInitialBtn,
                                    QPushButton *finalStepBtn,
                                    QToolButton *lockButton,
                                    QToolButton *editButton,
                                    QToolButton *postageLockButton,
                                    QComboBox *yearDD,
                                    QComboBox *quarterDD,
                                    QLineEdit *jobNumberBox,
                                    QLineEdit *postageBox,
                                    QLineEdit *countBox,
                                    QTextEdit *terminalWindow,
                                    QTableView *trackerView,
                                    QTextBrowser *htmlView)
{
    m_openBulkMailerBtn = openBulkMailerBtn;
    m_runInitialBtn = runInitialBtn;
    m_finalStepBtn = finalStepBtn;
    m_terminalWindow = terminalWindow;

    attachWidgets(yearDD, quarterDD, jobNumberBox, postageBox, countBox,
                  lockButton, editButton, postageLockButton, trackerView, htmlView);
    initialize();
}

void TMFarmController::populateYearCombo()
{
    if (!m_yearDD) return;
    m_yearDD->clear();

    const int y = QDate::currentDate().year();
    m_yearDD->addItem(QString());
    m_yearDD->addItem(QString::number(y - 1));
    m_yearDD->addItem(QString::number(y));
    m_yearDD->addItem(QString::number(y + 1));
}

void TMFarmController::setupTrackerModel()
{
    if (!m_trackerView) return;

    m_trackerModel = std::make_unique<QSqlTableModel>(this);
    m_trackerModel->setTable(TABLE_LOG);
    m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_trackerModel->select();

    QObject::connect(m_trackerModel.get(), &QAbstractItemModel::modelReset,
                     this, &TMFarmController::setupOptimizedTableLayout);

    setupOptimizedTableLayout();

    m_trackerView->setModel(m_trackerModel.get());
    m_trackerView->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void TMFarmController::setupOptimizedTableLayout()
{
    if (!m_trackerModel || !m_trackerView) return;

    m_trackerView->horizontalHeader()->setVisible(true);
    m_trackerView->verticalHeader()->setVisible(false);

    const int idxJob   = m_trackerModel->fieldIndex("job_number");
    const int idxDesc  = m_trackerModel->fieldIndex("description");
    const int idxPost  = m_trackerModel->fieldIndex("postage");
    const int idxCount = m_trackerModel->fieldIndex("count");
    const int idxAvg   = m_trackerModel->fieldIndex("per_piece");
    const int idxClass = m_trackerModel->fieldIndex("mail_class");
    const int idxShape = m_trackerModel->fieldIndex("shape");
    const int idxPermit= m_trackerModel->fieldIndex("permit");
    const int idxId    = m_trackerModel->fieldIndex("id");
    const int idxDate  = m_trackerModel->fieldIndex("log_date");
    const int idxYear  = m_trackerModel->fieldIndex("year");
    const int idxQtr   = m_trackerModel->fieldIndex("quarter");


    if (idxId   >= 0) m_trackerView->setColumnHidden(idxId, true);
    if (idxDate >= 0) m_trackerView->setColumnHidden(idxDate, true);
    if (idxYear >= 0) m_trackerView->setColumnHidden(idxYear, true);
    if (idxQtr  >= 0) m_trackerView->setColumnHidden(idxQtr, true);

    m_trackerView->setSortingEnabled(true);
    if (idxId >= 0) {
        m_trackerModel->setSort(idxId, Qt::DescendingOrder);
        m_trackerModel->select();
        m_trackerView->sortByColumn(idxId, Qt::DescendingOrder);
    } else if (idxDate >= 0) {
        m_trackerModel->setSort(idxDate, Qt::DescendingOrder);
        m_trackerModel->select();
        m_trackerView->sortByColumn(idxDate, Qt::DescendingOrder);
    }

    m_trackerView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_trackerView->horizontalHeader()->setStretchLastSection(true);
    m_trackerView->setAlternatingRowColors(true);
    m_trackerView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);


    // --- Debug: dump header data after layout to verify visible labels ---
    if (m_trackerModel) {
        const int cols = m_trackerModel->columnCount();
        for (int c = 0; c < cols; ++c) {
            const QVariant disp = m_trackerModel->headerData(c, Qt::Horizontal, Qt::DisplayRole);
            const QVariant edit = m_trackerModel->headerData(c, Qt::Horizontal, Qt::EditRole);
            qDebug() << "[TMFarmController] header column" << c
                     << "DisplayRole=" << disp.toString()
                     << "EditRole=" << edit.toString();
        }
    }

    // Apply header labels at the end so they persist after select()
    if (idxJob   >= 0) m_trackerModel->setHeaderData(idxJob,   Qt::Horizontal, "JOB", Qt::DisplayRole);
    if (idxDesc  >= 0) m_trackerModel->setHeaderData(idxDesc,  Qt::Horizontal, "DESCRIPTION", Qt::DisplayRole);
    if (idxPost  >= 0) m_trackerModel->setHeaderData(idxPost,  Qt::Horizontal, "POSTAGE", Qt::DisplayRole);
    if (idxCount >= 0) m_trackerModel->setHeaderData(idxCount, Qt::Horizontal, "COUNT", Qt::DisplayRole);
    if (idxAvg   >= 0) m_trackerModel->setHeaderData(idxAvg,   Qt::Horizontal, "AVG RATE", Qt::DisplayRole);
    if (idxClass >= 0) m_trackerModel->setHeaderData(idxClass, Qt::Horizontal, "CLASS", Qt::DisplayRole);
    if (idxShape >= 0) m_trackerModel->setHeaderData(idxShape, Qt::Horizontal, "SHAPE", Qt::DisplayRole);
    if (idxPermit>= 0) m_trackerModel->setHeaderData(idxPermit,Qt::Horizontal, "PERMIT", Qt::DisplayRole);
}

void TMFarmController::connectSignals()
{
    if (m_lockButton)        m_lockButton->setCheckable(true);
    if (m_editButton)        m_editButton->setCheckable(true);
    if (m_postageLockButton) m_postageLockButton->setCheckable(true);

    if (m_jobNumberBox)
        connect(m_jobNumberBox, &QLineEdit::editingFinished,
                this, &TMFarmController::onJobNumberEditingFinished);

    if (m_lockButton)
        connect(m_lockButton, &QAbstractButton::clicked,
                this, &TMFarmController::onLockClicked);

    if (m_editButton)
        connect(m_editButton, &QAbstractButton::clicked,
                this, &TMFarmController::onEditClicked);

    if (m_postageLockButton)
        connect(m_postageLockButton, &QAbstractButton::clicked,
                this, &TMFarmController::onPostageLockClicked);

    if (m_postageBox) {
        auto *validator = new QRegularExpressionValidator(
            QRegularExpression(R"(^\s*\$?\s*[0-9,]*(?:\.[0-9]{0,2})?\s*$)"), this);
        m_postageBox->setValidator(validator);
        connect(m_postageBox, &QLineEdit::editingFinished,
                this, &TMFarmController::formatPostageInput);
        connect(m_postageBox, &QLineEdit::textChanged, this, [this]{
            if (m_jobLocked) saveJobState();
        });
    }

    if (m_countBox) {
        connect(m_countBox, &QLineEdit::textChanged,
                this, &TMFarmController::formatCountInput);
        connect(m_countBox, &QLineEdit::textChanged, this, [this]{
            if (m_jobLocked) saveJobState();
        });
    }
}

QString TMFarmController::currentJobNumber() const
{
    return m_jobNumberBox ? m_jobNumberBox->text().trimmed() : QString();
}

QString TMFarmController::currentYearText() const
{
    return m_yearDD ? m_yearDD->currentText().trimmed() : QString();
}

QString TMFarmController::currentQuarterText() const
{
    return m_quarterDD ? m_quarterDD->currentText().trimmed() : QString();
}

bool TMFarmController::validateJobNumber(const QString &job) const
{
    static const QRegularExpression re(QStringLiteral("^[0-9]{1,10}$"));
    return re.match(job).hasMatch();
}

QString TMFarmController::normalizePostage(const QString &raw) const
{
    QString s = raw;
    s.remove(QRegularExpression("[^0-9.]"));
    const int firstDot = s.indexOf('.');
    if (firstDot != -1) {
        const QString lhs = s.left(firstDot + 1);
        QString rhs = s.mid(firstDot + 1);
        rhs.remove('.');
        s = lhs + rhs;
    }
    if (!s.isEmpty()) {
        bool ok = false;
        const double v = s.toDouble(&ok);
        if (ok) return QString("$%1").arg(QString::number(v, 'f', 2));
    }
    return QString();
}

QString TMFarmController::normalizeCount(const QString &raw) const
{
    QString s = raw;
    s.remove(QRegularExpression("[^0-9]"));
    return s;
}

void TMFarmController::formatPostageInput()
{
    if (!m_postageBox) return;
    QString text = m_postageBox->text().trimmed();
    if (text.isEmpty()) return;

    QString clean = text; clean.remove(QRegularExpression("[^0-9.]"));
    int dot = clean.indexOf('.');
    if (dot != -1) {
        QString lhs = clean.left(dot + 1);
        QString rhs = clean.mid(dot + 1).remove('.');
        clean = lhs + rhs;
    }
    bool ok = false;
    double v = clean.toDouble(&ok);
    if (ok) {
        QLocale us(QLocale::English, QLocale::UnitedStates);
        m_postageBox->setText(us.toCurrencyString(v, "$"));
        saveJobState();
    }
}

void TMFarmController::formatCountInput(const QString &text)
{
    if (!m_countBox) return;
    QString clean = text; clean.remove(QRegularExpression("[^0-9]"));
    if (clean.isEmpty()) return;

    bool ok = false;
    qlonglong n = clean.toLongLong(&ok);
    if (!ok) return;

    QLocale us(QLocale::English, QLocale::UnitedStates);
    const QString formatted = us.toString(n);

    if (m_countBox->text() != formatted) {
        const QSignalBlocker guard(m_countBox);
        m_countBox->setText(formatted);
    }
}

void TMFarmController::onJobNumberEditingFinished()
{
    const QString job = currentJobNumber();
    if (!validateJobNumber(job)) return;

    ensureTables();

    QSqlQuery q;
    q.prepare(QString("INSERT INTO %1(job_number, year, quarter) "
                      "VALUES(:job, :year, :q) "
                      "ON CONFLICT(job_number, year, quarter) DO UPDATE SET job_number=excluded.job_number")
              .arg(TABLE_JOBS));
    q.bindValue(":job", job);
    q.bindValue(":year", currentYearText());
    q.bindValue(":q", currentQuarterText());
    q.exec();

    saveJobState();
    refreshTracker();
}

void TMFarmController::onLockClicked()
{
    if (!m_lockButton) return;

    if (m_lockButton->isChecked()) {
        const QString job = currentJobNumber();
        if (!validateJobNumber(job) || currentYearText().isEmpty() || currentQuarterText().isEmpty()) {
            m_lockButton->setChecked(false);
            return;
        }
        m_jobLocked = true;
        if (m_editButton) m_editButton->setChecked(false);
        saveJobState();
    } else {
        m_lockButton->setChecked(true);
        return;
    }

    updateControlStates();
    updateHtmlDisplay();
}

void TMFarmController::onEditClicked()
{
    if (!m_editButton) return;

    if (!m_jobLocked) {
        m_editButton->setChecked(false);
        return;
    }

    if (m_editButton->isChecked()) {
        m_jobLocked = false;
        if (m_lockButton) m_lockButton->setChecked(false);
        updateControlStates();
        updateHtmlDisplay();
    }
}

void TMFarmController::onPostageLockClicked()
{
    if (!m_postageLockButton) return;
    m_postageLocked = m_postageLockButton->isChecked();
    updateControlStates();
    saveJobState();
}

void TMFarmController::onPostageEditingFinished()
{
    if (!m_jobLocked || !m_postageBox) return;
    const QString fmt = normalizePostage(m_postageBox->text());
    if (!fmt.isEmpty()) m_postageBox->setText(fmt);
    saveJobState();
}

void TMFarmController::onCountEditingFinished()
{
    if (!m_jobLocked || !m_countBox) return;
    const QString n = normalizeCount(m_countBox->text());
    if (!n.isEmpty()) m_countBox->setText(n);
    saveJobState();
}

void TMFarmController::updateControlStates()
{
    const bool jobFieldsEnabled = !m_jobLocked;

    if (m_jobNumberBox) m_jobNumberBox->setEnabled(jobFieldsEnabled);
    if (m_yearDD)       m_yearDD->setEnabled(jobFieldsEnabled);
    if (m_quarterDD)    m_quarterDD->setEnabled(jobFieldsEnabled);

    if (m_postageBox)   m_postageBox->setEnabled(!m_postageLocked);
    if (m_countBox)     m_countBox->setEnabled(!m_postageLocked);

    if (m_lockButton)        m_lockButton->setChecked(m_jobLocked);
    if (m_editButton)        m_editButton->setEnabled(m_jobLocked);
    if (m_postageLockButton) m_postageLockButton->setEnabled(m_jobLocked);
}

void TMFarmController::updateHtmlDisplay()
{
    if (!m_htmlView) return;
    m_htmlView->setSource(m_jobLocked
        ? QUrl("qrc:/resources/tmfarmworkers/instructions.html")
        : QUrl("qrc:/resources/tmfarmworkers/default.html"));
}

void TMFarmController::refreshTracker()
{
    if (!m_trackerModel) return;

    const QString job  = currentJobNumber();
    const QString year = currentYearText();
    const QString qtr  = currentQuarterText();

    QStringList parts;
    if (!job.isEmpty())  { QString esc = job;  esc.replace(QStringLiteral("'"), QStringLiteral("''")); parts << QString("job_number='%1'").arg(esc); }
    if (!year.isEmpty()) { QString esc = year; esc.replace(QStringLiteral("'"), QStringLiteral("''")); parts << QString("year='%1'").arg(esc); }
    if (!qtr.isEmpty())  { QString esc = qtr;  esc.replace(QStringLiteral("'"), QStringLiteral("''")); parts << QString("quarter='%1'").arg(esc); }

    const QString filter = parts.join(" AND ");
    m_trackerModel->setFilter(filter);
    m_trackerModel->select();
}

void TMFarmController::ensureTables()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isValid() || !db.isOpen()) {
        return;
    }

    QSqlQuery q(db);
    q.exec(QString("CREATE TABLE IF NOT EXISTS %1 ("
                   "job_number TEXT NOT NULL,"
                   "year TEXT,"
                   "quarter TEXT,"
                   "PRIMARY KEY(job_number, year, quarter))").arg(TABLE_JOBS));

    q.exec(QString("CREATE TABLE IF NOT EXISTS %1 ("
                   "job_number TEXT NOT NULL,"
                   "year TEXT,"
                   "quarter TEXT,"
                   "job_locked INTEGER DEFAULT 1,"
                   "postage_locked INTEGER DEFAULT 1,"
                   "postage TEXT,"
                   "count TEXT,"
                   "html_state INTEGER DEFAULT 1,"
                   "PRIMARY KEY(job_number, year, quarter))").arg(TABLE_STATE));

    q.exec(QString("CREATE TABLE IF NOT EXISTS %1 ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                   "log_date TEXT,"
                   "job_number TEXT,"
                   "year TEXT,"
                   "quarter TEXT,"
                   "description TEXT,"
                   "postage TEXT,"
                   "count TEXT,"
                   "per_piece TEXT,"
                   "mail_class TEXT,"
                   "shape TEXT,"
                   "permit TEXT)").arg(TABLE_LOG));
}

void TMFarmController::loadJobState()
{
    const QString job = currentJobNumber();
    if (job.isEmpty()) return;

    QSqlQuery q;
    q.prepare(QString("SELECT job_locked, postage_locked, postage, count, html_state "
                      "FROM %1 WHERE job_number=:job AND year=:y AND quarter=:q")
              .arg(TABLE_STATE));
    q.bindValue(":job", job);
    q.bindValue(":y", currentYearText());
    q.bindValue(":q", currentQuarterText());
    if (!q.exec()) return;
    if (q.next()) {
        m_jobLocked     = q.value(0).toInt() != 0;
        m_postageLocked = q.value(1).toInt() != 0;
        if (m_postageBox) m_postageBox->setText(q.value(2).toString());
        if (m_countBox)   m_countBox->setText(q.value(3).toString());
    }
}

void TMFarmController::saveJobState()
{
    const QString job = currentJobNumber();
    if (job.isEmpty()) return;

    const QString y   = currentYearText();
    const QString qtr = currentQuarterText();
    const QString postage = m_postageBox ? m_postageBox->text() : QString();
    const QString count   = m_countBox   ? m_countBox->text()   : QString();
    const int html_state  = m_jobLocked ? 1 : 0;

    QSqlQuery q;
    q.prepare(QString("INSERT INTO %1(job_number, year, quarter, job_locked, postage_locked, postage, count, html_state) "
                      "VALUES(:job, :y, :q, :jl, :pl, :p, :c, :h) "
                      "ON CONFLICT(job_number, year, quarter) DO UPDATE SET "
                      "job_locked=excluded.job_locked, "
                      "postage_locked=excluded.postage_locked, "
                      "postage=excluded.postage, "
                      "count=excluded.count, "
                      "html_state=excluded.html_state")
              .arg(TABLE_STATE));
    q.bindValue(":job", job);
    q.bindValue(":y", y);
    q.bindValue(":q", qtr);
    q.bindValue(":jl", m_jobLocked ? 1 : 0);
    q.bindValue(":pl", m_postageLocked ? 1 : 0);
    q.bindValue(":p", postage);
    q.bindValue(":c", count);
    q.bindValue(":h", html_state);
    q.exec();
}
