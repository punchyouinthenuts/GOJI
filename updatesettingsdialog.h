#ifndef UPDATESETTINGSDIALOG_H
#define UPDATESETTINGSDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

/**
 * @brief Dialog for configuring application update settings
 *
 * This dialog allows the user to configure settings related to
 * automatic updates, including check frequency and server configuration.
 */
class UpdateSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param settings Pointer to application settings
     * @param parent Parent widget
     */
    explicit UpdateSettingsDialog(QSettings* settings, QWidget* parent = nullptr);
    ~UpdateSettingsDialog();

private slots:
    /**
     * @brief Saves settings and closes dialog
     */
    void onSaveClicked();

    /**
     * @brief Cancels changes and closes dialog
     */
    void onCancelClicked();

    /**
     * @brief Tests connection to update server
     */
    void onTestConnectionClicked();

private:
    QSettings* m_settings;
    QCheckBox* m_checkOnStartupCheckBox;
    QComboBox* m_checkIntervalComboBox;
    QLineEdit* m_serverUrlLineEdit;
    QLineEdit* m_infoFileLineEdit;
    QLineEdit* m_credentialsPathLineEdit;
    QPushButton* m_saveButton;
    QPushButton* m_cancelButton;
    QPushButton* m_testConnectionButton;

    /**
     * @brief Loads settings from QSettings
     */
    void loadSettings();

    /**
     * @brief Sets up the dialog UI elements
     */
    void setupUI();

    /**
     * @brief Validates credentials file permissions
     * @param filePath Path to the credentials file
     * @return True if permissions are secure, false otherwise
     */
    bool validateCredentialsFile(const QString& filePath);

    /**
     * @brief Sets secure permissions on the credentials file
     * @param filePath Path to the credentials file
     * @return True if permissions were set successfully, false otherwise
     */
    bool secureCredentialsFile(const QString& filePath);

    /**
     * @brief Validates input before saving
     * @return True if all inputs are valid, false otherwise
     */
    bool validateSettings();
};

#endif // UPDATESETTINGSDIALOG_H
