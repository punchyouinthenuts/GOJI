#include "updatesettingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QDateTime>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QFileInfo>
#include <QDir>

UpdateSettingsDialog::UpdateSettingsDialog(QSettings* settings, QWidget* parent)
    : QDialog(parent), m_settings(settings)
{
    if (!m_settings) {
        qCritical("UpdateSettingsDialog: settings pointer is null");
        throw std::invalid_argument("Settings pointer cannot be null");
    }

    setWindowTitle(tr("Update Settings"));

    setupUI();
    loadSettings();
}

UpdateSettingsDialog::~UpdateSettingsDialog()
{
    // No additional cleanup needed
}

void UpdateSettingsDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Update settings group
    QGroupBox* updateGroupBox = new QGroupBox(tr("Update Settings"), this);
    QVBoxLayout* updateLayout = new QVBoxLayout(updateGroupBox);

    // Check on startup
    m_checkOnStartupCheckBox = new QCheckBox(tr("Check for updates on startup"), this);
    m_checkOnStartupCheckBox->setToolTip(tr("When enabled, the application will check for updates each time it starts"));
    updateLayout->addWidget(m_checkOnStartupCheckBox);

    // Check interval
    QHBoxLayout* intervalLayout = new QHBoxLayout();
    QLabel* intervalLabel = new QLabel(tr("Check for updates every:"), this);
    intervalLayout->addWidget(intervalLabel);

    m_checkIntervalComboBox = new QComboBox(this);
    m_checkIntervalComboBox->addItem(tr("Day"), 1);
    m_checkIntervalComboBox->addItem(tr("Week"), 7);
    m_checkIntervalComboBox->addItem(tr("Month"), 30);
    m_checkIntervalComboBox->setMinimumWidth(120);
    m_checkIntervalComboBox->setToolTip(tr("How frequently the application should check for updates"));
    intervalLayout->addWidget(m_checkIntervalComboBox);
    intervalLayout->addStretch();

    updateLayout->addLayout(intervalLayout);

    // Server URL
    QHBoxLayout* serverLayout = new QHBoxLayout();
    QLabel* serverLabel = new QLabel(tr("Update server URL:"), this);
    serverLayout->addWidget(serverLabel);

    m_serverUrlLineEdit = new QLineEdit(this);
    m_serverUrlLineEdit->setMinimumWidth(300);
    m_serverUrlLineEdit->setToolTip(tr("URL of the update server (e.g., https://example.com)"));
    serverLayout->addWidget(m_serverUrlLineEdit);

    m_testConnectionButton = new QPushButton(tr("Test"), this);
    m_testConnectionButton->setToolTip(tr("Test connection to the update server"));
    serverLayout->addWidget(m_testConnectionButton);

    updateLayout->addLayout(serverLayout);

    // Update Info File
    QHBoxLayout* infoFileLayout = new QHBoxLayout();
    QLabel* infoFileLabel = new QLabel(tr("Update info file:"), this);
    infoFileLayout->addWidget(infoFileLabel);

    m_infoFileLineEdit = new QLineEdit(this);
    m_infoFileLineEdit->setMinimumWidth(300);
    m_infoFileLineEdit->setToolTip(tr("Name of the file containing update information (e.g., latest.json)"));
    infoFileLayout->addWidget(m_infoFileLineEdit);
    infoFileLayout->addStretch();

    updateLayout->addLayout(infoFileLayout);

    // AWS Credentials Path
    QHBoxLayout* credentialsLayout = new QHBoxLayout();
    QLabel* credentialsLabel = new QLabel(tr("AWS credentials path:"), this);
    credentialsLayout->addWidget(credentialsLabel);

    m_credentialsPathLineEdit = new QLineEdit(this);
    m_credentialsPathLineEdit->setMinimumWidth(300);
    m_credentialsPathLineEdit->setToolTip(tr("Path to the AWS credentials file with owner-only permissions"));
    credentialsLayout->addWidget(m_credentialsPathLineEdit);
    credentialsLayout->addStretch();

    updateLayout->addLayout(credentialsLayout);

    // Add update group to main layout
    mainLayout->addWidget(updateGroupBox);

    // Add spacer
    mainLayout->addStretch();

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    m_saveButton = new QPushButton(tr("Save"), this);
    m_saveButton->setDefault(true);

    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_saveButton);

    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(m_saveButton, &QPushButton::clicked, this, &UpdateSettingsDialog::onSaveClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &UpdateSettingsDialog::onCancelClicked);
    connect(m_testConnectionButton, &QPushButton::clicked, this, &UpdateSettingsDialog::onTestConnectionClicked);

    // Set minimum dialog size
    setMinimumSize(500, 350);
}

void UpdateSettingsDialog::loadSettings()
{
    // Load check on startup setting
    bool checkOnStartup = m_settings->value("Updates/CheckOnStartup", true).toBool();
    m_checkOnStartupCheckBox->setChecked(checkOnStartup);

    // Load check interval setting
    int checkIntervalDays = m_settings->value("Updates/CheckIntervalDays", 1).toInt();

    // Ensure interval is positive
    if (checkIntervalDays <= 0) {
        checkIntervalDays = 1;
    }

    int comboIndex = 0; // Default to daily

    if (checkIntervalDays >= 30) {
        comboIndex = 2; // Monthly
    } else if (checkIntervalDays >= 7) {
        comboIndex = 1; // Weekly
    }

    m_checkIntervalComboBox->setCurrentIndex(comboIndex);

    // Load server URL
    QString serverUrl = m_settings->value("UpdateServerUrl", "https://goji-updates.s3.amazonaws.com").toString();
    m_serverUrlLineEdit->setText(serverUrl);

    // Load update info file
    QString infoFile = m_settings->value("UpdateInfoFile", "latest.json").toString();
    m_infoFileLineEdit->setText(infoFile);

    // Load AWS credentials path
    QString credentialsPath = m_settings->value("AwsCredentialsPath",
                                                QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/aws_credentials.json").toString();
    m_credentialsPathLineEdit->setText(credentialsPath);
}

bool UpdateSettingsDialog::validateSettings()
{
    // Validate server URL
    QString serverUrl = m_serverUrlLineEdit->text().trimmed();
    if (serverUrl.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Input"), tr("Please enter a valid server URL."));
        m_serverUrlLineEdit->setFocus();
        return false;
    }

    QUrl url(serverUrl);
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")) {
        QMessageBox::warning(this, tr("Invalid URL"),
                             tr("The server URL must be a valid HTTP or HTTPS URL."));
        m_serverUrlLineEdit->setFocus();
        return false;
    }

    // Validate update info file
    QString infoFile = m_infoFileLineEdit->text().trimmed();
    if (infoFile.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Input"), tr("Please enter a valid update info file name."));
        m_infoFileLineEdit->setFocus();
        return false;
    }

    // Validate credentials path
    QString credentialsPath = m_credentialsPathLineEdit->text().trimmed();
    if (credentialsPath.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Input"), tr("Please enter a valid AWS credentials path."));
        m_credentialsPathLineEdit->setFocus();
        return false;
    }

    return true;
}

bool UpdateSettingsDialog::validateCredentialsFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);

    // Create directory if it doesn't exist
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            return false;
        }
    }

    // If file doesn't exist yet, that's okay
    if (!fileInfo.exists()) {
        return true;
    }

    // Check if file is readable
    if (!fileInfo.isReadable()) {
        return false;
    }

    // Check permissions (owner read/write only)
    QFile::Permissions perms = QFile::permissions(filePath);
    QFile::Permissions ownerRW = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
    QFile::Permissions unwantedPerms = ~ownerRW &
                                       (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                                        QFileDevice::ReadOther | QFileDevice::WriteOther |
                                        QFileDevice::ExeOwner | QFileDevice::ExeGroup |
                                        QFileDevice::ExeOther);

    // Return true if no unwanted permissions are set
    return !(perms & unwantedPerms);
}

bool UpdateSettingsDialog::secureCredentialsFile(const QString& filePath)
{
    // Create directory structure if needed
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            return false;
        }
    }

    // Set permissions (owner read/write only)
    return QFile::setPermissions(filePath,
                                 QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

void UpdateSettingsDialog::onSaveClicked()
{
    // Validate inputs before saving
    if (!validateSettings()) {
        return;
    }

    // Save check on startup setting
    m_settings->setValue("Updates/CheckOnStartup", m_checkOnStartupCheckBox->isChecked());

    // Save check interval setting
    int intervalDays = m_checkIntervalComboBox->currentData().toInt();
    m_settings->setValue("Updates/CheckIntervalDays", intervalDays);

    // Save server URL
    m_settings->setValue("UpdateServerUrl", m_serverUrlLineEdit->text().trimmed());

    // Save update info file
    m_settings->setValue("UpdateInfoFile", m_infoFileLineEdit->text().trimmed());

    // Save AWS credentials path
    QString credentialsPath = m_credentialsPathLineEdit->text().trimmed();
    m_settings->setValue("AwsCredentialsPath", credentialsPath);

    // Secure credentials file if it exists
    QFile credFile(credentialsPath);
    if (credFile.exists()) {
        if (!secureCredentialsFile(credentialsPath)) {
            QMessageBox::warning(this, tr("Security Warning"),
                                 tr("Could not set secure permissions on the credentials file. "
                                    "Please check file permissions manually."));
        }
    }

    // Ensure settings are written to disk
    m_settings->sync();

    accept();
}

void UpdateSettingsDialog::onCancelClicked()
{
    reject();
}

void UpdateSettingsDialog::onTestConnectionClicked()
{
    QString serverUrl = m_serverUrlLineEdit->text().trimmed();
    QString infoFile = m_infoFileLineEdit->text().trimmed();
    QString credentialsPath = m_credentialsPathLineEdit->text().trimmed();

    if (serverUrl.isEmpty() || infoFile.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Input"), tr("Please enter a valid server URL and update info file."));
        return;
    }

    // Validate URL scheme
    QUrl url(serverUrl);
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")) {
        QMessageBox::warning(this, tr("Invalid URL"),
                             tr("The server URL must be a valid HTTP or HTTPS URL."));
        return;
    }

    // Check credentials file
    if (!credentialsPath.isEmpty()) {
        QFileInfo fileInfo(credentialsPath);
        if (fileInfo.exists()) {
            if (!validateCredentialsFile(credentialsPath)) {
                QMessageBox::warning(this, tr("Security Warning"),
                                     tr("The credentials file has insecure permissions. "
                                        "It should only be readable by the owner."));

                // Ask if user wants to fix permissions
                auto result = QMessageBox::question(this, tr("Fix Permissions?"),
                                                    tr("Would you like to fix the permissions on this file now?"),
                                                    QMessageBox::Yes | QMessageBox::No);
                if (result == QMessageBox::Yes) {
                    if (!secureCredentialsFile(credentialsPath)) {
                        QMessageBox::critical(this, tr("Error"),
                                              tr("Failed to set secure permissions on the credentials file."));
                        return;
                    }
                }
            }
        }
    }

    // Disable test button during the check
    m_testConnectionButton->setEnabled(false);
    m_testConnectionButton->setText(tr("Testing..."));

    // Create network manager for the test
    QNetworkAccessManager* networkManager = new QNetworkAccessManager(this);

    // Construct the test URL
    QString latestJsonUrl = serverUrl;
    if (!latestJsonUrl.endsWith("/")) {
        latestJsonUrl += "/";
    }
    latestJsonUrl += infoFile;
    QUrl testUrl(latestJsonUrl);

    if (!testUrl.isValid()) {
        QMessageBox::warning(this, tr("Invalid URL"), tr("The URL entered is not valid."));
        m_testConnectionButton->setEnabled(true);
        m_testConnectionButton->setText(tr("Test"));
        networkManager->deleteLater();
        return;
    }

    // Create request
    QNetworkRequest request(testUrl);

    // Set timeout for request
    request.setTransferTimeout(10000); // 10 seconds timeout

    // Add AWS Authentication if credentials are available
    QString awsAccessKey, awsSecretKey;
    QFile credFile(credentialsPath);
    if (credFile.exists() && credFile.open(QIODevice::ReadOnly)) {
        QJsonDocument credDoc = QJsonDocument::fromJson(credFile.readAll());
        QJsonObject credObj = credDoc.object();
        awsAccessKey = credObj["aws_access_key_id"].toString();
        awsSecretKey = credObj["aws_secret_access_key"].toString();
        credFile.close();

        if (!awsAccessKey.isEmpty() && !awsSecretKey.isEmpty()) {
            // AWS Signature V4 (simplified version based on UpdateManager::generateAuthorizationHeader)
            QDateTime now = QDateTime::currentDateTimeUtc();
            QString amzDate = now.toString("yyyyMMddTHHmmssZ");
            QString dateStamp = now.toString("yyyyMMdd");
            QString region = "us-east-1";
            QString serviceName = "s3";
            QString host = testUrl.host();
            QString canonicalUri = testUrl.path(QUrl::FullyEncoded);
            if (canonicalUri.isEmpty()) {
                canonicalUri = "/";
            }

            QUrlQuery query(testUrl);
            QList<QPair<QString, QString>> queryItems = query.queryItems(QUrl::FullyEncoded);
            std::sort(queryItems.begin(), queryItems.end());
            QStringList canonicalQueryParts;
            for (const auto& item : std::as_const(queryItems)) {
                canonicalQueryParts.append(item.first + "=" + item.second);
            }
            QString canonicalQueryString = canonicalQueryParts.join("&");

            QString canonicalHeaders = "host:" + host + "\n" + "x-amz-date:" + amzDate + "\n";
            QString signedHeaders = "host;x-amz-date";
            QString payloadHash = QCryptographicHash::hash(QByteArray(), QCryptographicHash::Sha256).toHex();
            QString canonicalRequest = "GET\n" + canonicalUri + "\n" + canonicalQueryString + "\n" +
                                       canonicalHeaders + "\n" + signedHeaders + "\n" + payloadHash;

            QString algorithm = "AWS4-HMAC-SHA256";
            QString credentialScope = dateStamp + "/" + region + "/" + serviceName + "/aws4_request";
            QString stringToSign = algorithm + "\n" + amzDate + "\n" + credentialScope + "\n" +
                                   QCryptographicHash::hash(canonicalRequest.toUtf8(), QCryptographicHash::Sha256).toHex();

            QByteArray kDate = QMessageAuthenticationCode::hash(
                dateStamp.toUtf8(), QByteArray("AWS4" + awsSecretKey.toUtf8()), QCryptographicHash::Sha256);
            QByteArray kRegion = QMessageAuthenticationCode::hash(region.toUtf8(), kDate, QCryptographicHash::Sha256);
            QByteArray kService = QMessageAuthenticationCode::hash(serviceName.toUtf8(), kRegion, QCryptographicHash::Sha256);
            QByteArray kSigning = QMessageAuthenticationCode::hash(QByteArray("aws4_request"), kService, QCryptographicHash::Sha256);
            QByteArray signature = QMessageAuthenticationCode::hash(stringToSign.toUtf8(), kSigning, QCryptographicHash::Sha256).toHex();

            QString authHeader = algorithm + " Credential=" + awsAccessKey + "/" + credentialScope +
                                 ", SignedHeaders=" + signedHeaders + ", Signature=" + signature;
            request.setRawHeader("Authorization", authHeader.toUtf8());
            request.setRawHeader("x-amz-date", amzDate.toUtf8());
        }
    }

    // Send request
    QNetworkReply* reply = networkManager->get(request);

    // Add error handling connection
    connect(reply, &QNetworkReply::errorOccurred, this, [=](QNetworkReply::NetworkError /*error*/) {
        m_testConnectionButton->setEnabled(true);
        m_testConnectionButton->setText(tr("Test"));
        QMessageBox::critical(this, tr("Connection Failed"),
                              tr("Network error: %1").arg(reply->errorString()));
        reply->deleteLater();
        networkManager->deleteLater();
    });

    // Handle response
    connect(reply, &QNetworkReply::finished, this, [=]() {
        m_testConnectionButton->setEnabled(true);
        m_testConnectionButton->setText(tr("Test"));

        if (reply->error() == QNetworkReply::NoError) {
            // Check HTTP status code
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (statusCode >= 400) {
                QMessageBox::critical(this, tr("Connection Failed"),
                                      tr("HTTP error: %1 - %2").arg(statusCode)
                                          .arg(reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()));
                reply->deleteLater();
                networkManager->deleteLater();
                return;
            }

            // Try to parse the response as JSON
            QByteArray responseData = reply->readAll();
            QJsonParseError jsonError;
            QJsonDocument document = QJsonDocument::fromJson(responseData, &jsonError);

            if (jsonError.error == QJsonParseError::NoError && document.isObject()) {
                QJsonObject updateInfo = document.object();

                // Check for required fields
                if (updateInfo.contains("version") && updateInfo.contains("url")) {
                    QString version = updateInfo["version"].toString();
                    QMessageBox::information(this, tr("Connection Successful"),
                                             tr("Successfully connected to update server.\nLatest version: %1").arg(version));
                } else {
                    QMessageBox::warning(this, tr("Invalid Response"),
                                         tr("The server responded with invalid update information."));
                }
            } else {
                QMessageBox::warning(this, tr("Invalid Response"),
                                     tr("The server response could not be parsed as JSON: %1").arg(jsonError.errorString()));
            }
        } else {
            QMessageBox::critical(this, tr("Connection Failed"),
                                  tr("Failed to connect to update server: %1").arg(reply->errorString()));
        }

        reply->deleteLater();
        networkManager->deleteLater();
    });
}
