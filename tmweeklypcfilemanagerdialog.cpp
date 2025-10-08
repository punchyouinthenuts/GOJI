#include "tmweeklypcfilemanagerdialog.h"
#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QScreen>
#include <QGuiApplication>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QListWidgetItem>
#include <QPixmap>
#include <QIcon>
#include <QTimer>
#include <QDataStream>
#include <QDebug>

TMWeeklyPCFileManagerDialog::TMWeeklyPCFileManagerDialog(const QString& proofPath,
                                                         const QString& outputPath,
                                                         QWidget* parent)
    : QDialog(parent), m_proofPath(proofPath), m_outputPath(outputPath)
{
    setWindowTitle("Weekly Merged Files");
    setModal(true);
    setFixedSize(650, 550);  // Vertically stacked layout
    
    setupUI();
    populateFileLists();
    
    // Center on screen
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        move(screen->geometry().center() - rect().center());
    }
    
    // Set focus to close button
    m_closeButton->setFocus();
}

void TMWeeklyPCFileManagerDialog::setupUI()
{
    // Main layout with compact margins
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    
    // Header label
    m_headerLabel = new QLabel("DRAG AND DROP THE PROOF PDF FILE & THE MERGED CSV FILE TO THE EMAIL", this);
    QFont headerFont("Blender Pro", 18, QFont::Bold);
    // Fallback to Arial if Blender Pro is not available
    if (!QFontDatabase::hasFamily("Blender Pro")) {
        headerFont.setFamily("Arial");
    }
    m_headerLabel->setFont(headerFont);
    m_headerLabel->setAlignment(Qt::AlignCenter);
    m_headerLabel->setWordWrap(true);
    m_headerLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_headerLabel->setStyleSheet(
        "QLabel {"
        "   color: #333333;"
        "   background-color: #f8f9fa;"
        "   border: 2px solid #dee2e6;"
        "   border-radius: 6px;"
        "   padding: 8px;"
        "   margin-bottom: 5px;"
        "}"
    );
    mainLayout->addWidget(m_headerLabel);
    
    // PROOF folder section (stacked vertically)
    QLabel* proofTitle = new QLabel("PROOF Folder", this);
    proofTitle->setFont(QFont("Arial", 11, QFont::Bold));
    proofTitle->setAlignment(Qt::AlignCenter);
    proofTitle->setStyleSheet("color: #495057; margin-bottom: 3px;");
    mainLayout->addWidget(proofTitle);
    
    m_proofFileList = new TMWeeklyPCDragDropListWidget(m_proofPath, this);
    m_proofFileList->setMaximumHeight(200);
    m_proofFileList->setStyleSheet(
        "QListWidget {"
        "   border: 1px solid #ced4da;"
        "   border-radius: 4px;"
        "   background-color: #ffffff;"
        "   alternate-background-color: #f8f9fa;"
        "}"
        "QListWidget::item {"
        "   padding: 6px;"
        "   border-bottom: 1px solid #e9ecef;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: #0078d4;"
        "   color: white;"
        "}"
        "QListWidget::item:hover {"
        "   background-color: #e3f2fd;"
        "}"
    );
    mainLayout->addWidget(m_proofFileList);
    
    // Add spacing between the two sections
    mainLayout->addSpacing(10);
    
    // OUTPUT folder section (stacked vertically)
    QLabel* outputTitle = new QLabel("OUTPUT Folder", this);
    outputTitle->setFont(QFont("Arial", 11, QFont::Bold));
    outputTitle->setAlignment(Qt::AlignCenter);
    outputTitle->setStyleSheet("color: #495057; margin-bottom: 3px;");
    mainLayout->addWidget(outputTitle);
    
    m_outputFileList = new TMWeeklyPCDragDropListWidget(m_outputPath, this);
    m_outputFileList->setMaximumHeight(200);
    m_outputFileList->setStyleSheet(
        "QListWidget {"
        "   border: 1px solid #ced4da;"
        "   border-radius: 4px;"
        "   background-color: #ffffff;"
        "   alternate-background-color: #f8f9fa;"
        "}"
        "QListWidget::item {"
        "   padding: 6px;"
        "   border-bottom: 1px solid #e9ecef;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: #0078d4;"
        "   color: white;"
        "}"
        "QListWidget::item:hover {"
        "   background-color: #e3f2fd;"
        "}"
    );
    mainLayout->addWidget(m_outputFileList);
    
    // Help text - compact
    QLabel* helpLabel = new QLabel("💡 Drag files directly into Outlook to attach them", this);
    helpLabel->setFont(QFont("Arial", 9));
    helpLabel->setStyleSheet("color: #6c757d; font-style: italic;");
    helpLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(helpLabel);
    
    // Close button - centered and compact
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_closeButton = new QPushButton("CLOSE", this);
    m_closeButton->setFont(QFont("Arial", 10, QFont::Bold));
    m_closeButton->setFixedSize(80, 30);
    m_closeButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #6c757d;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #5a6268;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #4e555b;"
        "}"
    );
    buttonLayout->addWidget(m_closeButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
    
    // Connect signals
    connect(m_closeButton, &QPushButton::clicked, this, &TMWeeklyPCFileManagerDialog::onCloseClicked);
}

void TMWeeklyPCFileManagerDialog::populateFileLists()
{
    populateFileList(m_proofFileList, m_proofPath);
    populateFileList(m_outputFileList, m_outputPath);
}

void TMWeeklyPCFileManagerDialog::populateFileList(QListWidget* listWidget, const QString& directoryPath)
{
    listWidget->clear();

    QDir dir(directoryPath);
    if (!dir.exists()) {
        QListWidgetItem* noFilesItem = new QListWidgetItem("Directory not found");
        noFilesItem->setFlags(Qt::NoItemFlags);
        noFilesItem->setForeground(QBrush(Qt::gray));
        listWidget->addItem(noFilesItem);
        return;
    }

    // Get all files from the directory
    QStringList filters;
    filters << "*.csv" << "*.xlsx" << "*.pdf" << "*.txt" << "*.zip";
    dir.setNameFilters(filters);

    QFileInfoList fileInfos = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

    if (fileInfos.isEmpty()) {
        QListWidgetItem* noFilesItem = new QListWidgetItem("No files found");
        noFilesItem->setFlags(Qt::NoItemFlags);
        noFilesItem->setForeground(QBrush(Qt::gray));
        listWidget->addItem(noFilesItem);
        return;
    }

    // Add files with icons using index-based iteration
    for (int i = 0; i < fileInfos.size(); ++i) {
        const QFileInfo& fileInfo = fileInfos.at(i);

        QListWidgetItem* item = new QListWidgetItem(fileInfo.fileName());

        // Set file type icon
        QIcon fileIcon = m_iconProvider.icon(fileInfo);
        if (!fileIcon.isNull()) {
            item->setIcon(fileIcon);
        }

        // Add file size as tooltip
        QString sizeText = QString::number(fileInfo.size() / 1024.0, 'f', 1) + " KB";
        item->setToolTip(QString("%1\n%2\nSize: %3")
                             .arg(fileInfo.fileName(),
                                  fileInfo.absoluteFilePath(),
                                  sizeText));

        listWidget->addItem(item);
    }
}

void TMWeeklyPCFileManagerDialog::onCloseClicked()
{
    accept();
}

// TMWeeklyPCDragDropListWidget implementation

TMWeeklyPCDragDropListWidget::TMWeeklyPCDragDropListWidget(const QString& folderPath, QWidget* parent)
    : QListWidget(parent), m_folderPath(folderPath)
{
    setDragEnabled(true);
    setDefaultDropAction(Qt::CopyAction);
    setDragDropMode(QAbstractItemView::DragOnly);
    setSelectionMode(QAbstractItemView::SingleSelection);
}

void TMWeeklyPCDragDropListWidget::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions)
    
    QListWidgetItem* item = currentItem();
    if (!item) {
        return;
    }
    
    QString fileName = item->text();
    QString filePath = QDir(m_folderPath).absoluteFilePath(fileName);
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        qDebug() << "File does not exist for drag:" << filePath;
        return;
    }
    
    // Create drag object
    QDrag* drag = new QDrag(this);
    QMimeData* mimeData = createOutlookMimeData(filePath);
    drag->setMimeData(mimeData);
    
    // Set drag icon
    QIcon fileIcon = m_iconProvider.icon(fileInfo);
    if (!fileIcon.isNull()) {
        drag->setPixmap(fileIcon.pixmap(32, 32));
    }
    
    qDebug() << "Starting drag for file:" << fileName << "Size:" << fileInfo.size() << "bytes";
    
    // Execute drag
    Qt::DropAction dropAction = drag->exec(Qt::CopyAction);
    Q_UNUSED(dropAction)
}

QMimeData* TMWeeklyPCDragDropListWidget::createOutlookMimeData(const QString& filePath) const
{
    QMimeData* mimeData = new QMimeData();
    
    // Grok's simplified approach: Use only URL-based MIME data
    // This mimics Windows Explorer behavior and creates proper file attachments in Outlook
    QUrl fileUrl = QUrl::fromLocalFile(filePath);
    mimeData->setUrls({ fileUrl });
    
    qDebug() << "Created simplified MIME data for" << QFileInfo(filePath).fileName() << "at" << fileUrl.toString();
    
    return mimeData;
}
