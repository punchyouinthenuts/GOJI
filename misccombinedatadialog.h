#ifndef MISCCOMBINEDATADIALOG_H
#define MISCCOMBINEDATADIALOG_H

#include <QDialog>
#include <QStringList>

#include "terminaloutputhelper.h"

class QLabel;
class QListWidget;
class QPushButton;
class QCloseEvent;

class MiscCombineDataDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MiscCombineDataDialog(QWidget* parent = nullptr);

    void setStatusMessage(const QString& message, TerminalSeverity severity = TerminalSeverity::Info);
    void setRunning(bool running);
    void setSelectedFiles(const QStringList& files);
    QStringList selectedFiles() const;

signals:
    void combineRequested(const QStringList& files);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onSelectFilesClicked();
    void onCombineClicked();
    void onCloseClicked();

private:
    void setupUi();
    void rebuildFileList();
    void updateControlStates();

    QPushButton* m_selectFilesButton;
    QListWidget* m_selectedFilesList;
    QPushButton* m_combineButton;
    QLabel* m_statusLabel;
    QPushButton* m_closeButton;

    QStringList m_selectedFiles;
    bool m_running;
};

#endif // MISCCOMBINEDATADIALOG_H