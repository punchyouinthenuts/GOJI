#ifndef TMFLAHCAEMAILDIALOG_H
#define TMFLAHCAEMAILDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileIconProvider>
#include <QCloseEvent>
#include "tmflahcaemailfilelistwidget.h"

class TMFLAHCAEmailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TMFLAHCAEmailDialog(const QString& jobNumber, QWidget* parent = nullptr,
                                const QString& directoryPath = QString());
    ~TMFLAHCAEmailDialog() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onFileClicked();
    void onCloseClicked();

private:
    void setupUI();
    void populateFileList();
    void updateCloseButtonState();
    QString getFileDirectory() const;

    QVBoxLayout* m_mainLayout;
    QLabel* m_headerLabel1;
    QLabel* m_headerLabel2;
    QLabel* m_filesLabel;
    TMFLAHCAEmailFileListWidget* m_fileList;
    QLabel* m_helpLabel;
    QPushButton* m_closeButton;

    QString m_jobNumber;
    QString m_directoryPath;
    QFileIconProvider m_iconProvider;
};

#endif // TMFLAHCAEMAILDIALOG_H
