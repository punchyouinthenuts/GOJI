#ifndef TMWEEKLYPCFILEMANAGERDIALOG_H
#define TMWEEKLYPCFILEMANAGERDIALOG_H

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

/**
 * @brief Compact modal file manager popup for TM WEEKLY PC files
 *
 * This dialog displays files from PROOF and OUTPUT folders with drag-and-drop 
 * support for Outlook attachments. Replaces the previous PyQt5 implementation
 * with a native Qt solution.
 */
class TMWeeklyPCFileManagerDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param proofPath Path to the PROOF folder
     * @param outputPath Path to the OUTPUT folder
     * @param parent Parent widget
     */
    explicit TMWeeklyPCFileManagerDialog(const QString& proofPath,
                                        const QString& outputPath,
                                        QWidget* parent = nullptr);

private slots:
    /**
     * @brief Close the dialog
     */
    void onCloseClicked();

private:
    QString m_proofPath;
    QString m_outputPath;
    QFileIconProvider m_iconProvider;
    
    // UI elements
    QLabel* m_headerLabel;
    QListWidget* m_proofFileList;
    QListWidget* m_outputFileList;
    QPushButton* m_closeButton;
    
    /**
     * @brief Set up the dialog UI with compact layout
     */
    void setupUI();
    
    /**
     * @brief Populate file lists from the directories
     */
    void populateFileLists();
    
    /**
     * @brief Populate a single file list widget
     * @param listWidget The list widget to populate
     * @param directoryPath The directory to scan for files
     */
    void populateFileList(QListWidget* listWidget, const QString& directoryPath);
};

/**
 * @brief Custom QListWidget with drag-and-drop support for Outlook attachments
 */
class TMWeeklyPCDragDropListWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit TMWeeklyPCDragDropListWidget(const QString& folderPath, QWidget* parent = nullptr);

protected:
    void startDrag(Qt::DropActions supportedActions) override;

private:
    QString m_folderPath;
    QFileIconProvider m_iconProvider;
    
    /**
     * @brief Create MIME data for Outlook compatibility
     * @param filePath Path to the file being dragged
     * @return MIME data configured for Outlook attachments
     */
    QMimeData* createOutlookMimeData(const QString& filePath) const;
};

#endif // TMWEEKLYPCFILEMANAGERDIALOG_H
