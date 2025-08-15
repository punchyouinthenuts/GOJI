#ifndef TMWEEKLYPIDOCONTROLLER_H
#define TMWEEKLYPIDOCONTROLLER_H

#include <QObject>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QTextEdit>
#include <QListView>
#include <QListWidget>
#include <QTextBrowser>
#include <QCheckBox>
#include <QStringListModel>
#include <QTimer>
#include <QMutex>
#include <QFileSystemWatcher>
#include "databasemanager.h"
#include "dropwindow.h"
#include "scriptrunner.h"
#include "tmweeklypcfilemanager.h"

// Forward declarations
class TMWeeklyPIDOZipFilesDialog;

class TMWeeklyPIDOController : public QObject
{
    Q_OBJECT

signals:
    void fileListCleared();

public:
    // Message type enum for colored terminal output
    enum MessageType {
        Info,
        Warning,
        Error,
        Success
    };

    explicit TMWeeklyPIDOController(QObject *parent = nullptr);
    ~TMWeeklyPIDOController();

    void initializeUI(
        QPushButton* runInitialTMWPIDOBtn,
        QPushButton* runProcessTMWPIDOBtn,
        QPushButton* runMergeTMWPIDOBtn,
        QPushButton* runSortTMWPIDOBtn,
        QPushButton* runPostPrintTMWPIDOBtn,
        QPushButton* openGeneratedFilesTMWPIDOBtn,
        QPushButton* dpzipbackupTMWPIDOBtn,
        QPushButton* printTMWPIDOBtn,
        QListView* fileListTMWPIDO,
        QTextEdit* terminalWindowTMWPIDO,
        QTextBrowser* textBrowserTMWPIDO,
        DropWindow* dropWindowTMWPIDO = nullptr
        );

    // Backward compatibility overload - CRITICAL: prevents breaking existing code
    void initializeUI(
        QPushButton* runInitialTMWPIDOBtn,
        QPushButton* runProcessTMWPIDOBtn,
        QPushButton* runMergeTMWPIDOBtn,
        QPushButton* runSortTMWPIDOBtn,
        QPushButton* runPostPrintTMWPIDOBtn,
        QPushButton* openGeneratedFilesTMWPIDOBtn,
        QPushButton* dpzipbackupTMWPIDOBtn,
        QListView* fileListTMWPIDO,
        QTextEdit* terminalWindowTMWPIDO,
        QTextBrowser* textBrowserTMWPIDO,
        DropWindow* dropWindowTMWPIDO = nullptr
        );

    void setTextBrowser(QTextBrowser* textBrowser);

private slots:
    // Button handlers
    void onRunInitialClicked();
    void onRunProcessClicked();
    void onRunMergeClicked();
    void onRunSortClicked();
    void onRunPostPrintClicked();
    void onOpenGeneratedFilesClicked();

    // File system monitoring
    void onInputDirectoryChanged(const QString& path);
    void onOutputDirectoryChanged(const QString& path);

    // Script signals
    void onScriptOutput(const QString& output);
    void onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus);

    // File list management
    void onFileListSelectionChanged();
    void onFileListDoubleClicked(const QModelIndex& index);

    // Drop window handlers
    void onFilesDropped(const QStringList& filePaths);
    void onFileDropError(const QString& errorMessage);

    // ZIP files dialog handler
    void onZipFilesDialogClosed();

    // Print button handler
    void onPrintTMWPIDOClicked();

    // Timer handler for sequential file opening
    void onSequentialFileOpenTimer();

    // Helper method for sequential file opening
    void openNextFile();

private:
    // RAII helper for UI state management during file operations
    class FileOperationGuard {
    public:
        explicit FileOperationGuard(TMWeeklyPIDOController* controller) 
            : m_controller(controller), m_wasEnabled(false) {
            if (m_controller && m_controller->m_printTMWPIDOBtn) {
                m_wasEnabled = m_controller->m_printTMWPIDOBtn->isEnabled();
                m_controller->m_printTMWPIDOBtn->setEnabled(false);
            }
            if (m_controller) {
                m_controller->enableWorkflowButtons(false);
            }
        }
        
        ~FileOperationGuard() {
            if (m_controller) {
                m_controller->enableWorkflowButtons(true);
                // Note: Print button state managed separately by file list logic
            }
        }
        
        // Non-copyable
        FileOperationGuard(const FileOperationGuard&) = delete;
        FileOperationGuard& operator=(const FileOperationGuard&) = delete;
        
    private:
        TMWeeklyPIDOController* m_controller;
        bool m_wasEnabled;
    };
    // UI element pointers
    QPushButton* m_runInitialBtn = nullptr;  // ADD THIS LINE
    QPushButton* m_runProcessBtn = nullptr;
    QPushButton* m_runMergeBtn = nullptr;
    QPushButton* m_runSortBtn = nullptr;
    QPushButton* m_runPostPrintBtn = nullptr;
    QPushButton* m_openGeneratedFilesBtn = nullptr;
    QPushButton* m_dpzipBackupBtn = nullptr;
    QPushButton* m_printTMWPIDOBtn = nullptr;
    QListView* m_fileList = nullptr;
    QTextEdit* m_terminalWindow = nullptr;
    QTextBrowser* m_textBrowser = nullptr;
    DropWindow* m_dropWindow = nullptr;

    // Support objects
    DatabaseManager* m_dbManager = nullptr;
    ScriptRunner* m_scriptRunner = nullptr;
    TMWeeklyPCFileManager* m_fileManager = nullptr;
    QStringListModel* m_fileListModel = nullptr;
    QFileSystemWatcher* m_inputWatcher = nullptr;
    QFileSystemWatcher* m_outputWatcher = nullptr;

    // State variables
    bool m_processRunning = false;
    QString m_currentWorkingDirectory;
    QStringList m_generatedFiles;
    QString m_selectedFileNumber;
    TMWeeklyPIDOZipFilesDialog* m_zipFilesDialog = nullptr;

    // File list tracking for print button enablement
    int m_previousFileCount = 0;
    bool m_fileListWasPopulated = false;
    mutable QMutex m_fileListStateMutex;  // Thread safety for state tracking
    int m_consecutiveEmptyChecks = 0;     // Prevent rapid enable/disable cycles

    // Sequential file opening
    QTimer* m_sequentialOpenTimer = nullptr;
    QTimer* m_fileOpenTimeoutTimer = nullptr;  // Timeout protection
    QStringList m_pendingFilesToOpen;
    int m_currentFileIndex = 0;
    static const int MAX_FILE_OPEN_TIMEOUT_MS = 45000;  // 45 second timeout per file

    // Utility methods
    void connectSignals();
    void setupInitialUIState();
    void loadInstructionsHtml();
    void outputToTerminal(const QString& message, MessageType type = Info);
    void updateFileList();
    void refreshInputFileList();
    void refreshOutputFileList();
    QString getInputDirectory() const;
    QString getOutputDirectory() const;
    QString getScriptPath(const QString& scriptName) const;
    void enableWorkflowButtons(bool enabled);
    void trackGeneratedFile(const QString& filePath);
    bool validateWorkingState() const;

    // New methods for improved functionality
    QString getBasePath() const;
    QString getScriptsDirectory() const;
    QString extractFileNumber(const QString& fileName) const;
    bool validateSelectedFile() const;
    void runInitialProcessing();
    void openBulkMailerApplication();
    bool createDirectoriesIfNeeded();
    void showZipFilesDialog();
};

#endif // TMWEEKLYPIDOCONTROLLER_H
