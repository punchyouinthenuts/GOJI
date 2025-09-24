// tmtermemaildialog.cpp
// 13% upscale + CLOSE always enabled
//
// Scaled constants (original → ×1.13 → new, rounded):
// - Window size: 600×500 → 678×565
// - Layout spacing: 15 → 16.95 → 17
// - Layout margins: 20 → 22.6 → 23
// - Header fonts: 14pt → 15.82 → 16pt
// - Header margins: 5px → 5.65 → 6px; 15px → 16.95 → 17px
// - "Network Path:" font: 12pt → 13.56 → 14pt
// - "Network Path:" margin-top: 10px → 11.3 → 11px
// - Path label font: 10pt → 11.3 → 11pt
// - Path label padding: 10px → 11.3 → 11px
// - Path label border radius: 8px → 9.04 → 9px
// - Path label border width: 2px → 2.26 → 2px
// - COPY button font: 12pt → 13.56 → 14pt
// - COPY button size: 80×40 → 90.4×45.2 → 90×45
// - "TERM Files:" font: 12pt → 13.56 → 14pt
// - "TERM Files:" margin-top: 15px → 16.95 → 17px
// - File list font: 10pt → 11.3 → 11pt
// - File list border radius: 8px → 9.04 → 9px
// - File list border width: 2px → 2.26 → 2px
// - Help label font: 10pt → 11.3 → 11pt
// - CLOSE button font: 12pt → 13.56 → 14pt
// - CLOSE button size: 100×35 → 113×39.55 → 113×40
// - CLOSE button border radius: 4px → 4.52 → 5px
//
// Note: No hardcoded icon sizes or row/delegate metrics exist in this file,
// so none were changed.

#include "tmtermemaildialog.h"
#include "tmhealthyemailfilelistwidget.h"
#include "logger.h"
#include <QCloseEvent>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QMessageBox>

// Static constants
const QString TMTermEmailDialog::DATA_DIR = "C:/Goji/TRACHMAR/TERM/DATA";
const QString TMTermEmailDialog::FONT_FAMILY = "Blender Pro";

TMTermEmailDialog::TMTermEmailDialog(const QString& networkPath, const QString& jobNumber, QWidget *parent)
    : QDialog(parent)
    , m_mainLayout(nullptr)
    , m_headerLabel1(nullptr)
    , m_headerLabel2(nullptr)
    , m_pathLabel(nullptr)
    , m_copyPathButton(nullptr)
    , m_fileList(nullptr)
    , m_closeButton(nullptr)
    , m_networkPath(networkPath)
    , m_jobNumber(jobNumber)
    , m_copyClicked(false)
    , m_fileClicked(false)
{
    setWindowTitle("Email Integration - TM TERM");
    setFixedSize(678, 565); // 600x500 → ×1.13
    setModal(true);

    // Keep current native title-bar behavior (no X button)
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);

    setupUI();
    populateFileList();
    updateCloseButtonState();

    Logger::instance().info("TMTermEmailDialog created");
}

TMTermEmailDialog::~TMTermEmailDialog()
{
    Logger::instance().info("TMTermEmailDialog destroyed");
}

void TMTermEmailDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(17);                  // 15 → ×1.13
    m_mainLayout->setContentsMargins(23, 23, 23, 23); // 20 → ×1.13

    // Header labels with Blender Pro Bold 16pt
    m_headerLabel1 = new QLabel("COPY THE NETWORK PATH AND PASTE INTO E-MAIL", this);
    m_headerLabel1->setFont(QFont(FONT_FAMILY + " Bold", 16, QFont::Bold)); // 14 → 16
    m_headerLabel1->setAlignment(Qt::AlignCenter);
    m_headerLabel1->setStyleSheet("color: #2c3e50; margin-bottom: 6px;"); // 5 → 6

    m_headerLabel2 = new QLabel("DRAG & DROP THE TERM FILES INTO THE E-MAIL", this);
    m_headerLabel2->setFont(QFont(FONT_FAMILY + " Bold", 16, QFont::Bold)); // 14 → 16
    m_headerLabel2->setAlignment(Qt::AlignCenter);
    m_headerLabel2->setStyleSheet("color: #2c3e50; margin-bottom: 17px;"); // 15 → 17

    m_mainLayout->addWidget(m_headerLabel1);
    m_mainLayout->addWidget(m_headerLabel2);

    // Path section with label and copy button
    QLabel* pathSectionLabel = new QLabel("Network Path:", this);
    pathSectionLabel->setFont(QFont(FONT_FAMILY + " Bold", 14, QFont::Bold)); // 12 → 14
    pathSectionLabel->setStyleSheet("color: #34495e; margin-top: 11px;"); // 10 → 11
    m_mainLayout->addWidget(pathSectionLabel);

    QHBoxLayout* pathLayout = new QHBoxLayout();

    m_pathLabel = new QLabel(m_networkPath, this);
    m_pathLabel->setFont(QFont(FONT_FAMILY, 11)); // 10 → 11
    m_pathLabel->setStyleSheet(
        "QLabel {"
        "   background-color: #f8f9fa;"
        "   border: 2px solid #bdc3c7;"   // 2 → 2 (rounded)
        "   border-radius: 9px;"          // 8 → 9
        "   padding: 11px;"               // 10 → 11
        "   color: #2c3e50;"
        "}"
    );
    m_pathLabel->setWordWrap(true);
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathLayout->addWidget(m_pathLabel, 1);

    m_copyPathButton = new QPushButton("COPY", this);
    m_copyPathButton->setFont(QFont(FONT_FAMILY + " Bold", 14, QFont::Bold)); // 12 → 14
    m_copyPathButton->setFixedSize(90, 45); // 80x40 → ×1.13
    m_copyPathButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #3498db;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 6px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: #2980b9;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #21618c;"
        "}"
    );
    pathLayout->addWidget(m_copyPathButton);
    m_mainLayout->addLayout(pathLayout);

    // Files section
    QLabel* filesLabel = new QLabel("TERM Files:", this);
    filesLabel->setFont(QFont(FONT_FAMILY + " Bold", 14, QFont::Bold)); // 12 → 14
    filesLabel->setStyleSheet("color: #34495e; margin-top: 17px;"); // 15 → 17
    m_mainLayout->addWidget(filesLabel);

    m_fileList = new TMHealthyEmailFileListWidget(this);
    m_fileList->setFont(QFont(FONT_FAMILY, 11)); // 10 → 11
    m_fileList->setStyleSheet(
        "QListWidget {"
        "   border: 2px solid #bdc3c7;" // 2 → 2
        "   border-radius: 9px;"        // 8 → 9
        "   background-color: white;"
        "   selection-background-color: #e3f2fd;"
        "}"
    );
    m_mainLayout->addWidget(m_fileList);

    // Help text
    QLabel* helpLabel = new QLabel("💡 Drag files from the list above directly into your Outlook email", this);
    helpLabel->setFont(QFont(FONT_FAMILY, 11)); // 10 → 11
    helpLabel->setStyleSheet("color: #666666; font-style: italic;");
    helpLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(helpLabel);

    // Close button
    QHBoxLayout* closeButtonLayout = new QHBoxLayout();
    closeButtonLayout->addStretch();

    m_closeButton = new QPushButton("CLOSE", this);
    m_closeButton->setFont(QFont(FONT_FAMILY + " Bold", 14, QFont::Bold)); // 12 → 14
    m_closeButton->setFixedSize(113, 40); // 100x35 → ×1.13
    m_closeButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #6c757d;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 5px;" // 4 → 5
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: #5a6268;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #4e555b;"
        "}"
        "QPushButton:disabled {"
        "   background-color: #cccccc;"
        "   color: #666666;"
        "}"
    );
    closeButtonLayout->addWidget(m_closeButton);
    closeButtonLayout->addStretch();
    m_mainLayout->addLayout(closeButtonLayout);

    // Connect signals
    connect(m_copyPathButton, &QPushButton::clicked, this, &TMTermEmailDialog::onCopyPathClicked);
    connect(m_fileList, &QListWidget::itemClicked, this, &TMTermEmailDialog::onFileClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &TMTermEmailDialog::onCloseClicked);
}

void TMTermEmailDialog::populateFileList()
{
    QString fileDirectory = getFileDirectory();
    QDir dir(fileDirectory);

    if (!dir.exists()) {
        QListWidgetItem* noFilesItem = new QListWidgetItem("No DATA directory found");
        noFilesItem->setFlags(Qt::NoItemFlags);
        noFilesItem->setForeground(QBrush(Qt::gray));
        m_fileList->addItem(noFilesItem);
        return;
    }

    // Job-aware filtering: look for specific job files first, then fallback to generic
    QStringList filters;
    if (!m_jobNumber.isEmpty()) {
        // Try job-specific pattern first (e.g., "12345 JAN PRESORTLIST_PRINT.csv")
        filters << QString("*%1*PRESORTLIST_PRINT.csv").arg(m_jobNumber);
    }
    // Always include these patterns as fallback
    filters << "*PRESORTLIST_PRINT.csv" << "FHK_TERM_UPDATED.xlsx";

    dir.setNameFilters(filters);
    QFileInfoList fileInfos = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

    if (fileInfos.isEmpty()) {
        QListWidgetItem* noFilesItem = new QListWidgetItem("No TERM output files found");
        noFilesItem->setFlags(Qt::NoItemFlags);
        noFilesItem->setForeground(QBrush(Qt::gray));
        m_fileList->addItem(noFilesItem);
        return;
    }

    for (const QFileInfo& fileInfo : fileInfos) {
        QString fileName = fileInfo.fileName();
        QString filePath = fileInfo.absoluteFilePath();

        QListWidgetItem* item = new QListWidgetItem(fileName);
        item->setData(Qt::UserRole, filePath);
        item->setToolTip(filePath);

        // Add file icon
        QIcon fileIcon = m_iconProvider.icon(fileInfo);
        if (!fileIcon.isNull()) {
            item->setIcon(fileIcon);
        }

        m_fileList->addItem(item);
    }
}

void TMTermEmailDialog::updateCloseButtonState()
{
    // CHANGE: CLOSE is always enabled; no gating on prior actions
    if (m_closeButton) {
        m_closeButton->setEnabled(true);
        m_closeButton->setToolTip("Click to close");
    }
}

QString TMTermEmailDialog::getFileDirectory()
{
    return DATA_DIR;
}

void TMTermEmailDialog::onCopyPathClicked()
{
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(m_networkPath);

    m_copyClicked = true;
    m_copyPathButton->setText("COPIED!");
    m_copyPathButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #27ae60;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 6px;"
        "   font-weight: bold;"
        "}"
    );

    updateCloseButtonState();

    Logger::instance().info("Network path copied to clipboard: " + m_networkPath);
}

void TMTermEmailDialog::onFileClicked()
{
    m_fileClicked = true;
    updateCloseButtonState();

    Logger::instance().info("File clicked - close button enabled");
}

void TMTermEmailDialog::onCloseClicked()
{
    accept();
}

void TMTermEmailDialog::closeEvent(QCloseEvent *event)
{
    // Allow close without restrictions (user can bypass the workflow if needed)
    event->accept();
}
