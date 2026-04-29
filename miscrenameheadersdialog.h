#ifndef MISCRENAMEHEADERSDIALOG_H
#define MISCRENAMEHEADERSDIALOG_H

#include <QDialog>
#include <QMap>
#include <QStringList>

#include "terminaloutputhelper.h"

class QLabel;
class QPushButton;
class QTableWidget;
class QCloseEvent;

class MiscRenameHeadersDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MiscRenameHeadersDialog(QWidget* parent = nullptr);

    void setStatusMessage(const QString& message,
                          TerminalSeverity severity = TerminalSeverity::Info);
    void setRunning(bool running);
    void setLoadedFileHeaders(const QString& filePath, const QStringList& headers);
    bool hasLoadedFile() const;
    QString loadedFilePath() const;
    QMap<int, QString> enteredHeaderChanges() const;

signals:
    void loadHeadersRequested(const QString& filePath);
    void saveRequested();
    void terminalMessageRequested(const QString& message, TerminalSeverity severity);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onOpenClicked();
    void onPrimaryClicked();

private:
    void setupUi();
    void resetToCloseState();
    void updateControlStates();
    void updatePrimaryButtonText();

    QPushButton* m_openButton;
    QLabel* m_filePathLabel;
    QTableWidget* m_headersTable;
    QLabel* m_statusLabel;
    QPushButton* m_primaryButton;

    QString m_loadedFilePath;
    bool m_running;
};

#endif // MISCRENAMEHEADERSDIALOG_H
