#ifndef PATHCOPYDIALOG_H
#define PATHCOPYDIALOG_H

#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;

class PathCopyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PathCopyDialog(const QString& windowTitle,
                            const QString& path,
                            QWidget* parent = nullptr);

private slots:
    void copyPath();

private:
    QString m_path;
    QLabel* m_pathLabel;
    QPushButton* m_copyButton;
};

#endif // PATHCOPYDIALOG_H
