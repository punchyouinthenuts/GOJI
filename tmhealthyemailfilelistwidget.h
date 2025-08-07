#ifndef TMHEALTHYEMAILFILELISTWIDGET_H
#define TMHEALTHYEMAILFILELISTWIDGET_H

#include <QListWidget>
#include <QFileIconProvider>
#include <QDrag>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QPainter>
#include <QMouseEvent>

/**
 * @brief Custom QListWidget with drag-and-drop support for email file attachments
 * 
 * This widget provides full drag-and-drop functionality for files in the MERGED 
 * directory, including file icons and proper MIME data for Outlook compatibility.
 * Based on the working TMWeeklyPIDOZipDragDropListWidget implementation.
 */
class TMHealthyEmailFileListWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit TMHealthyEmailFileListWidget(QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void startDrag(Qt::DropActions supportedActions) override;

private:
    QFileIconProvider m_iconProvider;
    QPoint m_dragStartPos;
    
    /**
     * @brief Create MIME data for Outlook compatibility
     * @param filePath Path to the file being dragged
     * @return MIME data configured for Outlook attachments
     */
    QMimeData* createOutlookMimeData(const QString& filePath) const;
    
    /**
     * @brief Configure drag and drop settings
     */
    void setupDragDrop();
};

#endif // TMHEALTHYEMAILFILELISTWIDGET_H
