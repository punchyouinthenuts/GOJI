#ifndef TMCACONTROLLER_H
#define TMCACONTROLLER_H

#include "basetrackercontroller.h"
#include "tmcafilemanager.h"
#include "tmcadbmanager.h"
#include "scriptrunner.h"

#include <QObject>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QTextBrowser>
#include <QTableView>
#include <QSqlTableModel>
#include <QToolButton>
#include <QProcess>

// Forward declaration
class DropWindow;

/**
 * @brief Controller for TM CA (CA EDR/BA) tab functionality
 *
 * Mirrors the dominant TRACHMAR TM* controller patterns (TMFLER/TMTERM/TMHEALTHY):
 * - Two-state lock model (UNLOCKED vs LOCKED, no third state)
 * - ScriptRunner execution with marker-driven pause dialog
 * - DropWindow wired to DROP then controller routes files into BA/INPUT or EDR/INPUT
 * - Tracker setup mirrors TMFLER's table behavior/styling
 */
class TMCAController : public BaseTrackerController
{
    Q_OBJECT

public:
    void refreshTrackerTable();
    explicit TMCAController(QObject *parent = nullptr);
    ~TMCAController();

    // UI Widget setters (wired from MainWindow)
    void setJobNumberBox(QLineEdit* lineEdit);
    void setYearDropdown(QComboBox* comboBox);
    void setMonthDropdown(QComboBox* comboBox);
    void setPostageBox(QLineEdit* lineEdit);
    void setCountBox(QLineEdit* lineEdit);
    void setJobDataLockButton(QToolButton* button);
    void setEditButton(QToolButton* button);
    void setPostageLockButton(QToolButton* button);
    void setRunInitialButton(QPushButton* button);
    void setFinalStepButton(QPushButton* button);
    void setTerminalWindow(QTextEdit* textEdit);
    void setTextBrowser(QTextBrowser* textBrowser);
    void setTracker(QTableView* tableView);
    void setDropWindow(DropWindow* dropWindow);

    // Job management (mirrors TMFLER/TMTERM)
    bool loadJob(const QString& jobNumber, const QString& year, const QString& month);
    void resetToDefaults();
    void saveJobState();
    void loadJobState();
    void loadJobState(const QString& jobNumber);

    // Public accessors used by MainWindow
    QString getJobNumber() const;
    QString getYear() const;
    QString getMonth() const;
    bool isJobDataLocked() const;
    bool isPostageDataLocked() const;
    bool hasJobData() const;

    // BaseTrackerController implementation
    void outputToTerminal(const QString& message, MessageType type) override;
    QTableView* getTrackerWidget() const override;
    QSqlTableModel* getTrackerModel() const override;
    QStringList getTrackerHeaders() const override;
    QList<int> getVisibleColumns() const override;
    QString formatCellData(int columnIndex, const QString& cellData) const override;
    QString formatCellDataForCopy(int columnIndex, const QString& cellData) const override;

    /**
     * @brief Auto-save and close current job before opening a new one
     */
    void autoSaveAndCloseCurrentJob();

signals:
    void jobOpened();
    void jobClosed();

private slots:
    // Lock button handlers
    void onJobDataLockClicked();
    void onEditButtonClicked();
    void onPostageLockClicked();

    // Script buttons
    void onRunInitialClicked();
    void onFinalStepClicked();

    // Script runner handlers
    void onScriptOutput(const QString& output);
    void onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus);

    // Drop window handlers
    void onFilesDropped(const QStringList& filePaths);
    void onFileDropError(const QString& errorMessage);

    void triggerArchivePhase();

private:
    // UI State Management
    enum HtmlDisplayState {
        UninitializedState = -1,
        DefaultState = 0,
        InstructionsState = 1
    };

    // Initialization methods
    void initializeComponents();
    void connectSignals();
    void setupInitialState();
    void initializeAfterConstruction(); // Safe post-construction initializer
    void createBaseDirectories();
    void setupDropWindow();

    // Lock state management
    void updateLockStates();
    void updateButtonStates();

    // HTML display management
    void updateHtmlDisplay();
    void loadHtmlFile(const QString& resourcePath);
    HtmlDisplayState determineHtmlState() const;

    // Script execution
    void executeScript(const QString& scriptName);

    // Drop routing helper (kept isolated for growth)
    void routeDroppedFile(const QString& absoluteFilePath);

    // Utility
    bool validateJobData() const;
    bool validatePostageData() const;

    // Core components
    TMCAFileManager* m_fileManager;
    TMCADBManager* m_tmcaDBManager;
    ScriptRunner* m_scriptRunner;

    // UI Widgets
    QLineEdit* m_jobNumberBox;
    QComboBox* m_yearDDbox;
    QComboBox* m_monthDDbox;
    QLineEdit* m_postageBox;
    QLineEdit* m_countBox;
    QToolButton* m_jobDataLockBtn;
    QToolButton* m_editBtn;
    QToolButton* m_postageLockBtn;
    QPushButton* m_runInitialBtn;
    QPushButton* m_finalStepBtn;
    QTextEdit* m_terminalWindow;
    QTextBrowser* m_textBrowser;
    QTableView* m_tracker;
    DropWindow* m_dropWindow;

    // State variables
    bool m_jobDataLocked;
    bool m_postageDataLocked;
    HtmlDisplayState m_currentHtmlState;
    QString m_lastExecutedScript;

    // Tracker model
    QSqlTableModel* m_trackerModel;
};

#endif // TMCACONTROLLER_H
