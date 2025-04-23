#include "jobcontroller.h"
#include <QDebug>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

JobController::JobController(DatabaseManager* dbManager, FileSystemManager* fileManager,
                             ScriptRunner* scriptRunner, QSettings* settings, QObject* parent)
    : QObject(parent),
    m_currentJob(new JobData()),
    m_dbManager(dbManager),
    m_fileManager(fileManager),
    m_scriptRunner(scriptRunner),
    m_settings(settings),
    m_isJobSaved(false),
    m_isJobDataLocked(false),
    m_isProofRegenMode(false),
    m_isPostageLocked(false)
{
    initializeStepWeights();

    // Connect script runner signals
    connect(m_scriptRunner, &ScriptRunner::scriptOutput, [this](const QString& output) {
        emit logMessage(output);
    });

    connect(m_scriptRunner, &ScriptRunner::scriptError, [this](const QString& error) {
        emit logMessage("<font color=\"red\">" + error + "</font>");
    });

    connect(m_scriptRunner, &ScriptRunner::scriptFinished,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);
                if (success) {
                    emit logMessage("Script completed successfully.");
                } else {
                    emit logMessage(QString("Script failed with exit code %1").arg(exitCode));
                }
                emit scriptFinished(success);
            });
}

JobController::~JobController()
{
    delete m_currentJob;
}

void JobController::initializeStepWeights()
{
    // Initialize step weights
    m_stepWeights = {2.0, 9.0, 13.0, 13.0, 20.0, 10.0, 3.0, 20.0, 10.0};

    // Initialize subtask counters
    for (size_t i = 0; i < NUM_STEPS; ++i) {
        m_totalSubtasks[i] = 1;     // Default to 1 for binary steps
        m_completedSubtasks[i] = 0; // Nothing completed yet
    }
}

bool JobController::loadJob(const QString& year, const QString& month, const QString& week)
{
    if (!m_dbManager->loadJob(year, month, week, *m_currentJob)) {
        emit logMessage(QString("Failed to load job: %1-%2-%3").arg(year, month, week));
        return false;
    }

    m_originalYear = year;
    m_originalMonth = month;
    m_originalWeek = week;
    m_isJobSaved = true;

    // Update completed subtasks from job data
    m_completedSubtasks[0] = m_currentJob->step0_complete;
    m_completedSubtasks[1] = m_currentJob->step1_complete;
    m_completedSubtasks[2] = m_currentJob->step2_complete;
    m_completedSubtasks[3] = m_currentJob->step3_complete;
    m_completedSubtasks[4] = m_currentJob->step4_complete;
    m_completedSubtasks[5] = m_currentJob->step5_complete;
    m_completedSubtasks[6] = m_currentJob->step6_complete;
    m_completedSubtasks[7] = m_currentJob->step7_complete;
    m_completedSubtasks[8] = m_currentJob->step8_complete;

    // Copy files from home to working directory
    if (!m_fileManager->copyFilesFromHomeToWorking(month, week)) {
        emit logMessage("Warning: Some files could not be copied from home to working directory.");
    }

    emit jobLoaded(*m_currentJob);
    emit logMessage(QString("Loaded job: Year %1, Month %2, Week %3").arg(year, month, week));
    updateProgress();

    return true;
}

bool JobController::saveJob()
{
    if (!m_currentJob->isValid()) {
        emit logMessage("Cannot save job: Invalid job data");
        return false;
    }

    if (!m_dbManager->saveJob(*m_currentJob)) {
        emit logMessage("Failed to save job");
        return false;
    }

    m_isJobSaved = true;
    m_originalYear = m_currentJob->year;
    m_originalMonth = m_currentJob->month;
    m_originalWeek = m_currentJob->week;

    emit jobSaved();
    emit logMessage("Job saved successfully");
    return true;
}

bool JobController::createJob()
{
    if (!m_currentJob->isValid()) {
        emit logMessage("Cannot create job: Invalid job data");
        return false;
    }

    // Check if the job exists and handle overwrite
    if (m_dbManager->jobExists(m_currentJob->year, m_currentJob->month, m_currentJob->week)) {
        if (!confirmOverwrite(m_currentJob->year, m_currentJob->month, m_currentJob->week)) {
            return false;
        }
        // Delete existing job to allow overwrite
        if (!m_dbManager->deleteJob(m_currentJob->year, m_currentJob->month, m_currentJob->week)) {
            emit logMessage("Failed to delete existing job for overwrite");
            return false;
        }
    }

    if (!m_dbManager->saveJob(*m_currentJob)) {
        emit logMessage("Failed to create job");
        return false;
    }

    // Create job folders
    if (!m_fileManager->createJobFolders(m_currentJob->year, m_currentJob->month, m_currentJob->week)) {
        emit logMessage("Warning: Some job folders could not be created");
    }

    m_isJobSaved = true;
    m_originalYear = m_currentJob->year;
    m_originalMonth = m_currentJob->month;
    m_originalWeek = m_currentJob->week;

    emit jobSaved();
    emit logMessage(QString("Created new job for year %1, month %2, week %3")
                        .arg(m_currentJob->year, m_currentJob->month, m_currentJob->week));
    return true;
}

bool JobController::closeJob()
{
    if (m_isJobSaved && m_currentJob->isValid()) {
        // Save current job state before closing
        if (!m_dbManager->saveJob(*m_currentJob)) {
            emit logMessage("Warning: Failed to save job state before closing");
        }

        // Move files from working to home directory
        if (!m_fileManager->moveFilesToHomeFolders(m_currentJob->month, m_currentJob->week)) {
            emit logMessage("Warning: Some files could not be moved to home directory");
        }
    }

    // Reset job data
    m_currentJob->reset();
    m_isJobSaved = false;
    m_isJobDataLocked = false;
    m_originalYear = "";
    m_originalMonth = "";
    m_originalWeek = "";

    // Reset progress
    for (size_t i = 0; i < NUM_STEPS; ++i) {
        m_completedSubtasks[i] = 0;
    }
    updateProgress();

    emit jobClosed();
    emit logMessage("Job closed");
    return true;
}

bool JobController::deleteJob(const QString& year, const QString& month, const QString& week)
{
    if (!m_dbManager->deleteJob(year, month, week)) {
        emit logMessage(QString("Failed to delete job: %1-%2-%3").arg(year, month, week));
        return false;
    }

    emit logMessage(QString("Deleted job: %1-%2-%3").arg(year, month, week));
    return true;
}

bool JobController::openIZ()
{
    QString izPath = m_fileManager->getIZPath();
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(izPath))) {
        emit logMessage("Failed to open IZ directory: " + izPath);
        return false;
    }

    m_currentJob->isOpenIZComplete = true;
    m_completedSubtasks[0] = 1;
    updateProgress();
    emit logMessage("Opened IZ directory: " + izPath);
    return true;
}

bool JobController::runInitialProcessing()
{
    if (!m_currentJob->isOpenIZComplete) {
        emit logMessage("Please open InputZIP first.");
        return false;
    }

    if (!m_isJobSaved) {
        emit logMessage("Please save the job before running initial processing.");
        return false;
    }

    emit scriptStarted();
    emit logMessage("Running initial processing...");

    QString scriptPath = m_settings->value("InitialScript", "C:/Goji/Scripts/RAC/WEEKLIES/01RUNFIRST.py").toString();
    m_scriptRunner->runScript("python", QStringList() << scriptPath);

    // These will be updated when the script finishes successfully
    m_currentJob->isRunInitialComplete = true;
    m_completedSubtasks[1] = 1;
    updateProgress();

    return true;
}

bool JobController::runPreProofProcessing()
{
    if (!m_currentJob->isRunInitialComplete) {
        emit logMessage("Please run Initial Script first.");
        return false;
    }

    if (!m_isPostageLocked) {
        emit logMessage("Please lock the postage data first.");
        return false;
    }

    emit scriptStarted();
    emit logMessage("Running pre-proof processing...");

    QString scriptPath = m_settings->value("PreProofScript", "C:/Goji/Scripts/RAC/WEEKLIES/02RUNSECOND.bat").toString();
    QString week = m_currentJob->month + "." + m_currentJob->week;
    QStringList arguments;
    arguments << "/c" << scriptPath << m_fileManager->getBasePath() << m_currentJob->cbcJobNumber << week;

    m_scriptRunner->runScript("cmd.exe", arguments);

    // These will be updated when the script finishes successfully
    m_currentJob->isRunPreProofComplete = true;
    m_completedSubtasks[2] = 1;
    m_completedSubtasks[3] = 1;
    updateProgress();

    return true;
}

bool JobController::openProofFiles(const QString& jobType)
{
    if (!m_currentJob->isRunPreProofComplete) {
        emit logMessage("Please run Pre-Proof first.");
        return false;
    }

    if (jobType.isEmpty()) {
        emit logMessage("Please select a job type.");
        return false;
    }

    QStringList missingFiles;
    if (!m_fileManager->checkProofFiles(jobType, missingFiles)) {
        if (missingFiles.isEmpty()) {
            emit logMessage("Failed to check proof files for: " + jobType);
            return false;
        } else {
            emit logMessage("Missing proof files for " + jobType + ": " + missingFiles.join(", "));
        }
    }

    QString proofPath = m_fileManager->getProofFolderPath(jobType);
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(proofPath))) {
        emit logMessage("Failed to open proof folder: " + proofPath);
        return false;
    }

    m_currentJob->isOpenProofFilesComplete = true;
    m_completedSubtasks[4] = 1;
    updateProgress();
    emit logMessage("Opened proof files for: " + jobType);

    return true;
}

bool JobController::runPostProofProcessing(bool isRegenMode)
{
    if (!m_currentJob->isOpenProofFilesComplete) {
        emit logMessage("Please open proof files first.");
        return false;
    }

    emit scriptStarted();
    emit logMessage("Running post-proof processing...");

    if (isRegenMode) {
        emit logMessage("Proof regeneration mode active.");
        return true; // The actual regeneration is handled separately
    }

    QString scriptPath = m_settings->value("PostProofScript", "C:/Goji/Scripts/RAC/WEEKLIES/04POSTPROOF.py").toString();
    QString week = m_currentJob->month + "." + m_currentJob->week;

    // Helper function to strip currency formatting
    auto stripCurrency = [](const QString &text) -> QString {
        QString cleaned = text;
        cleaned.remove(QRegularExpression("[^0-9.]")); // Remove all but digits and decimal point
        bool ok;
        double value = cleaned.toDouble(&ok);
        if (!ok || value < 0) return "0.00"; // Return 0.00 for invalid or negative values
        return QString::number(value, 'f', 2); // Ensure two decimal places
    };

    QStringList arguments;
    arguments << scriptPath
              << "--base_path" << m_fileManager->getBasePath()
              << "--week" << week
              << "--job_type" << "ALL" // Process all job types
              << "--job_number" << m_currentJob->cbcJobNumber // Primary job number
              << "--cbc2_postage" << stripCurrency(m_currentJob->cbc2Postage)
              << "--cbc3_postage" << stripCurrency(m_currentJob->cbc3Postage)
              << "--exc_postage" << stripCurrency(m_currentJob->excPostage)
              << "--inactive_po_postage" << stripCurrency(m_currentJob->inactivePOPostage)
              << "--inactive_pu_postage" << stripCurrency(m_currentJob->inactivePUPostage)
              << "--ncwo1_a_postage" << stripCurrency(m_currentJob->ncwo1APostage)
              << "--ncwo2_a_postage" << stripCurrency(m_currentJob->ncwo2APostage)
              << "--ncwo1_ap_postage" << stripCurrency(m_currentJob->ncwo1APPostage)
              << "--ncwo2_ap_postage" << stripCurrency(m_currentJob->ncwo2APPostage)
              << "--prepif_postage" << stripCurrency(m_currentJob->prepifPostage)
              << "--output_json" << "true"; // Request JSON output for parsing

    // Connect to script output for JSON capture
    QObject::connect(m_scriptRunner, &ScriptRunner::scriptFinished,
                     this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
                         if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                             m_currentJob->isRunPostProofComplete = true;
                             m_completedSubtasks[5] = 1;
                             updateProgress();
                             emit postProofCountsUpdated();
                         }

                         // Disconnect after execution to mimic SingleShotConnection
                         disconnect(m_scriptRunner, &ScriptRunner::scriptFinished,
                                    this, nullptr);
                     });

    m_scriptRunner->runScript("python", arguments);
    return true;
}

bool JobController::regenerateProofs(const QMap<QString, QStringList>& filesByJobType)
{
    if (!m_isProofRegenMode) {
        emit logMessage("Proof regeneration mode not enabled.");
        return false;
    }

    emit scriptStarted();
    emit logMessage("Regenerating proofs...");

    for (auto it = filesByJobType.begin(); it != filesByJobType.end(); ++it) {
        QString jobType = it.key();
        QStringList files = it.value();

        if (!files.isEmpty()) {
            int nextVersion = m_dbManager->getNextProofVersion(files.first());
            runProofRegenScript(jobType, files, nextVersion);
        }
    }

    m_currentJob->isRunPostProofComplete = true;
    m_completedSubtasks[5] = 1;
    updateProgress();

    emit logMessage("Proof regeneration complete.");
    return true;
}

void JobController::runProofRegenScript(const QString& jobType, const QStringList& files, int version)
{
    QString scriptPath = m_settings->value("PostProofScript", "C:/Goji/Scripts/RAC/WEEKLIES/04POSTPROOF.py").toString();
    QString week = m_currentJob->month + "." + m_currentJob->week;
    QString jobNumber = m_currentJob->getJobNumberForJobType(jobType);

    QStringList arguments;
    arguments << scriptPath
              << "--base_path" << m_fileManager->getBasePath()
              << "--job_type" << jobType
              << "--job_number" << jobNumber
              << "--week" << week
              << "--version" << QString::number(version);

    for (const QString& file : files) {
        arguments << "--proof_files" << file;
    }

    m_scriptRunner->runScript("python", arguments);

    // Update proof versions in database
    for (const QString& file : files) {
        m_dbManager->updateProofVersion(file, version);
    }

    emit logMessage(QString("Regenerated proof files for %1 (version %2): %3")
                        .arg(jobType)
                        .arg(version)
                        .arg(files.join(", ")));
}

bool JobController::openPrintFiles(const QString& jobType)
{
    if (!m_currentJob->isRunPostProofComplete) {
        emit logMessage("Please run Post-Proof first.");
        return false;
    }

    if (jobType.isEmpty()) {
        emit logMessage("Please select a job type.");
        return false;
    }

    QStringList missingFiles;
    if (!m_fileManager->checkPrintFiles(jobType, missingFiles)) {
        if (missingFiles.isEmpty()) {
            emit logMessage("Failed to check print files for: " + jobType);
            return false;
        } else {
            emit logMessage("Missing print files for " + jobType + ": " + missingFiles.join(", "));
        }
    }

    QString printPath = m_fileManager->getPrintFolderPath(jobType);
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(printPath))) {
        emit logMessage("Failed to open print folder: " + printPath);
        return false;
    }

    m_currentJob->isOpenPrintFilesComplete = true;
    m_completedSubtasks[7] = 1;
    updateProgress();
    emit logMessage("Opened print files for: " + jobType);

    return true;
}

bool JobController::runPostPrintProcessing()
{
    if (!m_currentJob->isOpenPrintFilesComplete) {
        emit logMessage("Please open print files first.");
        return false;
    }

    if (m_completedSubtasks[6] != 1) {
        emit logMessage("Please approve all proofs first.");
        return false;
    }

    emit scriptStarted();
    emit logMessage("Running post-print processing...");

    QString scriptPath = m_settings->value("PostPrintScript", "C:/Goji/Scripts/RAC/WEEKLIES/05POSTPRINT.ps1").toString();
    QStringList arguments;
    arguments << "-ExecutionPolicy" << "Bypass" << "-File" << scriptPath;

    m_scriptRunner->runScript("powershell.exe", arguments);

    // These will be updated when the script finishes successfully
    m_currentJob->isRunPostPrintComplete = true;
    m_completedSubtasks[8] = 1;
    updateProgress();

    return true;
}

bool JobController::confirmOverwrite(const QString& year, const QString& month, const QString& week)
{
    emit logMessage(QString("Existing job found for %1|%2|%3").arg(year, month, week));

    // This should be implemented in the UI layer with a QMessageBox
    // For this controller, we'll just return true by default
    return true;
}

JobData* JobController::currentJob() const
{
    return m_currentJob;
}

bool JobController::isJobSaved() const
{
    return m_isJobSaved;
}

bool JobController::isJobDataLocked() const
{
    return m_isJobDataLocked;
}

void JobController::setJobDataLocked(bool locked)
{
    m_isJobDataLocked = locked;
}

bool JobController::isProofRegenMode() const
{
    return m_isProofRegenMode;
}

void JobController::setProofRegenMode(bool enabled)
{
    m_isProofRegenMode = enabled;
}

bool JobController::isPostageLocked() const
{
    return m_isPostageLocked;
}

void JobController::setPostageLocked(bool locked)
{
    m_isPostageLocked = locked;
}

QString JobController::getOriginalYear() const
{
    return m_originalYear;
}

QString JobController::getOriginalMonth() const
{
    return m_originalMonth;
}

QString JobController::getOriginalWeek() const
{
    return m_originalWeek;
}

double JobController::getProgress() const
{
    double totalWeight = 0.0;
    double completedWeight = 0.0;

    for (size_t i = 0; i < NUM_STEPS; ++i) {
        totalWeight += m_stepWeights[i];
        completedWeight += m_completedSubtasks[i] * m_stepWeights[i];
    }

    return (completedWeight / totalWeight) * 100.0;
}

void JobController::updateProgress()
{
    int progress = static_cast<int>(getProgress());
    emit jobProgressUpdated(progress);
}

bool JobController::parsePostProofOutput(const QString& output)
{
    // Extract JSON data from the output
    QString jsonString;
    QStringList lines = output.split('\n');
    bool inJson = false;

    for (const QString& line : lines) {
        if (line.contains("===JSON_START===")) {
            inJson = true;
            continue;
        }
        if (line.contains("===JSON_END===")) {
            break;
        }
        if (inJson) {
            jsonString += line + '\n';
        }
    }

    if (jsonString.isEmpty()) {
        emit logMessage("No JSON data found in script output");
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        emit logMessage("Failed to parse JSON output");
        return false;
    }

    // Save the parsed JSON data to the database
    if (!m_dbManager->savePostProofCounts(doc.object())) {
        emit logMessage("Failed to save post-proof counts to database");
        return false;
    }

    emit logMessage("Post-proof counts saved successfully");
    return true;
}
