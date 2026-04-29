#ifndef MISCSPLITLARGELISTSDIALOG_H
#define MISCSPLITLARGELISTSDIALOG_H

#include <QDialog>

#include "terminaloutputhelper.h"

class QLabel;
class QPushButton;
class QRadioButton;
class QCloseEvent;

class MiscSplitLargeListsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MiscSplitLargeListsDialog(QWidget* parent = nullptr);

    void setStatusMessage(const QString& message,
                          TerminalSeverity severity = TerminalSeverity::Info);
    void setRunning(bool running);
    void setLoadedFileInfo(const QString& filePath, const QString& baseName, qint64 recordCount);

signals:
    void loadRequested(const QString& filePath);
    void runRequested(const QString& filePath,
                      int parts,
                      const QString& outputDirectory,
                      const QString& baseName);
    void terminalMessageRequested(const QString& message, TerminalSeverity severity);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onOpenClicked();
    void onDestinationSelectionChanged();
    void onSplitButtonClicked();
    void onPrimaryClicked();

private:
    enum class OutputDestination {
        Downloads,
        Input,
        Other
    };

    void setupUi();
    void updateControlStates();
    void updatePrimaryButtonText();
    void updateOutputPathLabel();
    void updatePreviewText();
    QString effectiveOutputDirectory() const;
    int selectedParts() const;
    void clearSplitSelection();
    static QString statusColorForSeverity(TerminalSeverity severity);
    static QString formatRecordCount(qint64 value);

    QPushButton* m_openButton;
    QLabel* m_selectedFileNameLabel;
    QRadioButton* m_downloadsRadio;
    QRadioButton* m_inputRadio;
    QRadioButton* m_otherRadio;
    QLabel* m_outputPathLabel;
    QPushButton* m_halfButton;
    QPushButton* m_thirdButton;
    QPushButton* m_quarterButton;
    QLabel* m_previewLabel;
    QLabel* m_statusLabel;
    QPushButton* m_primaryButton;

    QString m_loadedFilePath;
    QString m_baseOutputName;
    qint64 m_recordCount;
    OutputDestination m_outputDestination;
    QString m_otherOutputDirectory;
    bool m_running;
};

#endif // MISCSPLITLARGELISTSDIALOG_H
