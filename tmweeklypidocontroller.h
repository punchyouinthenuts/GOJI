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
#include <QFileSystemWatcher>
#include <memory>  // For std::unique_ptr
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
            : m_controller(controller) {
            if (m_controller) {
                m_controller->enableWorkflowButtons(false);
            }
        }
        
        ~FileOperationGuard() {
            if (m_controller) {
                m_controller->enableWorkflowButtons(true);
            }
        }
        
        // Non-copyable
        FileOperationGuard(const FileOperationGuard&) = delete;
        FileOperationGuard& operator=(const FileOperationGuard&) = delete;
        
    private:
        TMWeeklyPIDOController* m_controller;
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



    // Sequential file opening
    QTimer* m_sequentialOpenTimer = nullptr;
    QTimer* m_fileOpenTimeoutTimer = nullptr;  // Timeout protection
    QStringList m_pendingFilesToOpen;
    int m_currentFileIndex = 0;
    static const int MAX_FILE_OPEN_TIMEOUT_MS = 45000;  // 45 second timeout per file
    static constexpr int FIRST_FILE_DELAY_MS = 15000;   // 15 second delay after first file
    static constexpr int SUBSEQUENT_FILE_DELAY_MS = 7500; // 7.5 second delay between subsequent files
    
    // Directory and file extension constants
    static constexpr const char* PREFLIGHT_DIR = "PREFLIGHT";
    static constexpr const char* ART_DIR = "ART";
    static constexpr const char* CSV_EXTENSION = "*.csv";
    static constexpr const char* INDD_EXTENSION = "*.indd";
    
    bool m_openingFilesInProgress = false;  // Track file opening operation state
    std::unique_ptr<FileOperationGuard> m_fileOpGuard;  // Member guard for RAII

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
