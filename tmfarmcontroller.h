#ifndef TMFARMCONTROLLER_H
#define TMFARMCONTROLLER_H

#include "basetrackercontroller.h"
#include "tmfarmdbmanager.h"
#include "tmfarmfilemanager.h"
#include "scriptrunner.h"

#include <QObject>
#include <QPushButton>
#include <QToolButton>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QTextBrowser>
#include <QTableView>
#include <QSqlTableModel>
#include <QStringList>
#include <QPoint>
#include <QFile>        // for resource existence check
#include <QTextStream>  // fallback HTML
#include <QTimer>       // defer initial load
#include <QUrl>         // setSource(QUrl)

class TMFarmController : public BaseTrackerController
{
    Q_OBJECT
public:
    explicit TMFarmController(QObject* parent = nullptr);

    // Wiring
    void setTextBrowser(QTextBrowser* browser);
    void initializeUI(
        QPushButton* openBulkMailerBtn,
        QPushButton* runInitialBtn,
        QPushButton* finalStepBtn,
        QToolButton* lockBtn,
        QToolButton* editBtn,
        QToolButton* postageLockBtn,
        QComboBox* yearCombo,
        QComboBox* monthOrQuarterCombo,
        QLineEdit* jobNumberEdit,
        QLineEdit* postageEdit,
        QLineEdit* countEdit,
        QTextEdit* terminalEdit,
        QTableView* trackerView,
        QTextBrowser* textBrowser
    );

    // Operations
    bool loadJob(const QString& year, const QString& monthOrQuarter);
    void autoSaveAndCloseCurrentJob();

    // BaseTrackerController overrides
    void outputToTerminal(const QString& message, MessageType type) override;
    QTableView* getTrackerWidget() const override;
    QSqlTableModel* getTrackerModel() const override;
    QStringList getTrackerHeaders() const override;
    QList<int> getVisibleColumns() const override;
    QString formatCellData(int columnIndex, const QString& cellData) const override;
    QString formatCellDataForCopy(int columnIndex, const QString& cellData) const override;

    // UI/State helpers
    bool isJobDataLocked() const;
    void refreshTrackerTable();
    void updateControlStates();

signals:
    void jobOpened();
    void jobClosed();

private slots:
    void onScriptFinished(int exitCode, QProcess::ExitStatus status);
    void onScriptOutput(const QString& text);
    void onOpenBulkMailerClicked();
    void onRunInitialClicked();
    void onFinalStepClicked();
    void onLockButtonClicked();
    void onEditButtonClicked();
    void onPostageLockButtonClicked();
    void onYearChanged(const QString& year);
    void onMonthChanged(const QString& monthOrQuarter);
    void formatCountInput(const QString& text);
    void formatPostageInput();

private:
    void connectSignals();
    void setupOptimizedTableLayout();
    void showTableContextMenu(const QPoint& pos);
    void updateHtmlDisplay();
    bool saveJobState();
    bool validateJobNumber(const QString& jobNumber) const;
    void addLogEntry();

    // Loads an HTML file from a Qt resource path into the TMFW text browser.
    void loadHtmlFile(const QString& resourcePath);

private:
    // Widgets
    QPushButton* m_openBulkMailerBtn{nullptr};
    QPushButton* m_runInitialBtn{nullptr};
    QPushButton* m_finalStepBtn{nullptr};
    QToolButton* m_lockBtn{nullptr};
    QToolButton* m_editBtn{nullptr};
    QToolButton* m_postageLockBtn{nullptr};
    QComboBox* m_yearCombo{nullptr};
    QComboBox* m_monthOrQuarterCombo{nullptr};
    QLineEdit* m_jobNumberEdit{nullptr};
    QLineEdit* m_postageEdit{nullptr};
    QLineEdit* m_countEdit{nullptr};
    QTextEdit* m_terminal{nullptr};
    QTableView* m_tracker{nullptr};
    QTextBrowser* m_textBrowser{nullptr};

    // Data / state
    bool m_jobDataLocked{false};
    bool m_postageDataLocked{false};
    QString m_lastExecutedScript;
    QString m_cachedJobNumber;
    QSqlTableModel* m_trackerModel{nullptr};
};

#endif // TMFARMCONTROLLER_H
