#include "tmbrokennetworkdialog.h"
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

TMBrokenNetworkDialog::TMBrokenNetworkDialog(const QString& networkPath,
                                               const QString& jobNumber,
                                               QWidget* parent)
    : QDialog(parent), m_networkPath(networkPath), m_jobNumber(jobNumber), m_fileSelected(false)
{
    setWindowTitle("TRACHMAR BROKEN APPOINTMENTS - Network Files");
    setModal(true);
    
    // Initialize timer
    m_closeTimer = new QTimer(this);
    m_closeTimer->setSingleShot(true);
    m_closeTimer->setInterval(10000); // 10 seconds
    
    setupUI();
    populateFileList();
    calculateOptimalSize();
    
    // Center on screen
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        move(screen->geometry().center() - rect().center());
    }
    
    // Start timer to enable close button after 10 seconds
    m_closeTimer->start();
}

void TMBrokenNetworkDialog::setupUI()
{
    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Header label with word wrap after "DROP"
    m_headerLabel = new QLabel("REPLY ALL AND DRAG & DROP\nTHESE ZIP FILES INTO THE EMAIL", this);
    m_headerLabel->setFont(QFont("Blender Pro Bold", 18, QFont::Bold));
    m_headerLabel->setAlignment(Qt::AlignCenter);
    m_headerLabel->setStyleSheet("color: #2c3e50; margin-bottom: 15px;");
    m_headerLabel->setWordWrap(true);
    mainLayout->addWidget(m_headerLabel);

    // File list - ZIP files only from MERGED directory
    m_fileList = new TMBrokenFileListWidget(this);
    m_fileList->setMinimumHeight(200);
    m_fileList->setMaximumHeight(400);
    mainLayout->addWidget(m_fileList);

    // Connect file list selection
    connect(m_fileList, &QListWidget::itemClicked, this, &TMBrokenNetworkDialog::onFileClicked);

    // Close button (disabled initially)
    m_closeButton = new QPushButton("Close", this);
    m_closeButton->setEnabled(false);
    m_closeButton->setMinimumHeight(40);
    connect(m_closeButton, &QPushButton::clicked, this, &TMBrokenNetworkDialog::onCloseClicked);
    connect(m_closeTimer, &QTimer::timeout, this, &TMBrokenNetworkDialog::onTimerTimeout);
    
    mainLayout->addWidget(m_closeButton);
}

void TMBrokenNetworkDialog::populateFileList()
{
    if (!m_fileList) return;
    
    m_fileList->clear();
    
    // Look in MERGED directory only for ZIP files
    QString mergedDir = "C:/Goji/TRACHMAR/BROKEN APPOINTMENTS/DATA/MERGED";
    QDir dir(mergedDir);
    
    if (!dir.exists()) {
        QListWidgetItem* item = new QListWidgetItem("MERGED directory not found");
        item->setFlags(Qt::NoItemFlags); // Make non-selectable
        m_fileList->addItem(item);
        return;
    }
    
    // Get only ZIP files from MERGED directory
    QStringList zipFiles = dir.entryList({"*.zip", "*.ZIP"}, QDir::Files, QDir::Name);
    
    if (zipFiles.isEmpty()) {
        QListWidgetItem* item = new QListWidgetItem("No ZIP files found in MERGED directory");
        item->setFlags(Qt::NoItemFlags);
        m_fileList->addItem(item);
    } else {
        for (const QString& fileName : zipFiles) {
            QListWidgetItem* item = new QListWidgetItem(fileName);
            item->setData(Qt::UserRole, dir.absoluteFilePath(fileName)); // Store full path
            m_fileList->addItem(item);
        }
    }
}

void TMBrokenNetworkDialog::calculateOptimalSize()
{
    // Base dimensions for simplified dialog
    int baseWidth = 500;
    int baseHeight = 350;
    
    // Adjust width based on longest file name if we have files
    if (m_fileList && m_fileList->count() > 0) {
        QFontMetrics fm(m_fileList->font());
        int maxTextWidth = 0;
        
        for (int i = 0; i < m_fileList->count(); ++i) {
            QListWidgetItem* item = m_fileList->item(i);
            if (item) {
                int textWidth = fm.horizontalAdvance(item->text());
                maxTextWidth = qMax(maxTextWidth, textWidth);
            }
        }
        
        // Add padding and ensure minimum width
        int suggestedWidth = qMax(baseWidth, maxTextWidth + 80);
        baseWidth = qMin(suggestedWidth, 700); // Cap at 700px
    }
    
    setFixedSize(baseWidth, baseHeight);
}

void TMBrokenNetworkDialog::onFileClicked()
{
    if (!m_fileSelected) {
        m_fileSelected = true;
        m_closeButton->setEnabled(true);
        m_closeTimer->stop(); // Stop timer since user interacted
    }
}

void TMBrokenNetworkDialog::onCloseClicked()
{
    accept();
}

void TMBrokenNetworkDialog::onTimerTimeout()
{
    m_closeButton->setEnabled(true);
}

// TMBrokenFileListWidget implementation

TMBrokenFileListWidget::TMBrokenFileListWidget(QWidget* parent)
    : QListWidget(parent)
{
    setupDragDrop();
}

void TMBrokenFileListWidget::setupDragDrop()
{
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setDefaultDropAction(Qt::CopyAction);
    
    // Set selection mode to allow multiple files
    setSelectionMode(QAbstractItemView::ExtendedSelection);
}

void TMBrokenFileListWidget::startDrag(Qt::DropActions supportedActions)
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

QMimeData* TMBrokenFileListWidget::createMimeData(const QList<QListWidgetItem*>& items) const
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
