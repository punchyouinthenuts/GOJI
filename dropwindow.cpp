#include "dropwindow.h"
#include <QDir>
#include <QFile>
#include <QApplication>
#include <QDesktopServices>
#include <QMessageBox>
#include <QDateTime>

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
                if (isValidFileType(filePath)) {
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

        if (!isValidFileType(filePath)) {
            errorFiles << QString("%1 (unsupported file type)").arg(QFileInfo(filePath).fileName());
            continue;
        }

        // Copy file to target directory
        if (copyFileToTarget(filePath, m_targetDirectory)) {
            QString targetPath = QDir(m_targetDirectory).filePath(QFileInfo(filePath).fileName());
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
            instructionText = "Drag XLSX, XLS, or CSV files here\nto upload to RAW FILES folder";
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

bool DropWindow::copyFileToTarget(const QString& sourcePath, const QString& targetDir)
{
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return false;
    }

    // Ensure target directory exists
    QDir dir;
    if (!dir.exists(targetDir)) {
        if (!dir.mkpath(targetDir)) {
            return false;
        }
    }

    // Generate target path
    QString targetPath = QDir(targetDir).filePath(sourceInfo.fileName());

    // Handle existing files by generating unique name
    if (QFile::exists(targetPath)) {
        targetPath = generateUniqueFilename(targetPath);
    }

    // Perform the copy
    return QFile::copy(sourcePath, targetPath);
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
