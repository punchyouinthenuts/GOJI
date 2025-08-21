#ifndef TMBROKENEMAILDIALOG_H
#define TMBROKENEMAILDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QClipboard>
#include "tmbrokenemailfilelistwidget.h"
#include <QApplication>
#include <QFont>
#include <QFrame>
#include <QDir>
#include <QFileInfo>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QUrl>
#include <QFileIconProvider>

/**
 * @brief Email integration dialog for TM BROKEN APPOINTMENTS
 *
 * This dialog displays network path and MERGED files for email attachment.
 * Features drag-and-drop support for Outlook integration.
 */
class TMBrokenEmailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TMBrokenEmailDialog(const QString& networkPath, const QString& jobNumber, QWidget *parent = nullptr);
    ~TMBrokenEmailDialog();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onCopyPathClicked();
    void onFileClicked();
    void onCloseClicked();

private:
    void setupUI();
    void populateFileList();
    void updateCloseButtonState();
    QString getFileDirectory();

    // UI Components
    QVBoxLayout* m_mainLayout;
    QLabel* m_headerLabel1;
    QLabel* m_headerLabel2;
    QLabel* m_pathLabel;
    QPushButton* m_copyPathButton;
    TMBrokenEmailFileListWidget* m_fileList;
    QPushButton* m_closeButton;

    // State variables
    QString m_networkPath;
    QString m_jobNumber;
    bool m_copyClicked;
    bool m_fileClicked;

    // File icon provider
    QFileIconProvider m_iconProvider;
    
    // Constants
    static const QString MERGED_DIR;
    static const QString FONT_FAMILY;
};

#endif // TMBROKENEMAILDIALOG_H
