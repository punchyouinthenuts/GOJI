#include "jobcontroller.h"
#include <QDebug>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>
#include "filelocationsdialog.h"

FileOperationException::FileOperationException(const QString& message)
    : m_message(message), m_messageStd(message.toStdString())
{
}

const char* FileOperationException::what() const noexcept
{
    return m_messageStd.c_str();
}

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
    m_stepWeights = {2.0, 9.0, 13.0, 13.0, 20.0, 10.0, 3.0, 20.0, 10.0};
    for (size_t i = 0; i < NUM_STEPS; ++i) {
        m_totalSubtasks[i] = 1;
        m_completedSubtasks[i] = 0;
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

    m_completedSubtasks[0] = m_currentJob->step0_complete;
    m_completedSubtasks[1] = m_currentJob->step1_complete;
    m_completedSubtasks[2] = m_currentJob->step2_complete;
    m_completedSubtasks[3] = m_currentJob->step3_complete;
    m_completedSubtasks[4] = m_currentJob->step4_complete;
    m_completedSubtasks[5] = m_currentJob->step5_complete;
    m_completedSubtasks[6] = m_currentJob->step6_complete;
    m_completedSubtasks[7] = m_currentJob->step7_complete;
    m_completedSubtasks[8] = m_currentJob->step8_complete;

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

    if (m_dbManager->jobExists(m_currentJob->year, m_currentJob->month, m_currentJob->week)) {
        if (!confirmOverwrite(m_currentJob->year, m_currentJob->month, m_currentJob->week)) {
            return false;
        }
        if (!m_dbManager->deleteJob(m_currentJob->year, m_currentJob->month, m_currentJob->week)) {
            emit logMessage("Failed to delete existing job for overwrite");
            return false;
        }
    }

    if (!m_dbManager->saveJob(*m_currentJob)) {
        emit logMessage("Failed to create job");
        return false;
    }

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
        if (!m_dbManager->saveJob(*m_currentJob)) {
            emit logMessage("Warning: Failed to save job state before closing");
            return false;
        }

        try {
            QString basePath = m_fileManager->getBasePath();
            QStringList jobTypes = {"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"};
            QString homeFolder = m_currentJob->month + "." + m_currentJob->week;

            for (const QString& jobType : jobTypes) {
                QString homeDir = basePath + "/" + jobType + "/" + homeFolder;
                QDir dir(homeDir);
                if (!dir.exists()) {
                    if (!dir.mkpath(".")) {
                        throw FileOperationException(QString("Failed to create home folder: %1").arg(homeDir));
                    }
                }

                for (const QString& subDir : QStringList{"INPUT", "OUTPUT", "PRINT", "PROOF"}) {
                    QString homePath = homeDir + "/" + subDir;
                    QString workingPath = basePath + "/" + jobType + "/JOB/" + subDir;

                    QDir homeSubDir(homePath);
                    QDir workingSubDir(workingPath);

                    if (!homeSubDir.exists() && !homeSubDir.mkpath(".")) {
                        throw FileOperationException(QString("Failed to create home subdirectory: %1").arg(homePath));
                    }

                    if (!workingSubDir.exists()) {
                        continue;
                    }

                    QStringList files = workingSubDir.entryList(QDir::Files);
                    for (const QString& file : files) {
                        QString srcPath = workingSubDir.filePath(file);
                        QString destPath = homeSubDir.filePath(file);

                        if (!validateFileOperation("move", srcPath, destPath)) {
                            throw FileOperationException(QString("File operation validation failed for: %1 to %2").arg(srcPath, destPath));
                        }
                    }
                }
            }

            if (!m_fileManager->moveFilesToHomeFolders(m_currentJob->month, m_currentJob->week)) {
                throw FileOperationException("Failed to move files to home folders");
            }
        }
        catch (const FileOperationException& e) {
            emit logMessage(QString("<font color=\"red\">Error closing job: %1</font>").arg(e.what()));
            return false;
        }
    }

    m_currentJob->reset();
    m_isJobSaved = false;
    m_isJobDataLocked = false;
    m_originalYear = "";
    m_originalMonth = "";
    m_originalWeek = "";

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
        return true;
    }

    QString scriptPath = m_settings->value("PostProofScript", "C:/Goji/Scripts/RAC/WEEKLIES/04POSTPROOF.py").toString();
    QString week = m_currentJob->month + "." + m_currentJob->week;

    auto stripCurrency = [](const QString &text) -> QString {
        QString cleaned = text;
        cleaned.remove(QRegularExpression("[^0-9.]"));
        bool ok;
        double value = cleaned.toDouble(&ok);
        if (!ok || value < 0) return "0.00";
        return QString::number(value, 'f', 2);
    };

    QStringList arguments;
    arguments << scriptPath
              << "--base_path" << m_fileManager->getBasePath()
              << "--week" << week
              << "--job_type" << "ALL"
              << "--job_number" << m_currentJob->cbcJobNumber
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
              << "--output_json" << "true";

    connect(m_scriptRunner, &ScriptRunner::scriptFinished,
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
                if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                    m_currentJob->isRunPostProofComplete = true;
                    m_completedSubtasks[5] = 1;
                    updateProgress();
                    emit postProofCountsUpdated();
                }

                disconnect(m_scriptRunner, &ScriptRunner::scriptFinished, this, nullptr);
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

    QMap<QString, bool> printFileStatus;
    QStringList missingFilePaths;
    QStringList jobTypes = {"CBC", "EXC", "NCWO", "PREPIF"};

    for (const QString& jobType : jobTypes) {
        QStringList missingFiles;
        bool filesExist = m_fileManager->checkPrintFiles(jobType, missingFiles);
        printFileStatus[jobType] = filesExist;

        if (!filesExist) {
            QString printPath = m_fileManager->getPrintFolderPath(jobType);
            for (const QString& file : missingFiles) {
                missingFilePaths.append(printPath + "/" + file);
            }
        }
    }

    if (!missingFilePaths.isEmpty()) {
        QString message = "The following print files are missing:\n\n" + missingFilePaths.join("\n") +
                          "\n\nThis might affect the post-print processing. Do you want to continue?";
        emit logMessage("<font color=\"orange\">Warning: Missing print files</font>");
        emit logMessage(message);
        emit logMessage("<font color=\"orange\">Continuing despite missing files...</font>");
    }

    emit scriptStarted();
    emit logMessage("Running post-print processing...");

    QString scriptPath = m_settings->value("PostPrintScript", "C:/Goji/Scripts/RAC/WEEKLIES/05POSTPRINT.ps1").toString();
    QString week = m_currentJob->month + "." + m_currentJob->week;
    QString basePath = m_fileManager->getBasePath();

    QStringList arguments;
    arguments << "-ExecutionPolicy" << "Bypass"
              << "-WindowStyle" << "Hidden"
              << "-File" << scriptPath
              << "-cbcJobNumber" << m_currentJob->cbcJobNumber
              << "-excJobNumber" << m_currentJob->excJobNumber
              << "-ncwoJobNumber" << m_currentJob->ncwoJobNumber
              << "-prepifJobNumber" << m_currentJob->prepifJobNumber
              << "-inactiveJobNumber" << m_currentJob->inactiveJobNumber
              << "-weekNumber" << week
              << "-basePath" << basePath
              << "-year" << m_currentJob->year;

    connect(m_scriptRunner, &ScriptRunner::scriptOutput, this,
            [this](const QString& output) {
                QString formattedOutput = output.trimmed();
                if (formattedOutput.contains("error", Qt::CaseInsensitive) ||
                    formattedOutput.contains("exception", Qt::CaseInsensitive) ||
                    formattedOutput.contains("failed", Qt::CaseInsensitive)) {
                    emit logMessage("<font color=\"red\">" + formattedOutput + "</font>");
                }
                else if (formattedOutput.contains("warning", Qt::CaseInsensitive)) {
                    emit logMessage("<font color=\"orange\">" + formattedOutput + "</font>");
                }
                else if (formattedOutput.contains("success", Qt::CaseInsensitive) ||
                         formattedOutput.contains("complete", Qt::CaseInsensitive)) {
                    emit logMessage("<font color=\"green\">" + formattedOutput + "</font>");
                }
                else {
                    emit logMessage(formattedOutput);
                }
            }, Qt::DirectConnection);

    connect(m_scriptRunner, &ScriptRunner::scriptFinished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                    m_currentJob->isRunPostPrintComplete = true;
                    m_completedSubtasks[8] = 1;
                    updateProgress();
                    emit logMessage("<font color=\"green\">Post-print processing completed successfully.</font>");

                    // Show file locations dialog
                    QStringList fileLocations;
                    fileLocations << "Inactive data file on Buskro, print files located below\n";
                    QStringList jobTypes = {"NCWO", "PREPIF", "CBC", "EXC"};
                    QString week = m_currentJob->month + "." + m_currentJob->week;
                    for (const QString& jobType : jobTypes) {
                        QString jobNumber = m_currentJob->getJobNumberForJobType(jobType);
                        QString path = QString("\\\\NAS1069D9\\AMPrintData\\%1_SrcFiles\\I\\Innerworkings\\%2 %3\\%4")
                                           .arg(m_currentJob->year, jobNumber, jobType, week);
                        fileLocations << path;
                    }
                    QString locationsText = fileLocations.join("\n");
                    FileLocationsDialog dialog(locationsText);
                    dialog.exec();
                } else {
                    emit logMessage("<font color=\"red\">Post-print processing failed with exit code " +
                                    QString::number(exitCode) + "</font>");
                    emit logMessage("Please check the terminal output above for details on what went wrong.");
                }

                disconnect(m_scriptRunner, &ScriptRunner::scriptOutput, this, nullptr);
                disconnect(m_scriptRunner, &ScriptRunner::scriptFinished, this, nullptr);
            }, Qt::DirectConnection);

    m_scriptRunner->runScript("powershell.exe", arguments);
    return true;
}

bool JobController::validateFileOperation(const QString& operation, const QString& sourcePath, const QString& destPath)
{
    if (!QFile::exists(sourcePath)) {
        emit logMessage(QString("Source file does not exist: %1").arg(sourcePath));
        return false;
    }

    QFileInfo destInfo(destPath);
    QDir destDir = destInfo.dir();

    if (!destDir.exists()) {
        if (!destDir.mkpath(".")) {
            emit logMessage(QString("Failed to create destination directory: %1").arg(destDir.path()));
            return false;
        }
    }

    if (operation == "move" && QFile::exists(destPath)) {
        if (!QFile::remove(destPath)) {
            emit logMessage(QString("Failed to remove existing file at destination: %1").arg(destPath));
            return false;
        }
    }

    return true;
}

bool JobController::confirmOverwrite(const QString& year, const QString& month, const QString& week)
{
    emit logMessage(QString("Existing job found for %1|%2|%3").arg(year, month, week));
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

    if (!m_dbManager->savePostProofCounts(doc.object())) {
        emit logMessage("Failed to save post-proof counts to database");
        return false;
    }

    emit logMessage("Post-proof counts saved successfully");
    return true;
}
