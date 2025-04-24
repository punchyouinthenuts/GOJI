#ifndef FILELOCATIONSDIALOG_H
#define FILELOCATIONSDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

class FileLocationsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FileLocationsDialog(const QString& locationsText, QWidget* parent = nullptr);

private slots:
    void onCopyButtonClicked();
    void onCloseButtonClicked();

private:
    QTextEdit* m_textEdit;
    QPushButton* m_copyButton;
    QPushButton* m_closeButton;
};

#endif // FILELOCATIONSDIALOG_H
