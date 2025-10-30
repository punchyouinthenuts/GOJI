#ifndef FHCONTROLLER_H
#define FHCONTROLLER_H

#include "basetrackercontroller.h"
#include "fhfilemanager.h"
#include "fhdbmanager.h"
#include "scriptrunner.h"
#include <QObject>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QTextBrowser>
#include <QTableView>
#include <QSqlTableModel>
#include <QTimer>
#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QToolButton>

// Forward declarations
class DropWindow;

/**
 * @brief Controller for FOUR HANDS tab functionality
 *
 * This class manages all operations specific to the FOUR HANDS tab,
 * including job management, script execution, file operations, and
 * tracker functionality. Inherits shared tracker copy functionality
 * from BaseTrackerController.
 */
class FHController : public BaseTrackerController
{
    Q_OBJECT

public:
    void refreshTrackerTable();
    explicit FHController(QObject *parent = nullptr);
    ~FHController();

    // UI Widget setters
    void setJobNumberBox(QLineEdit* lineEdit);
    void setYearDropdown(QComboBox* comboBox);
    void setMonthDropdown(QComboBox* comboBox);
    void setDropNumberDropdown(QComboBox* comboBox);
    void setPostageBox(QLineEdit* lineEdit);
    void setCountBox(QLineEdit* lineEdit);
    void setJobDataLockButton(QToolButton* button);
    void setPostageLockButton(QToolButton* button);
    void setEditButton(QToolButton* button);
    void setRunInitialButton(QPushButton* button);
    void setFinalStepButton(QPushButton* button);
    void setTerminalWindow(QTextEdit* textEdit);
    void setTracker(QTableView* tableView);
    void setDropWindow(DropWindow* dropWindow);
    void setTextBrowser(QTextBrowser* browser);

    // Job management
    bool loadJob(const QString& year, const QString& month);
    void resetToDefaults();
    void saveJobState();
    void loadJobState();

    // Add log entry when postage is locked
    void addLogEntry();
    void updateHtmlDisplay();

    // Utility method to convert month number to abbreviation
    QString convertMonthToAbbreviation(const QString& monthNumber) const;

    // Public methods for external access (use cached values)
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

public slots:
    void onAddToTracker();
    void onCopyRowClicked();

signals:
    void jobOpened();
    void jobClosed();

private slots:
    void showTableContextMenu(const QPoint& pos);

    // Lock button handlers
    void onJobDataLockClicked();
    void onPostageLockClicked();
    void onEditButtonClicked();

    // Script execution handlers
    void onRunInitialClicked();
    void onFinalStepClicked();

    // Script runner handlers
    void onScriptOutput(const QString& output);
    void onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus);

    // File system handlers
    void onFileSystemChanged();

    // Drop window handlers
    void onFilesDropped(const QStringList& filePaths);
    void onFileDropError(const QString& errorMessage);

private:
    void applyTrackerHeaders();
    bool validateJobNumber(const QString& jobNumber) const;

    // UI State Management
    enum HtmlDisplayState {
        UninitializedState = -1,
        DefaultState = 0,
        InstructionsState = 1
    };

    // Core components
    FHFileManager* m_fileManager;
    FHDBManager* m_fhDBManager;
    ScriptRunner* m_scriptRunner;

    // UI Widgets
    QLineEdit* m_jobNumberBox;
    QComboBox* m_yearDDbox;
    QComboBox* m_monthDDbox;
    QComboBox* m_dropNumberComboBox;
    QLineEdit* m_postageBox;
    QLineEdit* m_countBox;
    QTextBrowser* m_textBrowser;
    QToolButton* m_jobDataLockBtn;
    QToolButton* m_postageLockBtn;
    QToolButton* m_editBtn;
    QPushButton* m_runInitialBtn;
    QPushButton* m_finalStepBtn;
    QTextEdit* m_terminalWindow;
    QTableView* m_tracker;
    DropWindow* m_dropWindow;

    // State variables
    bool m_jobDataLocked;
    bool m_postageDataLocked;
    HtmlDisplayState m_currentHtmlState;
    QString m_lastExecutedScript;
    QString m_currentDropNumber;

    // Tracker model
    QSqlTableModel* m_trackerModel;

    // Cached job state (aligned with TM pattern)
    QString m_currentYear;
    QString m_currentMonth;
    QString m_cachedJobNumber;

    // Initialization guard flag
    bool m_initializing;

    // Initialization methods
    void initializeComponents();
    void connectSignals();
    void setupInitialState();
    void initializeAfterConstruction();
    void createJobFolder();
    void setupDropWindow();

    // Lock state management
    void updateLockStates();
    void updateButtonStates();

    // Script execution
    void executeScript(const QString& scriptName);

    // Script output parsing
    void parseScriptOutput(const QString& output);

    // Directory management
    void createBaseDirectories();

    // Dropdown population methods
    void populateYearDropdown();
    void populateMonthDropdown();

    // Dropdown change handlers
    void onYearChanged(const QString& year);
    void onMonthChanged(const QString& month);
    void onDropNumberChanged(const QString& dropNumber);

    // Validation
    bool validateJobData();
    bool validatePostageData();
    bool validateScriptExecution(const QString& scriptName) const;

    // HTML display management
    HtmlDisplayState determineHtmlState() const;
    void loadHtmlFile(const QString& resourcePath);

    // Input formatting methods
    void formatPostageInput();
    void formatCountInput(const QString& text);

    // Tracker operations
    void setupTrackerModel();
    void setupOptimizedTableLayout();

    // File management
    bool moveFilesToHomeFolder();
    bool copyFilesFromHomeFolder();
};

#endif // FHCONTROLLER_H
