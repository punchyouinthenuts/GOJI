#pragma once

#include <QDialog>
#include <QString>

class QListWidget;

class TMFLEREmailDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TMFLEREmailDialog(const QString &nasPath,
                               const QString &jobNumber,
                               QWidget *parent = nullptr);

private:
    void refreshFileList();

private:
    QString      m_nasPath;
    QString      m_jobNumber;
    QListWidget *m_list = nullptr;
};
