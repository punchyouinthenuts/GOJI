#ifndef TMTERMEMAILDIALOG_H
#define TMTERMEMAILDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QClipboard>
#include <QCloseEvent>
#include "tmhealthyemailfilelistwidget.h"
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
 * @brief Email integration dialog for TM TERM
 *
 * This dialog displays network path and TERM DATA files for email attachment.
 * Features drag-and-drop support for Outlook integration.
 * Based on TMHealthyEmailDialog but adapted for TERM workflow.
 */
class TMTermEmailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TMTermEmailDialog(const QString& networkPath, const QString& jobNumber, QWidget *parent = nullptr);
    ~TMTermEmailDialog();

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
    TMHealthyEmailFileListWidget* m_fileList;
    QPushButton* m_closeButton;

    // State variables
    QString m_networkPath;
    QString m_jobNumber;
    bool m_copyClicked;
    bool m_fileClicked;

    // File icon provider
    QFileIconProvider m_iconProvider;
    
    // Constants
    static const QString DATA_DIR;
    static const QString FONT_FAMILY;
};

#endif // TMTERMEMAILDIALOG_H
