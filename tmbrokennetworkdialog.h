#ifndef TMBROKENNETWORKDIALOG_H
#define TMBROKENNETWORKDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QTimer>
#include <QClipboard>
#include <QApplication>
#include <QFont>
#include <QFrame>
#include <QDir>
#include <QFileInfo>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QUrl>

/**
 * @brief Specialized dialog for TM HEALTHY BEGINNINGS network file display
 *
 * This dialog displays a network path with copy functionality and shows
 * files from the MERGED directory with drag-and-drop support for Outlook.
 */
class TMBrokenNetworkDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param networkPath The network path to display
     * @param jobNumber The job number for finding MERGED files
     * @param parent Parent widget
     */
    explicit TMBrokenNetworkDialog(const QString& networkPath,
                                   const QString& jobNumber,
                                   QWidget* parent = nullptr);

private slots:
    /**
     * @brief Handle file selection
     */
    void onFileClicked();

    /**
     * @brief Close the dialog (enabled after file click or timer)
     */
    void onCloseClicked();

    /**
     * @brief Timer timeout to enable close button
     */
    void onTimerTimeout();

private:
    QString m_networkPath;
    QString m_jobNumber;
    bool m_fileSelected;
    
    // UI elements
    QLabel* m_headerLabel;
    QListWidget* m_fileList;
    QPushButton* m_closeButton;
    QTimer* m_closeTimer;
    
    /**
     * @brief Set up the dialog UI
     */
    void setupUI();
    
    /**
     * @brief Populate the file list with MERGED files
     */
    void populateFileList();
    
    /**
     * @brief Calculate optimal dialog size
     */
    void calculateOptimalSize();
};

/**
 * @brief Custom QListWidget with drag-and-drop support for Outlook
 */
class TMBrokenFileListWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit TMBrokenFileListWidget(QWidget* parent = nullptr);

protected:
    void startDrag(Qt::DropActions supportedActions);
    QMimeData* createMimeData(const QList<QListWidgetItem*>& items) const;

private:
    /**
     * @brief Configure drag and drop settings
     */
    void setupDragDrop();
};

#endif // TMBROKENNETWORKDIALOG_H
