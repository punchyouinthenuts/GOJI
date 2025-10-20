#ifndef TMFLEREMAILDIALOG_H
#define TMFLEREMAILDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDir>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QUrl>
#include <QFileInfo>

class TMFLEREmailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TMFLEREmailDialog(const QString &jobNumber, QWidget *parent = nullptr);
    ~TMFLEREmailDialog() override = default;

signals:
    void dialogClosed();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onCloseClicked();

private:
    void populateFileList();

    QString m_jobNumber;
    QListWidget *m_fileList;
    QPushButton *m_closeButton;
    QLabel *m_instructionLabel;
    QVBoxLayout *m_mainLayout;
};

#endif // TMFLEREMAILDIALOG_H
