#ifndef TMCAEMAILDIALOG_H
#define TMCAEMAILDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QCloseEvent>
#include <QFileIconProvider>
#include <QStringList>

/**
 * @brief Email integration dialog for TM CA (CA EDR/BA).
 *
 * Displays a financial summary (LA/SA counts, postage, rate, NAS destination)
 * and a drag-and-drop file list of merged CSV files for Outlook attachment.
 *
 * CLOSE is always enabled — no gating, no timers.
 * Emits dialogClosed() on any close path; TMCAController connects this signal
 * to triggerArchivePhase() to start Phase 2 (--phase archive).
 *
 * Constructor parameters mirror the pending state fields populated in
 * TMCAController::handlePhase1Success() after Phase 1 JSON is validated.
 */
class TMCAEmailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TMCAEmailDialog(
        const QString&     jobNumber,
        const QString&     jobType,       // "BA" or "EDR"
        int                laValidCount,
        int                saValidCount,
        double             laPostage,
        double             saPostage,
        double             rate,
        const QString&     nasDest,
        const QStringList& mergedFiles,
        QWidget*           parent = nullptr);

    ~TMCAEmailDialog() override;

signals:
    void dialogClosed();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onCloseClicked();

private:
    void setupUI();
    void populateFileList();

    // Layout
    QVBoxLayout* m_mainLayout;

    // Header
    QLabel* m_headerLabel;
    QLabel* m_subHeaderLabel;

    // Summary grid
    QLabel* m_jobLabel;
    QLabel* m_jobTypeLabel;
    QLabel* m_laCountLabel;
    QLabel* m_saCountLabel;
    QLabel* m_laPostageLabel;
    QLabel* m_saPostageLabel;
    QLabel* m_rateLabel;
    QLabel* m_nasDestLabel;

    // File list
    QLabel*      m_filesLabel;
    QListWidget* m_fileList;
    QLabel*      m_helpLabel;

    // Close
    QPushButton* m_closeButton;

    // Data
    QString     m_jobNumber;
    QString     m_jobType;
    int         m_laValidCount;
    int         m_saValidCount;
    double      m_laPostage;
    double      m_saPostage;
    double      m_rate;
    QString     m_nasDest;
    QStringList m_mergedFiles;

    QFileIconProvider m_iconProvider;

    bool m_closeInitiated;   // true only after CLOSE button clicked
};

#endif // TMCAEMAILDIALOG_H
