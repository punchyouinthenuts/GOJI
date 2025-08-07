#include "tmhealthyemaildialog.h"
#include "tmhealthyemailfilelistwidget.h"
#include "logger.h"
#include <QCloseEvent>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QMessageBox>

// Static constants
const QString TMHealthyEmailDialog::MERGED_DIR = "C:/Goji/TRACHMAR/HEALTHY BEGINNINGS/DATA/MERGED";
const QString TMHealthyEmailDialog::FONT_FAMILY = "Blender Pro";

TMHealthyEmailDialog::TMHealthyEmailDialog(const QString& networkPath, const QString& jobNumber, QWidget *parent)
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
    setWindowTitle("Email Integration - TM HEALTHY BEGINNINGS");
    setFixedSize(600, 500);
    setModal(true);
    
    // Remove the X button
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
    
    setupUI();
    populateFileList();
    updateCloseButtonState();
    
    Logger::instance().info("TMHealthyEmailDialog created");
}

TMHealthyEmailDialog::~TMHealthyEmailDialog()
{
    Logger::instance().info("TMHealthyEmailDialog destroyed");
}

void TMHealthyEmailDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(15);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Header labels with Blender Pro Bold 14pt
    m_headerLabel1 = new QLabel("COPY THE NETWORK PATH AND PASTE INTO E-MAIL", this);
    m_headerLabel1->setFont(QFont(FONT_FAMILY + " Bold", 14, QFont::Bold));
    m_headerLabel1->setAlignment(Qt::AlignCenter);
    m_headerLabel1->setStyleSheet("color: #2c3e50; margin-bottom: 5px;");
    
    m_headerLabel2 = new QLabel("DRAG & DROP THE MERGED LIST(S) INTO THE E-MAIL", this);
    m_headerLabel2->setFont(QFont(FONT_FAMILY + " Bold", 14, QFont::Bold));
    m_headerLabel2->setAlignment(Qt::AlignCenter);
    m_headerLabel2->setStyleSheet("color: #2c3e50; margin-bottom: 15px;");
    
    m_mainLayout->addWidget(m_headerLabel1);
    m_mainLayout->addWidget(m_headerLabel2);
    
    // Network path section
    QFrame* pathFrame = new QFrame(this);
    pathFrame->setFrameStyle(QFrame::Box);
    pathFrame->setStyleSheet("QFrame { border: 2px solid #bdc3c7; border-radius: 8px; background-color: #ecf0f1; padding: 10px; }");
    
    QVBoxLayout* pathLayout = new QVBoxLayout(pathFrame);
    
    QLabel* pathTitleLabel = new QLabel("Network Path:", this);
    pathTitleLabel->setFont(QFont(FONT_FAMILY, 12, QFont::Bold));
    pathTitleLabel->setStyleSheet("color: #34495e;");
    
    m_pathLabel = new QLabel(m_networkPath, this);
    m_pathLabel->setFont(QFont("Consolas", 8)); // Reduced from 10 to 8 (2 points smaller)
    m_pathLabel->setTextFormat(Qt::PlainText); // Preserve double-backslashes
    m_pathLabel->setStyleSheet("color: #2c3e50; background-color: white; padding: 8px; border: 1px solid #bdc3c7; border-radius: 4px;");
    m_pathLabel->setWordWrap(true);
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    
    m_copyPathButton = new QPushButton("COPY", this);
    m_copyPathButton->setFont(QFont(FONT_FAMILY + " Bold", 12, QFont::Bold));
    m_copyPathButton->setFixedSize(80, 35);
    m_copyPathButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #3498db;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 4px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: #2980b9;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #21618c;"
        "}"
        );
    
    QHBoxLayout* pathButtonLayout = new QHBoxLayout();
    pathButtonLayout->addWidget(pathTitleLabel);
    pathButtonLayout->addStretch();
    pathButtonLayout->addWidget(m_copyPathButton);
    
    pathLayout->addLayout(pathButtonLayout);
    pathLayout->addWidget(m_pathLabel);
    
    m_mainLayout->addWidget(pathFrame);
    
    // File list section
    QLabel* filesLabel = new QLabel("MERGED Files (drag into email):", this);
    filesLabel->setFont(QFont(FONT_FAMILY, 12, QFont::Bold));
    filesLabel->setStyleSheet("color: #34495e;");
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
    connect(m_copyPathButton, &QPushButton::clicked, this, &TMHealthyEmailDialog::onCopyPathClicked);
    connect(m_fileList, &QListWidget::itemClicked, this, &TMHealthyEmailDialog::onFileClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &TMHealthyEmailDialog::onCloseClicked);
}

void TMHealthyEmailDialog::populateFileList()
{
    QString fileDirectory = getFileDirectory();
    QDir dir(fileDirectory);

    if (!dir.exists()) {
        QListWidgetItem* noFilesItem = new QListWidgetItem("No MERGED directory found");
        noFilesItem->setFlags(Qt::NoItemFlags);
        noFilesItem->setForeground(QBrush(Qt::gray));
        m_fileList->addItem(noFilesItem);
        qDebug() << "⚠️ Directory does not exist:" << fileDirectory;
        return;
    }

    QStringList filters;
    filters << "*.csv" << "*.zip" << "*.xlsx" << "*.txt";
    dir.setNameFilters(filters);

    QFileInfoList fileInfos = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

    if (fileInfos.isEmpty()) {
        QListWidgetItem* noFilesItem = new QListWidgetItem("No files found in MERGED directory");
        noFilesItem->setFlags(Qt::NoItemFlags);
        noFilesItem->setForeground(QBrush(Qt::gray));
        m_fileList->addItem(noFilesItem);
        qDebug() << "📂 No matching files found in:" << fileDirectory;
        return;
    }

    for (const QFileInfo& fileInfo : fileInfos) {
        QString fileName = fileInfo.fileName();
        QString filePath = fileInfo.absoluteFilePath().trimmed();

        if (!fileInfo.exists()) {
            qDebug() << "❌ Skipping non-existent file:" << filePath;
            continue;
        }

        QListWidgetItem* item = new QListWidgetItem(fileName);
        item->setData(Qt::UserRole, filePath);
        item->setToolTip(filePath);

        QIcon fileIcon = m_iconProvider.icon(fileInfo);
        if (!fileIcon.isNull()) {
            item->setIcon(fileIcon);
        } else {
            qDebug() << "⚠️ No icon found for:" << filePath;
        }

        m_fileList->addItem(item);
        qDebug() << "✅ Added file to list:" << filePath;
    }
}

void TMHealthyEmailDialog::updateCloseButtonState()
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

QString TMHealthyEmailDialog::getFileDirectory()
{
    return MERGED_DIR;
}

void TMHealthyEmailDialog::onCopyPathClicked()
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
        "   border-radius: 4px;"
        "   font-weight: bold;"
        "}"
        );
    
    updateCloseButtonState();
    
    Logger::instance().info("Network path copied to clipboard: " + m_networkPath);
}

void TMHealthyEmailDialog::onFileClicked()
{
    m_fileClicked = true;
    updateCloseButtonState();
    
    Logger::instance().info("File clicked in list");
}

void TMHealthyEmailDialog::onCloseClicked()
{
    if (m_closeButton->isEnabled()) {
        accept();
    }
}

void TMHealthyEmailDialog::closeEvent(QCloseEvent *event)
{
    // Only allow close if both actions completed
    if (m_copyClicked && m_fileClicked) {
        event->accept();
    } else {
        event->ignore();
        QMessageBox::information(this, "Action Required", 
            "Please complete both actions before closing:\n"
            "1. Copy the network path\n"
            "2. Click on a file in the list");
    }
}
