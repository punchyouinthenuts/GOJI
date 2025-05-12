#ifndef FILELOCATIONSDIALOG_H
#define FILELOCATIONSDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QPushButton>
#include <QString>

class FileLocationsDialog : public QDialog
{
    Q_OBJECT

public:
    enum ButtonType {
        CopyCloseButtons,
        YesNoButtons,
        OkButton
    };

    explicit FileLocationsDialog(const QString& locationsText, ButtonType buttonType = CopyCloseButtons, QWidget* parent = nullptr);

private slots:
    void onCopyButtonClicked();
    void onCloseButtonClicked();

private:
    QTextEdit* m_textEdit;
    QPushButton* m_copyButton;
    QPushButton* m_closeButton;
    QPushButton* m_yesButton;
    QPushButton* m_noButton;
    QPushButton* m_okButton;
};

#endif // FILELOCATIONSDIALOG_H
