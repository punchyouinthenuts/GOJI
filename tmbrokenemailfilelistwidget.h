#ifndef TMBROKENEMAILFILELISTWIDGET_H
#define TMBROKENEMAILFILELISTWIDGET_H

#include <QListWidget>
#include <QFileIconProvider>
#include <QDrag>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QPainter>


/**
 * @brief Custom QListWidget with drag-and-drop support for email file attachments
 * 
 * This widget provides full drag-and-drop functionality for files in the MERGED 
 * directory, including file icons and proper MIME data for Outlook compatibility.
 * Based on the working TM Weekly PIDO ZipDragDropListWidget implementation.
 */
class TMBrokenEmailFileListWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit TMBrokenEmailFileListWidget(QWidget* parent = nullptr);

protected:

    void startDrag(Qt::DropActions supportedActions) override;

private:
    QFileIconProvider m_iconProvider;

    
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

#endif // TMBROKENEMAILFILELISTWIDGET_H
