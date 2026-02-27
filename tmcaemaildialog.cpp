#include "tmcaemaildialog.h"
#include "logger.h"

#include <QFileInfo>
#include <QFont>
#include <QFrame>

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
    , m_jobLabel(nullptr)
    , m_jobTypeLabel(nullptr)
    , m_laCountLabel(nullptr)
    , m_saCountLabel(nullptr)
    , m_laPostageLabel(nullptr)
    , m_saPostageLabel(nullptr)
    , m_rateLabel(nullptr)
    , m_nasDestLabel(nullptr)
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

    // ---- Summary grid ----
    QGridLayout* grid = new QGridLayout();
    grid->setHorizontalSpacing(20);
    grid->setVerticalSpacing(6);

    auto makeCaption = [&](const QString& text) -> QLabel* {
        QLabel* l = new QLabel(text, this);
        l->setFont(QFont("Blender Pro Bold", 10, QFont::Bold));
        l->setStyleSheet("color: #555555;");
        return l;
    };
    auto makeValue = [&](const QString& text) -> QLabel* {
        QLabel* l = new QLabel(text, this);
        l->setFont(normalFont);
        l->setStyleSheet("color: #1a1a1a;");
        return l;
    };

    // Row 0: Job / Type
    grid->addWidget(makeCaption("JOB NUMBER"),  0, 0);
    m_jobLabel = makeValue(m_jobNumber);
    grid->addWidget(m_jobLabel, 0, 1);

    grid->addWidget(makeCaption("JOB TYPE"),    0, 2);
    m_jobTypeLabel = makeValue(m_jobType);
    grid->addWidget(m_jobTypeLabel, 0, 3);

    // Row 1: LA count / LA postage
    grid->addWidget(makeCaption("LA COUNT"),    1, 0);
    m_laCountLabel = makeValue(QString::number(m_laValidCount));
    grid->addWidget(m_laCountLabel, 1, 1);

    grid->addWidget(makeCaption("LA POSTAGE"),  1, 2);
    m_laPostageLabel = makeValue(QString("$%1").arg(m_laPostage, 0, 'f', 2));
    grid->addWidget(m_laPostageLabel, 1, 3);

    // Row 2: SA count / SA postage
    grid->addWidget(makeCaption("SA COUNT"),    2, 0);
    m_saCountLabel = makeValue(QString::number(m_saValidCount));
    grid->addWidget(m_saCountLabel, 2, 1);

    grid->addWidget(makeCaption("SA POSTAGE"),  2, 2);
    m_saPostageLabel = makeValue(QString("$%1").arg(m_saPostage, 0, 'f', 2));
    grid->addWidget(m_saPostageLabel, 2, 3);

    // Row 3: Rate / NAS destination
    grid->addWidget(makeCaption("RATE"),        3, 0);
    m_rateLabel = makeValue(QString("%1").arg(m_rate, 0, 'f', 3));
    grid->addWidget(m_rateLabel, 3, 1);

    grid->addWidget(makeCaption("NAS DEST"),    3, 2);
    m_nasDestLabel = makeValue(m_nasDest.isEmpty() ? "(none)" : m_nasDest);
    m_nasDestLabel->setWordWrap(true);
    grid->addWidget(m_nasDestLabel, 3, 3);

    m_mainLayout->addLayout(grid);

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

    m_fileList = new QListWidget(this);
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
