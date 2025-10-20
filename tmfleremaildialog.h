#ifndef TMFLEREMAILDIALOG_H
#define TMFLEREMAILDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileIconProvider>
#include <QCloseEvent>
#include "tmfleremailfilelistwidget.h"

class TMFLEREmailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TMFLEREmailDialog(const QString& jobNumber, QWidget* parent = nullptr);
    ~TMFLEREmailDialog() override;

signals:
    void dialogClosed();

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
    TMFLEREmailFileListWidget* m_fileList;
    QLabel* m_helpLabel;
    QPushButton* m_closeButton;

    QString m_jobNumber;
    QFileIconProvider m_iconProvider;
};

#endif // TMFLEREMAILDIALOG_H
