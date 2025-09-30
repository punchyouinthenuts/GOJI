#include "tmfleremaildialog.h"
#include "tmfleremailfilelistwidget.h"
#include "logger.h"
#include <QCloseEvent>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QMessageBox>

// Static constants
const QString TMFLEREmailDialog::MERGED_DIR = "C:/Goji/TRACHMAR/FL ER/DATA";
const QString TMFLEREmailDialog::FONT_FAMILY = "Blender Pro";

TMFLEREmailDialog::TMFLEREmailDialog(const QString& networkPath, const QString& jobNumber, QWidget *parent)
    : QDialog(parent)
    , m_mainLayout(nullptr)
    , m_headerLabel1(nullptr)
    , m_fileList(nullptr)
    , m_closeButton(nullptr)
    , m_networkPath(networkPath)
    , m_jobNumber(jobNumber)
    , m_fileClicked(false)
{
    setWindowTitle("Email Integration – TM FL ER");
    setFixedSize(600, 450);
    setModal(true);
    
    // Remove the X button
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
    
    setupUI();
    populateFileList();
    updateCloseButtonState();
    
    Logger::instance().info("TMFLEREmailDialog created");
}

TMFLEREmailDialog::~TMFLEREmailDialog()
{
    Logger::instance().info("TMFLEREmailDialog destroyed");
}

void TMFLEREmailDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(15);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Header label with Blender Pro Bold 14pt
    m_headerLabel1 = new QLabel("DRAG & DROP THE MERGED CSV INTO THE E-MAIL", this);
    m_headerLabel1->setFont(QFont(FONT_FAMILY + " Bold", 14, QFont::Bold));
    m_headerLabel1->setAlignment(Qt::AlignCenter);
    m_headerLabel1->setStyleSheet("color: #2c3e50; margin-bottom: 15px;");
    
    m_mainLayout->addWidget(m_headerLabel1);
    
    // File list section
    QLabel* filesLabel = new QLabel("MERGED CSV File (drag into email):", this);
    filesLabel->setFont(QFont(FONT_FAMILY, 12, QFont::Bold));
    filesLabel->setStyleSheet("color: #34495e;");
    m_mainLayout->addWidget(filesLabel);
    
    m_fileList = new TMFLEREmailFileListWidget(this);
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
    QLabel* helpLabel = new QLabel("💡 Drag the merged CSV file directly into your Outlook email", this);
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
    connect(m_fileList, &QListWidget::itemClicked, this, &TMFLEREmailDialog::onFileClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &TMFLEREmailDialog::onCloseClicked);
}

void TMFLEREmailDialog::populateFileList()
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

    // Filter for CSV files containing "_MERGED" in the name
    QStringList filters;
    filters << "*_MERGED.csv";
    dir.setNameFilters(filters);

    QFileInfoList fileInfos = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

    if (fileInfos.isEmpty()) {
        QListWidgetItem* noFilesItem = new QListWidgetItem("No _MERGED CSV found");
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

void TMFLEREmailDialog::updateCloseButtonState()
{
    bool canClose = m_fileClicked;
    m_closeButton->setEnabled(canClose);
    
    if (!canClose) {
        m_closeButton->setToolTip("Click a file to enable close");
    } else {
        m_closeButton->setToolTip("Click to close");
    }
}

QString TMFLEREmailDialog::getFileDirectory()
{
    return MERGED_DIR;
}

void TMFLEREmailDialog::onFileClicked()
{
    m_fileClicked = true;
    updateCloseButtonState();
    
    Logger::instance().info("File clicked in list");
}

void TMFLEREmailDialog::onCloseClicked()
{
    if (m_closeButton->isEnabled()) {
        accept();
    }
}

void TMFLEREmailDialog::closeEvent(QCloseEvent *event)
{
    // Only allow close if file clicked
    if (m_fileClicked) {
        event->accept();
    } else {
        event->ignore();
        QMessageBox::information(this, "Action Required", 
            "Please click on the file in the list before closing.");
    }
}
