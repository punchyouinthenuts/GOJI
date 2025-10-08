#ifndef TMFLERCONTROLLER_H
#define TMFLERCONTROLLER_H

#include "basetrackercontroller.h"
#include "tmflerfilemanager.h"
#include "tmflerdbmanager.h"
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
class EmailConfirmationDialog;
class DropWindow;

/**
 * @brief Controller for TM FL ER tab functionality
 *
 * This class manages all operations specific to the TM FL ER tab,
 * including job management, script execution, file operations, and
 * tracker functionality. Inherits shared tracker copy functionality
 * from BaseTrackerController.
 */
class TMFLERController : public BaseTrackerController
{
    Q_OBJECT

public:
    void refreshTrackerTable();
    explicit TMFLERController(QObject *parent = nullptr);
    ~TMFLERController();

    // UI Widget setters
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

    // Job management
    bool loadJob(const QString& year, const QString& month);
    void resetToDefaults();
    void saveJobState();
    void loadJobState();

    // CRITICAL FIX: Add log entry when postage is locked
    void addLogEntry();

    // Utility method to convert month number to abbreviation
    QString convertMonthToAbbreviation(const QString& monthNumber) const;

    // Public methods for external access
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
    // Lock button handlers
    void onJobDataLockClicked();
    void onEditButtonClicked();
    void onPostageLockClicked();

    // Script execution handlers
    void onRunInitialClicked();
    void onFinalStepClicked();

    // Script runner handlers
    void onScriptOutput(const QString& output);
    void onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus);

    // File system handlers
    void onFileSystemChanged();

    // Email dialog handlers
    void onEmailDialogConfirmed();
    void onEmailDialogCancelled();

    // Drop window handlers
    void onFilesDropped(const QStringList& filePaths);
    void onFileDropError(const QString& errorMessage);

private:
    void applyTrackerHeaders();  // sets DisplayRole headers by field name
    bool validateJobNumber(const QString& jobNumber) const;
    // UI State Management
    enum HtmlDisplayState {
        UninitializedState = -1,
        DefaultState = 0,
        InstructionsState = 1
    };

    // Core components
    TMFLERFileManager* m_fileManager;
    TMFLERDBManager* m_tmFlerDBManager;
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
    QString m_capturedNASPath;
    bool m_capturingNASPath;

    // Email dialog support
    bool m_waitingForEmailConfirmation;
    QString m_emailDialogPath;
    EmailConfirmationDialog* m_emailDialog;

    // Tracker model
    QSqlTableModel* m_trackerModel;

    // Track previous period for auto-save on period change
    int m_lastYear;
    int m_lastMonth;
    QString m_cachedJobNumber;

    // Initialization methods
    void initializeComponents();
    void connectSignals();
    void setupInitialState();
    void initializeAfterConstruction(); // Safe post-construction initializer
    void createJobFolder();
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

    // Script output parsing
    void parseScriptOutput(const QString& output);
    void showNASLinkDialog(const QString& nasPath);
    void showEmailConfirmationDialog(const QString& directoryPath);

    // Directory management
    void createBaseDirectories();

    // Dropdown population methods
    void populateYearDropdown();
    void populateMonthDropdown();

    // Dropdown change handlers
    void onYearChanged(const QString& year);
    void onMonthChanged(const QString& month);

    // Validation
    bool validateJobData();
    bool validatePostageData();
    bool validateScriptExecution(const QString& scriptName) const;

    // Input formatting methods
    void formatPostageInput();
    void formatCountInput(const QString& text);

    // Tracker operations
    void setupTrackerModel();
    void setupOptimizedTableLayout();

    // Excel copy functionality
    bool createExcelAndCopy(const QStringList& headers, const QStringList& rowData);

    // File management
    bool moveFilesToHomeFolder();
    bool copyFilesFromHomeFolder();

    // Opens the modal FL ER email dialog and resumes the script on close.
    void showEmailDialog(const QString &nasPath, const QString &jobNumber);
};

/**
 * @brief Custom dialog for email confirmation with countdown timer
 */
class EmailConfirmationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EmailConfirmationDialog(const QString& directoryPath, QWidget *parent = nullptr);

signals:
    void confirmed();
    void cancelled();

private slots:
    void onTimerTick();
    void onContinueClicked();
    void onCancelClicked();

private:
    QLabel* m_messageLabel;
    QPushButton* m_continueButton;
    QPushButton* m_cancelButton;
    QTimer* m_countdownTimer;
    int m_secondsRemaining;
    QString m_directoryPath;

    void setupUI();
    void updateButtonText();
    void openDirectory();
};

#endif // TMFLERCONTROLLER_H