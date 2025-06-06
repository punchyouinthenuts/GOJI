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

#include "logger.h"

TMWeeklyPIDOController::TMWeeklyPIDOController(QObject *parent)
    : QObject(parent),
    m_dbManager(nullptr),
    m_scriptRunner(nullptr),
    m_fileManager(nullptr),
    m_fileListModel(nullptr),
    m_inputWatcher(nullptr),
    m_outputWatcher(nullptr),
    m_processRunning(false)
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

    // Set current working directory
    m_currentWorkingDirectory = QDir::currentPath();

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
    QPushButton* runProcessTMWPIDOBtn,
    QPushButton* runMergeTMWPIDOBtn,
    QPushButton* runSortTMWPIDOBtn,
    QPushButton* runPostPrintTMWPIDOBtn,
    QPushButton* openGeneratedFilesTMWPIDOBtn,
    QListView* fileListTMWPIDO,
    QTextEdit* terminalWindowTMWPIDO,
    QTextBrowser* textBrowserTMWPIDO)
{
    Logger::instance().info("Initializing TM WEEKLY PACK/IDO UI elements");

    // Store UI element pointers
    m_runProcessBtn = runProcessTMWPIDOBtn;
    m_runMergeBtn = runMergeTMWPIDOBtn;
    m_runSortBtn = runSortTMWPIDOBtn;
    m_runPostPrintBtn = runPostPrintTMWPIDOBtn;
    m_openGeneratedFilesBtn = openGeneratedFilesTMWPIDOBtn;
    m_fileList = fileListTMWPIDO;
    m_terminalWindow = terminalWindowTMWPIDO;
    m_textBrowser = textBrowserTMWPIDO;

    // Setup file list view
    if (m_fileList) {
        m_fileList->setModel(m_fileListModel);
        m_fileList->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_fileList->setSelectionMode(QAbstractItemView::SingleSelection);
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

    Logger::instance().info("TM WEEKLY PACK/IDO UI initialization complete");
}

void TMWeeklyPIDOController::connectSignals()
{
    // Connect buttons
    connect(m_runProcessBtn, &QPushButton::clicked, this, &TMWeeklyPIDOController::onRunProcessClicked);
    connect(m_runMergeBtn, &QPushButton::clicked, this, &TMWeeklyPIDOController::onRunMergeClicked);
    connect(m_runSortBtn, &QPushButton::clicked, this, &TMWeeklyPIDOController::onRunSortClicked);
    connect(m_runPostPrintBtn, &QPushButton::clicked, this, &TMWeeklyPIDOController::onRunPostPrintClicked);
    connect(m_openGeneratedFilesBtn, &QPushButton::clicked, this, &TMWeeklyPIDOController::onOpenGeneratedFilesClicked);

    // Connect file list signals
    if (m_fileList) {
        connect(m_fileList->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &TMWeeklyPIDOController::onFileListSelectionChanged);
        connect(m_fileList, &QListView::doubleClicked,
                this, &TMWeeklyPIDOController::onFileListDoubleClicked);
    }

    // Connect script runner signals
    connect(m_scriptRunner, &ScriptRunner::scriptOutput, this, &TMWeeklyPIDOController::onScriptOutput);
    connect(m_scriptRunner, &ScriptRunner::scriptFinished, this, &TMWeeklyPIDOController::onScriptFinished);

    // Connect file system watchers
    connect(m_inputWatcher, &QFileSystemWatcher::directoryChanged,
            this, &TMWeeklyPIDOController::onInputDirectoryChanged);
    connect(m_outputWatcher, &QFileSystemWatcher::directoryChanged,
            this, &TMWeeklyPIDOController::onOutputDirectoryChanged);
}

void TMWeeklyPIDOController::setupInitialUIState()
{
    // Enable all workflow buttons initially
    enableWorkflowButtons(true);

    // Set initial status
    outputToTerminal("TM WEEKLY PACK/IDO controller initialized", Success);
    outputToTerminal("Ready to process files", Info);
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
                                      "<p>Instructions file could not be loaded from resources.</p>"
                                      "<p>Please ensure instructions.html is properly included in the build.</p>"
                                      "<h3>Basic Workflow:</h3>"
                                      "<ol>"
                                      "<li>Run Process - Initial data processing</li>"
                                      "<li>Run Merge - Merge processed data files</li>"
                                      "<li>Run Sort - Sort merged data</li>"
                                      "<li>Run Post Print - Final processing and output</li>"
                                      "<li>Open Generated Files - View output files</li>"
                                      "</ol>"
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

void TMWeeklyPIDOController::onRunProcessClicked()
{
    if (m_processRunning) {
        outputToTerminal("A script is already running. Please wait for completion.", Warning);
        return;
    }

    outputToTerminal("Running Process script...");

    // Disable workflow buttons
    enableWorkflowButtons(false);
    m_processRunning = true;

    // Get script path
    QString script = getScriptPath("01PROCESS");

    // Run the script
    m_scriptRunner->runScript("python", QStringList() << script);
}

void TMWeeklyPIDOController::onRunMergeClicked()
{
    if (m_processRunning) {
        outputToTerminal("A script is already running. Please wait for completion.", Warning);
        return;
    }

    outputToTerminal("Running Merge script...");

    // Disable workflow buttons
    enableWorkflowButtons(false);
    m_processRunning = true;

    // Get script path
    QString script = getScriptPath("02MERGE");

    // Run the script
    m_scriptRunner->runScript("python", QStringList() << script);
}

void TMWeeklyPIDOController::onRunSortClicked()
{
    if (m_processRunning) {
        outputToTerminal("A script is already running. Please wait for completion.", Warning);
        return;
    }

    outputToTerminal("Running Sort script...");

    // Disable workflow buttons
    enableWorkflowButtons(false);
    m_processRunning = true;

    // Get script path
    QString script = getScriptPath("03SORT");

    // Run the script
    m_scriptRunner->runScript("python", QStringList() << script);
}

void TMWeeklyPIDOController::onRunPostPrintClicked()
{
    if (m_processRunning) {
        outputToTerminal("A script is already running. Please wait for completion.", Warning);
        return;
    }

    outputToTerminal("Running Post Print script...");

    // Disable workflow buttons
    enableWorkflowButtons(false);
    m_processRunning = true;

    // Get script path
    QString script = getScriptPath("04POSTPRINT");

    // Run the script
    m_scriptRunner->runScript("python", QStringList() << script);
}

void TMWeeklyPIDOController::onOpenGeneratedFilesClicked()
{
    QString outputDir = getOutputDirectory();

    if (!QDir(outputDir).exists()) {
        outputToTerminal("Output directory does not exist: " + outputDir, Error);
        return;
    }

    outputToTerminal("Opening output directory: " + outputDir);

    // Open the output directory in file explorer
    QDesktopServices::openUrl(QUrl::fromLocalFile(outputDir));
}

void TMWeeklyPIDOController::onInputDirectoryChanged(const QString& path)
{
    outputToTerminal("Input directory changed: " + path);
    refreshInputFileList();
}

void TMWeeklyPIDOController::onOutputDirectoryChanged(const QString& path)
{
    outputToTerminal("Output directory changed: " + path);
    refreshOutputFileList();
}

void TMWeeklyPIDOController::onScriptOutput(const QString& output)
{
    // Display output in terminal
    outputToTerminal(output);

    // Track any generated files mentioned in output
    if (output.contains("Generated file:") || output.contains("Created:") || output.contains("Output:")) {
        // Extract file path if possible and track it
        QStringList words = output.split(' ', Qt::SkipEmptyParts);
        for (const QString& word : words) {
            if (word.contains('.') && (word.endsWith(".csv") || word.endsWith(".txt") || word.endsWith(".pdf"))) {
                trackGeneratedFile(word);
            }
        }
    }
}

void TMWeeklyPIDOController::onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // Re-enable workflow buttons
    enableWorkflowButtons(true);
    m_processRunning = false;

    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        outputToTerminal("Script execution completed successfully.", Success);

        // Refresh file lists to show any new files
        updateFileList();
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
        outputToTerminal("Selected file: " + selectedFile);
    }
}

void TMWeeklyPIDOController::onFileListDoubleClicked(const QModelIndex& index)
{
    if (!m_fileListModel) {
        return;
    }

    QString fileName = m_fileListModel->data(index, Qt::DisplayRole).toString();
    QString filePath = getOutputDirectory() + "/" + fileName;

    if (QFile::exists(filePath)) {
        outputToTerminal("Opening file: " + fileName);
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    } else {
        outputToTerminal("File not found: " + filePath, Error);
    }
}

void TMWeeklyPIDOController::outputToTerminal(const QString& message, MessageType type)
{
    if (m_terminalWindow) {
        QString formattedMessage;

        switch (type) {
        case Error:
            formattedMessage = QString("<span style='color:#ff5555;'>%1</span>").arg(message);
            break;
        case Warning:
            formattedMessage = QString("<span style='color:#ffff55;'>%1</span>").arg(message);
            break;
        case Success:
            formattedMessage = QString("<span style='color:#55ff55;'>%1</span>").arg(message);
            break;
        case Info:
        default:
            formattedMessage = message; // Default white color
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
    refreshInputFileList();
    refreshOutputFileList();
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
    for (const QString& file : inputFiles) {
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
    for (const QString& file : outputFiles) {
        prefixedOutputFiles << QString("OUTPUT: %1").arg(file);
    }

    // Combine with existing input files
    QStringList currentList = m_fileListModel->stringList();
    QStringList inputFiles;

    // Filter out old output files, keep input files
    for (const QString& item : currentList) {
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
    // Use TM WEEKLY PC input path as base, modify for PACK/IDO
    if (m_fileManager) {
        QString basePath = m_fileManager->getBasePath();
        return basePath + "/WEEKLY PACK&IDO/JOB/INPUT";
    }
    return "C:/Goji/TRACHMAR/WEEKLY PACK&IDO/JOB/INPUT";
}

QString TMWeeklyPIDOController::getOutputDirectory() const
{
    // Use TM WEEKLY PC output path as base, modify for PACK/IDO
    if (m_fileManager) {
        QString basePath = m_fileManager->getBasePath();
        return basePath + "/WEEKLY PACK&IDO/JOB/OUTPUT";
    }
    return "C:/Goji/TRACHMAR/WEEKLY PACK&IDO/JOB/OUTPUT";
}

QString TMWeeklyPIDOController::getScriptPath(const QString& scriptName) const
{
    // Get scripts directory for PACK/IDO
    QString scriptsDir;
    if (m_fileManager) {
        QString basePath = m_fileManager->getBasePath();
        scriptsDir = "C:/Goji/Scripts/TRACHMAR/WEEKLY PACKET & IDO";
    } else {
        scriptsDir = "C:/Goji/Scripts/TRACHMAR/WEEKLY PACKET & IDO";
    }

    return scriptsDir + "/" + scriptName + ".py";
}

void TMWeeklyPIDOController::enableWorkflowButtons(bool enabled)
{
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
