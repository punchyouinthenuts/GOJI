#include "updatedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QApplication>
#include <QStyle>
#include <QTextBrowser>
#include <QSettings>
#include <QDateTime>

UpdateDialog::UpdateDialog(UpdateManager* updateManager, QWidget* parent)
    : QDialog(parent), m_updateManager(updateManager)
{
    setWindowTitle(tr("Software Update"));
    setMinimumSize(500, 350);

    // Connect update manager signals
    connect(m_updateManager, &UpdateManager::updateCheckStarted,
            this, &UpdateDialog::onUpdateCheckStarted);
    connect(m_updateManager, &UpdateManager::updateCheckFinished,
            this, &UpdateDialog::onUpdateCheckFinished);
    connect(m_updateManager, &UpdateManager::updateDownloadStarted,
            this, &UpdateDialog::onUpdateDownloadStarted);
    connect(m_updateManager, &UpdateManager::updateDownloadProgress,
            this, &UpdateDialog::onUpdateDownloadProgress);
    connect(m_updateManager, &UpdateManager::updateDownloadFinished,
            this, &UpdateDialog::onUpdateDownloadFinished);
    connect(m_updateManager, &UpdateManager::updateInstallStarted,
            this, &UpdateDialog::onUpdateInstallStarted);
    connect(m_updateManager, &UpdateManager::updateInstallFinished,
            this, &UpdateDialog::onUpdateInstallFinished);
    connect(m_updateManager, &UpdateManager::errorOccurred,
            this, &UpdateDialog::onErrorOccurred);

    setupUI();
    updateUI();

    // Check for updates automatically when dialog is created
    QTimer::singleShot(100, this, &UpdateDialog::onCheckForUpdatesClicked);
}

UpdateDialog::~UpdateDialog()
{
}

void UpdateDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Status label with icon
    QHBoxLayout* statusLayout = new QHBoxLayout();
    QLabel* iconLabel = new QLabel(this);
    iconLabel->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxInformation).pixmap(32, 32));
    statusLayout->addWidget(iconLabel);

    m_statusLabel = new QLabel(tr("Checking for updates..."), this);
    m_statusLabel->setWordWrap(true);
    QFont statusFont = m_statusLabel->font();
    statusFont.setBold(true);
    statusFont.setPointSize(statusFont.pointSize() + 1);
    m_statusLabel->setFont(statusFont);
    statusLayout->addWidget(m_statusLabel, 1);

    mainLayout->addLayout(statusLayout);

    // Version info
    QGridLayout* versionLayout = new QGridLayout();
    versionLayout->addWidget(new QLabel(tr("Current Version:"), this), 0, 0);
    m_currentVersionLabel = new QLabel(m_updateManager->getCurrentVersion(), this);
    versionLayout->addWidget(m_currentVersionLabel, 0, 1);

    versionLayout->addWidget(new QLabel(tr("Latest Version:"), this), 1, 0);
    m_latestVersionLabel = new QLabel(tr("Unknown"), this);
    versionLayout->addWidget(m_latestVersionLabel, 1, 1);

    mainLayout->addLayout(versionLayout);

    // Release notes
    mainLayout->addWidget(new QLabel(tr("Release Notes:"), this));
    m_notesBrowser = new QTextBrowser(this);
    m_notesBrowser->setReadOnly(true);
    m_notesBrowser->setMinimumHeight(120);
    m_notesBrowser->setMinimumWidth(450);
    mainLayout->addWidget(m_notesBrowser);

    // Progress bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_checkButton = new QPushButton(tr("Check for Updates"), this);
    m_downloadButton = new QPushButton(tr("Download Update"), this);
    m_installButton = new QPushButton(tr("Install Update"), this);
    m_remindLaterButton = new QPushButton(tr("Remind Me Later"), this);
    m_skipButton = new QPushButton(tr("Skip This Version"), this);

    buttonLayout->addWidget(m_checkButton);
    buttonLayout->addWidget(m_downloadButton);
    buttonLayout->addWidget(m_installButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_remindLaterButton);
    buttonLayout->addWidget(m_skipButton);

    mainLayout->addLayout(buttonLayout);

    // Connect button signals
    connect(m_checkButton, &QPushButton::clicked, this, &UpdateDialog::onCheckForUpdatesClicked);
    connect(m_downloadButton, &QPushButton::clicked, this, &UpdateDialog::onDownloadUpdateClicked);
    connect(m_installButton, &QPushButton::clicked, this, &UpdateDialog::onInstallUpdateClicked);
    connect(m_remindLaterButton, &QPushButton::clicked, this, &UpdateDialog::onRemindLaterClicked);
    connect(m_skipButton, &QPushButton::clicked, this, &UpdateDialog::onSkipUpdateClicked);

    // Set the release notes to display in the browser
    connect(m_updateManager, &UpdateManager::updateCheckFinished, this, [this](bool available) {
        if (available) {
            m_notesBrowser->setHtml(m_updateManager->getUpdateNotes());
        }
    });
}

void UpdateDialog::updateUI()
{
    // Update version labels
    m_currentVersionLabel->setText(m_updateManager->getCurrentVersion());
    m_latestVersionLabel->setText(m_updateManager->isUpdateAvailable() ?
                                      m_updateManager->getLatestVersion() : tr("Unknown"));

    // Update buttons based on state
    bool updateAvailable = m_updateManager->isUpdateAvailable();
    bool updateDownloaded = m_updateManager->isDownloaded();

    m_checkButton->setEnabled(true);
    m_downloadButton->setEnabled(updateAvailable && !updateDownloaded);
    m_installButton->setEnabled(updateAvailable && updateDownloaded);
    m_remindLaterButton->setEnabled(updateAvailable);
    m_skipButton->setEnabled(updateAvailable);

    // Hide/show buttons based on state
    m_downloadButton->setVisible(updateAvailable);
    m_installButton->setVisible(updateAvailable);
    m_remindLaterButton->setVisible(updateAvailable);
    m_skipButton->setVisible(updateAvailable);
}

void UpdateDialog::onCheckForUpdatesClicked()
{
    m_checkButton->setEnabled(false);
    m_statusLabel->setText(tr("Checking for updates..."));
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // Indeterminate progress
    m_updateManager->checkForUpdates();
}

void UpdateDialog::onDownloadUpdateClicked()
{
    m_downloadButton->setEnabled(false);
    m_statusLabel->setText(tr("Downloading update..."));
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_updateManager->downloadUpdate();
}

void UpdateDialog::onInstallUpdateClicked()
{
    // Confirm installation
    int result = QMessageBox::question(
        this,
        tr("Install Update"),
        tr("The application will close and update to version %1. Continue?")
            .arg(m_updateManager->getLatestVersion()),
        QMessageBox::Yes | QMessageBox::No
        );

    if (result == QMessageBox::Yes) {
        m_installButton->setEnabled(false);
        m_statusLabel->setText(tr("Installing update..."));
        m_progressBar->setVisible(true);
        m_progressBar->setRange(0, 0); // Indeterminate progress
        m_updateManager->applyUpdate();
    }
}

void UpdateDialog::onRemindLaterClicked()
{
    // Save last check time in settings
    QSettings settings;
    settings.setValue("Updates/LastCheckTime", QDateTime::currentDateTime());
    settings.setValue("Updates/LastCheckVersion", m_updateManager->getLatestVersion());
    settings.setValue("Updates/RemindLater", true);

    // Close dialog
    accept();
}

void UpdateDialog::onSkipUpdateClicked()
{
    // Save skipped version in settings
    QSettings settings;
    settings.setValue("Updates/SkippedVersion", m_updateManager->getLatestVersion());

    // Close dialog
    accept();
}

void UpdateDialog::onUpdateCheckStarted()
{
    m_statusLabel->setText(tr("Checking for updates..."));
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // Indeterminate progress
    m_checkButton->setEnabled(false);
}

void UpdateDialog::onUpdateCheckFinished(bool available)
{
    m_progressBar->setVisible(false);
    m_checkButton->setEnabled(true);

    if (available) {
        m_statusLabel->setText(tr("Update available: version %1")
                                   .arg(m_updateManager->getLatestVersion()));
        m_latestVersionLabel->setText(m_updateManager->getLatestVersion());

        // Check if it's already downloaded
        if (m_updateManager->isDownloaded()) {
            m_downloadButton->setEnabled(false);
            m_installButton->setEnabled(true);
        } else {
            m_downloadButton->setEnabled(true);
            m_installButton->setEnabled(false);
        }

        m_downloadButton->setVisible(true);
        m_installButton->setVisible(true);
        m_remindLaterButton->setVisible(true);
        m_skipButton->setVisible(true);
    } else {
        m_statusLabel->setText(tr("You have the latest version (%1).")
                                   .arg(m_updateManager->getCurrentVersion()));
        m_downloadButton->setVisible(false);
        m_installButton->setVisible(false);
        m_remindLaterButton->setVisible(false);
        m_skipButton->setVisible(false);
    }
}

void UpdateDialog::onUpdateDownloadStarted()
{
    m_statusLabel->setText(tr("Downloading update..."));
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_downloadButton->setEnabled(false);
}

void UpdateDialog::onUpdateDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        int percentage = static_cast<int>((bytesReceived * 100) / bytesTotal);
        m_progressBar->setValue(percentage);
    }
}

void UpdateDialog::onUpdateDownloadFinished(bool success)
{
    m_progressBar->setVisible(false);

    if (success) {
        m_statusLabel->setText(tr("Update downloaded successfully. Ready to install."));
        m_downloadButton->setEnabled(false);
        m_installButton->setEnabled(true);
    } else {
        m_statusLabel->setText(tr("Download failed. Please try again."));
        m_downloadButton->setEnabled(true);
    }
}

void UpdateDialog::onUpdateInstallStarted()
{
    m_statusLabel->setText(tr("Installing update..."));
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // Indeterminate progress
    m_installButton->setEnabled(false);
}

void UpdateDialog::onUpdateInstallFinished(bool success)
{
    m_progressBar->setVisible(false);

    if (success) {
        m_statusLabel->setText(tr("Update will be installed when the application restarts."));
        // Application will close itself
    } else {
        m_statusLabel->setText(tr("Installation failed. Please try again."));
        m_installButton->setEnabled(true);
    }
}

void UpdateDialog::onErrorOccurred(const QString& errorMessage)
{
    m_progressBar->setVisible(false);
    m_statusLabel->setText(tr("Error: %1").arg(errorMessage));

    // Re-enable buttons
    m_checkButton->setEnabled(true);
    if (m_updateManager->isUpdateAvailable()) {
        m_downloadButton->setEnabled(!m_updateManager->isDownloaded());
        m_installButton->setEnabled(m_updateManager->isDownloaded());
    }
}
