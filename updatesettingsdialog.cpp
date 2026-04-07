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
#include <stdexcept>

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
    QString serverUrl = m_settings->value("UpdateServerUrl", "https://punchyouinthenuts.github.io/GOJI/updates").toString();
    m_serverUrlLineEdit->setText(serverUrl);

    // Load update info file
    QString infoFile = m_settings->value("UpdateInfoFile", "latest.json").toString();
    m_infoFileLineEdit->setText(infoFile);

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

    return true;
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

                // Check for required fields (legacy top-level url OR packages.full.url)
                bool hasLegacyUrl = updateInfo.contains("url") &&
                                    updateInfo.value("url").isString() &&
                                    !updateInfo.value("url").toString().trimmed().isEmpty();
                bool hasStructuredFullUrl = false;
                if (updateInfo.contains("packages") && updateInfo.value("packages").isObject()) {
                    const QJsonObject packages = updateInfo.value("packages").toObject();
                    if (packages.contains("full") && packages.value("full").isObject()) {
                        const QJsonObject full = packages.value("full").toObject();
                        hasStructuredFullUrl = full.contains("url") &&
                                               full.value("url").isString() &&
                                               !full.value("url").toString().trimmed().isEmpty();
                    }
                }

                if (updateInfo.contains("version") && (hasLegacyUrl || hasStructuredFullUrl)) {
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
