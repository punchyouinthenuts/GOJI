#include "tmflahcafilemanager.h"
#include "logger.h"
#include "fileutils.h"
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>

TMFLAHCAFileManager::TMFLAHCAFileManager(QSettings* settings, const QString& basePath)
    : BaseFileSystemManager(settings)
    , m_basePath(basePath)
{
    initializeScriptPaths();
}

QString TMFLAHCAFileManager::getBasePath() const
{
    if (!m_basePath.isEmpty()) return m_basePath;
    return FileUtils::resolveTrachmarBasePath(m_settings, "TM FL AHCA") + "/FL AHCA";
}

QString TMFLAHCAFileManager::getDataPath() const
{
    return getBasePath() + "/DATA";
}

QString TMFLAHCAFileManager::getArchivePath() const
{
    return getBasePath() + "/ARCHIVE";
}

QString TMFLAHCAFileManager::getScriptsPath() const
{
    if (!m_basePath.isEmpty()) return m_basePath + "/scripts";
    return "C:/Goji/scripts/TRACHMAR/FL AHCA";
}

void TMFLAHCAFileManager::initializeScriptPaths()
{
    QString scriptsDir = getScriptsPath();

    // Map script names to their full paths
    m_scriptPaths["01INITIAL"] = scriptsDir + "/01 INITIAL.py";
    m_scriptPaths["02FINALPROCESS"] = scriptsDir + "/02 FINAL PROCESS.py";

    Logger::instance().info("TMFLAHCA script paths initialized");
}

QString TMFLAHCAFileManager::getJobFolderPath(const QString& year, const QString& month) const
{
    // Convert month number to abbreviated name
    QMap<QString, QString> monthMap = {
        {"01", "JAN"}, {"02", "FEB"}, {"03", "MAR"}, {"04", "APR"},
        {"05", "MAY"}, {"06", "JUN"}, {"07", "JUL"}, {"08", "AUG"},
        {"09", "SEP"}, {"10", "OCT"}, {"11", "NOV"}, {"12", "DEC"}
    };

    QString monthAbbrev = monthMap.value(month, month); // Use original if not found

    return getArchivePath() + "/" + monthAbbrev + " " + year;
}

QString TMFLAHCAFileManager::getJobFolderPath(const QString& jobNumber, const QString& year, const QString& month) const
{
    // Convert month number to abbreviated name
    QMap<QString, QString> monthMap = {
        {"01", "JAN"}, {"02", "FEB"}, {"03", "MAR"}, {"04", "APR"},
        {"05", "MAY"}, {"06", "JUN"}, {"07", "JUL"}, {"08", "AUG"},
        {"09", "SEP"}, {"10", "OCT"}, {"11", "NOV"}, {"12", "DEC"}
    };

    QString monthAbbrev = monthMap.value(month, month); // Use original if not found

    return getArchivePath() + "/" + jobNumber + " " + monthAbbrev + " " + year;
}

QString TMFLAHCAFileManager::getScriptPath(const QString& scriptName) const
{
    if (m_scriptPaths.contains(scriptName)) {
        return m_scriptPaths[scriptName];
    }

    // If not in the map, try to resolve using the scripts path
    return getScriptsPath() + "/" + scriptName + ".py";
}

bool TMFLAHCAFileManager::createBaseDirectories()
{
    QStringList directories = {
        getBasePath(),
        getDataPath(),
        getBasePath() + "/RAW INPUT",
        getArchivePath()
    };

    bool allCreated = true;
    for (const QString& dir : directories) {
        if (!createDirectoryIfNotExists(dir)) {
            allCreated = false;
            Logger::instance().error("Failed to create FLAHCA directory: " + dir);
        }
    }

    if (allCreated) {
        Logger::instance().info("All FLAHCA base directories created successfully");
    }

    return allCreated;
}

bool TMFLAHCAFileManager::createJobFolder(const QString& year, const QString& month)
{
    if (year.isEmpty() || month.isEmpty()) {
        Logger::instance().error("Cannot create FLAHCA job folder: year or month is empty");
        return false;
    }

    QString folderPath = getJobFolderPath(year, month);
    if (!createDirectoryIfNotExists(folderPath)) {
        Logger::instance().error("Failed to create FLAHCA job folder: " + folderPath);
        return false;
    }

    Logger::instance().info("Created FLAHCA job folder: " + folderPath);
    return true;
}

bool TMFLAHCAFileManager::openDataFolder() const
{
    QString dataPath = getDataPath();

    // Check if folder exists
    QFileInfo folderInfo(dataPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("FLAHCA DATA folder does not exist: " + dataPath);
        return false;
    }

    // Open folder in Windows Explorer
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(dataPath));
    if (success) {
        Logger::instance().info("Opened FLAHCA DATA folder: " + dataPath);
    } else {
        Logger::instance().error("Failed to open FLAHCA DATA folder: " + dataPath);
    }

    return success;
}

bool TMFLAHCAFileManager::openArchiveFolder() const
{
    QString archivePath = getArchivePath();

    // Check if folder exists
    QFileInfo folderInfo(archivePath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("FLAHCA ARCHIVE folder does not exist: " + archivePath);
        return false;
    }

    // Open folder in Windows Explorer
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(archivePath));
    if (success) {
        Logger::instance().info("Opened FLAHCA ARCHIVE folder: " + archivePath);
    } else {
        Logger::instance().error("Failed to open FLAHCA ARCHIVE folder: " + archivePath);
    }

    return success;
}

bool TMFLAHCAFileManager::openScriptsFolder() const
{
    QString scriptsPath = getScriptsPath();

    // Check if folder exists
    QFileInfo folderInfo(scriptsPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("FLAHCA scripts folder does not exist: " + scriptsPath);
        return false;
    }

    // Open folder in Windows Explorer
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(scriptsPath));
    if (success) {
        Logger::instance().info("Opened FLAHCA scripts folder: " + scriptsPath);
    } else {
        Logger::instance().error("Failed to open FLAHCA scripts folder: " + scriptsPath);
    }

    return success;
}

bool TMFLAHCAFileManager::openJobFolder(const QString& year, const QString& month) const
{
    QString jobFolderPath = getJobFolderPath(year, month);

    // Check if folder exists
    QFileInfo folderInfo(jobFolderPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("FLAHCA job folder does not exist: " + jobFolderPath);
        return false;
    }

    // Open folder in Windows Explorer
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(jobFolderPath));
    if (success) {
        Logger::instance().info("Opened FLAHCA job folder: " + jobFolderPath);
    } else {
        Logger::instance().error("Failed to open FLAHCA job folder: " + jobFolderPath);
    }

    return success;
}
