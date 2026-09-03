#ifndef TMMAEMAILDIALOG_H
#define TMMAEMAILDIALOG_H

#include <QDialog>
#include <QStringList>

class QLabel;
class QPushButton;
class TMFLEREmailFileListWidget;

class TMMAEmailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TMMAEmailDialog(const QStringList& filePaths, QWidget* parent = nullptr);

private:
    QStringList m_filePaths;
    QLabel* m_headingLabel;
    TMFLEREmailFileListWidget* m_fileList;
    QLabel* m_helpLabel;
    QPushButton* m_closeButton;
};

#endif // TMMAEMAILDIALOG_H
