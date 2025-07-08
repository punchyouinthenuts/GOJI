#ifndef NASLINKDIALOG_H
#define NASLINKDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QClipboard>
#include <QApplication>
#include <QFont>
#include <QFrame>

/**
 * @brief Generic dialog that displays a network path with copy functionality
 *
 * This reusable dialog can display any file/folder location with customizable
 * title and description text, plus a button to copy the combined text to clipboard.
 */
class NASLinkDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructor with full customization
     * @param windowTitle The title of the dialog window
     * @param descriptionText The descriptive text above the path
     * @param networkPath The full network path to display
     * @param parent Parent widget
     */
    explicit NASLinkDialog(const QString& windowTitle,
                           const QString& descriptionText,
                           const QString& networkPath,
                           QWidget* parent = nullptr);

    /**
     * @brief Simplified constructor for common use case
     * @param networkPath The full network path to display
     * @param parent Parent widget
     *
     * Uses default title "File Location" and description "File located below"
     */
    explicit NASLinkDialog(const QString& networkPath, QWidget* parent = nullptr);

private slots:
    /**
     * @brief Copy the combined description and NAS path to clipboard
     */
    void onCopyClicked();

    /**
     * @brief Close the dialog
     */
    void onCloseClicked();

private:
    QString m_networkPath;
    QString m_descriptionText;
    QLabel* m_descriptionLabel;
    QTextEdit* m_textDisplay;  // CRITICAL FIX: Changed from QLineEdit to QTextEdit for multi-line support
    QPushButton* m_copyButton;
    QPushButton* m_closeButton;

    /**
     * @brief Set up the dialog UI
     */
    void setupUI();

    /**
     * @brief Calculate optimal dialog width based on path length
     * @return Recommended dialog width
     */
    int calculateOptimalWidth() const;
};

#endif // NASLINKDIALOG_H
