#include "tmfarmcontroller.h"
#include "tmfarmdbmanager.h"

#include <QSqlRecord>
#include <QSqlError>
#include <QHeaderView>
#include <QAbstractItemView>
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

    setupTrackerModel();
    setupOptimizedTableLayout();
}

void TMFarmController::setupTrackerModel()
{
    if (!m_trackerView)
        return;

    m_trackerModel = std::make_unique<QSqlTableModel>(this, TMFarmDBManager::instance()->getDatabase());
    m_trackerModel->setTable(QStringLiteral("tm_farm_log"));
    m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_trackerModel->select();
    m_trackerView->setModel(m_trackerModel.get());
}

void TMFarmController::setupOptimizedTableLayout()
{
    if (!m_trackerModel || !m_trackerView)
        return;

    m_trackerModel->setSort(0, Qt::DescendingOrder);
    m_trackerModel->select();

    // Headers 1..8 to mirror TMTERM
    m_trackerModel->setHeaderData(1, Qt::Horizontal, QObject::tr("JOB"),         Qt::DisplayRole);
    m_trackerModel->setHeaderData(2, Qt::Horizontal, QObject::tr("DESCRIPTION"), Qt::DisplayRole);
    m_trackerModel->setHeaderData(3, Qt::Horizontal, QObject::tr("POSTAGE"),     Qt::DisplayRole);
    m_trackerModel->setHeaderData(4, Qt::Horizontal, QObject::tr("COUNT"),       Qt::DisplayRole);
    m_trackerModel->setHeaderData(5, Qt::Horizontal, QObject::tr("AVG RATE"),    Qt::DisplayRole);
    m_trackerModel->setHeaderData(6, Qt::Horizontal, QObject::tr("CLASS"),       Qt::DisplayRole);
    m_trackerModel->setHeaderData(7, Qt::Horizontal, QObject::tr("SHAPE"),       Qt::DisplayRole);
    m_trackerModel->setHeaderData(8, Qt::Horizontal, QObject::tr("PERMIT"),      Qt::DisplayRole);

    auto *hdrH = m_trackerView->horizontalHeader();
    auto *hdrV = m_trackerView->verticalHeader();
    if (hdrH) hdrH->setVisible(true);
    if (hdrV) hdrV->setVisible(false);

    m_trackerView->setAlternatingRowColors(true);
    m_trackerView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_trackerView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_trackerView->setEditTriggers(QAbstractItemView::NoEditTriggers);

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    hdrH->setSectionResizeMode(QHeaderView::Stretch);
#else
    hdrH->setResizeMode(QHeaderView::Stretch);
#endif

    // Hide columns like TMTERM
    m_trackerView->setColumnHidden(0, true);
    const int idxDate = m_trackerModel->fieldIndex("date");
    if (idxDate >= 0) m_trackerView->setColumnHidden(idxDate, true);

    const int totalCols = m_trackerModel->columnCount();
    for (int c = 9; c < totalCols; ++c) {
        m_trackerView->setColumnHidden(c, true);
    }
}

void TMFarmController::refreshTracker(const QString &jobNumber)
{
    if (!m_trackerModel)
        return;

    m_trackerModel->setFilter(QStringLiteral("job='%1'").arg(jobNumber));
    m_trackerModel->setSort(0, Qt::DescendingOrder);
    m_trackerModel->select();

    // Reapply headers after select
    m_trackerModel->setHeaderData(1, Qt::Horizontal, QObject::tr("JOB"),         Qt::DisplayRole);
    m_trackerModel->setHeaderData(2, Qt::Horizontal, QObject::tr("DESCRIPTION"), Qt::DisplayRole);
    m_trackerModel->setHeaderData(3, Qt::Horizontal, QObject::tr("POSTAGE"),     Qt::DisplayRole);
    m_trackerModel->setHeaderData(4, Qt::Horizontal, QObject::tr("COUNT"),       Qt::DisplayRole);
    m_trackerModel->setHeaderData(5, Qt::Horizontal, QObject::tr("AVG RATE"),    Qt::DisplayRole);
    m_trackerModel->setHeaderData(6, Qt::Horizontal, QObject::tr("CLASS"),       Qt::DisplayRole);
    m_trackerModel->setHeaderData(7, Qt::Horizontal, QObject::tr("SHAPE"),       Qt::DisplayRole);
    m_trackerModel->setHeaderData(8, Qt::Horizontal, QObject::tr("PERMIT"),      Qt::DisplayRole);
}
