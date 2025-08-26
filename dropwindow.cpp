#include "dropwindow.h"
#include "archiveutils.h"
#include "configmanager.h"
#include <QDir>
#include <QFile>
#include <QApplication>
#include <QDesktopServices>
#include <QMessageBox>
#include <QDateTime>
#include <QTemporaryDir>
#include <QDirIterator>
#include <QMimeDatabase>
#include <QFileIconProvider>
#include <QStandardItemModel>
#include <QStyle>
#include <QHeaderView>
#include <QScrollBar>
#include <QScopedPointer>

DropWindow::DropWindow(QWidget* parent)
    : QListView(parent)
    , m_targetDirectory("C:/Goji/TRACHMAR/WEEKLY IDO FULL/RAW FILES")
    , m_model(nullptr)
    , m_isDragActive(false)
{
    // Set up supported file extensions
    m_supportedExtensions << "xlsx" << "xls" << "csv";

    // Enable drag and drop
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::DropOnly);
    setDefaultDropAction(Qt::CopyAction);

    // Set up the model
    setupModel();

    // Configure view properties
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setAlternatingRowColors(true);

    // Prevent horizontal scrolling and handle long filenames
    setWordWrap(true);
    setTextElideMode(Qt::ElideMiddle);  // Elide in middle to show beginning and extension
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // Set fixed item height to accommodate longer text and icons
    setUniformItemSizes(true);
    setGridSize(QSize(-1, 32));  // Fixed height of 32px for all items
    setResizeMode(QListView::Adjust);

    // Connect double-click signal
    connect(this, &QAbstractItemView::doubleClicked,
            this, &DropWindow::onItemDoubleClicked);

    // Set up visual styling
    setStyleSheet(
        "DropWindow {"
        "    border: 2px dashed #aaa;"
        "    border-radius: 5px;"
        "    background-color: #f9f9f9;"
        "    selection-background-color: #d0d0ff;"
        "}"
        "DropWindow[dragActive=\"true\"] {"
        "    border: 2px dashed #0078d4;"
        "    background-color: #e6f3ff;"
        "}"
        );
}

void DropWindow::setTargetDirectory(const QString& targetPath)
{
    m_targetDirectory = targetPath;

    // Ensure target directory exists
    QDir dir;
    if (!dir.exists(targetPath)) {
        dir.mkpath(targetPath);
    }
}

QString DropWindow::getTargetDirectory() const
{
    return m_targetDirectory;
}

void DropWindow::setSupportedExtensions(const QStringList& extensions)
{
    m_supportedExtensions = extensions;
}

void DropWindow::addFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        return;
    }

    // Create new item
    QStandardItem* item = new QStandardItem();
    item->setText(fileInfo.fileName());
    item->setData(filePath, Qt::UserRole); // Store full path
    item->setToolTip(filePath);

    // Set icon based on file type
    QString extension = fileInfo.suffix().toLower();
    if (extension == "xlsx" || extension == "xls") {
        item->setText(item->text() + " [Excel]");
    } else if (extension == "csv") {
        item->setText(item->text() + " [CSV]");
    } else if (extension == "zip") {
        item->setText(item->text() + " [ZIP]");
    }

    m_model->appendRow(item);
    emit fileCountChanged(m_model->rowCount());
}

void DropWindow::clearFiles()
{
    m_model->clear();
    emit fileCountChanged(0);
}

QStringList DropWindow::getFiles() const
{
    QStringList files;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QStandardItem* item = m_model->item(i);
        if (item) {
            QString filePath = item->data(Qt::UserRole).toString();
            if (!filePath.isEmpty()) {
                files << filePath;
            }
        }
    }
    return files;
}

void DropWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        // Check if any of the dragged files are valid
        bool hasValidFiles = false;
        const QList<QUrl> urls = event->mimeData()->urls();

        for (const QUrl& url : urls) {
            if (url.isLocalFile()) {
                QString filePath = url.toLocalFile();
                bool expand = ConfigManager::instance().getBool("ui/expandArchivesOnDrop", true);
                if (isValidFileType(filePath) || (expand && isZip(filePath))) {
                    hasValidFiles = true;
                    break;
                }
            }
        }

        if (hasValidFiles) {
            event->acceptProposedAction();
            m_isDragActive = true;
            setProperty("dragActive", true);
            style()->unpolish(this);
            style()->polish(this);
            update();
        } else {
            event->ignore();
        }
    } else {
        event->ignore();
    }
}

void DropWindow::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DropWindow::dropEvent(QDropEvent* event)
{
    m_isDragActive = false;
    setProperty("dragActive", false);
    style()->unpolish(this);
    style()->polish(this);
    update();

    const QMimeData* mimeData = event->mimeData();
    if (!mimeData->hasUrls()) {
        event->ignore();
        return;
    }

    const QList<QUrl> urls = mimeData->urls();
    QStringList processedFiles;
    QStringList errorFiles;

    for (const QUrl& url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }

        QString filePath = url.toLocalFile();

        // ZIP expansion guard: only intercept .zip when the feature flag is true.
        if (isZip(filePath)) {
            const bool expand = ConfigManager::instance().getBool("ui/expandArchivesOnDrop", true);
            if (expand) {
                // 1) Move/copy the ZIP into this drop window's target directory,
                //    so "INPUT ZIP" receives the archive file.
                QString zipTargetPath = copyFileToTarget(filePath, m_targetDirectory);
                if (zipTargetPath.isEmpty()) {
                    // Copy failed: record an error but still allow virtual listing from original
                    errorFiles << QString("%1 (copy failed)").arg(QFileInfo(filePath).fileName());
                }
                else { processedFiles << zipTargetPath; }
                // 2) Virtually list contents using the new on-disk location if available,
                //    otherwise fall back to the original path.
                const QString archiveForListing = zipTargetPath.isEmpty() ? filePath : zipTargetPath;
                handleZipDrop(archiveForListing);  // virtual listing only; no extraction
                continue;
            }
            // else: fall through to existing behavior that lists "Files.zip [ZIP]" or similar.
        }

        if (!isValidFileType(filePath)) {
            errorFiles << QString("%1 (unsupported file type)").arg(QFileInfo(filePath).fileName());
            continue;
        }

        // Copy file to target directory
        const QString targetPath = copyFileToTarget(filePath, m_targetDirectory);
        if (!targetPath.isEmpty()) {
            addFile(targetPath);
            processedFiles << targetPath;
        } else {
            errorFiles << QString("%1 (copy failed)").arg(QFileInfo(filePath).fileName());
        }
    }

    // Emit results
    if (!processedFiles.isEmpty()) {
        emit filesDropped(processedFiles);
        event->acceptProposedAction();
    }

    if (!errorFiles.isEmpty()) {
        QString errorMessage = QString("Failed to process %1 file(s):\n%2")
        .arg(errorFiles.size())
            .arg(errorFiles.join("\n"));
        emit fileDropError(errorMessage);
    }

    if (processedFiles.isEmpty() && !errorFiles.isEmpty()) {
        event->ignore();
    }
}

void DropWindow::paintEvent(QPaintEvent* event)
{
    QListView::paintEvent(event);

    // Draw instruction text if no files
    if (m_model->rowCount() == 0) {
        QPainter painter(viewport());
        painter.setPen(QPen(QColor(150, 150, 150)));

        QString instructionText;
        if (m_isDragActive) {
            instructionText = "Drop files here...";
        } else {
            // Generate dynamic hint text from m_supportedExtensions
            QStringList uppercaseExtensions;
            for (const QString& ext : m_supportedExtensions) {
                uppercaseExtensions << ext.toUpper();
            }

            QString extensionsList;
            if (uppercaseExtensions.size() == 1) {
                extensionsList = uppercaseExtensions.first();
            } else if (uppercaseExtensions.size() == 2) {
                extensionsList = QString("%1 or %2").arg(uppercaseExtensions.first(), uppercaseExtensions.last());
            } else if (uppercaseExtensions.size() > 2) {
                QStringList allButLast = uppercaseExtensions.mid(0, uppercaseExtensions.size() - 1);
                extensionsList = QString("%1, or %2").arg(allButLast.join(", "), uppercaseExtensions.last());
            }

            instructionText = QString("Drag %1 files here\nto upload to RAW FILES folder").arg(extensionsList);
        }

        QRect textRect = viewport()->rect();
        painter.drawText(textRect, Qt::AlignCenter, instructionText);
    }
}

void DropWindow::onItemDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    QStandardItem* item = m_model->itemFromIndex(index);
    if (!item) {
        return;
    }

    QString filePath = item->data(Qt::UserRole).toString();
    if (!filePath.isEmpty() && QFile::exists(filePath)) {
        // Open file with default application
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    }
}

bool DropWindow::isValidFileType(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    return m_supportedExtensions.contains(extension);
}

QString DropWindow::copyFileToTarget(const QString& sourcePath, const QString& targetDir)
{
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return QString();
    }

    // Ensure target directory exists
    QDir dir;
    if (!dir.exists(targetDir)) {
        if (!dir.mkpath(targetDir)) {
            return QString();
        }
    }

    // Generate target path
    QString targetPath = QDir(targetDir).filePath(sourceInfo.fileName());

    // Handle existing files by generating unique name
    if (QFile::exists(targetPath)) {
        targetPath = generateUniqueFilename(targetPath);
    }

    // Perform the copy
    if (QFile::copy(sourcePath, targetPath)) {
        return targetPath;
    }
    return QString();
}

QString DropWindow::generateUniqueFilename(const QString& targetPath) const
{
    QFileInfo fileInfo(targetPath);
    QString baseName = fileInfo.completeBaseName();
    QString extension = fileInfo.suffix();
    QString directory = fileInfo.absolutePath();

    int counter = 1;
    QString newPath;

    do {
        QString newName = QString("%1_%2.%3").arg(baseName).arg(counter).arg(extension);
        newPath = QDir(directory).filePath(newName);
        counter++;
    } while (QFile::exists(newPath) && counter < 1000); // Safety limit

    return newPath;
}

void DropWindow::setupModel()
{
    m_model = new QStandardItemModel(this);
    setModel(m_model);

    // Set up headers (though we're not showing them)
    m_model->setHorizontalHeaderLabels(QStringList() << "Dropped Files");
}

void DropWindow::handleZipDrop(const QString& zipPath) {
    QString err;
    const auto entries = listZipEntries(zipPath, &err);
    if (!err.isEmpty()) {
        // Optional: surface 'err' using your existing error UI/logging if available.
        return;
    }

    for (const auto& e : entries) {
        if (e.isDir) continue; // skip folders in the list
        const QString internalPath = e.pathInArchive;               // e.g., "reports/summary.xlsx"
        const QString displayName  = QFileInfo(internalPath).fileName();

        // Respect existing extension filtering
        if (!isValidFileType(displayName)) {
            continue;
        }

        addVirtualZipEntry(zipPath, internalPath, displayName, e.size, false);
    }
}

void DropWindow::addVirtualZipEntry(const QString& archivePath,
                                    const QString& internalPath,
                                    const QString& displayName,
                                    quint64 size,
                                    bool /*isDir*/) {
    // Derive an icon by filename/extension
    const QIcon icn = iconForFileName(displayName);

    // Create new item mirroring the addFile logic but for virtual entries
    QStandardItem* item = new QStandardItem(icn, displayName);
    item->setData(QStringLiteral("zip"),        Qt::UserRole + 1);
    item->setData(archivePath,                 Qt::UserRole + 2);
    item->setData(internalPath,                Qt::UserRole + 3);
    item->setData(QVariant::fromValue<qulonglong>(size), Qt::UserRole + 4);
    item->setToolTip(QString("%1\n(inside %2)")
                     .arg(internalPath)
                     .arg(QFileInfo(archivePath).fileName()));

    m_model->appendRow(item);
    emit fileCountChanged(m_model->rowCount());
}

QIcon DropWindow::iconForFileName(const QString& fileName) {
    // Prefer QFileIconProvider based on a short-lived placeholder path
    // in a session temp directory with the same extension.
    static QScopedPointer<QTemporaryDir> s_iconScratch;
    if (!s_iconScratch || !s_iconScratch->isValid()) {
        s_iconScratch.reset(new QTemporaryDir("GOJI_icon_scratch_XXXXXX"));
    }

    const QString ext = QFileInfo(fileName).suffix();
    QString placeholder = s_iconScratch->path() + QDir::separator() +
                          "icon_placeholder." + (ext.isEmpty() ? "bin" : ext);

    // Create once per extension if missing (0-byte is fine)
    if (!QFile::exists(placeholder)) {
        QFile f(placeholder);
        if (f.open(QIODevice::WriteOnly)) {
            f.write("", 0);
            f.close();
        }
    }

    QFileIconProvider provider;
    return provider.icon(QFileInfo(placeholder));
}
