#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QDateTime>

class UpdateManager : public QObject
{
    Q_OBJECT

public:
    explicit UpdateManager(QSettings* settings, QObject* parent = nullptr);
    ~UpdateManager();

    bool checkForUpdates(bool silent = false);
    bool downloadUpdate();
    bool applyUpdate();
    QString getCurrentVersion() const;
    QString getLatestVersion() const;
    QString getUpdateNotes() const;
    bool isUpdateAvailable() const;
    bool isDownloaded() const;

signals:
    // Removed const qualifiers from signal declarations
    void updateCheckStarted();
    void updateCheckFinished(bool available);
    void updateDownloadStarted();
    void updateDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void updateDownloadFinished(bool success);
    void updateInstallStarted();
    void updateInstallFinished(bool success);
    void errorOccurred(const QString& errorMessage);
    void logMessage(const QString& message);

private slots:
    void onUpdateInfoRequestFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();
    void onSslErrors(QNetworkReply* reply, const QList<QSslError>& errors);
    void onNetworkError(QNetworkReply::NetworkError error);

private:
    // Networking
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;

    // Update info
    QString m_currentVersion;
    QString m_latestVersion;
    QString m_updateNotes;
    QUrl m_updateFileUrl;
    QString m_updateFileName;
    QString m_updateChecksum;
    bool m_updateAvailable;
    bool m_updateDownloaded;
    bool m_silentCheck;

    // Paths
    QString m_updateFilePath;
    QString m_updateDir;
    QString m_backupDir;
    QString m_appDir;

    // Settings
    QSettings* m_settings;
    QString m_updateServerUrl;
    QString m_updateInfoFile;
    QString m_credentialsFile;
    QString m_awsAccessKey;
    QString m_awsSecretKey;

    // Methods
    void loadSettings();
    void prepareUpdateDirectories();
    QString calculateFileChecksum(const QString& filePath);
    bool extractUpdateFile();
    bool backupCurrentApp();
    bool restoreBackup();
    QByteArray generateAuthorizationHeader(const QUrl& url, const QString& httpMethod) const;
    bool validateUpdateInfo(const QJsonObject& updateInfo) const;
    QString generateS3Url(const QString& bucket, const QString& objectKey) const;
    QString formatBytes(qint64 bytes) const;
    bool isNewerVersion(const QString& current, const QString& latest) const;
};

#endif // UPDATEMANAGER_H
