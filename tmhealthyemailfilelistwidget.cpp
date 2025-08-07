#include "tmhealthyemailfilelistwidget.h"
#include "logger.h"
#include <QDrag>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include <QApplication>

TMHealthyEmailFileListWidget::TMHealthyEmailFileListWidget(QWidget* parent)
    : QListWidget(parent)
{
    setupDragDrop();
    Logger::instance().info("TMHealthyEmailFileListWidget initialized with drag-and-drop support");
}

void TMHealthyEmailFileListWidget::setupDragDrop()
{
    // Configure drag and drop settings - exactly like TMWeeklyPIDOZipDragDropListWidget
    setDragEnabled(true);
    setDefaultDropAction(Qt::CopyAction);
    setDragDropMode(QAbstractItemView::DragOnly);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
}

void TMHealthyEmailFileListWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = event->pos();
    }
    QListWidget::mousePressEvent(event);
}

void TMHealthyEmailFileListWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        QListWidget::mouseMoveEvent(event);
        return;
    }
    
    if ((event->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance()) {
        QListWidget::mouseMoveEvent(event);
        return;
    }
    
    // Start drag operation
    startDrag(Qt::CopyAction);
}

void TMHealthyEmailFileListWidget::startDrag(Qt::DropActions supportedActions)
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

    // Create drag object
    QDrag* drag = new QDrag(this);
    QMimeData* mimeData = new QMimeData();

    // Set file URLs for drag and drop
    QList<QUrl> urls;
    for (const QString& filePath : filePaths) {
        urls << QUrl::fromLocalFile(filePath);
    }
    mimeData->setUrls(urls);
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

QMimeData* TMHealthyEmailFileListWidget::createOutlookMimeData(const QString& filePath) const
{
    QMimeData* mimeData = new QMimeData();
    
    // Use URL-based MIME data for Outlook compatibility - same as working examples
    QUrl fileUrl = QUrl::fromLocalFile(filePath);
    mimeData->setUrls({ fileUrl });
    
    return mimeData;
}
