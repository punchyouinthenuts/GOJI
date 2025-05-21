#include "tmweeklypcfilemanager.h"
#include "logger.h"
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>

TMWeeklyPCFileManager::TMWeeklyPCFileManager(QSettings* settings)
    : BaseFileSystemManager(settings)
{
    initializeScriptPaths();
}

QString TMWeeklyPCFileManager::getBasePath() const
{
    // Check for configured path in settings or use default
    return m_settings->value("TM/BasePath", "C:/Goji/TRACHMAR").toString();
}

QString TMWeeklyPCFileManager::getInputPath() const
{
    return m_settings->value("TM/InputPath", getBasePath() + "/WEEKLY PC/JOB/INPUT").toString();
}

QString TMWeeklyPCFileManager::getOutputPath() const
{
    return m_settings->value("TM/OutputPath", getBasePath() + "/WEEKLY PC/JOB/OUTPUT").toString();
}

QString TMWeeklyPCFileManager::getProofPath() const
{
    return m_settings->value("TM/ProofPath", getBasePath() + "/WEEKLY PC/JOB/PROOF").toString();
}

QString TMWeeklyPCFileManager::getPrintPath() const
{
    return m_settings->value("TM/PrintPath", getBasePath() + "/WEEKLY PC/JOB/PRINT").toString();
}

QString TMWeeklyPCFileManager::getArtPath() const
{
    return m_settings->value("TM/ArtPath", getBasePath() + "/WEEKLY PC/ART").toString();
}

QString TMWeeklyPCFileManager::getScriptsPath() const
{
    return m_settings->value("TM/ScriptsPath", "C:/Goji/Scripts/TRACHMAR/WEEKLY PC").toString();
}

QString TMWeeklyPCFileManager::getJobFolderPath(const QString& month, const QString& week) const
{
    if (month.isEmpty() || week.isEmpty()) {
        Logger::instance().warning("Month or week is empty when getting job folder path");
        return QString();
    }

    return getBasePath() + "/WEEKLY PC/" + month + "." + week;
}

QString TMWeeklyPCFileManager::getScriptPath(const QString& scriptName) const
{
    if (m_scriptPaths.contains(scriptName)) {
        return m_scriptPaths[scriptName];
    }

    // If not in the map, try to resolve using the scripts path
    return getScriptsPath() + "/" + scriptName + ".py";
}

QString TMWeeklyPCFileManager::getProofFilePath(const QString& variant) const
{
    return getArtPath() + "/WEEKLY (" + variant + ") PROOF.indd";
}

QString TMWeeklyPCFileManager::getPrintFilePath(const QString& variant) const
{
    return getArtPath() + "/WEEKLY (" + variant + ") PRINT.indd";
}

bool TMWeeklyPCFileManager::createBaseDirectories()
{
    QStringList directories = {
        "C:/Goji",
        getBasePath(),
        getBasePath() + "/WEEKLY PC",
        getBasePath() + "/WEEKLY PC/JOB",
        getInputPath(),
        getOutputPath(),
        getProofPath(),
        getPrintPath(),
        getArtPath(),
        getScriptsPath()
    };

    bool allCreated = true;
    for (const QString& dir : directories) {
        if (!createDirectoryIfNotExists(dir)) {
            allCreated = false;
        }
    }

    return allCreated;
}

bool TMWeeklyPCFileManager::createJobFolder(const QString& month, const QString& week)
{
    if (month.isEmpty() || week.isEmpty()) {
        Logger::instance().error("Cannot create job folder: month or week is empty");
        return false;
    }

    QString folderPath = getJobFolderPath(month, week);
    if (!createDirectoryIfNotExists(folderPath)) {
        return false;
    }

    // Create standard subfolders
    QStringList subfolders = {
        "/INPUT",
        "/OUTPUT",
        "/PROOF",
        "/PRINT"
    };

    for (const QString& subfolder : subfolders) {
        QString fullPath = folderPath + subfolder;
        if (!createDirectoryIfNotExists(fullPath)) {
            Logger::instance().error("Failed to create subfolder: " + fullPath);
            return false;
        }
    }

    return true;
}

bool TMWeeklyPCFileManager::openProofFile(const QString& variant) const
{
    if (variant.isEmpty()) {
        Logger::instance().error("Cannot open proof file: variant is empty");
        return false;
    }

    QString inddFile = getProofFilePath(variant);

    // Check if file exists
    QFileInfo fileInfo(inddFile);
    if (!fileInfo.exists()) {
        Logger::instance().warning("Proof file does not exist: " + inddFile);

        // Open proof folder instead
        return QDesktopServices::openUrl(QUrl::fromLocalFile(getProofPath()));
    }

    // Launch the file using QDesktopServices
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(inddFile));

    if (success) {
        Logger::instance().info("Opened proof file: " + inddFile);
    } else {
        Logger::instance().error("Failed to open proof file: " + inddFile);
    }

    return success;
}

bool TMWeeklyPCFileManager::openPrintFile(const QString& variant) const
{
    if (variant.isEmpty()) {
        Logger::instance().error("Cannot open print file: variant is empty");
        return false;
    }

    QString inddFile = getPrintFilePath(variant);

    // Check if file exists
    QFileInfo fileInfo(inddFile);
    if (!fileInfo.exists()) {
        Logger::instance().warning("Print file does not exist: " + inddFile);

        // Open print folder instead
        return QDesktopServices::openUrl(QUrl::fromLocalFile(getPrintPath()));
    }

    // Launch the file using QDesktopServices
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(inddFile));

    if (success) {
        Logger::instance().info("Opened print file: " + inddFile);
    } else {
        Logger::instance().error("Failed to open print file: " + inddFile);
    }

    return success;
}

void TMWeeklyPCFileManager::initializeScriptPaths()
{
    // Map script names to their file paths
    m_scriptPaths["initial"] = getScriptsPath() + "/01INITIAL.py";
    m_scriptPaths["proofdata"] = getScriptsPath() + "/02PROOFDATA.py";
    m_scriptPaths["weeklymerged"] = getScriptsPath() + "/03WEEKLYMERGED.py";
    m_scriptPaths["postprint"] = getScriptsPath() + "/04POSTPRINT.py";
}
