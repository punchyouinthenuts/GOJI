#include "tmcaemaildialog.h"
#include "logger.h"

#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QHeaderView>
#include <QDrag>
#include <QMimeData>
#include <QUrl>

// ---------------------------------------------------------------------------
// Local subclass: overrides startDrag() so Outlook accepts file drops
// ---------------------------------------------------------------------------
class TMCAEmailFileListWidget : public QListWidget
{
public:
    explicit TMCAEmailFileListWidget(QWidget* parent = nullptr)
        : QListWidget(parent) {}

protected:
    void startDrag(Qt::DropActions /*supportedActions*/) override
    {
        QList<QListWidgetItem*> items = selectedItems();
        if (items.isEmpty()) return;

        QList<QUrl> urlList;
        for (QListWidgetItem* item : items) {
            const QString path = item->data(Qt::UserRole).toString();
            if (!path.isEmpty())
                urlList.append(QUrl::fromLocalFile(path));
        }
        if (urlList.isEmpty()) return;

        QMimeData* mimeData = new QMimeData;
        mimeData->setUrls(urlList);

        QDrag* drag = new QDrag(this);
        drag->setMimeData(mimeData);
        drag->exec(Qt::CopyAction);
    }
};

TMCAEmailDialog::TMCAEmailDialog(
    const QString&     jobNumber,
    const QString&     jobType,
    int                laValidCount,
    int                saValidCount,
    double             laPostage,
    double             saPostage,
    double             rate,
    const QString&     nasDest,
    const QStringList& mergedFiles,
    QWidget*           parent)
    : QDialog(parent)
    , m_mainLayout(nullptr)
    , m_headerLabel(nullptr)
    , m_subHeaderLabel(nullptr)
    , m_postageTable(nullptr)
    , m_copyPostageButton(nullptr)
    , m_nasDestHeaderLabel(nullptr)
    , m_nasDestLabel(nullptr)
    , m_copyNasDestButton(nullptr)
    , m_filesLabel(nullptr)
    , m_fileList(nullptr)
    , m_helpLabel(nullptr)
    , m_closeButton(nullptr)
    , m_jobNumber(jobNumber)
    , m_jobType(jobType)
    , m_laValidCount(laValidCount)
    , m_saValidCount(saValidCount)
    , m_laPostage(laPostage)
    , m_saPostage(saPostage)
    , m_rate(rate)
    , m_nasDest(nasDest)
    , m_mergedFiles(mergedFiles)
    , m_closeInitiated(false)
{
    setWindowTitle(QString("Email Integration - TM CA %1").arg(jobType));
    setFixedSize(700, 580);
    setModal(true);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);

    setupUI();
    populateFileList();

    Logger::instance().info(QString("TMCAEmailDialog created for job %1 type %2")
                            .arg(jobNumber, jobType));
}

TMCAEmailDialog::~TMCAEmailDialog()
{
    Logger::instance().info("TMCAEmailDialog destroyed");
}

// ============================================================
// UI construction
// ============================================================

void TMCAEmailDialog::setupUI()
{
    const QFont boldFont("Blender Pro Bold", 13, QFont::Bold);
    const QFont normalFont("Blender Pro", 11);
    const QFont smallFont("Blender Pro", 10);

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(12);
    m_mainLayout->setContentsMargins(24, 20, 24, 20);

    // ---- Header ----
    m_headerLabel = new QLabel(
        QString("DRAG & DROP MERGED FILES INTO YOUR E-MAIL"), this);
    m_headerLabel->setFont(QFont("Blender Pro Bold", 15, QFont::Bold));
    m_headerLabel->setAlignment(Qt::AlignCenter);
    m_headerLabel->setStyleSheet("color: #2c3e50;");
    m_mainLayout->addWidget(m_headerLabel);

    m_subHeaderLabel = new QLabel(
        QString("TM CA %1 — JOB %2").arg(m_jobType, m_jobNumber), this);
    m_subHeaderLabel->setFont(QFont("Blender Pro Bold", 13, QFont::Bold));
    m_subHeaderLabel->setAlignment(Qt::AlignCenter);
    m_subHeaderLabel->setStyleSheet("color: #2c3e50; margin-bottom: 6px;");
    m_mainLayout->addWidget(m_subHeaderLabel);

    // ---- Horizontal rule ----
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    m_mainLayout->addWidget(line);

    // ---- Postage table ----
    static const QStringList kHeaders = {
        "JOB", "DESCRIPTION", "POSTAGE", "COUNT", "AVG RATE", "CLASS", "SHAPE", "PERMIT"
    };
    const int COL_JOB  = 0, COL_DESC = 1, COL_POST = 2, COL_CNT = 3;
    const int COL_AVG  = 4, COL_CLS  = 5, COL_SHP  = 6, COL_PRM = 7;

    m_postageTable = new QTableWidget(3, 8, this);
    m_postageTable->setHorizontalHeaderLabels(kHeaders);
    m_postageTable->setFont(normalFont);
    m_postageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_postageTable->setSortingEnabled(false);
    m_postageTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_postageTable->setAlternatingRowColors(true);
    m_postageTable->verticalHeader()->setVisible(true);
    m_postageTable->horizontalHeader()->setFont(QFont("Blender Pro Bold", 10, QFont::Bold));
    m_postageTable->horizontalHeader()->setStretchLastSection(true);
    m_postageTable->setFixedHeight(110);

    // Helper: create a read-only, center-aligned cell item
    auto makeCell = [](const QString& text) -> QTableWidgetItem* {
        QTableWidgetItem* it = new QTableWidgetItem(text);
        it->setTextAlignment(Qt::AlignCenter);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        return it;
    };

    const double totalPostage = m_laPostage + m_saPostage;
    const int    totalCount   = m_laValidCount + m_saValidCount;
    const double avgRate      = (totalCount > 0)
                                    ? (totalPostage / totalCount)
                                    : 0.0;
    const double laAvgRate    = (m_laValidCount > 0)
                                    ? (m_laPostage / m_laValidCount)
                                    : 0.0;
    const double saAvgRate    = (m_saValidCount > 0)
                                    ? (m_saPostage / m_saValidCount)
                                    : 0.0;

    // Row 0 — LA
    m_postageTable->setItem(0, COL_JOB,  makeCell(m_jobNumber));
    m_postageTable->setItem(0, COL_DESC, makeCell("LA"));
    m_postageTable->setItem(0, COL_POST, makeCell(QString("$%1").arg(m_laPostage, 0, 'f', 2)));
    m_postageTable->setItem(0, COL_CNT,  makeCell(QString::number(m_laValidCount)));
    m_postageTable->setItem(0, COL_AVG,  makeCell(QString("%1").arg(laAvgRate, 0, 'f', 3)));
    m_postageTable->setItem(0, COL_CLS,  makeCell(""));
    m_postageTable->setItem(0, COL_SHP,  makeCell(""));
    m_postageTable->setItem(0, COL_PRM,  makeCell(""));

    // Row 1 — SA
    m_postageTable->setItem(1, COL_JOB,  makeCell(m_jobNumber));
    m_postageTable->setItem(1, COL_DESC, makeCell("SA"));
    m_postageTable->setItem(1, COL_POST, makeCell(QString("$%1").arg(m_saPostage, 0, 'f', 2)));
    m_postageTable->setItem(1, COL_CNT,  makeCell(QString::number(m_saValidCount)));
    m_postageTable->setItem(1, COL_AVG,  makeCell(QString("%1").arg(saAvgRate, 0, 'f', 3)));
    m_postageTable->setItem(1, COL_CLS,  makeCell(""));
    m_postageTable->setItem(1, COL_SHP,  makeCell(""));
    m_postageTable->setItem(1, COL_PRM,  makeCell(""));

    // Row 2 — TOTAL (display-only; not saved to DB)
    m_postageTable->setItem(2, COL_JOB,  makeCell(""));
    m_postageTable->setItem(2, COL_DESC, makeCell("TOTAL"));
    m_postageTable->setItem(2, COL_POST, makeCell(QString("$%1").arg(totalPostage, 0, 'f', 2)));
    m_postageTable->setItem(2, COL_CNT,  makeCell(QString::number(totalCount)));
    m_postageTable->setItem(2, COL_AVG,  makeCell(QString("%1").arg(avgRate, 0, 'f', 3)));
    m_postageTable->setItem(2, COL_CLS,  makeCell(""));
    m_postageTable->setItem(2, COL_SHP,  makeCell(""));
    m_postageTable->setItem(2, COL_PRM,  makeCell(""));

    // Bold the TOTAL row
    QFont totalFont("Blender Pro Bold", 11, QFont::Bold);
    for (int c = 0; c < 8; ++c) {
        if (m_postageTable->item(2, c))
            m_postageTable->item(2, c)->setFont(totalFont);
    }

    m_mainLayout->addWidget(m_postageTable);

    // ---- COPY button for postage table ----
    {
        QHBoxLayout* copyRow = new QHBoxLayout();
        copyRow->addStretch();
        m_copyPostageButton = new QPushButton("COPY", this);
        m_copyPostageButton->setFont(QFont("Blender Pro Bold", 10, QFont::Bold));
        m_copyPostageButton->setFixedSize(80, 28);
        m_copyPostageButton->setStyleSheet(
            "QPushButton {"
            "  background-color: #3d8eb9;"
            "  color: white;"
            "  border: none;"
            "  border-radius: 4px;"
            "}"
            "QPushButton:hover   { background-color: #2e7aa8; }"
            "QPushButton:pressed { background-color: #256690; }"
        );
        copyRow->addWidget(m_copyPostageButton);
        m_mainLayout->addLayout(copyRow);
    }
    connect(m_copyPostageButton, &QPushButton::clicked,
            this, &TMCAEmailDialog::onCopyPostageClicked);

    // ---- NAS DEST section ----
    {
        QFrame* nasFrame = new QFrame(this);
        nasFrame->setFrameShape(QFrame::StyledPanel);
        nasFrame->setFrameShadow(QFrame::Sunken);
        nasFrame->setStyleSheet(
            "QFrame { background-color: #f8f9fa;"
            "         border: 1px solid #ced4da;"
            "         border-radius: 4px; }"
        );

        QVBoxLayout* nasLayout = new QVBoxLayout(nasFrame);
        nasLayout->setContentsMargins(10, 6, 10, 6);
        nasLayout->setSpacing(4);

        // Header row: label + COPY button
        QHBoxLayout* nasHeaderRow = new QHBoxLayout();
        nasHeaderRow->setSpacing(8);

        m_nasDestHeaderLabel = new QLabel("NAS DEST", nasFrame);
        m_nasDestHeaderLabel->setFont(QFont("Blender Pro Bold", 10, QFont::Bold));
        m_nasDestHeaderLabel->setStyleSheet("color: #555555; background: transparent; border: none;");
        nasHeaderRow->addWidget(m_nasDestHeaderLabel);
        nasHeaderRow->addStretch();

        m_copyNasDestButton = new QPushButton("COPY", nasFrame);
        m_copyNasDestButton->setFont(QFont("Blender Pro Bold", 10, QFont::Bold));
        m_copyNasDestButton->setFixedSize(60, 24);
        m_copyNasDestButton->setStyleSheet(
            "QPushButton {"
            "  background-color: #3d8eb9;"
            "  color: white;"
            "  border: none;"
            "  border-radius: 3px;"
            "}"
            "QPushButton:hover   { background-color: #2e7aa8; }"
            "QPushButton:pressed { background-color: #256690; }"
        );
        nasHeaderRow->addWidget(m_copyNasDestButton);
        nasLayout->addLayout(nasHeaderRow);

        // Path label
        m_nasDestLabel = new QLabel(
            m_nasDest.isEmpty() ? "(none)" : m_nasDest, nasFrame);
        m_nasDestLabel->setFont(normalFont);
        m_nasDestLabel->setWordWrap(true);
        m_nasDestLabel->setStyleSheet("color: #1a1a1a; background: transparent; border: none;");
        nasLayout->addWidget(m_nasDestLabel);

        m_mainLayout->addWidget(nasFrame);
    }
    connect(m_copyNasDestButton, &QPushButton::clicked,
            this, &TMCAEmailDialog::onCopyNasDestClicked);

    // ---- Horizontal rule ----
    QFrame* line2 = new QFrame(this);
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);
    m_mainLayout->addWidget(line2);

    // ---- File list ----
    m_filesLabel = new QLabel("MERGED FILES  (drag into Outlook)", this);
    m_filesLabel->setFont(QFont("Blender Pro Bold", 11, QFont::Bold));
    m_filesLabel->setStyleSheet("color: #34495e;");
    m_mainLayout->addWidget(m_filesLabel);

    m_fileList = new TMCAEmailFileListWidget(this);
    m_fileList->setFont(normalFont);
    m_fileList->setDragEnabled(true);
    m_fileList->setDragDropMode(QAbstractItemView::DragOnly);
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileList->setMinimumHeight(120);
    m_fileList->setStyleSheet(
        "QListWidget {"
        "  border: 2px solid #bdc3c7;"
        "  border-radius: 6px;"
        "  background-color: white;"
        "  selection-background-color: #e3f2fd;"
        "}"
        "QListWidget::item { padding: 4px; }"
    );
    m_mainLayout->addWidget(m_fileList);

    m_helpLabel = new QLabel(
        "💡 Drag files from the list above directly into your Outlook email", this);
    m_helpLabel->setFont(smallFont);
    m_helpLabel->setStyleSheet("color: #666666;");
    m_helpLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(m_helpLabel);

    // ---- Close button ----
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_closeButton = new QPushButton("CLOSE", this);
    m_closeButton->setFont(QFont("Blender Pro Bold", 13, QFont::Bold));
    m_closeButton->setFixedSize(120, 40);
    m_closeButton->setEnabled(true);   // always enabled — no gating
    m_closeButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #6c757d;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 5px;"
        "}"
        "QPushButton:hover   { background-color: #5a6268; }"
        "QPushButton:pressed { background-color: #4e555b; }"
    );
    btnLayout->addWidget(m_closeButton);
    btnLayout->addStretch();
    m_mainLayout->addLayout(btnLayout);

    connect(m_closeButton, &QPushButton::clicked,
            this, &TMCAEmailDialog::onCloseClicked);
}

void TMCAEmailDialog::populateFileList()
{
    if (!m_fileList) return;

    if (m_mergedFiles.isEmpty()) {
        QListWidgetItem* item = new QListWidgetItem("No merged files available");
        item->setFlags(Qt::NoItemFlags);
        item->setForeground(QBrush(Qt::gray));
        m_fileList->addItem(item);
        return;
    }

    for (const QString& path : m_mergedFiles) {
        QFileInfo fi(path);
        QListWidgetItem* item = new QListWidgetItem(fi.fileName());
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        item->setIcon(m_iconProvider.icon(fi));
        m_fileList->addItem(item);
    }
}

// ============================================================
// Slots / event handlers
// ============================================================

void TMCAEmailDialog::onCopyPostageClicked()
{
    // Pre-compute the same values used to populate the table
    const double totalPostage = m_laPostage + m_saPostage;
    const int    totalCount   = m_laValidCount + m_saValidCount;
    const double laAvgRate    = (m_laValidCount > 0) ? (m_laPostage  / m_laValidCount) : 0.0;
    const double saAvgRate    = (m_saValidCount > 0) ? (m_saPostage  / m_saValidCount) : 0.0;
    const double avgRate      = (totalCount     > 0) ? (totalPostage / totalCount)     : 0.0;

    // --- Build HTML table ---
    QString html;
    html += "<table border=\"1\" cellspacing=\"0\" cellpadding=\"4\" "
            "style=\"border-collapse:collapse; font-family:Arial; font-size:11pt;\">";

    // Header row
    html += "<tr style=\"background-color:#dce6f1; font-weight:bold;\">";
    const QStringList headers = {
        "JOB", "DESCRIPTION", "POSTAGE", "COUNT", "AVG RATE", "CLASS", "SHAPE", "PERMIT"
    };
    for (const QString& h : headers)
        html += QString("<th>%1</th>").arg(h);
    html += "</tr>";

    // Row 0 — LA
    html += "<tr>";
    html += QString("<td align=\"center\">%1</td>").arg(m_jobNumber);
    html += "<td align=\"center\">LA</td>";
    html += QString("<td align=\"center\">$%1</td>").arg(m_laPostage,   0, 'f', 2);
    html += QString("<td align=\"center\">%1</td>") .arg(m_laValidCount);
    html += QString("<td align=\"center\">%1</td>") .arg(laAvgRate,    0, 'f', 3);
    html += "<td></td><td></td><td></td>";
    html += "</tr>";

    // Row 1 — SA
    html += "<tr>";
    html += QString("<td align=\"center\">%1</td>").arg(m_jobNumber);
    html += "<td align=\"center\">SA</td>";
    html += QString("<td align=\"center\">$%1</td>").arg(m_saPostage,   0, 'f', 2);
    html += QString("<td align=\"center\">%1</td>") .arg(m_saValidCount);
    html += QString("<td align=\"center\">%1</td>") .arg(saAvgRate,    0, 'f', 3);
    html += "<td></td><td></td><td></td>";
    html += "</tr>";

    // Row 2 — TOTAL
    html += "<tr style=\"font-weight:bold; background-color:#f2f2f2;\">";
    html += "<td></td>";
    html += "<td align=\"center\">TOTAL</td>";
    html += QString("<td align=\"center\">$%1</td>").arg(totalPostage, 0, 'f', 2);
    html += QString("<td align=\"center\">%1</td>") .arg(totalCount);
    html += QString("<td align=\"center\">%1</td>") .arg(avgRate,     0, 'f', 3);
    html += "<td></td><td></td><td></td>";
    html += "</tr>";

    html += "</table>";

    // --- Build plain-text fallback ---
    const QString plain =
        QString("JOB\tDESCRIPTION\tPOSTAGE\tCOUNT\tAVG RATE\tCLASS\tSHAPE\tPERMIT\n")
        + QString("%1\tLA\t$%2\t%3\t%4\t\t\t\n")
              .arg(m_jobNumber)
              .arg(m_laPostage,   0, 'f', 2)
              .arg(m_laValidCount)
              .arg(laAvgRate,    0, 'f', 3)
        + QString("%1\tSA\t$%2\t%3\t%4\t\t\t\n")
              .arg(m_jobNumber)
              .arg(m_saPostage,   0, 'f', 2)
              .arg(m_saValidCount)
              .arg(saAvgRate,    0, 'f', 3)
        + QString("\tTOTAL\t$%1\t%2\t%3\t\t\t\n")
              .arg(totalPostage, 0, 'f', 2)
              .arg(totalCount)
              .arg(avgRate,     0, 'f', 3);

    QMimeData* mime = new QMimeData;
    mime->setHtml(html);
    mime->setText(plain);
    QApplication::clipboard()->setMimeData(mime);
}

void TMCAEmailDialog::onCopyNasDestClicked()
{
    const QString path = m_nasDest.isEmpty() ? QString() : m_nasDest;
    QApplication::clipboard()->setText(path);
}

void TMCAEmailDialog::onCloseClicked()
{
    if (m_closeInitiated) return;  // already in progress
    m_closeInitiated = true;
    emit dialogClosed();
    accept();
}

void TMCAEmailDialog::closeEvent(QCloseEvent* event)
{
    // Block all non-button close paths (Alt-F4, X button, system close).
    // dialogClosed() is emitted exclusively from onCloseClicked().
    if (!m_closeInitiated) {
        event->ignore();
        return;
    }
    event->accept();
}
