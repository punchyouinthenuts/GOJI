#ifndef AILIEMAILDIALOG_H
#define AILIEMAILDIALOG_H

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QFileIconProvider>

class QLabel;
class QPushButton;
class QTableWidget;
class QListWidget;

/*
 * AILIEmailDialog
 *
 * This dialog pauses the final processing step and presents the user with:
 *
 * 1) A postage table that can be copied to the clipboard as a formatted Word table
 * 2) The output CSV files that can be dragged into an Outlook email
 *
 * The CLOSE button remains disabled until:
 *      - the COPY button has been clicked
 *      - at least one file in the list has been clicked
 *
 * Once both actions occur, the user may close the dialog and the archive process continues.
 */

class AILIEmailDialog : public QDialog
{
    Q_OBJECT

public:

    explicit AILIEmailDialog(QWidget *parent = nullptr);

    void setPostageTable(const QVector<QStringList> &tableData);

    void setAttachmentFiles(const QStringList &filePaths);

    bool copyWasClicked() const;
    bool fileWasClicked() const;

signals:

    void dialogCompleted();

private slots:

    void handleCopyClicked();
    void handleFileClicked();
    void handleCloseClicked();

private:

    void updateCloseButtonState();

    void buildUI();
    void populateTable();

    QLabel *m_tableHeaderLabel;
    QLabel *m_fileHeaderLabel;

    QTableWidget *m_tableWidget;

    QPushButton *m_copyButton;
    QPushButton *m_closeButton;

    QListWidget *m_fileList;
    QFileIconProvider m_iconProvider;

    QStringList m_attachmentFilePaths;

    QVector<QStringList> m_tableData;

    bool m_copyClicked;
    bool m_fileClicked;
};

#endif // AILIEMAILDIALOG_H



