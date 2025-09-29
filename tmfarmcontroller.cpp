#include "tmfarmcontroller.h"
#include "tmfarmdbmanager.h"

#include <QSqlRecord>
#include <QSqlError>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QFont>
#include <QFontMetrics>
#include <QVector>
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

    // Use TMFarmDBManager’s database
    m_trackerModel = std::make_unique<QSqlTableModel>(this, TMFarmDBManager::instance()->getDatabase());
    m_trackerModel->setTable(QStringLiteral("tm_farm_log"));
    m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_trackerModel->select();
    m_trackerView->setModel(m_trackerModel.get());
}

int TMFarmController::computeOptimalFontSize() const
{
    // Calculate optimal font size - START BIGGER (matches TMTERM)
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

    if (idxJob >= 0)         m_trackerModel->setHeaderData(idxJob,         Qt::Horizontal, QObject::tr("JOB"),         Qt::DisplayRole);
    if (idxDescription >= 0) m_trackerModel->setHeaderData(idxDescription, Qt::Horizontal, QObject::tr("DESCRIPTION"), Qt::DisplayRole);
    if (idxPostage >= 0)     m_trackerModel->setHeaderData(idxPostage,     Qt::Horizontal, QObject::tr("POSTAGE"),     Qt::DisplayRole);
    if (idxCount >= 0)       m_trackerModel->setHeaderData(idxCount,       Qt::Horizontal, QObject::tr("COUNT"),       Qt::DisplayRole);
    if (idxAvgRate >= 0)     m_trackerModel->setHeaderData(idxAvgRate,     Qt::Horizontal, QObject::tr("AVG RATE"),    Qt::DisplayRole);
    if (idxMailClass >= 0)   m_trackerModel->setHeaderData(idxMailClass,   Qt::Horizontal, QObject::tr("CLASS"),       Qt::DisplayRole);
    if (idxShape >= 0)       m_trackerModel->setHeaderData(idxShape,       Qt::Horizontal, QObject::tr("SHAPE"),       Qt::DisplayRole);
    if (idxPermit >= 0)      m_trackerModel->setHeaderData(idxPermit,      Qt::Horizontal, QObject::tr("PERMIT"),      Qt::DisplayRole);
}

void TMFarmController::enforceVisibilityMask()
{
    if (!m_trackerModel || !m_trackerView) return;

    // Show ONLY columns 1..8, hide everything else
    const int total = m_trackerModel->columnCount();
    for (int c = 0; c < total; ++c) {
        const bool shouldShow = (c >= 1 && c <= 8);
        m_trackerView->setColumnHidden(c, !shouldShow);
    }

    // Extra safety: hide DATE by name if present (even if not in 1..8)
    const int idxDate = m_trackerModel->fieldIndex(QStringLiteral("date"));
    if (idxDate >= 0) m_trackerView->setColumnHidden(idxDate, true);
}

void TMFarmController::applyFixedColumnWidths()
{
    if (!m_trackerModel || !m_trackerView) return;

    // Calculate optimal font size and column widths (EXACT MATCH to TMTERM)
    const int tableWidth = 611; // Fixed widget width from UI
    const int borderWidth = 2;   // Account for table borders
    const int availableWidth = tableWidth - borderWidth;

    // Define maximum content widths based on TMTERM data format
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
        {"CLASS", "STD", 60},
        {"SHAPE", "LTR", 33},
        {"PERMIT", "NKLN", 36}
    };

    // Calculate optimal font size - START BIGGER
    QFont testFont(QStringLiteral("Blender Pro Bold"), 7);
    QFontMetrics fm(testFont);

    int optimalFontSize = 7;
    for (int fontSize = 11; fontSize >= 7; fontSize--) {
        testFont.setPointSize(fontSize);
        fm = QFontMetrics(testFont);

        int totalWidth = 0;
        bool fits = true;

        for (const auto& col : columns) {
            int headerWidth = fm.horizontalAdvance(col.header) + 12; // Increased padding
            int contentWidth = fm.horizontalAdvance(col.maxContent) + 12; // Increased padding
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

    // Apply the optimal font
    QFont tableFont(QStringLiteral("Blender Pro Bold"), optimalFontSize);
    m_trackerView->setFont(tableFont);

    // Calculate and set precise column widths
    fm = QFontMetrics(tableFont);
    for (int i = 0; i < columns.size(); i++) {
        const auto& col = columns[i];
        int headerWidth = fm.horizontalAdvance(col.header) + 12; // Increased padding
        int contentWidth = fm.horizontalAdvance(col.maxContent) + 12; // Increased padding
        int colWidth = qMax(headerWidth, qMax(contentWidth, col.minWidth));

        m_trackerView->setColumnWidth(i + 1, colWidth); // +1 because we hide column 0
    }

    // Disable horizontal header resize to maintain fixed widths
    m_trackerView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    // Enable only vertical scrolling
    m_trackerView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_trackerView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Apply enhanced styling for better readability (matches TMTERM)
    m_trackerView->setStyleSheet(
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
}

void TMFarmController::setupOptimizedTableLayout()
{
    if (!m_trackerModel || !m_trackerView)
        return;

    m_trackerModel->setSort(0, Qt::DescendingOrder);
    m_trackerModel->select();

    // Apply headers, widths, and STRICT visibility (only 1..8 visible) — in this order
    applyHeaderLabels();
    applyFixedColumnWidths();
    enforceVisibilityMask(); // LAST: guarantees hidden columns stay hidden

    // View behavior
    m_trackerView->horizontalHeader()->setVisible(true);
    m_trackerView->verticalHeader()->setVisible(false);
    m_trackerView->setAlternatingRowColors(true);
    m_trackerView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_trackerView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_trackerView->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void TMFarmController::refreshTracker(const QString &jobNumber)
{
    if (!m_trackerModel)
        return;

    m_trackerModel->setFilter(QStringLiteral("job='%1'").arg(jobNumber));
    m_trackerModel->setSort(0, Qt::DescendingOrder);
    m_trackerModel->select();

    // Re-apply headers/widths and visibility mask after select
    applyHeaderLabels();
    applyFixedColumnWidths();
    enforceVisibilityMask();
}
