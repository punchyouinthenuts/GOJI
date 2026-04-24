#ifndef TMWEEKLYPCPOSTPRINTDIALOG_H
#define TMWEEKLYPCPOSTPRINTDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QFileIconProvider>

class QLabel;
class QPushButton;
class QListWidgetItem;
class TMHealthyEmailFileListWidget;

/**
 * @brief Drag-capable dialog for TM WEEKLY PC post-print output files.
 *
 * Displays only files created in the current 04POSTPRINT.py run.
 */
class TMWeeklyPCPostPrintDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TMWeeklyPCPostPrintDialog(const QString& outputPath, const QStringList& filePaths, QWidget* parent = nullptr);

private slots:
    void onCopyPathClicked();
    void onCloseClicked();

private:
    void setupUI();
    void populateFileList(const QStringList& filePaths);

    QString m_outputPath;
    QLabel* m_headerLabel;
    QLabel* m_outputPathLabel;
    QPushButton* m_copyPathButton;
    TMHealthyEmailFileListWidget* m_fileList;
    QPushButton* m_closeButton;
    QFileIconProvider m_iconProvider;
};

#endif // TMWEEKLYPCPOSTPRINTDIALOG_H
