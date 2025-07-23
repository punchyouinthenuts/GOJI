#include "tmhealthynetworkdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFontMetrics>
#include <QScreen>
#include <QGuiApplication>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDrag>
#include <QMimeData>
#include <QUrl>
#include <QPainter>
#include <QDataStream>

TMHealthyNetworkDialog::TMHealthyNetworkDialog(const QString& networkPath,
                                               const QString& jobNumber,
                                               QWidget* parent)
    : QDialog(parent), m_networkPath(networkPath), m_jobNumber(jobNumber), m_isFallbackMode(false)
{
    m_fallbackPath = "C:/Users/JCox/Desktop/MOVE TO NETWORK DRIVE";
    
    // Check if we should use fallback mode
    m_isFallbackMode = checkFallbackMode();
    
    setWindowTitle("TM HEALTHY BEGINNINGS - Network Files");
    setModal(true);
    
    setupUI();
    populateFileList();
    calculateOptimalSize();
    
    // Center on screen
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        move(screen->geometry().center() - rect().center());
    }
    
    // Set focus to copy button for quick access
    m_copyPathButton->setFocus();
}

TMHealthyNetworkDialog* TMHealthyNetworkDialog::createDialog(const QString& networkPath,
                                                           const QString& jobNumber,
                                                           QWidget* parent)
{
    return new TMHealthyNetworkDialog(networkPath, jobNumber, parent);
}

void TMHealthyNetworkDialog::setupUI()
{
    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Title label
    QString titleText = m_isFallbackMode ? "Network Files (Fallback Mode)" : "Network Location and Files";
    m_titleLabel = new QLabel(titleText, this);
    m_titleLabel->setFont(QFont("Blender Pro", 16, QFont::Bold));
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet("color: #333333; margin-bottom: 10px;");
    mainLayout->addWidget(m_titleLabel);
    
    // Status label for fallback mode
    if (m_isFallbackMode) {
        m_statusLabel = new QLabel("⚠️ Network unavailable - Files copied to local fallback location", this);
        m_statusLabel->setFont(QFont("Blender Pro", 12, QFont::Bold));
        m_statusLabel->setAlignment(Qt::AlignCenter);
        m_statusLabel->setStyleSheet(
            "QLabel {"
            "   color: #d63384;"
            "   background-color: #f8d7da;"
            "   border: 1px solid #f1aeb5;"
            "   border-radius: 4px;"
            "   padding: 8px;"
            "   margin-bottom: 5px;"
            "}"
            );
        mainLayout->addWidget(m_statusLabel);
    }
    
    // Separator line
    QFrame* separator1 = new QFrame(this);
    separator1->setFrameShape(QFrame::HLine);
    separator1->setFrameShadow(QFrame::Sunken);
    separator1->setStyleSheet("border: 1px solid #cccccc;");
    mainLayout->addWidget(separator1);
    
    // Path section label
    QString pathLabelText = m_isFallbackMode ? "Intended Network Path:" : "Network Path:";
    QLabel* pathLabel = new QLabel(pathLabelText, this);
    pathLabel->setFont(QFont("Blender Pro", 12, QFont::Bold));
    pathLabel->setStyleSheet("color: #555555;");
    mainLayout->addWidget(pathLabel);
    
    // Path display - always show intended network path
    m_pathDisplay = new QTextEdit(this);
    m_pathDisplay->setPlainText(m_networkPath);
    m_pathDisplay->setFont(QFont("Consolas", 11));
    m_pathDisplay->setFixedHeight(60);
    m_pathDisplay->setReadOnly(true);
    QString pathDisplayStyle = m_isFallbackMode ?
        "QTextEdit {"
        "   border: 1px solid #f1aeb5;"
        "   border-radius: 4px;"
        "   padding: 8px;"
        "   background-color: #f8d7da;"
        "   selection-background-color: #007acc;"
        "   color: #721c24;"
        "}" :
        "QTextEdit {"
        "   border: 1px solid #ddd;"
        "   border-radius: 4px;"
        "   padding: 8px;"
        "   background-color: #f8f9fa;"
        "   selection-background-color: #007acc;"
        "}";
    m_pathDisplay->setStyleSheet(pathDisplayStyle);
    mainLayout->addWidget(m_pathDisplay);
    
    // Show fallback path if in fallback mode
    if (m_isFallbackMode) {
        QLabel* fallbackLabel = new QLabel("Actual File Location:", this);
        fallbackLabel->setFont(QFont("Blender Pro", 12, QFont::Bold));
        fallbackLabel->setStyleSheet("color: #555555; margin-top: 10px;");
        mainLayout->addWidget(fallbackLabel);
        
        QTextEdit* fallbackDisplay = new QTextEdit(this);
        fallbackDisplay->setPlainText(m_fallbackPath);
        fallbackDisplay->setFont(QFont("Consolas", 11));
        fallbackDisplay->setFixedHeight(60);
        fallbackDisplay->setReadOnly(true);
        fallbackDisplay->setStyleSheet(
            "QTextEdit {"
            "   border: 1px solid #0c5460;"
            "   border-radius: 4px;"
            "   padding: 8px;"
            "   background-color: #d1ecf1;"
            "   selection-background-color: #007acc;"
            "   color: #0c5460;"
            "}"
            );
        mainLayout->addWidget(fallbackDisplay);
    }
    
    // Copy path button
    QHBoxLayout* pathButtonLayout = new QHBoxLayout();
    pathButtonLayout->addStretch();
    
    m_copyPathButton = new QPushButton("COPY PATH", this);
    m_copyPathButton->setFont(QFont("Blender Pro Bold", 12, QFont::Bold));
    m_copyPathButton->setFixedSize(120, 35);
    m_copyPathButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #0078d4;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 4px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: #106ebe;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #005a9e;"
        "}"
        );
    pathButtonLayout->addWidget(m_copyPathButton);
    pathButtonLayout->addStretch();
    mainLayout->addLayout(pathButtonLayout);
    
    // Separator line
    QFrame* separator2 = new QFrame(this);
    separator2->setFrameShape(QFrame::HLine);
    separator2->setFrameShadow(QFrame::Sunken);
    separator2->setStyleSheet("border: 1px solid #cccccc;");
    mainLayout->addWidget(separator2);
    
    // File list section
    QLabel* filesLabel = new QLabel("MERGED Files (Drag to Outlook):", this);
    filesLabel->setFont(QFont("Blender Pro", 12, QFont::Bold));
    filesLabel->setStyleSheet("color: #555555;");
    mainLayout->addWidget(filesLabel);
    
    // Custom file list widget with drag-and-drop
    m_fileList = new TMHealthyFileListWidget(this);
    m_fileList->setFixedHeight(200);
    m_fileList->setStyleSheet(
        "QListWidget {"
        "   border: 1px solid #ddd;"
        "   border-radius: 4px;"
        "   background-color: white;"
        "   alternate-background-color: #f8f9fa;"
        "   selection-background-color: #0078d4;"
        "   selection-color: white;"
        "}"
        "QListWidget::item {"
        "   padding: 8px;"
        "   border-bottom: 1px solid #eee;"
        "}"
        "QListWidget::item:hover {"
        "   background-color: #e3f2fd;"
        "}"
        );
    mainLayout->addWidget(m_fileList);
    
    // Help text
    QLabel* helpLabel = new QLabel("💡 Drag files from the list above directly into your Outlook email", this);
    helpLabel->setFont(QFont("Blender Pro", 10));
    helpLabel->setStyleSheet("color: #666666; font-style: italic;");
    helpLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(helpLabel);
    
    // Close button
    QHBoxLayout* closeButtonLayout = new QHBoxLayout();
    closeButtonLayout->addStretch();
    
    m_closeButton = new QPushButton("CLOSE", this);
    m_closeButton->setFont(QFont("Blender Pro Bold", 12, QFont::Bold));
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
        );
    closeButtonLayout->addWidget(m_closeButton);
    closeButtonLayout->addStretch();
    mainLayout->addLayout(closeButtonLayout);
    
    // Connect signals
    connect(m_copyPathButton, &QPushButton::clicked, this, &TMHealthyNetworkDialog::onCopyPathClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &TMHealthyNetworkDialog::onCloseClicked);
}

void TMHealthyNetworkDialog::populateFileList()
{
    // Get the appropriate directory based on mode
    QString fileDirectory = getFileDirectory();
    QDir dir(fileDirectory);
    
    if (!dir.exists()) {
        // Add a message if directory doesn't exist
        QString message = m_isFallbackMode ? "No fallback directory found" : "No MERGED directory found";
        QListWidgetItem* noFilesItem = new QListWidgetItem(message);
        noFilesItem->setFlags(Qt::NoItemFlags); // Make it non-selectable
        noFilesItem->setForeground(QBrush(Qt::gray));
        m_fileList->addItem(noFilesItem);
        return;
    }
    
    // Get all files from the directory
    QStringList filters;
    filters << "*.csv" << "*.zip" << "*.xlsx" << "*.txt";
    dir.setNameFilters(filters);
    
    QFileInfoList fileInfos = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    
    if (fileInfos.isEmpty()) {
        // Add a message if no files found
        QString message = m_isFallbackMode ? "No files found in fallback directory" : "No files found in MERGED directory";
        QListWidgetItem* noFilesItem = new QListWidgetItem(message);
        noFilesItem->setFlags(Qt::NoItemFlags); // Make it non-selectable
        noFilesItem->setForeground(QBrush(Qt::gray));
        m_fileList->addItem(noFilesItem);
        return;
    }
    
    // Add each file to the list
    for (const QFileInfo& fileInfo : fileInfos) {
        QListWidgetItem* item = new QListWidgetItem();
        
        // Format: filename [size] [modified]
        QString displayText = fileInfo.fileName();
        QString sizeStr = QString("%1 KB").arg(fileInfo.size() / 1024);
        QString modifiedStr = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm");
        
        // Add fallback indicator for non-MERGED files
        QString statusIndicator = "";
        if (m_isFallbackMode) {
            statusIndicator = " [FALLBACK]";
        }
        
        item->setText(QString("%1%2\n%3 - Modified: %4")
                     .arg(displayText)
                     .arg(statusIndicator)
                     .arg(sizeStr)
                     .arg(modifiedStr));
        
        // Store full file path for drag operations
        item->setData(Qt::UserRole, fileInfo.absoluteFilePath());
        
        // Set tooltip with full path
        item->setToolTip(fileInfo.absoluteFilePath());
        
        // Color code fallback items
        if (m_isFallbackMode) {
            item->setForeground(QBrush(QColor(220, 53, 132))); // Bootstrap danger color
        }
        
        m_fileList->addItem(item);
    }
}

bool TMHealthyNetworkDialog::checkFallbackMode()
{
    // Check if fallback directory exists and has relevant files
    QDir fallbackDir(m_fallbackPath);
    if (!fallbackDir.exists()) {
        return false;
    }
    
    // Look for files that would be related to this job
    QStringList filters;
    filters << "*.csv" << "*.zip" << "*.xlsx" << "*.txt";
    fallbackDir.setNameFilters(filters);
    
    QFileInfoList files = fallbackDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Time);
    
    // Check if any files were recently created (within last hour) and contain job number or "HEALTHY"
    QDateTime recentThreshold = QDateTime::currentDateTime().addSecs(-3600); // 1 hour ago
    
    for (const QFileInfo& fileInfo : files) {
        if (fileInfo.lastModified() > recentThreshold) {
            QString fileName = fileInfo.fileName();
            if (fileName.contains(m_jobNumber, Qt::CaseInsensitive) || 
                fileName.contains("HEALTHY", Qt::CaseInsensitive) ||
                fileName.contains("MERGED", Qt::CaseInsensitive)) {
                return true;
            }
        }
    }
    
    return false;
}

QString TMHealthyNetworkDialog::getFileDirectory() const
{
    if (m_isFallbackMode) {
        return m_fallbackPath;
    } else {
        return "C:/Users/JCox/Desktop/AUTOMATION/TRACHMAR/HEALTHY BEGINNINGS/DATA/MERGED";
    }
}

void TMHealthyNetworkDialog::calculateOptimalSize()
{
    // Calculate based on content
    int minWidth = 500;
    int maxWidth = 800;
    
    // Calculate width needed for network path
    QFontMetrics fontMetrics(QFont("Consolas", 11));
    int pathWidth = fontMetrics.horizontalAdvance(m_networkPath);
    
    int optimalWidth = qMax(minWidth, qMin(maxWidth, pathWidth + 100));
    
    // Set fixed size
    resize(optimalWidth, 550);
}

void TMHealthyNetworkDialog::onCopyPathClicked()
{
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(m_networkPath);
    
    // Provide visual feedback
    m_copyPathButton->setText("COPIED!");
    m_copyPathButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #28a745;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 4px;"
        "   font-weight: bold;"
        "}"
        );
    
    // Reset button after 2 seconds
    QTimer::singleShot(2000, this, [this]() {
        m_copyPathButton->setText("COPY PATH");
        m_copyPathButton->setStyleSheet(
            "QPushButton {"
            "   background-color: #0078d4;"
            "   color: white;"
            "   border: none;"
            "   border-radius: 4px;"
            "   font-weight: bold;"
            "}"
            "QPushButton:hover {"
            "   background-color: #106ebe;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #005a9e;"
            "}"
            );
    });
}

void TMHealthyNetworkDialog::onCloseClicked()
{
    accept();
}

// TMHealthyFileListWidget implementation

TMHealthyFileListWidget::TMHealthyFileListWidget(QWidget* parent)
    : QListWidget(parent)
{
    setupDragDrop();
}

void TMHealthyFileListWidget::setupDragDrop()
{
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setDefaultDropAction(Qt::CopyAction);
    
    // Set selection mode to allow multiple files
    setSelectionMode(QAbstractItemView::ExtendedSelection);
}

void TMHealthyFileListWidget::startDrag(Qt::DropActions supportedActions)
{
    QList<QListWidgetItem*> selectedItems = this->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }
    
    QMimeData* mimeData = createMimeData(selectedItems);
    if (!mimeData) {
        return;
    }
    
    QDrag* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    
    // Set drag cursor
    QPixmap dragPixmap(32, 32);
    dragPixmap.fill(Qt::transparent);
    QPainter painter(&dragPixmap);
    painter.fillRect(0, 0, 32, 32, QBrush(QColor(0, 120, 212, 128)));
    painter.setPen(Qt::white);
    painter.drawText(dragPixmap.rect(), Qt::AlignCenter, QString::number(selectedItems.size()));
    drag->setPixmap(dragPixmap);
    
    drag->exec(supportedActions, Qt::CopyAction);
}

QMimeData* TMHealthyFileListWidget::createMimeData(const QList<QListWidgetItem*>& items) const
{
    QMimeData* mimeData = new QMimeData();
    QList<QUrl> urls;
    QStringList textList;
    
    for (QListWidgetItem* item : items) {
        QString filePath = item->data(Qt::UserRole).toString();
        if (!filePath.isEmpty() && QFile::exists(filePath)) {
            urls << QUrl::fromLocalFile(filePath);
            textList << QFileInfo(filePath).fileName();
        }
    }
    
    if (!urls.isEmpty()) {
        mimeData->setUrls(urls);
        mimeData->setText(textList.join(", "));
        
        // Add additional format for better Outlook compatibility
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream << urls;
        mimeData->setData("application/x-qt-windows-mime;value=\"FileGroupDescriptor\"", data);
    }
    
    return mimeData;
}
