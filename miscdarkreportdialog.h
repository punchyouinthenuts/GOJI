#ifndef MISCDARKREPORTDIALOG_H
#define MISCDARKREPORTDIALOG_H

#include <QDialog>

#include "terminaloutputhelper.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;

class MiscDarkReportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MiscDarkReportDialog(QWidget* parent = nullptr);

    void setStatusMessage(const QString& message,
                          TerminalSeverity severity = TerminalSeverity::Info);

signals:
    void terminalMessageRequested(const QString& message, TerminalSeverity severity);

private slots:
    void onChooseFileClicked();
    void onProcessClicked();
    void onCopyClicked();
    void onCloseClicked();

private:
    void setupUi();
    void updateControlStates();
    void resetTable();
    void populateResultsTable(const QString& jobNumber,
                              int domesticCount,
                              int internationalCount);
    bool runProcessorScript(const QString& filePath,
                            const QString& jobNumber,
                            QString* errorMessage,
                            int* domesticCount,
                            int* internationalCount,
                            int* totalCount,
                            QString* outputFilePath);
    static QString statusColorForSeverity(TerminalSeverity severity);
    static QString formatCurrency(double value);
    void setCell(int row,
                 int column,
                 const QString& text,
                 Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter,
                 bool bold = false);
    QString buildClipboardText() const;

    QPushButton* m_chooseFileButton;
    QLabel* m_selectedFileLabel;
    QLabel* m_jobNumberLabel;
    QLineEdit* m_jobNumberEdit;
    QPushButton* m_processButton;
    QTableWidget* m_resultsTable;
    QPushButton* m_copyButton;
    QPushButton* m_closeButton;
    QLabel* m_statusLabel;

    QString m_selectedFilePath;
    bool m_running;
    bool m_hasResults;
};

#endif // MISCDARKREPORTDIALOG_H
