#ifndef TMFLEREMAILDIALOG_H
#define TMFLEREMAILDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QClipboard>
#include "tmfleremailfilelistwidget.h"
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
 * @brief Email integration dialog for TM FL ER
 *
 * This dialog displays MERGED CSV file for email attachment.
 * Features drag-and-drop support for Outlook integration.
 */
class TMFLEREmailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TMFLEREmailDialog(const QString& networkPath, const QString& jobNumber, QWidget *parent = nullptr);
    ~TMFLEREmailDialog();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
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
    TMFLEREmailFileListWidget* m_fileList;
    QPushButton* m_closeButton;

    // State variables
    QString m_networkPath;
    QString m_jobNumber;
    bool m_fileClicked;

    // File icon provider
    QFileIconProvider m_iconProvider;
    
    // Constants
    static const QString MERGED_DIR;
    static const QString FONT_FAMILY;
};

#endif // TMFLEREMAILDIALOG_H
