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

class UpdateSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateSettingsDialog(QSettings* settings, QWidget* parent = nullptr);
    ~UpdateSettingsDialog();

private slots:
    void onSaveClicked();
    void onCancelClicked();
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

    void loadSettings();
    void setupUI();
};

#endif // UPDATESETTINGSDIALOG_H
