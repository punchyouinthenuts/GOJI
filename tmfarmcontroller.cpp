#include "tmfarmcontroller.h"
#include "tmfarmdbmanager.h"

#include <QSqlRecord>
#include <QSqlError>
#include <QHeaderView>
#include <QDebug>

TMFarmController::TMFarmController(QObject *parent)
    : QObject(parent),
      m_trackerView(nullptr),
      m_textBrowser(nullptr),
      m_openBulkMailerBtn(nullptr),
      m_runInitialBtn(nullptr),
      m_finalStepBtn(nullptr),
      m_lockButton(nullptr),
      m_editButton(nullptr),
      m_postageLockButton(nullptr),
      m_yearDD(nullptr),
      m_quarterDD(nullptr),
      m_jobNumberBox(nullptr),
      m_postageBox(nullptr),
      m_countBox(nullptr),
      m_terminalWindow(nullptr)
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
    // Store widget pointers exactly as called from MainWindow
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

    setupTrackerModel();
    setupOptimizedTableLayout();
}

void TMFarmController::setupTrackerModel()
{
    if (!m_trackerView)
        return;

    // Use TMFarmDBManager’s database (preserved fix)
    m_trackerModel = std::make_unique<QSqlTableModel>(this, TMFarmDBManager::instance()->getDatabase());
    m_trackerModel->setTable("tm_farm_log");
    m_trackerModel->select();
    m_trackerView->setModel(m_trackerModel.get());
}

void TMFarmController::setupOptimizedTableLayout()
{
    if (!m_trackerModel || !m_trackerView)
        return;

    // Correct field names (preserved fixes)
    const int idxJob         = m_trackerModel->fieldIndex("job");
    const int idxDescription = m_trackerModel->fieldIndex("description");
    const int idxPostage     = m_trackerModel->fieldIndex("postage");
    const int idxCount       = m_trackerModel->fieldIndex("count");
    const int idxAvgRate     = m_trackerModel->fieldIndex("avg_rate");
    const int idxMailClass   = m_trackerModel->fieldIndex("mail_class");
    const int idxShape       = m_trackerModel->fieldIndex("shape");
    const int idxPermit      = m_trackerModel->fieldIndex("permit");
    const int idxDate        = m_trackerModel->fieldIndex("date");

    m_trackerModel->setHeaderData(idxJob,         Qt::Horizontal, "JOB",         Qt::DisplayRole);
    m_trackerModel->setHeaderData(idxDescription, Qt::Horizontal, "DESCRIPTION", Qt::DisplayRole);
    m_trackerModel->setHeaderData(idxPostage,     Qt::Horizontal, "POSTAGE",     Qt::DisplayRole);
    m_trackerModel->setHeaderData(idxCount,       Qt::Horizontal, "COUNT",       Qt::DisplayRole);
    m_trackerModel->setHeaderData(idxAvgRate,     Qt::Horizontal, "AVG RATE",    Qt::DisplayRole);
    m_trackerModel->setHeaderData(idxMailClass,   Qt::Horizontal, "MAIL CLASS",  Qt::DisplayRole);
    m_trackerModel->setHeaderData(idxShape,       Qt::Horizontal, "SHAPE",       Qt::DisplayRole);
    m_trackerModel->setHeaderData(idxPermit,      Qt::Horizontal, "PERMIT",      Qt::DisplayRole);
    m_trackerModel->setHeaderData(idxDate,        Qt::Horizontal, "DATE",        Qt::DisplayRole);

    // View configuration mirroring TMTermController
    m_trackerView->horizontalHeader()->setVisible(true);
    m_trackerView->verticalHeader()->setVisible(false);
    m_trackerView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_trackerView->setAlternatingRowColors(true);
    m_trackerView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_trackerView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_trackerView->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void TMFarmController::refreshTracker(const QString &jobNumber)
{
    if (!m_trackerModel)
        return;

    // Correct filter column (preserved fix)
    m_trackerModel->setFilter(QString("job='%1'").arg(jobNumber));
    m_trackerModel->select();
}
