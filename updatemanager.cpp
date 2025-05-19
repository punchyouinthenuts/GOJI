#include "updatemanager.h"
#include "fileutils.h"
#include "errormanager.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QtNetwork/QNetworkRequest>
#include <QTemporaryDir>
#include <QVersionNumber>
#include <QUrlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <QMessageAuthenticationCode>

// For AWS Signature V4
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>

// Current application version - should match VERSION in mainwindow.cpp
#ifdef APP_VERSION
const QString VERSION = QString(APP_VERSION);
#else
const QString VERSION = "1.0.0";
#endif

UpdateManager::UpdateManager(QSettings* settings, QObject* parent)
    : QObject(parent),
    m_networkManager(new QNetworkAccessManager(this)),
    m_currentReply(nullptr),
    m_currentVersion(VERSION),
    m_updateAvailable(false),
    m_updateDownloaded(false),
    m_silentCheck(false),
    m_settings(settings)
{
    // Connect network manager signals
    connect(m_networkManager, &QNetworkAccessManager::sslErrors,
            this, &UpdateManager::onSslErrors);

    // Load settings
    loadSettings();

    // Prepare update directories
    prepareUpdateDirectories();

    emit logMessage("Update manager initialized. Current version: " + m_currentVersion);
}

UpdateManager::~UpdateManager()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
}

void UpdateManager::loadSettings()
{
    // Load AWS credentials
    m_credentialsFile = m_settings->value("AwsCredentialsPath",
                                          QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                                              "/aws_credentials.json").toString();

    // Try to load AWS credentials from the file
    QFile credFile(m_credentialsFile);
    if (credFile.open(QIODevice::ReadOnly)) {
        QJsonDocument credDoc = QJsonDocument::fromJson(credFile.readAll());
        QJsonObject credObj = credDoc.object();

        m_awsAccessKey = credObj["aws_access_key_id"].toString();
        m_awsSecretKey = credObj["aws_secret_access_key"].toString();
        credFile.close();
    } else {
        emit logMessage("Failed to open AWS credentials file: " + m_credentialsFile);
    }

    // S3 configuration
    m_updateServerUrl = m_settings->value("UpdateServerUrl",
                                          "https://goji-updates.s3.amazonaws.com").toString();
    m_updateInfoFile = m_settings->value("UpdateInfoFile", "latest.json").toString();
}

void UpdateManager::prepareUpdateDirectories()
{
    // Application directory
    m_appDir = QCoreApplication::applicationDirPath();

    // Update directory (in AppData location for better permissions)
    m_updateDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/updates";
    try {
        FileUtils::ensureDirectoryExists(m_updateDir);
    } catch (const FileOperationException& e) {
        emit errorOccurred(QString("Failed to create update directory: %1").arg(e.message()));
        return;
    }

    // Backup directory
    m_backupDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/backup";
    try {
        FileUtils::ensureDirectoryExists(m_backupDir);
    } catch (const FileOperationException& e) {
        emit errorOccurred(QString("Failed to create backup directory: %1").arg(e.message()));
        return;
    }

    emit logMessage("Update directories prepared. Update dir: " + m_updateDir);
}

bool UpdateManager::checkForUpdates(bool silent)
{
    m_silentCheck = silent;
    emit updateCheckStarted();
    emit logMessage("Checking for updates...");

    // Construct the update info URL
    QUrl updateInfoUrl(m_updateServerUrl + "/" + m_updateInfoFile);
    emit logMessage("Request URL: " + updateInfoUrl.toString());

    // Create request
    QNetworkRequest request(updateInfoUrl);

    // Explicitly skip AWS authentication for public access
    emit logMessage("Skipping AWS authentication for public bucket access");

    // Log request headers
    emit logMessage("Request Headers:");
    for (const auto& header : request.rawHeaderList()) {
        emit logMessage(QString("%1: %2").arg(QString(header), QString(request.rawHeader(header))));
    }

    // Send request
    m_currentReply = m_networkManager->get(request);

    // Connect signals for this request
    connect(m_currentReply, &QNetworkReply::finished,
            this, &UpdateManager::onUpdateInfoRequestFinished);
    connect(m_currentReply, &QNetworkReply::errorOccurred, this, &UpdateManager::onNetworkError);

    return true;
}

void UpdateManager::onUpdateInfoRequestFinished()
{
    if (!m_currentReply) {
        emit errorOccurred("Invalid network reply");
        emit updateCheckFinished(false);
        return;
    }

    // Check for network errors
    if (m_currentReply->error() != QNetworkReply::NoError) {
        emit errorOccurred("Network error: " + m_currentReply->errorString());
        emit updateCheckFinished(false);
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    // Read the response
    QByteArray responseData = m_currentReply->readAll();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    // Parse JSON
    QJsonParseError jsonError;
    QJsonDocument document = QJsonDocument::fromJson(responseData, &jsonError);

    if (jsonError.error != QJsonParseError::NoError) {
        QString errorMsg = "JSON parsing error: " + jsonError.errorString();
        ErrorManager::instance().handleError(errorMsg, "JSON Error");
        emit errorOccurred(errorMsg);
        emit updateCheckFinished(false);
        return;
    }

    // Process update info
    QJsonObject updateInfo = document.object();

    // Validate update info
    if (!validateUpdateInfo(updateInfo)) {
        emit errorOccurred("Invalid update information received");
        emit updateCheckFinished(false);
        return;
    }

    // Extract update info
    m_latestVersion = updateInfo["version"].toString();
    m_updateNotes = updateInfo["notes"].toString();
    m_updateFileUrl = QUrl(updateInfo["url"].toString());
    m_updateFileName = updateInfo["filename"].toString();
    m_updateChecksum = updateInfo["checksum"].toString();

    // Check if update is available by comparing version numbers
    m_updateAvailable = isNewerVersion(m_currentVersion, m_latestVersion);

    // Update file path
    m_updateFilePath = m_updateDir + "/" + m_updateFileName;

    // Check if update is already downloaded
    QFile updateFile(m_updateFilePath);
    if (updateFile.exists()) {
        QString existingChecksum = calculateFileChecksum(m_updateFilePath);
        m_updateDownloaded = (existingChecksum == m_updateChecksum);
    } else {
        m_updateDownloaded = false;
    }

    // Log results
    if (m_updateAvailable) {
        emit logMessage("Update available! Current: " + m_currentVersion +
                        ", Latest: " + m_latestVersion);
        if (m_updateDownloaded) {
            emit logMessage("Update is already downloaded and verified.");
        }
    } else {
        emit logMessage("No updates available. You are running the latest version.");
    }

    // Emit update check finished with result
    emit updateCheckFinished(m_updateAvailable);

    return;
}

bool UpdateManager::downloadUpdate()
{
    if (!m_updateAvailable) {
        emit errorOccurred("No update available to download");
        return false;
    }

    if (m_updateDownloaded) {
        emit logMessage("Update already downloaded.");
        emit updateDownloadFinished(true);
        return true;
    }

    // Ensure update directory exists
    QDir updateDir(m_updateDir);
    if (!updateDir.exists()) {
        updateDir.mkpath(".");
    }

    // Create the output file
    QFile* outputFile = new QFile(m_updateFilePath);
    if (!outputFile->open(QIODevice::WriteOnly)) {
        QString error = outputFile->errorString();
        delete outputFile;
        emit errorOccurred("Failed to create update file: " + error);
        return false;
    }

    // Create network request
    QNetworkRequest request(m_updateFileUrl);

    // Skip AWS authentication for S3 requests if the URL is a public bucket
    bool isPublicBucket = m_updateFileUrl.host().contains("s3.amazonaws.com");
    bool skipAuth = isPublicBucket;

    if (!skipAuth && !m_awsAccessKey.isEmpty() && !m_awsSecretKey.isEmpty()) {
        emit logMessage("Using AWS authentication for download");
        QByteArray authHeader = generateAuthorizationHeader(m_updateFileUrl, "GET");
        if (!authHeader.isEmpty()) {
            request.setRawHeader("Authorization", authHeader);
            request.setRawHeader("x-amz-date", QDateTime::currentDateTimeUtc().toString("yyyyMMddTHHmmssZ").toUtf8());
        }
    } else if (skipAuth) {
        emit logMessage("Skipping authentication for public S3 bucket");
    }

    // Start download
    emit updateDownloadStarted();
    emit logMessage("Starting download from: " + m_updateFileUrl.toString());

    m_currentReply = m_networkManager->get(request);

    // Connect signals
    connect(m_currentReply, &QNetworkReply::downloadProgress,
            this, &UpdateManager::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::readyRead, this, [this, outputFile]() {
        if (m_currentReply) {
            outputFile->write(m_currentReply->readAll());
        }
    });
    connect(m_currentReply, &QNetworkReply::finished, this, [this, outputFile]() {
        outputFile->close();
        delete outputFile;
        onDownloadFinished();
    });
    connect(m_currentReply, &QNetworkReply::errorOccurred, this, &UpdateManager::onNetworkError);

    return true;
}

void UpdateManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    // Calculate and display download progress
    QString received = formatBytes(bytesReceived);
    QString total = formatBytes(bytesTotal);
    double percentage = (bytesTotal > 0) ? (bytesReceived * 100.0 / bytesTotal) : 0;

    emit logMessage(QString("Downloading: %1 of %2 (%3%)").arg(received, total).arg(percentage, 0, 'f', 1));
    emit updateDownloadProgress(bytesReceived, bytesTotal);
}

void UpdateManager::onDownloadFinished()
{
    if (!m_currentReply) {
        emit errorOccurred("Invalid network reply");
        emit updateDownloadFinished(false);
        return;
    }

    // Check for network errors
    if (m_currentReply->error() != QNetworkReply::NoError) {
        emit errorOccurred("Download error: " + m_currentReply->errorString());
        emit updateDownloadFinished(false);

        // Delete the partial file
        if (QFile::exists(m_updateFilePath)) {
            try {
                FileUtils::safeRemoveFile(m_updateFilePath);
            } catch (const FileOperationException& e) {
                emit logMessage(QString("Failed to remove partial update file: %1").arg(e.message()));
            }
        }

        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    // Verify checksum
    QString fileChecksum = calculateFileChecksum(m_updateFilePath);
    if (fileChecksum != m_updateChecksum) {
        emit errorOccurred("Checksum verification failed");
        emit updateDownloadFinished(false);

        // Delete the corrupted file
        if (QFile::exists(m_updateFilePath)) {
            try {
                FileUtils::safeRemoveFile(m_updateFilePath);
            } catch (const FileOperationException& e) {
                emit logMessage(QString("Failed to remove corrupted update file: %1").arg(e.message()));
            }
        }
        return;
    }

    // Mark as downloaded
    m_updateDownloaded = true;

    emit logMessage("Download completed and verified.");
    emit updateDownloadFinished(true);
}

bool UpdateManager::applyUpdate()
{
    if (!m_updateDownloaded) {
        emit errorOccurred("No update downloaded to apply");
        return false;
    }

    emit updateInstallStarted();
    emit logMessage("Starting update installation...");

    // 1. Create a backup of the current application
    if (!backupCurrentApp()) {
        emit errorOccurred("Failed to create backup");
        emit updateInstallFinished(false);
        return false;
    }

    // 2. Extract the update package
    if (!extractUpdateFile()) {
        emit errorOccurred("Failed to extract update package");
        emit updateInstallFinished(false);
        return false;
    }

    // 3. Create a restart script to complete the update after the application closes
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString updateBatPath = tempDir + "/goji_update.bat";

    QFile updateBat(updateBatPath);
    if (!updateBat.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorOccurred("Failed to create update script");
        emit updateInstallFinished(false);
        return false;
    }

    // Wait for the application to close
    // Copy files from temp to app directory
    // Restart the application
    QTextStream out(&updateBat);
    out << "@echo off\n";
    out << "rem Wait for the application to close\n";
    out << "echo Waiting for application to close...\n";
    out << "timeout /t 2 /nobreak >nul\n";
    out << "set \"counter=0\"\n";
    out << ":wait_loop\n";
    out << "set /a \"counter+=1\"\n";
    out << "if %counter% gtr 30 goto :timeout\n";
    out << "tasklist | find /i \"GOJI.exe\" >nul\n";
    out << "if not errorlevel 1 (\n";
    out << "  timeout /t 1 /nobreak >nul\n";
    out << "  goto :wait_loop\n";
    out << ")\n";
    out << "echo Application closed, applying update...\n";
    out << "rem Copy extracted files to the application directory\n";
    out << "xcopy /s /y \"" << m_updateDir << "\\extracted\\*\" \"" << m_appDir << "\" >nul\n";
    out << "rem Start the updated application\n";
    out << "start \"\" \"" << m_appDir << "\\GOJI.exe\"\n";
    out << "echo Update completed!\n";
    out << "goto :end\n";
    out << ":timeout\n";
    out << "echo Timeout waiting for application to close.\n";
    out << ":end\n";
    out << "del \"%~f0\"\n";
    updateBat.close();

    // 4. Start the update script and close the application
    QProcess::startDetached("cmd.exe", QStringList() << "/c" << updateBatPath);

    emit logMessage("Update will be applied when the application closes.");
    emit updateInstallFinished(true);

    // Close the application after a short delay
    QTimer::singleShot(500, []() {
        QCoreApplication::quit();
    });

    return true;
}

QString UpdateManager::getCurrentVersion() const
{
    return m_currentVersion;
}

QString UpdateManager::getLatestVersion() const
{
    return m_latestVersion;
}

QString UpdateManager::getUpdateNotes() const
{
    return m_updateNotes;
}

bool UpdateManager::isUpdateAvailable() const
{
    return m_updateAvailable;
}

bool UpdateManager::isDownloaded() const
{
    return m_updateDownloaded;
}

void UpdateManager::onSslErrors(QNetworkReply* reply, const QList<QSslError>& errors)
{
    Q_UNUSED(reply); // Suppress unused parameter warning
    QString errorString;
    for (const QSslError& error : errors) {
        errorString += error.errorString() + "\n";
    }
    emit errorOccurred("SSL Error: " + errorString);
}

void UpdateManager::onNetworkError(QNetworkReply::NetworkError error)
{
    if (m_currentReply) {
        emit errorOccurred("Network Error: " + m_currentReply->errorString());
    } else {
        emit errorOccurred("Network Error: " + QString::number(error));
    }
}

QString UpdateManager::calculateFileChecksum(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("Failed to open file for checksum: " + filePath);
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        emit errorOccurred("Failed to compute checksum for: " + filePath);
        return QString();
    }

    return hash.result().toHex();
}

bool UpdateManager::extractUpdateFile()
{
    // Create extracted directory
    QString extractDir = m_updateDir + "/extracted";
    QDir dir(extractDir);
    if (dir.exists()) {
        // Remove old extracted files
        dir.removeRecursively();
    }
    dir.mkpath(".");

    // Check if 7-Zip is available
    QString sevenZipPath = "C:/Program Files/7-Zip/7z.exe";
    if (!QFile::exists(sevenZipPath)) {
        emit errorOccurred("7-Zip not found at: " + sevenZipPath);
        return false;
    }

    // Extract the update file (assuming it's a ZIP file)
    QProcess process;
    process.setWorkingDirectory(extractDir);

    QStringList args;
    args << "x" << "-y" << m_updateFilePath;

    process.start(sevenZipPath, args);
    if (!process.waitForStarted()) {
        emit errorOccurred("Failed to start 7-Zip: " + process.errorString());
        return false;
    }
    if (!process.waitForFinished(60000)) {
        emit errorOccurred("7-Zip process timed out");
        return false;
    }

    if (process.exitCode() != 0) {
        emit errorOccurred("7-Zip extraction failed with exit code: " + QString::number(process.exitCode()));
        return false;
    }

    emit logMessage("Update file extracted successfully to: " + extractDir);
    return true;
}

bool UpdateManager::backupCurrentApp()
{
    emit logMessage("Creating backup of current application");
    // Create a timestamped backup directory
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString backupPath = m_backupDir + "/backup_" + timestamp;

    try {
        FileUtils::ensureDirectoryExists(backupPath);
    } catch (const FileOperationException& e) {
        emit errorOccurred(QString("Failed to create backup directory: %1").arg(e.message()));
        return false;
    }

    // Copy the current application files to the backup directory
    QDir appDir(m_appDir);
    QStringList entryList = appDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& entry : entryList) {
        QString srcPath = m_appDir + "/" + entry;
        QString destPath = backupPath + "/" + entry;

        QFileInfo fileInfo(srcPath);
        if (fileInfo.isDir()) {
            QDir sourceDir(srcPath);
            QDir targetDir(destPath);
            try {
                FileUtils::ensureDirectoryExists(destPath);
            } catch (const FileOperationException& e) {
                emit logMessage(QString("Warning: Failed to create directory %1: %2").arg(destPath, e.message()));
                continue;
            }

            // Recursively copy directory
            QStringList dirFiles = sourceDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString& dirFile : dirFiles) {
                QString srcFilePath = srcPath + "/" + dirFile;
                QString destFilePath = destPath + "/" + dirFile;

                QFileInfo dirFileInfo(srcFilePath);
                if (dirFileInfo.isDir()) {
                    try {
                        FileUtils::ensureDirectoryExists(destFilePath);
                    } catch (const FileOperationException& e) {
                        emit logMessage(QString("Warning: Failed to create subdirectory %1: %2").arg(destFilePath, e.message()));
                        continue;
                    }
                } else {
                    try {
                        FileUtils::safeCopyFile(srcFilePath, destFilePath);
                    } catch (const FileOperationException& e) {
                        emit logMessage(QString("Warning: Failed to copy file %1: %2").arg(srcFilePath, e.message()));
                    }
                }
            }
        } else {
            try {
                FileUtils::safeCopyFile(srcPath, destPath);
            } catch (const FileOperationException& e) {
                emit logMessage(QString("Warning: Failed to copy file %1: %2").arg(srcPath, e.message()));
            }
        }
    }

    // Create a backup info file
    QFile infoFile(backupPath + "/backup_info.txt");
    if (infoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&infoFile);
        out << "Backup created: " << QDateTime::currentDateTime().toString() << "\n";
        out << "Application version: " << m_currentVersion << "\n";
        out << "Backup created before updating to: " << m_latestVersion << "\n";
        infoFile.close();
    } else {
        emit logMessage("Warning: Failed to create backup info file: " + infoFile.fileName());
    }

    emit logMessage("Backup completed: " + backupPath);
    return true;
}

bool UpdateManager::restoreBackup()
{
    // Find the latest backup
    QDir backupDir(m_backupDir);
    QStringList backups = backupDir.entryList(QStringList() << "backup_*", QDir::Dirs, QDir::Time);

    if (backups.isEmpty()) {
        emit errorOccurred("No backups available to restore");
        return false;
    }

    QString latestBackup = m_backupDir + "/" + backups.first();

    // Copy backup files back to the application directory
    QDir latestBackupDir(latestBackup);
    QStringList entryList = latestBackupDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& entry : entryList) {
        QString srcPath = latestBackup + "/" + entry;
        QString destPath = m_appDir + "/" + entry;

        QFileInfo fileInfo(srcPath);
        if (fileInfo.isDir()) {
            // Recursively copy directory
            QDir sourceDir(srcPath);
            QDir targetDir(destPath);
            try {
                FileUtils::ensureDirectoryExists(destPath);
            } catch (const FileOperationException& e) {
                emit logMessage(QString("Warning: Failed to create directory %1: %2").arg(destPath, e.message()));
                continue;
            }

            QStringList dirFiles = sourceDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString& dirFile : dirFiles) {
                QString srcFilePath = srcPath + "/" + dirFile;
                QString destFilePath = destPath + "/" + dirFile;

                QFileInfo dirFileInfo(srcFilePath);
                if (dirFileInfo.isDir()) {
                    try {
                        FileUtils::ensureDirectoryExists(destFilePath);
                    } catch (const FileOperationException& e) {
                        emit logMessage(QString("Warning: Failed to create subdirectory %1: %2").arg(destFilePath, e.message()));
                        continue;
                    }
                } else {
                    if (QFile::exists(destFilePath)) {
                        try {
                            FileUtils::safeRemoveFile(destFilePath);
                        } catch (const FileOperationException& e) {
                            emit logMessage(QString("Warning: Failed to remove existing file %1: %2").arg(destFilePath, e.message()));
                        }
                    }
                    try {
                        FileUtils::safeCopyFile(srcFilePath, destFilePath);
                    } catch (const FileOperationException& e) {
                        emit logMessage(QString("Warning: Failed to restore file %1: %2").arg(srcFilePath, e.message()));
                    }
                }
            }
        } else {
            // Skip backup_info.txt
            if (entry != "backup_info.txt") {
                if (QFile::exists(destPath)) {
                    try {
                        FileUtils::safeRemoveFile(destPath);
                    } catch (const FileOperationException& e) {
                        emit logMessage(QString("Warning: Failed to remove existing file %1: %2").arg(destPath, e.message()));
                    }
                }
                try {
                    FileUtils::safeCopyFile(srcPath, destPath);
                } catch (const FileOperationException& e) {
                    emit logMessage(QString("Warning: Failed to restore file %1: %2").arg(srcPath, e.message()));
                }
            }
        }
    }

    emit logMessage("Backup restored from: " + latestBackup);
    return true;
}

QByteArray UpdateManager::generateAuthorizationHeader(const QUrl& url, const QString& httpMethod) const
{
    // Implementing AWS Signature Version 4
    // Reference: https://docs.aws.amazon.com/AmazonS3/latest/API/sigv4-auth-using-authorization-header.html

    if (m_awsAccessKey.isEmpty() || m_awsSecretKey.isEmpty()) {
        emit logMessage("AWS credentials missing, skipping authentication");
        return QByteArray();
    }

    // Current time in UTC
    QDateTime now = QDateTime::currentDateTimeUtc();
    QString amzDate = now.toString("yyyyMMddTHHmmssZ");
    QString dateStamp = now.toString("yyyyMMdd");
    emit logMessage("AMZ Date: " + amzDate + ", Date Stamp: " + dateStamp);

    // Region and service name
    QString region = "us-east-1";
    QString serviceName = "s3";
    emit logMessage("Region: " + region + ", Service: " + serviceName);

    // Extract host from URL
    QString host = url.host();
    emit logMessage("Host: " + host);

    // Create canonical URI (ensure proper encoding)
    QString canonicalUri = url.path(QUrl::FullyEncoded);
    if (canonicalUri.isEmpty()) {
        canonicalUri = "/";
    }
    emit logMessage("Canonical URI: " + canonicalUri);

    // Create canonical query string
    QUrlQuery query(url);
    QList<QPair<QString, QString>> queryItems = query.queryItems(QUrl::FullyEncoded);
    std::sort(queryItems.begin(), queryItems.end());
    QStringList canonicalQueryParts;
    for (const auto& item : queryItems) {
        canonicalQueryParts.append(QUrl::toPercentEncoding(item.first) + "=" + QUrl::toPercentEncoding(item.second));
    }
    QString canonicalQueryString = canonicalQueryParts.join("&");
    emit logMessage("Canonical Query String: " + canonicalQueryString);

    // Create canonical headers (lowercase header names, trimmed values)
    QString canonicalHeaders = QString("host:%1\nx-amz-date:%2\n").arg(host.toLower(), amzDate);
    emit logMessage("Canonical Headers: " + canonicalHeaders);

    // Create signed headers
    QString signedHeaders = "host;x-amz-date";
    emit logMessage("Signed Headers: " + signedHeaders);

    // Create payload hash (empty for GET)
    QString payloadHash = QCryptographicHash::hash(QByteArray(), QCryptographicHash::Sha256).toHex().toLower();
    emit logMessage("Payload Hash: " + payloadHash);

    // Create canonical request
    QString canonicalRequest = QString("%1\n%2\n%3\n%4\n%5\n%6")
                                   .arg(httpMethod, canonicalUri, canonicalQueryString,
                                        canonicalHeaders, signedHeaders, payloadHash);
    emit logMessage("Canonical Request:\n" + canonicalRequest);

    // Create string to sign
    QString algorithm = "AWS4-HMAC-SHA256";
    QString credentialScope = QString("%1/%2/%3/aws4_request").arg(dateStamp, region, serviceName);
    QString stringToSign = QString("%1\n%2\n%3\n%4")
                               .arg(algorithm, amzDate, credentialScope,
                                    QCryptographicHash::hash(canonicalRequest.toUtf8(), QCryptographicHash::Sha256).toHex().toLower());
    emit logMessage("String to Sign:\n" + stringToSign);

    // Calculate signature
    QByteArray kDate = QMessageAuthenticationCode::hash(
        dateStamp.toUtf8(), QByteArray("AWS4" + m_awsSecretKey.toUtf8()), QCryptographicHash::Sha256);
    QByteArray kRegion = QMessageAuthenticationCode::hash(region.toUtf8(), kDate, QCryptographicHash::Sha256);
    QByteArray kService = QMessageAuthenticationCode::hash(serviceName.toUtf8(), kRegion, QCryptographicHash::Sha256);
    QByteArray kSigning = QMessageAuthenticationCode::hash(QByteArray("aws4_request"), kService, QCryptographicHash::Sha256);
    QByteArray signature = QMessageAuthenticationCode::hash(stringToSign.toUtf8(), kSigning, QCryptographicHash::Sha256).toHex().toLower();
    emit logMessage("Signature: " + QString(signature));

    // Create authorization header
    QString authHeader = QString("%1 Credential=%2/%3, SignedHeaders=%4, Signature=%5")
                             .arg(algorithm, m_awsAccessKey, credentialScope, signedHeaders, QString(signature));
    emit logMessage("Authorization Header: " + authHeader);

    return authHeader.toUtf8();
}

bool UpdateManager::validateUpdateInfo(const QJsonObject& updateInfo) const
{
    // Check required fields
    QStringList requiredFields = {"version", "url", "filename", "checksum"};
    for (const QString& field : requiredFields) {
        if (!updateInfo.contains(field) || updateInfo[field].toString().isEmpty()) {
            const_cast<UpdateManager*>(this)->logMessage("Validation failed: Missing or empty field: " + field);
            return false;
        }
    }

    return true;
}

QString UpdateManager::generateS3Url(const QString& bucket, const QString& objectKey) const
{
    // Format: https://bucket-name.s3.amazonaws.com/object-key
    return QString("https://%1.s3.amazonaws.com/%2")
        .arg(bucket)
        .arg(objectKey);
}

QString UpdateManager::formatBytes(qint64 bytes) const
{
    const qint64 kb = 1024;
    const qint64 mb = kb * 1024;
    const qint64 gb = mb * 1024;

    if (bytes >= gb) {
        return QString("%1 GB").arg(QString::number(bytes / static_cast<double>(gb), 'f', 2));
    } else if (bytes >= mb) {
        return QString("%1 MB").arg(QString::number(bytes / static_cast<double>(mb), 'f', 2));
    } else if (bytes >= kb) {
        return QString("%1 KB").arg(QString::number(bytes / static_cast<double>(kb), 'f', 2));
    } else {
        return QString("%1 bytes").arg(bytes);
    }
}

bool UpdateManager::isNewerVersion(const QString& current, const QString& latest) const
{
    emit logMessage("Comparing versions: Current=" + current + ", Latest=" + latest);

    // Split version into components and compare numerically
    QStringList currentParts = current.split('.');
    QStringList latestParts = latest.split('.');

    // Ensure we have at least 3 parts (major.minor.patch)
    while (currentParts.size() < 3) currentParts.append("0");
    while (latestParts.size() < 3) latestParts.append("0");

    // Compare major.minor.patch numerically
    for (int i = 0; i < 3; ++i) {
        bool currentOk, latestOk;
        int currentNum = currentParts.at(i).toInt(&currentOk);
        int latestNum = latestParts.at(i).toInt(&latestOk);

        // If either part isn't a valid number, fall back to string comparison
        if (!currentOk || !latestOk) {
            return latest.compare(current, Qt::CaseInsensitive) > 0;
        }

        if (latestNum > currentNum) {
            emit logMessage("Latest version has higher component at position " + QString::number(i));
            return true;
        }
        if (latestNum < currentNum) {
            emit logMessage("Current version has higher component at position " + QString::number(i));
            return false;
        }
    }

    // If all numeric components are equal, check if there's a fourth component
    if (currentParts.size() > 3 && latestParts.size() > 3) {
        bool currentOk, latestOk;
        int currentNum = currentParts.at(3).toInt(&currentOk);
        int latestNum = latestParts.at(3).toInt(&latestOk);

        if (currentOk && latestOk) {
            return latestNum > currentNum;
        }
    } else if (latestParts.size() > currentParts.size()) {
        // More components in latest = newer
        return true;
    } else if (currentParts.size() > latestParts.size()) {
        // More components in current = newer
        return false;
    }

    // Versions are exactly equal
    return false;
}
