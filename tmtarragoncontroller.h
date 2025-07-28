#ifndef TMTARRAGONCONTROLLER_H
#define TMTARRAGONCONTROLLER_H

#include "basetrackercontroller.h"
#include <QObject>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QTextEdit>
#include <QTableView>
#include <QTextBrowser>
#include <QSqlTableModel>
#include <QTimer>
#include <QRegularExpression>
#include "databasemanager.h"
#include "tmtarragondbmanager.h"
#include "scriptrunner.h"
#include "tmtarragonfilemanager.h"
#include "naslinkdialog.h"

class TMTarragonController : public BaseTrackerController
{
    Q_OBJECT

public:
    // HTML display states
    enum HtmlDisplayState {
        UninitializedState = -1,  // Initial state before any HTML is loaded
        DefaultState = 0,         // When no job is loaded - shows default.html
        InstructionsState = 1     // When job is locked - shows instructions.html
    };

    explicit TMTarragonController(QObject *parent = nullptr);
    ~TMTarragonController();

    // Initialize with UI elements from mainwindow
    void initializeUI(
        QPushButton* openBulkMailerBtn, QPushButton* runInitialBtn,
        QPushButton* finalStepBtn, QToolButton* lockBtn, QToolButton* editBtn,
        QToolButton* postageLockBtn, QComboBox* yearDDbox, QComboBox* monthDDbox,
        QComboBox* dropNumberDDbox, QLineEdit* jobNumberBox, QLineEdit* postageBox,
        QLineEdit* countBox, QTextEdit* terminalWindow, QTableView* tracker,
        QTextBrowser* textBrowser
        );

    // Load saved data
    bool loadJob(const QString& year, const QString& month, const QString& dropNumber);
    void setTextBrowser(QTextBrowser* textBrowser);
    void resetToDefaults();

    // Public getters for external access
    QString getJobNumber() const { return m_jobNumberBox ? m_jobNumberBox->text() : ""; }
    QString getYear() const { return m_yearDDbox ? m_yearDDbox->currentText() : ""; }
    QString getMonth() const { return m_monthDDbox ? m_monthDDbox->currentText() : ""; }
    QString getDropNumber() const { return m_dropNumberDDbox ? m_dropNumberDDbox->currentText() : ""; }
    bool isJobDataLocked() const { return m_jobDataLocked; }
    bool isPostageDataLocked() const { return m_postageDataLocked; }

    // BaseTrackerController implementation
    void outputToTerminal(const QString& message, MessageType type) override;
    QTableView* getTrackerWidget() const override;
    QSqlTableModel* getTrackerModel() const override;
    QStringList getTrackerHeaders() const override;
    QList<int> getVisibleColumns() const override;
    QString formatCellData(int columnIndex, const QString& cellData) const override;

    /**
     * @brief Auto-save and close current job before opening a new one
     */
    void autoSaveAndCloseCurrentJob();

signals:
    void jobOpened();
    void jobClosed();

private slots:
    // Button handlers
    void onOpenBulkMailerClicked();
    void onRunInitialClicked();
    void onFinalStepClicked();

    // Lock button handlers
    void onLockButtonClicked();
    void onEditButtonClicked();
    void onPostageLockButtonClicked();

    // Dropdown change handlers
    void onYearChanged(const QString& year);
    void onMonthChanged(const QString& month);
    void onDropNumberChanged(const QString& dropNumber);

    // Script output handlers
    void onScriptOutput(const QString& output);
    void onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus);

    // Table context menu
    void showTableContextMenu(const QPoint& pos);

private:
    // Core components
    DatabaseManager* m_dbManager;
    TMTarragonFileManager* m_fileManager;
    TMTarragonDBManager* m_tmTarragonDBManager;
    ScriptRunner* m_scriptRunner;

    // UI element pointers
    QPushButton* m_openBulkMailerBtn;
    QPushButton* m_runInitialBtn;
    QPushButton* m_finalStepBtn;
    QToolButton* m_lockBtn;
    QToolButton* m_editBtn;
    QToolButton* m_postageLockBtn;
    QComboBox* m_yearDDbox;
    QComboBox* m_monthDDbox;
    QComboBox* m_dropNumberDDbox;
    QLineEdit* m_jobNumberBox;
    QLineEdit* m_postageBox;
    QLineEdit* m_countBox;
    QTextEdit* m_terminalWindow;
    QTableView* m_tracker;
    QTextBrowser* m_textBrowser;

    // State variables
    bool m_jobDataLocked;
    bool m_postageDataLocked;
    HtmlDisplayState m_currentHtmlState;
    QString m_capturedNASPath;
    bool m_capturingNASPath;
    QString m_lastExecutedScript;

    // Tracker model
    QSqlTableModel* m_trackerModel;

    // Private helper methods
    void connectSignals();
    void setupInitialUIState();
    void populateDropdowns();
    void setupOptimizedTableLayout();
    void createBaseDirectories();
    void createJobFolder();

    // Data management
    void saveJobState();
    void loadJobState();
    void saveJobToDatabase();

    // Job and log management
    void addLogEntry();

    // Validation and utility
    bool validateJobData();
    bool validatePostageData();
    bool validateJobNumber(const QString& jobNumber);
    bool validateDropNumber(const QString& dropNumber);
    bool validateMonthSelection(const QString& month);
    QString convertMonthToAbbreviation(const QString& monthNumber) const;
    QString getJobDescription() const;
    bool hasJobData() const;

    // UI state management
    void updateControlStates();
    void updateHtmlDisplay();
    void loadHtmlFile(const QString& resourcePath);
    HtmlDisplayState determineHtmlState() const;
    void formatPostageInput();
    void formatCountInput(const QString& text);

    // Script output processing
    void parseScriptOutput(const QString& output);
    void showNASLinkDialog(const QString& nasPath);

    // Inherited method implementation
    QString copyFormattedRow();
    bool createExcelAndCopy(const QStringList& headers, const QStringList& rowData);

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
};

#endif // TMTARRAGONCONTROLLER_H
