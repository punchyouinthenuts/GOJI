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
    explicit TMFLERController(QObject *parent = nullptr);
    ~TMFLERController();

    // UI Widget setters
    void setJobNumberBox(QLineEdit* lineEdit);
    void setYearDropdown(QComboBox* comboBox);
    void setMonthDropdown(QComboBox* comboBox);
    void setPostageBox(QLineEdit* lineEdit);     // ADDED: Postage input field
    void setCountBox(QLineEdit* lineEdit);       // ADDED: Count input field
    void setJobDataLockButton(QToolButton* button);
    void setEditButton(QToolButton* button);
    void setPostageLockButton(QToolButton* button);
    void setRunInitialButton(QPushButton* button);
    void setFinalStepButton(QPushButton* button);
    void setTerminalWindow(QTextEdit* textEdit);
    void setTextBrowser(QTextBrowser* textBrowser);
    void setTracker(QTableView* tableView);

    // Job management
    bool loadJob(const QString& year, const QString& month);
    void resetToDefaults();
    void saveJobState();
    void loadJobState();

    // Public methods for external access
    QString getJobNumber() const;
    QString getYear() const;
    QString getMonth() const;
    bool isJobDataLocked() const;
    bool isPostageDataLocked() const;

    // ADDED: Job data validation support
    bool hasJobData() const;

    // BaseTrackerController implementation
    void outputToTerminal(const QString& message, MessageType type) override;
    QTableView* getTrackerWidget() const override;
    QSqlTableModel* getTrackerModel() const override;
    QStringList getTrackerHeaders() const override;
    QList<int> getVisibleColumns() const override;
    QString formatCellData(int columnIndex, const QString& cellData) const override;

public slots:
    void onAddToTracker();
    void onCopyRowClicked();

signals:
    void jobOpened();
    void jobClosed();

private slots:
    // Lock button handlers
    void onJobDataLockClicked();
    void onEditButtonClicked();                          // ADDED: Edit button handler
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

private:
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
    QLineEdit* m_postageBox;              // ADDED: Postage input field
    QLineEdit* m_countBox;                // ADDED: Count input field
    QToolButton* m_jobDataLockBtn;
    QToolButton* m_editBtn;
    QToolButton* m_postageLockBtn;
    QPushButton* m_runInitialBtn;
    QPushButton* m_finalStepBtn;
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

    // Email dialog support
    bool m_waitingForEmailConfirmation;
    QString m_emailDialogPath;
    EmailConfirmationDialog* m_emailDialog;

    // Tracker model
    QSqlTableModel* m_trackerModel;

    // Initialization methods
    void initializeComponents();
    void connectSignals();
    void setupInitialState();
    void createJobFolder();

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

    // Validation - UPDATED: Now provides user feedback
    bool validateJobData();                              // CHANGED: No longer const, provides feedback
    bool validatePostageData();                          // ADDED: Postage validation
    bool validateScriptExecution(const QString& scriptName) const;
    
    // ADDED: Input formatting methods (like TMTERM)
    void formatPostageInput(const QString& text);
    void formatCountInput(const QString& text);

    // Tracker operations
    void refreshTrackerTable();
    void setupTrackerModel();

    // Excel copy functionality
    bool createExcelAndCopy(const QStringList& headers, const QStringList& rowData);

    // File management
    bool moveFilesToHomeFolder();
    bool copyFilesFromHomeFolder();
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
