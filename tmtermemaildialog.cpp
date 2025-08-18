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
    setFixedSize(600, 500);
    setModal(true);
    
    // Remove the X button
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
    m_mainLayout->setSpacing(15);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Header labels with Blender Pro Bold 14pt
    m_headerLabel1 = new QLabel("COPY THE NETWORK PATH AND PASTE INTO E-MAIL", this);
    m_headerLabel1->setFont(QFont(FONT_FAMILY + " Bold", 14, QFont::Bold));
    m_headerLabel1->setAlignment(Qt::AlignCenter);
    m_headerLabel1->setStyleSheet("color: #2c3e50; margin-bottom: 5px;");
    
    m_headerLabel2 = new QLabel("DRAG & DROP THE TERM FILES INTO THE E-MAIL", this);
    m_headerLabel2->setFont(QFont(FONT_FAMILY + " Bold", 14, QFont::Bold));
    m_headerLabel2->setAlignment(Qt::AlignCenter);
    m_headerLabel2->setStyleSheet("color: #2c3e50; margin-bottom: 15px;");
    
    m_mainLayout->addWidget(m_headerLabel1);
    m_mainLayout->addWidget(m_headerLabel2);
    
    // Path section with label and copy button
    QLabel* pathSectionLabel = new QLabel("Network Path:", this);
    pathSectionLabel->setFont(QFont(FONT_FAMILY + " Bold", 12, QFont::Bold));
    pathSectionLabel->setStyleSheet("color: #34495e; margin-top: 10px;");
    m_mainLayout->addWidget(pathSectionLabel);
    
    QHBoxLayout* pathLayout = new QHBoxLayout();
    
    m_pathLabel = new QLabel(m_networkPath, this);
    m_pathLabel->setFont(QFont(FONT_FAMILY, 10));
    m_pathLabel->setStyleSheet(
        "QLabel {"
        "   background-color: #f8f9fa;"
        "   border: 2px solid #bdc3c7;"
        "   border-radius: 8px;"
        "   padding: 10px;"
        "   color: #2c3e50;"
        "}"
        );
    m_pathLabel->setWordWrap(true);
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathLayout->addWidget(m_pathLabel, 1);
    
    m_copyPathButton = new QPushButton("COPY", this);
    m_copyPathButton->setFont(QFont(FONT_FAMILY + " Bold", 12, QFont::Bold));
    m_copyPathButton->setFixedSize(80, 40);
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
    filesLabel->setFont(QFont(FONT_FAMILY + " Bold", 12, QFont::Bold));
    filesLabel->setStyleSheet("color: #34495e; margin-top: 15px;");
    m_mainLayout->addWidget(filesLabel);
    
    m_fileList = new TMHealthyEmailFileListWidget(this);
    m_fileList->setFont(QFont(FONT_FAMILY, 10));
    m_fileList->setStyleSheet(
        "QListWidget {"
        "   border: 2px solid #bdc3c7;"
        "   border-radius: 8px;"
        "   background-color: white;"
        "   selection-background-color: #e3f2fd;"
        "}"
        );
    m_mainLayout->addWidget(m_fileList);
    
    // Help text
    QLabel* helpLabel = new QLabel("💡 Drag files from the list above directly into your Outlook email", this);
    helpLabel->setFont(QFont(FONT_FAMILY, 10));
    helpLabel->setStyleSheet("color: #666666; font-style: italic;");
    helpLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(helpLabel);
    
    // Close button
    QHBoxLayout* closeButtonLayout = new QHBoxLayout();
    closeButtonLayout->addStretch();
    
    m_closeButton = new QPushButton("CLOSE", this);
    m_closeButton->setFont(QFont(FONT_FAMILY + " Bold", 12, QFont::Bold));
    m_closeButton->setFixedSize(100, 35);
    m_closeButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #6c757d;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 4px;"
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
    bool canClose = m_copyClicked && m_fileClicked;
    m_closeButton->setEnabled(canClose);
    
    if (!canClose) {
        QString tooltip = "Complete both actions to enable: ";
        QStringList remaining;
        if (!m_copyClicked) remaining << "Copy network path";
        if (!m_fileClicked) remaining << "Click a file";
        tooltip += remaining.join(", ");
        m_closeButton->setToolTip(tooltip);
    } else {
        m_closeButton->setToolTip("All actions completed - click to close");
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
