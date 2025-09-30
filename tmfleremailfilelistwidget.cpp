#include "tmfleremailfilelistwidget.h"
#include "logger.h"
#include <QDrag>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QPainter>
#include <QDebug>


TMFLEREmailFileListWidget::TMFLEREmailFileListWidget(QWidget* parent)
    : QListWidget(parent)
{
    setupDragDrop();
    Logger::instance().info("TMFLEREmailFileListWidget initialized with drag-and-drop support");
}

void TMFLEREmailFileListWidget::setupDragDrop()
{
    // Configure drag and drop settings - exactly like TMWeeklyPIDOZipDragDropListWidget
    setDragEnabled(true);
    setDefaultDropAction(Qt::CopyAction);
    setDragDropMode(QAbstractItemView::DragOnly);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
}

void TMFLEREmailFileListWidget::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions)

    QList<QListWidgetItem*> items = selectedItems();
    if (items.isEmpty()) {
        return;
    }

    QStringList filePaths;
    for (QListWidgetItem* item : items) {
        // Get the file path from the item's UserRole data
        QString filePath = item->data(Qt::UserRole).toString();

        QFileInfo fileInfo(filePath);
        if (fileInfo.exists() && fileInfo.isFile()) {
            filePaths << filePath;
        }
    }

    if (filePaths.isEmpty()) {
        return;
    }

    // Create drag object using Outlook-compatible MIME data
    QDrag* drag = new QDrag(this);
    QMimeData* mimeData = createOutlookMimeData(filePaths.first());
    
    // For multiple files, add all URLs
    if (filePaths.count() > 1) {
        QList<QUrl> urls;
        for (const QString& filePath : filePaths) {
            urls << QUrl::fromLocalFile(filePath);
        }
        mimeData->setUrls(urls);
    }
    drag->setMimeData(mimeData);

    // Set drag icon using the first file's icon
    if (!items.isEmpty()) {
        QString filePath = items.first()->data(Qt::UserRole).toString();
        QFileInfo fileInfo(filePath);
        QIcon fileIcon = m_iconProvider.icon(fileInfo);
        if (!fileIcon.isNull()) {
            drag->setPixmap(fileIcon.pixmap(32, 32));
        } else {
            // Fallback icon
            QIcon fallbackIcon = m_iconProvider.icon(QFileIconProvider::File);
            drag->setPixmap(fallbackIcon.pixmap(32, 32));
        }
    }

    Logger::instance().info(QString("Starting drag for %1 MERGED file(s)").arg(filePaths.count()));

    // Execute drag
    Qt::DropAction dropAction = drag->exec(Qt::CopyAction);
    Q_UNUSED(dropAction)
}

QMimeData* TMFLEREmailFileListWidget::createOutlookMimeData(const QString& filePath) const
{
    QMimeData* mimeData = new QMimeData();
    
    // Use URL-based MIME data for Outlook compatibility - same as working examples
    QUrl fileUrl = QUrl::fromLocalFile(filePath);
    mimeData->setUrls({ fileUrl });
    
    return mimeData;
}
