#ifndef TMTERMCONTROLLER_H
#define TMTERMCONTROLLER_H

#include "basetrackercontroller.h"
#include "tmtermfilemanager.h"
#include "tmtermdbmanager.h"
#include "scriptrunner.h"
#include "databasemanager.h"
#include <QObject>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QTextEdit>
#include <QTextBrowser>
#include <QTableView>
#include <QSqlTableModel>
#include <QTimer>
#include <QRegularExpression>

// Forward declaration
class NASLinkDialog;

class TMTermController : public BaseTrackerController
{
    Q_OBJECT

public:
    explicit TMTermController(QObject *parent = nullptr);
    ~TMTermController();

    // Main UI initialization method
    void initializeUI(
        QPushButton* openBulkMailerBtn, QPushButton* runInitialBtn,
        QPushButton* finalStepBtn, QToolButton* lockBtn, QToolButton* editBtn,
        QToolButton* postageLockBtn, QComboBox* yearDDbox, QComboBox* monthDDbox,
        QLineEdit* jobNumberBox, QLineEdit* postageBox, QLineEdit* countBox,
        QTextEdit* terminalWindow, QTableView* tracker, QTextBrowser* textBrowser);

    // Text browser setter (called separately after UI setup)
    void setTextBrowser(QTextBrowser* textBrowser);

    // Job management
    bool loadJob(const QString& year, const QString& month);
    void resetToDefaults();
    void saveJobState();

    // Public getters for external access
    QString getJobNumber() const { return m_jobNumberBox ? m_jobNumberBox->text() : ""; }
    QString getYear() const { return m_yearDDbox ? m_yearDDbox->currentText() : ""; }
    QString getMonth() const { return m_monthDDbox ? m_monthDDbox->currentText() : ""; }
    bool isJobDataLocked() const { return m_jobDataLocked; }
    bool isPostageDataLocked() const { return m_postageDataLocked; }

    // BaseTrackerController implementation
    void outputToTerminal(const QString& message, MessageType type) override;
    QTableView* getTrackerWidget() const override;
    QSqlTableModel* getTrackerModel() const override;
    QStringList getTrackerHeaders() const override;
    QList<int> getVisibleColumns() const override;
    QString formatCellData(int columnIndex, const QString& cellData) const override;

signals:
    void jobOpened();
    void jobClosed();

private slots:
    // Button handlers
    void onOpenBulkMailerClicked();
    void onRunInitialClicked();
    void onFinalStepClicked();
    void onLockButtonClicked();
    void onEditButtonClicked();
    void onPostageLockButtonClicked();

    // Dropdown handlers
    void onYearChanged(const QString& year);
    void onMonthChanged(const QString& month);

    // Input formatting
    void formatPostageInput(const QString& text);
    void formatCountInput(const QString& text);

    // Script runner handlers
    void onScriptOutput(const QString& output);
    void onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus);

    // Context menu
    void showTableContextMenu(const QPoint& pos);

private:
    // UI State Management
    enum HtmlDisplayState {
        UninitializedState = -1,
        DefaultState = 0,
        InstructionsState = 1
    };

    // Core components
    DatabaseManager* m_dbManager;
    TMTermFileManager* m_fileManager;
    TMTermDBManager* m_tmTermDBManager;
    ScriptRunner* m_scriptRunner;

    // UI Widgets
    QPushButton* m_openBulkMailerBtn;
    QPushButton* m_runInitialBtn;
    QPushButton* m_finalStepBtn;
    QToolButton* m_lockBtn;
    QToolButton* m_editBtn;
    QToolButton* m_postageLockBtn;
    QComboBox* m_yearDDbox;
    QComboBox* m_monthDDbox;
    QLineEdit* m_jobNumberBox;
    QLineEdit* m_postageBox;
    QLineEdit* m_countBox;
    QTextEdit* m_terminalWindow;
    QTextBrowser* m_textBrowser;
    QTableView* m_tracker;

    // State variables
    bool m_jobDataLocked;
    bool m_postageDataLocked;
    HtmlDisplayState m_currentHtmlState;
    QString m_lastExecutedScript;
    QString m_capturedNASPath;
    bool m_capturingNASPath;

    // Tracker model
    QSqlTableModel* m_trackerModel;

    // Private methods
    void connectSignals();
    void setupInitialUIState();
    void populateDropdowns();
    void setupOptimizedTableLayout();
    void updateControlStates();
    void updateHtmlDisplay();
    void loadHtmlFile(const QString& resourcePath);
    HtmlDisplayState determineHtmlState() const;
    void createBaseDirectories();
    void createJobFolder();
    void parseScriptOutput(const QString& output);
    void showNASLinkDialog(const QString& nasPath);

    // Validation methods
    bool validateJobData();
    bool validatePostageData();
    bool validateJobNumber(const QString& jobNumber);
    bool validateMonthSelection(const QString& month);

    // Utility methods
    QString convertMonthToAbbreviation(const QString& monthNumber) const;
    QString getJobDescription() const;
    bool hasJobData() const;

    // Database operations
    void saveJobToDatabase();
    void loadJobState();
    void addLogEntry();

    // CRITICAL FIX: Remove copyFormattedRow() declaration - uses inherited BaseTrackerController method
    // CRITICAL FIX: Remove any createExcelAndCopy() method declaration - not needed

    /**
     * @brief Move files from JOB folders to HOME folders when closing job
     * @return True if files were moved successfully
     */
    bool moveFilesToHomeFolder();

    /**
     * @brief Copy files from HOME folders to JOB folders when opening job
     * @return True if files were copied successfully
     */
    bool copyFilesFromHomeFolder();

    /**
     * @brief Helper method to move files when no job number is available
     * @param year Year for the job (YYYY format)
     * @param month Month for the job (MM format)
     * @return True if files were moved successfully
     */
    bool moveFilesToBasicHomeFolder(const QString& year, const QString& month);
};

#endif // TMTERMCONTROLLER_H
