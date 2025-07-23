#ifndef TMHEALTHYNETWORKDIALOG_H
#define TMHEALTHYNETWORKDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QListWidget>
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
class TMHealthyNetworkDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param networkPath The network path to display
     * @param jobNumber The job number for finding MERGED files
     * @param parent Parent widget
     */
    explicit TMHealthyNetworkDialog(const QString& networkPath,
                                   const QString& jobNumber,
                                   QWidget* parent = nullptr);

    /**
     * @brief Check if fallback mode should be used and create dialog accordingly
     * @param networkPath The intended network path
     * @param jobNumber The job number for finding files
     * @param parent Parent widget
     * @return Pointer to the appropriate dialog (network or fallback)
     */
    static TMHealthyNetworkDialog* createDialog(const QString& networkPath,
                                               const QString& jobNumber,
                                               QWidget* parent = nullptr);

private slots:
    /**
     * @brief Copy the network path to clipboard
     */
    void onCopyPathClicked();

    /**
     * @brief Close the dialog
     */
    void onCloseClicked();

private:
    QString m_networkPath;
    QString m_jobNumber;
    bool m_isFallbackMode;
    QString m_fallbackPath;
    
    // UI elements
    QLabel* m_titleLabel;
    QLabel* m_statusLabel;
    QTextEdit* m_pathDisplay;
    QPushButton* m_copyPathButton;
    QListWidget* m_fileList;
    QPushButton* m_closeButton;
    
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
    
    /**
     * @brief Check if fallback directory exists and has files
     */
    bool checkFallbackMode();
    
    /**
     * @brief Get the appropriate file directory based on mode
     */
    QString getFileDirectory() const;
};

/**
 * @brief Custom QListWidget with drag-and-drop support for Outlook
 */
class TMHealthyFileListWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit TMHealthyFileListWidget(QWidget* parent = nullptr);

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    QMimeData* createMimeData(const QList<QListWidgetItem*>& items) const;

private:
    /**
     * @brief Configure drag and drop settings
     */
    void setupDragDrop();
};

#endif // TMHEALTHYNETWORKDIALOG_H
