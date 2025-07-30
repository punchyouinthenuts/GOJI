#ifndef TMWEEKLYPIDOZIPFILESDIALOG_H
#define TMWEEKLYPIDOZIPFILESDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QFrame>
#include <QFileIconProvider>
#include <QDir>
#include <QFileInfo>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QUrl>
#include <QDrag>
#include <QTimer>

/**
 * @brief Modal ZIP files dialog for TM WEEKLY PACK/IDO email integration
 *
 * This dialog displays ZIP files created by 06DPZIP.py with drag-and-drop 
 * support for Outlook attachments. Prevents closure until files are clicked
 * or 10-second timer override elapses.
 */
class TMWeeklyPIDOZipFilesDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param zipDirectory Path to directory containing ZIP files
     * @param parent Parent widget
     */
    explicit TMWeeklyPIDOZipFilesDialog(const QString& zipDirectory, QWidget* parent = nullptr);

private slots:
    /**
     * @brief Handle file click events
     */
    void onFileClicked();
    
    /**
     * @brief Handle timer timeout for close button override
     */
    void onTimerTimeout();
    
    /**
     * @brief Close the dialog
     */
    void onCloseClicked();

private:
    QString m_zipDirectory;
    QFileIconProvider m_iconProvider;
    
    // UI elements
    QLabel* m_headerLabel1;
    QLabel* m_headerLabel2;
    QListWidget* m_zipFileList;
    QPushButton* m_closeButton;
    
    // State tracking
    bool m_fileClicked;
    QTimer* m_overrideTimer;
    
    // Constants
    static const QString FONT_FAMILY;
    static const QString ZIP_DIR;
    
    /**
     * @brief Set up the dialog UI
     */
    void setupUI();
    
    /**
     * @brief Populate ZIP file list from directory
     */
    void populateZipFileList();
    
    /**
     * @brief Update close button state based on file clicks and timer
     */
    void updateCloseButtonState();
};

/**
 * @brief Custom QListWidget with drag-and-drop support for ZIP files
 */
class TMWeeklyPIDOZipDragDropListWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit TMWeeklyPIDOZipDragDropListWidget(const QString& folderPath, QWidget* parent = nullptr);

protected:
    void startDrag(Qt::DropActions supportedActions) override;

private:
    QString m_folderPath;
    QFileIconProvider m_iconProvider;
    
    /**
     * @brief Create MIME data for Outlook compatibility
     * @param filePath Path to the ZIP file being dragged
     * @return MIME data configured for Outlook attachments
     */
    QMimeData* createOutlookMimeData(const QString& filePath) const;
};

#endif // TMWEEKLYPIDOZIPFILESDIALOG_H