#include "tmweeklypidocontroller.h"
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QStandardPaths>
#include <QRegularExpression>
#include <memory>  // For std::unique_ptr
#include <utility> // For std::as_const
#include "logger.h"
#include "tmweeklypidozipfilesdialog.h"

TMWeeklyPIDOController::TMWeeklyPIDOController(QObject *parent)
    : QObject(parent),
    m_dbManager(nullptr),
    m_scriptRunner(nullptr),
    m_fileManager(nullptr),
    m_fileListModel(nullptr),
    m_inputWatcher(nullptr),
    m_outputWatcher(nullptr),
    m_processRunning(false),
    m_sequentialOpenTimer(nullptr),
    m_currentFileIndex(0),
    m_openingFilesInProgress(false)
{
    Logger::instance().info("Initializing TMWeeklyPIDOController...");

    // Get the database manager
    m_dbManager = DatabaseManager::instance();
    if (!m_dbManager) {
        Logger::instance().error("Failed to get DatabaseManager instance");
    }

    // Create a script runner
    m_scriptRunner = new ScriptRunner(this);

    // Create file manager (reusing TM Weekly PC paths for now)
    m_fileManager = new TMWeeklyPCFileManager(new QSettings(QSettings::IniFormat, QSettings::UserScope, "GojiApp", "Goji"));

    // Setup file list model
    m_fileListModel = new QStringListModel(this);

    // Setup file system watchers
    m_inputWatcher = new QFileSystemWatcher(this);
    m_outputWatcher = new QFileSystemWatcher(this);

    // Setup sequential file opening timer
    m_sequentialOpenTimer = new QTimer(this);
    m_sequentialOpenTimer->setSingleShot(true);
    connect(m_sequentialOpenTimer, &QTimer::timeout, this, &TMWeeklyPIDOController::onSequentialFileOpenTimer);
    
    // Setup file open timeout timer
    m_fileOpenTimeoutTimer = new QTimer(this);
    m_fileOpenTimeoutTimer->setSingleShot(true);
    connect(m_fileOpenTimeoutTimer, &QTimer::timeout, this, [this]() {
        outputToTerminal("File open timeout - continuing with next file", Warning);
        m_currentFileIndex++;
        openNextFile();
    });

    // Set current working directory
    m_currentWorkingDirectory = QDir::currentPath();

    // Create required directories
    createDirectoriesIfNeeded();

    Logger::instance().info("TMWeeklyPIDOController initialization complete");
}

TMWeeklyPIDOController::~TMWeeklyPIDOController()
{
    // Clean up settings if we created it
    if (m_fileManager && m_fileManager->getSettings()) {
        delete m_fileManager->getSettings();
    }

    Logger::instance().info("TMWeeklyPIDOController destroyed");
}

void TMWeeklyPIDOController::initializeUI(
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
    DropWindow* dropWindowTMWPIDO)
{
    Logger::instance().info("Initializing TM WEEKLY PACK/IDO UI elements");

    // Store UI element pointers
    m_runInitialBtn = runInitialTMWPIDOBtn;
    m_runProcessBtn = runProcessTMWPIDOBtn;
    m_runMergeBtn = runMergeTMWPIDOBtn;
    m_runSortBtn = runSortTMWPIDOBtn;
    m_runPostPrintBtn = runPostPrintTMWPIDOBtn;
    m_openGeneratedFilesBtn = openGeneratedFilesTMWPIDOBtn;
    m_dpzipBackupBtn = dpzipbackupTMWPIDOBtn;
    m_printTMWPIDOBtn = printTMWPIDOBtn;
    m_fileList = fileListTMWPIDO;
    m_terminalWindow = terminalWindowTMWPIDO;
    m_textBrowser = textBrowserTMWPIDO;

    // Setup drop window
    m_dropWindow = dropWindowTMWPIDO;
    if (m_dropWindow) {
        m_dropWindow->setTargetDirectory(getInputDirectory());
        m_dropWindow->setSupportedExtensions({"xlsx", "xls", "csv"});
        connect(m_dropWindow, &DropWindow::filesDropped,
                this, &TMWeeklyPIDOController::onFilesDropped);
        connect(m_dropWindow, &DropWindow::fileDropError,
                this, &TMWeeklyPIDOController::onFileDropError);
        outputToTerminal("Drop window connected and ready", Info);
    }

    // Setup file list view
    if (m_fileList) {
        m_fileList->setModel(m_fileListModel);
        m_fileList->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_fileList->setSelectionMode(QAbstractItemView::SingleSelection);

        // Set larger font for better readability
        QFont listFont("Iosevka Custom", 16); // 16pt Iosevka Custom font
        listFont.setWeight(QFont::Medium);    // Set to Medium weight
        m_fileList->setFont(listFont);
    }

    // Connect UI signals to slots
    connectSignals();

    // Setup initial UI state
    setupInitialUIState();

    // Load instructions HTML
    loadInstructionsHtml();

    // Setup file system monitoring
    QString inputDir = getInputDirectory();
    QString outputDir = getOutputDirectory();

    if (QDir(inputDir).exists()) {
        m_inputWatcher->addPath(inputDir);
        outputToTerminal("Monitoring input directory: " + inputDir);
    }

    if (QDir(outputDir).exists()) {
        m_outputWatcher->addPath(outputDir);
        outputToTerminal("Monitoring output directory: " + outputDir);
    }

    // Initial file list population
    updateFileList();
    
    // Ensure print button is enabled
    if (m_printTMWPIDOBtn) {
        m_printTMWPIDOBtn->setEnabled(true);
    }

    Logger::instance().info("TM WEEKLY PACK/IDO UI initialization complete");
}

// Backward compatibility overload - delegates to new version with nullptr for print button
void TMWeeklyPIDOController::initializeUI(
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
    DropWindow* dropWindowTMWPIDO)
{
    // Delegate to the new version with nullptr for printTMWPIDOBtn
    initializeUI(
        runInitialTMWPIDOBtn,
        runProcessTMWPIDOBtn,
        runMergeTMWPIDOBtn,
        runSortTMWPIDOBtn,
        runPostPrintTMWPIDOBtn,
        openGeneratedFilesTMWPIDOBtn,
        dpzipbackupTMWPIDOBtn,
        nullptr,  // printTMWPIDOBtn - not available in old signature
        fileListTMWPIDO,
        terminalWindowTMWPIDO,
        textBrowserTMWPIDO,
        dropWindowTMWPIDO
    );
}

void TMWeeklyPIDOController::connectSignals()
{
    // Connect buttons with null pointer checks
    if (m_runInitialBtn) {
        connect(m_runInitialBtn, &QPushButton::clicked, this, &TMWeeklyPIDOController::onRunInitialClicked);
    }
    if (m_runProcessBtn) {
        connect(m_runProcessBtn, &QPushButton::clicked, this, &TMWeeklyPIDOController::onRunProcessClicked);
    }
    if (m_runMergeBtn) {
        connect(m_runMergeBtn, &QPushButton::clicked, this, &TMWeeklyPIDOController::onRunMergeClicked);
    }
    if (m_runSortBtn) {
        connect(m_runSortBtn, &QPushButton::clicked, this, &TMWeeklyPIDOController::onRunSortClicked);
    }
    if (m_runPostPrintBtn) {
        connect(m_runPostPrintBtn, &QPushButton::clicked, this, &TMWeeklyPIDOController::onRunPostPrintClicked);
    }
    if (m_openGeneratedFilesBtn) {
        connect(m_openGeneratedFilesBtn, &QPushButton::clicked, this, &TMWeeklyPIDOController::onOpenGeneratedFilesClicked);
    }
    if (m_printTMWPIDOBtn) {
        connect(m_printTMWPIDOBtn, &QPushButton::clicked, this, &TMWeeklyPIDOController::onPrintTMWPIDOClicked);
    }

    // Connect file list signals with null pointer checks
    if (m_fileList) {
        if (m_fileList->selectionModel()) {
            connect(m_fileList->selectionModel(), &QItemSelectionModel::selectionChanged,
                    this, &TMWeeklyPIDOController::onFileListSelectionChanged);
        }
        connect(m_fileList, &QListView::doubleClicked,
                this, &TMWeeklyPIDOController::onFileListDoubleClicked);
    }

    // Connect script runner signals with null pointer check
    if (m_scriptRunner) {
        connect(m_scriptRunner, &ScriptRunner::scriptOutput, this, &TMWeeklyPIDOController::onScriptOutput);
        connect(m_scriptRunner, &ScriptRunner::scriptFinished, this, &TMWeeklyPIDOController::onScriptFinished);
    }

    // Connect file system watchers with null pointer checks
    if (m_inputWatcher) {
        connect(m_inputWatcher, &QFileSystemWatcher::directoryChanged,
                this, &TMWeeklyPIDOController::onInputDirectoryChanged);
    }
    if (m_outputWatcher) {
        connect(m_outputWatcher, &QFileSystemWatcher::directoryChanged,
                this, &TMWeeklyPIDOController::onOutputDirectoryChanged);
    }
}

void TMWeeklyPIDOController::setupInitialUIState()
{
    // Enable all workflow buttons initially
    enableWorkflowButtons(true);

    // Set initial status
    outputToTerminal("TM WEEKLY PACK/IDO controller initialized", Success);
    outputToTerminal("Ready to process files", Info);
    outputToTerminal("Using folder structure: " + getBasePath(), Info);
}

void TMWeeklyPIDOController::loadInstructionsHtml()
{
    if (!m_textBrowser) {
        return;
    }

    // Load instructions HTML from resources
    QFile file(":/resources/tmweeklypido/instructions.html");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        QString content = stream.readAll();
        file.close();

        m_textBrowser->setHtml(content);
        outputToTerminal("Loaded instructions HTML", Info);
    } else {
        // Create fallback HTML content
        QString fallbackContent = QString(
                                      "<html><body style='font-family: Arial; padding: 20px;'>"
                                      "<h2>TM Weekly Packets & IDO</h2>"
                                      "<h3>Instructions:</h3>"
                                      "<ol>"
                                      "<li><strong>Upload Files:</strong> Drag XLSX/CSV files to the drop box on the left</li>"
                                      "<li><strong>Initial Processing:</strong> Files will be automatically processed when dropped</li>"
                                      "<li><strong>Select File:</strong> Choose a numbered file from the list for individual processing</li>"
                                      "<li><strong>Process Individual 1:</strong> Run 04DPINITIAL.py with selected file</li>"
                                      "<li><strong>Process Individual 2:</strong> Run 05DPMERGED.py with selected file</li>"
                                      "<li><strong>Run DPZIP:</strong> Create ZIP files for distribution</li>"
                                      "<li><strong>Run DPZIP BACKUP:</strong> Archive ZIP files and clean up</li>"
                                      "</ol>"
                                      "<h3>File Structure:</h3>"
                                      "<p><strong>RAW FILES:</strong> Input files (XLSX/CSV)</p>"
                                      "<p><strong>PROCESSED:</strong> Output files after processing</p>"
                                      "<p><strong>BM INPUT:</strong> Individual file processing</p>"
                                      "<p><strong>PREFLIGHT:</strong> Preflight files</p>"
                                      "<p><strong>BACKUP:</strong> Archived files</p>"
                                      "<p>Current time: %1</p>"
                                      "</body></html>"
                                      ).arg(QDateTime::currentDateTime().toString());

        m_textBrowser->setHtml(fallbackContent);
        outputToTerminal("Loaded fallback instructions", Warning);
    }
}

void TMWeeklyPIDOController::setTextBrowser(QTextBrowser* textBrowser)
{
    m_textBrowser = textBrowser;

    if (m_textBrowser) {
        // Load instructions immediately
        loadInstructionsHtml();
    }
}

void TMWeeklyPIDOController::onRunInitialClicked()
{
    if (m_processRunning) {
        outputToTerminal("A script is already running. Please wait for completion.", Warning);
        return;
    }

    outputToTerminal("Running initial processing script...", Info);

    // Disable workflow buttons
    enableWorkflowButtons(false);
    m_processRunning = true;

    // Get script path - using 01INITIAL.bat
    QString scriptPath = getScriptsDirectory() + "/01INITIAL.bat";

    // Run the batch script directly
    QStringList arguments;
    m_scriptRunner->runScript(scriptPath, arguments);
}

void TMWeeklyPIDOController::onRunProcessClicked()
{
    if (m_processRunning) {
        outputToTerminal("A script is already running. Please wait for completion.", Warning);
        return;
    }

    if (!validateSelectedFile()) {
        outputToTerminal("Please select a numbered file from the list first.", Warning);
        return;
    }

    outputToTerminal("Running Individual Processing 1 for file: " + m_selectedFileNumber, Info);

    enableWorkflowButtons(false);
    m_processRunning = true;

    QString scriptPath = getScriptsDirectory() + "/04DPINITIAL.py";

    QStringList arguments;
    arguments << m_selectedFileNumber;
    m_scriptRunner->runScript(scriptPath, arguments);
}

void TMWeeklyPIDOController::onRunMergeClicked()
{
    if (m_processRunning) {
        outputToTerminal("A script is already running. Please wait for completion.", Warning);
        return;
    }

    if (!validateSelectedFile()) {
        outputToTerminal("Please select a numbered file from the list first.", Warning);
        return;
    }

    outputToTerminal("Running Individual Processing 2 for file: " + m_selectedFileNumber, Info);

    enableWorkflowButtons(false);
    m_processRunning = true;

    QString scriptPath = getScriptsDirectory() + "/05DPMERGED.py";

    QStringList arguments;
    arguments << m_selectedFileNumber;
    m_scriptRunner->runScript(scriptPath, arguments);
}

void TMWeeklyPIDOController::onRunSortClicked()
{
    if (m_processRunning) {
        outputToTerminal("A script is already running. Please wait for completion.", Warning);
        return;
    }

    outputToTerminal("Running DPZIP processing...", Info);

    enableWorkflowButtons(false);
    m_processRunning = true;

    QString scriptPath = getScriptsDirectory() + "/06DPZIP.py";

    QStringList arguments;
    m_scriptRunner->runScript(scriptPath, arguments);
}

void TMWeeklyPIDOController::onRunPostPrintClicked()
{
    if (m_processRunning) {
        outputToTerminal("A script is already running. Please wait for completion.", Warning);
        return;
    }

    outputToTerminal("Running DPZIP BACKUP processing...", Info);

    enableWorkflowButtons(false);
    m_processRunning = true;

    QString scriptPath = getScriptsDirectory() + "/07DPZIPBACKUP.py";

    QStringList arguments;
    m_scriptRunner->runScript(scriptPath, arguments);
}

void TMWeeklyPIDOController::onOpenGeneratedFilesClicked()
{
    // Open the Bulk Mailer application or output directory
    openBulkMailerApplication();
}

void TMWeeklyPIDOController::onInputDirectoryChanged(const QString& path)
{
    Q_UNUSED(path)

    // Only update the file list to show numbered files
    // This will show an empty list if no numbered files exist
    updateFileList();
}

void TMWeeklyPIDOController::onOutputDirectoryChanged(const QString& path)
{
    Q_UNUSED(path)

    // Output directory changes don't affect the numbered file list
    outputToTerminal("Output directory updated", Info);
}

void TMWeeklyPIDOController::onScriptOutput(const QString& output)
{
    // Parse script output for status messages
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString& line : std::as_const(lines)) {
        if (line.contains("=== SCRIPT_SUCCESS ===")) {
            outputToTerminal("Script completed successfully", Success);
        } else if (line.contains("=== SCRIPT_ERROR ===")) {
            outputToTerminal("Script encountered an error", Error);
        } else if (line.contains("=== ") && line.endsWith(" ===")) {
            // Extract status messages from script
            QString statusMsg = line;
            statusMsg.remove("=== ").remove(" ===");
            statusMsg = statusMsg.replace('_', ' ').toLower();
            outputToTerminal("Status: " + statusMsg, Info);
        } else if (!line.trimmed().isEmpty()) {
            outputToTerminal(line, Info);
        }
    }

    // Track any generated files mentioned in output
    if (output.contains("Generated file:") || output.contains("Created:") || output.contains("Output:")) {
        // Extract file path if possible and track it
        QStringList words = output.split(' ', Qt::SkipEmptyParts);
        for (const QString& word : std::as_const(words)) {
            if (word.contains('.') && (word.endsWith(".csv") || word.endsWith(".txt") || word.endsWith(".pdf"))) {
                trackGeneratedFile(word);
            }
        }
    }
}

void TMWeeklyPIDOController::onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // Check if this was the 06DPZIP.py script that finished successfully
    bool was06DPZIP = m_scriptRunner && m_scriptRunner->getLastActualScript().contains("06DPZIP.py");
    
    // Re-enable workflow buttons
    enableWorkflowButtons(true);
    m_processRunning = false;

    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        outputToTerminal("Script execution completed successfully.", Success);

        // If 06DPZIP.py finished successfully, show ZIP files dialog instead of updating file list
        if (was06DPZIP) {
            showZipFilesDialog();
            return;
        }

        // After script completion, show numbered files if they exist, otherwise show input files
        updateFileList();

        // If no numbered files were found, fall back to showing input files
        if (m_fileListModel->stringList().isEmpty()) {
            refreshInputFileList();
        }
    } else {
        outputToTerminal("Script execution failed with exit code: " + QString::number(exitCode), Error);
    }
}

void TMWeeklyPIDOController::onFileListSelectionChanged()
{
    if (!m_fileList || !m_fileListModel) {
        return;
    }

    QModelIndex currentIndex = m_fileList->currentIndex();
    if (currentIndex.isValid()) {
        QString selectedFile = m_fileListModel->data(currentIndex, Qt::DisplayRole).toString();

        // Extract file number if this is a numbered file
        m_selectedFileNumber = extractFileNumber(selectedFile);

        outputToTerminal("Selected file: " + selectedFile, Info);
        if (!m_selectedFileNumber.isEmpty()) {
            outputToTerminal("File number extracted: " + m_selectedFileNumber, Info);
        }
    }
}

void TMWeeklyPIDOController::onFileListDoubleClicked(const QModelIndex& index)
{
    if (!m_fileListModel) {
        return;
    }

    QString fileName = m_fileListModel->data(index, Qt::DisplayRole).toString();
    
    // Strip prefixes before building file path
    if (fileName.startsWith("INPUT: ")) {
        fileName = fileName.mid(7);  // Remove "INPUT: " prefix
    } else if (fileName.startsWith("OUTPUT: ")) {
        fileName = fileName.mid(8);  // Remove "OUTPUT: " prefix
    }

    // Try input directory first
    QString filePath = getInputDirectory() + "/" + fileName;
    if (QFile::exists(filePath)) {
        outputToTerminal("Opening file: " + fileName, Info);
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        return;
    }

    // Try output directory
    filePath = getOutputDirectory() + "/" + fileName;
    if (QFile::exists(filePath)) {
        outputToTerminal("Opening file: " + fileName, Info);
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        return;
    }

    outputToTerminal("File not found: " + fileName, Error);
}

void TMWeeklyPIDOController::outputToTerminal(const QString& message, MessageType type)
{
    if (m_terminalWindow) {
        QString formattedMessage;
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");

        switch (type) {
        case Error:
            formattedMessage = QString("[%1] <span style='color:#ff5555;'>ERROR: %2</span>")
                                   .arg(timestamp, message);
            break;
        case Warning:
            formattedMessage = QString("[%1] <span style='color:#ffff55;'>WARNING: %2</span>")
                                   .arg(timestamp, message);
            break;
        case Success:
            formattedMessage = QString("[%1] <span style='color:#55ff55;'>SUCCESS: %2</span>")
                                   .arg(timestamp, message);
            break;
        case Info:
        default:
            formattedMessage = QString("[%1] %2").arg(timestamp, message);
            break;
        }

        m_terminalWindow->append(formattedMessage);
        m_terminalWindow->ensureCursorVisible();
    }

    // Also log to the logger
    Logger::instance().info(message);
}

void TMWeeklyPIDOController::updateFileList()
{
    QStringList files;
    QString inputDir = getInputDirectory(); // RAW FILES directory

    QDir dir(inputDir);
    if (dir.exists()) {
        // Only get files that start with 2 digits followed by a space (01 filename.csv, 02 filename.csv, etc.)
        QStringList nameFilters;
        nameFilters << "?? *.xlsx" << "?? *.csv"; // ?? matches exactly 2 characters

        QFileInfoList fileInfoList = dir.entryInfoList(nameFilters, QDir::Files, QDir::Name);

        for (int i = 0; i < fileInfoList.size(); ++i) {
            const QFileInfo& fileInfo = fileInfoList.at(i);
            QString fileName = fileInfo.fileName();
            // Additional check to ensure it starts with 2 digits and a space
            if (fileName.length() >= 3 &&
                fileName.at(0).isDigit() &&
                fileName.at(1).isDigit() &&
                fileName.at(2) == ' ') {
                files << QString("INPUT: %1").arg(fileName);
            }
        }
    }

    m_fileListModel->setStringList(files);

    // Only output messages when the file count changes, not on every call
    static int lastFileCount = -1;
    if (files.size() != lastFileCount) {
        lastFileCount = files.size();

        if (files.isEmpty()) {
            outputToTerminal("No numbered files found. Run INITIAL processing first.", Info);
        } else {
            outputToTerminal(QString("Found %1 numbered file(s) ready for processing").arg(files.size()), Success);
        }
    }
}

void TMWeeklyPIDOController::refreshInputFileList()
{
    if (!m_fileListModel) {
        return;
    }

    QString inputDir = getInputDirectory();
    QDir dir(inputDir);

    if (!dir.exists()) {
        return;
    }

    // Get input files (CSV, TXT, Excel files)
    QStringList filters;
    filters << "*.csv" << "*.txt" << "*.xlsx" << "*.xls";

    QStringList inputFiles = dir.entryList(filters, QDir::Files, QDir::Name);

    // Add "INPUT:" prefix to distinguish from output files
    QStringList prefixedInputFiles;
    for (const QString& file : std::as_const(inputFiles)) {
        prefixedInputFiles << QString("INPUT: %1").arg(file);
    }

    // Update model with input files (this will be combined with output files)
    m_fileListModel->setStringList(prefixedInputFiles);
}

void TMWeeklyPIDOController::refreshOutputFileList()
{
    if (!m_fileListModel) {
        return;
    }

    QString outputDir = getOutputDirectory();
    QDir dir(outputDir);

    if (!dir.exists()) {
        return;
    }

    // Get output files (CSV, TXT, PDF files)
    QStringList filters;
    filters << "*.csv" << "*.txt" << "*.pdf";

    QStringList outputFiles = dir.entryList(filters, QDir::Files, QDir::Name);

    // Add "OUTPUT:" prefix to distinguish from input files
    QStringList prefixedOutputFiles;
    for (const QString& file : std::as_const(outputFiles)) {
        prefixedOutputFiles << QString("OUTPUT: %1").arg(file);
    }

    // Combine with existing input files
    QStringList currentList = m_fileListModel->stringList();
    QStringList inputFiles;

    // Filter out old output files, keep input files
    for (const QString& item : std::as_const(currentList)) {
        if (item.startsWith("INPUT:")) {
            inputFiles << item;
        }
    }

    // Combine input and output files
    QStringList combinedList = inputFiles + prefixedOutputFiles;
    m_fileListModel->setStringList(combinedList);
}

QString TMWeeklyPIDOController::getInputDirectory() const
{
    // Updated to use WEEKLY IDO FULL structure
    return "C:/Goji/TRACHMAR/WEEKLY IDO FULL/RAW FILES";
}

QString TMWeeklyPIDOController::getOutputDirectory() const
{
    // Updated to use WEEKLY IDO FULL structure
    return "C:/Goji/TRACHMAR/WEEKLY IDO FULL/PROCESSED";
}

QString TMWeeklyPIDOController::getScriptPath(const QString& scriptName) const
{
    // Updated to use same directory as getScriptsDirectory() for consistency
    return getScriptsDirectory() + "/" + scriptName + ".py";
}

void TMWeeklyPIDOController::enableWorkflowButtons(bool enabled)
{
    if (m_runInitialBtn) m_runInitialBtn->setEnabled(enabled);      // ADD THIS LINE
    if (m_runProcessBtn) m_runProcessBtn->setEnabled(enabled);
    if (m_runMergeBtn) m_runMergeBtn->setEnabled(enabled);
    if (m_runSortBtn) m_runSortBtn->setEnabled(enabled);
    if (m_runPostPrintBtn) m_runPostPrintBtn->setEnabled(enabled);
    if (m_openGeneratedFilesBtn) m_openGeneratedFilesBtn->setEnabled(enabled);
}

void TMWeeklyPIDOController::trackGeneratedFile(const QString& filePath)
{
    if (!m_generatedFiles.contains(filePath)) {
        m_generatedFiles.append(filePath);
        outputToTerminal("Tracking generated file: " + filePath, Success);
    }
}

bool TMWeeklyPIDOController::validateWorkingState() const
{
    // Check if required directories exist
    QString inputDir = getInputDirectory();
    QString outputDir = getOutputDirectory();

    if (!QDir(inputDir).exists()) {
        return false;
    }

    if (!QDir(outputDir).exists()) {
        // Try to create output directory
        QDir().mkpath(outputDir);
        return QDir(outputDir).exists();
    }

    return true;
}

// New helper methods for improved functionality
QString TMWeeklyPIDOController::getBasePath() const
{
    return "C:/Goji/TRACHMAR/WEEKLY IDO FULL";
}

QString TMWeeklyPIDOController::getScriptsDirectory() const
{
    return "C:/Goji/scripts/TRACHMAR/WEEKLY IDO FULL";
}

QString TMWeeklyPIDOController::extractFileNumber(const QString& fileName) const
{
    // Extract number from filename like "01 filename.xlsx" or "INPUT: 02 filename.csv"
    QString cleanFileName = fileName;
    if (cleanFileName.startsWith("INPUT: ") || cleanFileName.startsWith("OUTPUT: ")) {
        cleanFileName = cleanFileName.mid(cleanFileName.indexOf(' ') + 1);
    }
    static const QRegularExpression regex(R"(^(\d{2})\s+)");
    QRegularExpressionMatch match = regex.match(cleanFileName);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    return QString();
}

bool TMWeeklyPIDOController::validateSelectedFile() const
{
    return !m_selectedFileNumber.isEmpty();
}

void TMWeeklyPIDOController::runInitialProcessing()
{
    outputToTerminal("Running initial processing script...", Info);

    QString scriptPath = getScriptsDirectory() + "/01INITIAL.bat";

    enableWorkflowButtons(false);
    m_processRunning = true;

    // Run the initial processing batch file
    m_scriptRunner->runScript(scriptPath, QStringList());
}

void TMWeeklyPIDOController::openBulkMailerApplication()
{
    // Updated path to the correct Bulk Mailer executable
    QString bulkMailerPath = "C:/Program Files (x86)/Satori Software/Bulk Mailer/BulkMailer.exe";

    if (QFile::exists(bulkMailerPath)) {
        outputToTerminal("Opening Bulk Mailer application...", Info);
        QDesktopServices::openUrl(QUrl::fromLocalFile(bulkMailerPath));
    } else {
        outputToTerminal("Bulk Mailer application not found at: " + bulkMailerPath, Warning);
        outputToTerminal("Please verify the Bulk Mailer installation.", Warning);
    }
}

bool TMWeeklyPIDOController::createDirectoriesIfNeeded()
{
    QStringList requiredDirs = {
        getBasePath(),                    // Base directory for all operations
        getInputDirectory(),              // RAW FILES - where users drop input files
        getOutputDirectory(),             // PROCESSED - main output directory
        getBasePath() + "/BM INPUT",      // Bulk Mailer input staging
        getBasePath() + "/" + PREFLIGHT_DIR, // CSV files for print matching
        getBasePath() + "/BACKUP",       // Archived files
        getBasePath() + "/TEMP",         // Temporary processing files
        getBasePath() + "/" + ART_DIR     // INDD template files
    };

    QDir dir;
    bool allCreated = true;

    for (const QString& dirPath : requiredDirs) {
        if (!dir.exists(dirPath)) {
            if (dir.mkpath(dirPath)) {
                Logger::instance().info("Created directory: " + dirPath);
            } else {
                Logger::instance().error("Failed to create directory: " + dirPath);
                allCreated = false;
            }
        }
    }

    return allCreated;
}

void TMWeeklyPIDOController::onFilesDropped(const QStringList& filePaths)
{
    outputToTerminal(QString("Processed %1 dropped file(s)...").arg(filePaths.size()), Info);

    // The DropWindow has already copied the files to the target directory
    // filePaths contains the paths to the files in the RAW FILES directory
    for (const QString& filePath : filePaths) {
        QFileInfo fileInfo(filePath);
        QString fileName = fileInfo.fileName();
        outputToTerminal("File available: " + fileName + " in RAW FILES", Success);
    }

    // DO NOT refresh the file list here - we only want to show numbered files
    // The list should remain empty until after initial processing creates numbered files

    outputToTerminal("Files ready for initial processing. Click 'Run Initial' to process dropped files.", Info);
}

void TMWeeklyPIDOController::onFileDropError(const QString& errorMessage)
{
    outputToTerminal("File drop error: " + errorMessage, Warning);
}

void TMWeeklyPIDOController::showZipFilesDialog()
{
    if (m_zipFilesDialog) {
        m_zipFilesDialog->close();
        m_zipFilesDialog->deleteLater();
        m_zipFilesDialog = nullptr;
    }

    QString zipDirectory = "C:/Goji/TRACHMAR/WEEKLY IDO FULL";
    m_zipFilesDialog = new TMWeeklyPIDOZipFilesDialog(zipDirectory, this->parent() ? qobject_cast<QWidget*>(this->parent()) : nullptr);
    
    // Disable dpzipbackup button while dialog is open
    if (m_dpzipBackupBtn) {
        m_dpzipBackupBtn->setEnabled(false);
    }
    
    // Connect dialog finished signal to our cleanup slot
    connect(m_zipFilesDialog, &QDialog::finished, this, &TMWeeklyPIDOController::onZipFilesDialogClosed);
    
    outputToTerminal("Opening ZIP files dialog for email integration...", Info);
    m_zipFilesDialog->show();
}

void TMWeeklyPIDOController::onZipFilesDialogClosed()
{
    // Re-enable dpzipbackup button
    if (m_dpzipBackupBtn) {
        m_dpzipBackupBtn->setEnabled(true);
    }
    
    // Clean up dialog pointer
    if (m_zipFilesDialog) {
        m_zipFilesDialog->deleteLater();
        m_zipFilesDialog = nullptr;
    }
    
    outputToTerminal("ZIP files dialog closed - dpzipbackup button re-enabled", Info);
}

void TMWeeklyPIDOController::onPrintTMWPIDOClicked()
{
    if (m_processRunning) {
        outputToTerminal("Cannot start file opening - a script is currently running", Warning);
        return;
    }

    if (m_openingFilesInProgress) {
        outputToTerminal("File opening sequence already in progress", Warning);
        return;
    }

    outputToTerminal("Scanning for CSV files in PREFLIGHT directory...", Info);

    // Scan CSV files in PREFLIGHT directory
    QString preflightDir = getBasePath() + "/" + PREFLIGHT_DIR;
    QDir dir(preflightDir);

    if (!dir.exists()) {
        outputToTerminal("PREFLIGHT directory not found: " + preflightDir, Error);
        return;
    }

    QStringList csvFiles = dir.entryList(QStringList() << CSV_EXTENSION, QDir::Files, QDir::Name);

    if (csvFiles.isEmpty()) {
        outputToTerminal("No CSV files found in PREFLIGHT directory", Warning);
        return;
    }

    outputToTerminal(QString("Found %1 CSV file(s) in PREFLIGHT").arg(csvFiles.size()), Info);

    // Find matching INDD files in ART directory
    QString artDir = getBasePath() + "/" + ART_DIR;
    QDir artDirectory(artDir);

    if (!artDirectory.exists()) {
        outputToTerminal("ART directory not found: " + artDir, Error);
        return;
    }

    QStringList inddFiles = artDirectory.entryList(QStringList() << INDD_EXTENSION, QDir::Files, QDir::Name);

    // Match CSV base names to INDD files
    m_pendingFilesToOpen.clear();

    for (int i = 0; i < csvFiles.size(); ++i) {
        const QString& csvFile = csvFiles.at(i);
        QString baseName = QFileInfo(csvFile).baseName();

        // Look for matching INDD file
        for (int j = 0; j < inddFiles.size(); ++j) {
            const QString& inddFile = inddFiles.at(j);
            QString inddBaseName = QFileInfo(inddFile).baseName();

            if (baseName == inddBaseName) {
                QString fullPath = artDirectory.absoluteFilePath(inddFile);
                m_pendingFilesToOpen.append(fullPath);
                outputToTerminal(QString("Matched: %1 -> %2").arg(csvFile, inddFile), Success);
                break;
            }
        }
    }

    if (m_pendingFilesToOpen.isEmpty()) {
        outputToTerminal("No matching INDD files found for CSV files", Warning);
        return;
    }

    outputToTerminal(QString("Found %1 matching INDD file(s) to open").arg(m_pendingFilesToOpen.size()), Success);

    // Set state flags and create RAII guard
    m_openingFilesInProgress = true;
    m_fileOpGuard = std::make_unique<FileOperationGuard>(this);

    // Start sequential file opening
    m_currentFileIndex = 0;
    openNextFile();
}

void TMWeeklyPIDOController::openNextFile()
{
    // Stop any existing timeout timer
    if (m_fileOpenTimeoutTimer) {
        m_fileOpenTimeoutTimer->stop();
    }
    
    if (m_currentFileIndex >= m_pendingFilesToOpen.size()) {
        // All files opened - clean up state
        m_fileOpGuard.reset();
        m_openingFilesInProgress = false;
        m_pendingFilesToOpen.clear();  // Clear for next run
        outputToTerminal("All INDD files opened successfully", Success);
        return;
    }
    
    QString filePath = m_pendingFilesToOpen[m_currentFileIndex];
    QFileInfo fileInfo(filePath);
    
    if (!fileInfo.exists()) {
        outputToTerminal("File not found: " + filePath, Error);
        m_currentFileIndex++;
        // Check if this was the last file
        if (m_currentFileIndex >= m_pendingFilesToOpen.size()) {
            // Clean up state
            m_fileOpGuard.reset();
            m_openingFilesInProgress = false;
            m_pendingFilesToOpen.clear();  // Clear for next run
            outputToTerminal("File opening sequence completed with errors", Warning);
            return;
        }
        // Continue with next file after short delay
        if (m_sequentialOpenTimer) {
            m_sequentialOpenTimer->start(1000);
        }
        return;
    }
    
    outputToTerminal(QString("Opening file %1 of %2: %3")
                     .arg(m_currentFileIndex + 1)
                     .arg(m_pendingFilesToOpen.size())
                     .arg(fileInfo.fileName()), Info);
    
    // IMPROVED: Start timeout timer to prevent hanging on slow file opens
    if (m_fileOpenTimeoutTimer) {
        m_fileOpenTimeoutTimer->start(MAX_FILE_OPEN_TIMEOUT_MS);
    }
    
    // Open the file
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    
    if (!success) {
        outputToTerminal("Failed to open: " + fileInfo.fileName(), Error);
        // Stop timeout timer since open failed immediately
        if (m_fileOpenTimeoutTimer) {
            m_fileOpenTimeoutTimer->stop();
        }
    }
    
    m_currentFileIndex++;
    
    // Stop timeout timer before starting next delay
    if (m_fileOpenTimeoutTimer) {
        m_fileOpenTimeoutTimer->stop();
    }
    
    // Set appropriate delay before next file
    int delay;
    if (m_currentFileIndex == 1) {
        // First file opened, wait before second
        delay = FIRST_FILE_DELAY_MS;
        outputToTerminal("Waiting 15 seconds before opening next file...", Info);
    } else {
        // Subsequent files, shorter delay
        delay = SUBSEQUENT_FILE_DELAY_MS;
        outputToTerminal("Waiting 7.5 seconds before opening next file...", Info);
    }
    
    if (m_sequentialOpenTimer) {
        m_sequentialOpenTimer->start(delay);
    }
}

void TMWeeklyPIDOController::onSequentialFileOpenTimer()
{
    openNextFile();
}
