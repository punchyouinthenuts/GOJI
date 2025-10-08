#include "tmweeklypidozipfilesdialog.h"
#include "logger.h"
#include <QCloseEvent>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QMessageBox>

// Static constants
const QString TMWeeklyPIDOZipFilesDialog::ZIP_DIR = "C:/Goji/TRACHMAR/WEEKLY IDO FULL";
const QString TMWeeklyPIDOZipFilesDialog::FONT_FAMILY = "Blender Pro";

TMWeeklyPIDOZipFilesDialog::TMWeeklyPIDOZipFilesDialog(const QString& zipDirectory, QWidget *parent)
    : QDialog(parent)
    , m_zipDirectory(zipDirectory)
    , m_headerLabel1(nullptr)
    , m_headerLabel2(nullptr)
    , m_zipFileList(nullptr)
    , m_closeButton(nullptr)
    , m_fileClicked(false)
    , m_overrideTimer(nullptr)
{
    setWindowTitle("Email Integration - TM WEEKLY PACK/IDO");
    setFixedSize(600, 400);
    setModal(true);
    
    // Remove the X button
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
    
    setupUI();
    populateZipFileList();
    updateCloseButtonState();
    
    // Start 10-second override timer
    m_overrideTimer = new QTimer(this);
    m_overrideTimer->setSingleShot(true);
    m_overrideTimer->setInterval(10000); // 10 seconds
    connect(m_overrideTimer, &QTimer::timeout, this, &TMWeeklyPIDOZipFilesDialog::onTimerTimeout);
    m_overrideTimer->start();
    
    Logger::instance().info("TMWeeklyPIDOZipFilesDialog created");
}

void TMWeeklyPIDOZipFilesDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Header labels with Blender Pro Bold 18pt, word-wrapped after "DROP"
    m_headerLabel1 = new QLabel("REPLY ALL AND DRAG & DROP", this);
    m_headerLabel1->setFont(QFont(FONT_FAMILY + " Bold", 18, QFont::Bold));
    m_headerLabel1->setAlignment(Qt::AlignCenter);
    m_headerLabel1->setStyleSheet("color: #2c3e50; margin-bottom: 2px;");
    
    m_headerLabel2 = new QLabel("THESE ZIP FILES INTO THE EMAIL", this);
    m_headerLabel2->setFont(QFont(FONT_FAMILY + " Bold", 18, QFont::Bold));
    m_headerLabel2->setAlignment(Qt::AlignCenter);
    m_headerLabel2->setStyleSheet("color: #2c3e50; margin-bottom: 15px;");
    
    mainLayout->addWidget(m_headerLabel1);
    mainLayout->addWidget(m_headerLabel2);
    
    // ZIP file list with drag-and-drop support
    m_zipFileList = new TMWeeklyPIDOZipDragDropListWidget(m_zipDirectory, this);
    m_zipFileList->setMinimumHeight(200);
    m_zipFileList->setStyleSheet(
        "QListWidget {"
        "    border: 2px solid #3498db;"
        "    border-radius: 8px;"
        "    background-color: #f8f9fa;"
        "    selection-background-color: #3498db;"
        "    selection-color: white;"
        "}"
        "QListWidget::item {"
        "    padding: 8px;"
        "    border-bottom: 1px solid #dee2e6;"
        "}"
        "QListWidget::item:hover {"
        "    background-color: #e3f2fd;"
        "}"
    );
    
    // Connect file click signal
    connect(m_zipFileList, &QListWidget::itemClicked, this, &TMWeeklyPIDOZipFilesDialog::onFileClicked);
    
    mainLayout->addWidget(m_zipFileList);
    
    // Close button
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_closeButton = new QPushButton("CLOSE", this);
    m_closeButton->setFont(QFont(FONT_FAMILY + " Bold", 14, QFont::Bold));
    m_closeButton->setMinimumSize(120, 40);
    m_closeButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #95a5a6;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 4px;"
        "    padding: 8px 20px;"
        "}"
        "QPushButton:enabled {"
        "    background-color: #27ae60;"
        "}"
        "QPushButton:enabled:hover {"
        "    background-color: #2ecc71;"
        "}"
    );
    connect(m_closeButton, &QPushButton::clicked, this, &TMWeeklyPIDOZipFilesDialog::onCloseClicked);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_closeButton);
    mainLayout->addLayout(buttonLayout);
}

void TMWeeklyPIDOZipFilesDialog::populateZipFileList()
{
    m_zipFileList->clear();

    QDir dir(m_zipDirectory);
    if (!dir.exists()) {
        Logger::instance().warning("ZIP directory does not exist: " + m_zipDirectory);
        return;
    }

    // Get only ZIP files
    QStringList zipFiles = dir.entryList(QStringList() << "*.zip", QDir::Files, QDir::Name);

    for (int i = 0; i < zipFiles.size(); ++i) {
        const QString& zipFile = zipFiles.at(i);

        // ✅ Whitelist: only include PROCESSED_ and PDF_ prefixes
        if (!zipFile.startsWith("PROCESSED_", Qt::CaseInsensitive) &&
            !zipFile.startsWith("PDF_", Qt::CaseInsensitive)) {
            continue;
        }

        QListWidgetItem* item = new QListWidgetItem(zipFile);
        item->setIcon(m_iconProvider.icon(QFileIconProvider::File));
        m_zipFileList->addItem(item);
    }

    Logger::instance().info(
        QString("Populated ZIP file list with %1 whitelisted files")
            .arg(m_zipFileList->count()));
}

void TMWeeklyPIDOZipFilesDialog::onFileClicked()
{
    if (!m_fileClicked) {
        m_fileClicked = true;
        updateCloseButtonState();
        Logger::instance().info("ZIP file clicked - close button enabled");
    }
}

void TMWeeklyPIDOZipFilesDialog::onTimerTimeout()
{
    updateCloseButtonState();
    Logger::instance().info("10-second timer override - close button enabled");
}

void TMWeeklyPIDOZipFilesDialog::updateCloseButtonState()
{
    bool canClose = m_fileClicked || (m_overrideTimer && !m_overrideTimer->isActive());
    m_closeButton->setEnabled(canClose);
}

void TMWeeklyPIDOZipFilesDialog::onCloseClicked()
{
    Logger::instance().info("TMWeeklyPIDOZipFilesDialog closing");
    accept();
}

// TMWeeklyPIDOZipDragDropListWidget implementation
TMWeeklyPIDOZipDragDropListWidget::TMWeeklyPIDOZipDragDropListWidget(const QString& folderPath, QWidget* parent)
    : QListWidget(parent), m_folderPath(folderPath)
{
    setDragEnabled(true);
    setDefaultDropAction(Qt::CopyAction);
    setDragDropMode(QAbstractItemView::DragOnly);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
}

void TMWeeklyPIDOZipDragDropListWidget::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions)

    QList<QListWidgetItem*> items = selectedItems();
    if (items.isEmpty()) {
        return;
    }

    QStringList filePaths;
    for (int i = 0; i < items.size(); ++i) {
        QListWidgetItem* item = items.at(i);
        QString fileName = item->text();
        QString filePath = QDir(m_folderPath).absoluteFilePath(fileName);

        QFileInfo fileInfo(filePath);
        if (fileInfo.exists() && fileInfo.isFile()) {
            filePaths << filePath;
        }
    }

    if (filePaths.isEmpty()) {
        return;
    }

    // Create drag object
    QDrag* drag = new QDrag(this);
    QMimeData* mimeData = new QMimeData();

    // Set file URLs for drag and drop
    QList<QUrl> urls;
    for (int i = 0; i < filePaths.size(); ++i) {
        const QString& filePath = filePaths.at(i);
        urls << QUrl::fromLocalFile(filePath);
    }
    mimeData->setUrls(urls);
    drag->setMimeData(mimeData);

    // Set drag icon
    if (!items.isEmpty()) {
        QIcon fileIcon = m_iconProvider.icon(QFileIconProvider::File);
        if (!fileIcon.isNull()) {
            drag->setPixmap(fileIcon.pixmap(32, 32));
        }
    }

    Logger::instance().info(QString("Starting drag for %1 ZIP file(s)").arg(filePaths.count()));

    // Execute drag
    Qt::DropAction dropAction = drag->exec(Qt::CopyAction);
    Q_UNUSED(dropAction)
}

QMimeData* TMWeeklyPIDOZipDragDropListWidget::createOutlookMimeData(const QString& filePath) const
{
    QMimeData* mimeData = new QMimeData();
    
    // Use URL-based MIME data for Outlook compatibility
    QUrl fileUrl = QUrl::fromLocalFile(filePath);
    mimeData->setUrls({ fileUrl });
    
    return mimeData;
}
