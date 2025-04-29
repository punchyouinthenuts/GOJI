#ifndef UPDATEDIALOG_H
#define UPDATEDIALOG_H

#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QTextBrowser>
#include <QtNetwork/QNetworkAccessManager>
#include "updatemanager.h"

class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateDialog(UpdateManager* updateManager, QWidget* parent = nullptr);
    ~UpdateDialog();

private slots:
    void onCheckForUpdatesClicked();
    void onDownloadUpdateClicked();
    void onInstallUpdateClicked();
    void onRemindLaterClicked();
    void onSkipUpdateClicked();

    // UpdateManager slots
    void onUpdateCheckStarted();
    void onUpdateCheckFinished(bool available);
    void onUpdateDownloadStarted();
    void onUpdateDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onUpdateDownloadFinished(bool success);
    void onUpdateInstallStarted();
    void onUpdateInstallFinished(bool success);
    void onErrorOccurred(const QString& errorMessage);

private:
    UpdateManager* m_updateManager;

    // UI elements
    QLabel* m_statusLabel;
    QLabel* m_currentVersionLabel;
    QLabel* m_latestVersionLabel;
    QTextBrowser* m_notesBrowser; // For rich text release notes
    QProgressBar* m_progressBar;
    QPushButton* m_checkButton;
    QPushButton* m_downloadButton;
    QPushButton* m_installButton;
    QPushButton* m_remindLaterButton;
    QPushButton* m_skipButton;

    void setupUI();
    void updateUI();
};

#endif // UPDATEDIALOG_H
