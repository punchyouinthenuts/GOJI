#include "tmflerfilemanager.h"
#include "logger.h"
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>

TMFLERFileManager::TMFLERFileManager(QSettings* settings)
    : BaseFileSystemManager(settings)
{
    initializeScriptPaths();
}

QString TMFLERFileManager::getBasePath() const
{
    return "C:/Goji/TRACHMAR/FL ER";
}

QString TMFLERFileManager::getDataPath() const
{
    return getBasePath() + "/DATA";
}

QString TMFLERFileManager::getArchivePath() const
{
    return getBasePath() + "/ARCHIVE";
}

QString TMFLERFileManager::getScriptsPath() const
{
    return "C:/Goji/scripts/TRACHMAR/FL ER";
}

void TMFLERFileManager::initializeScriptPaths()
{
    QString scriptsDir = getScriptsPath();

    // Map script names to their full paths
    m_scriptPaths["01INITIAL"] = scriptsDir + "/01 INITIAL.py";
    m_scriptPaths["02FINALPROCESS"] = scriptsDir + "/02 FINAL PROCESS.py";

    Logger::instance().info("TMFLER script paths initialized");
}

QString TMFLERFileManager::getJobFolderPath(const QString& year, const QString& month) const
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

QString TMFLERFileManager::getJobFolderPath(const QString& jobNumber, const QString& year, const QString& month) const
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

QString TMFLERFileManager::getScriptPath(const QString& scriptName) const
{
    if (m_scriptPaths.contains(scriptName)) {
        return m_scriptPaths[scriptName];
    }

    // If not in the map, try to resolve using the scripts path
    return getScriptsPath() + "/" + scriptName + ".py";
}

bool TMFLERFileManager::createBaseDirectories()
{
    QStringList directories = {
        "C:/Goji",
        "C:/Goji/TRACHMAR",
        getBasePath(),
        getDataPath(),
        getArchivePath(),
        getScriptsPath()
    };

    bool allCreated = true;
    for (const QString& dir : directories) {
        if (!createDirectoryIfNotExists(dir)) {
            allCreated = false;
            Logger::instance().error("Failed to create FLER directory: " + dir);
        }
    }

    if (allCreated) {
        Logger::instance().info("All FLER base directories created successfully");
    }

    return allCreated;
}

bool TMFLERFileManager::createJobFolder(const QString& year, const QString& month)
{
    if (year.isEmpty() || month.isEmpty()) {
        Logger::instance().error("Cannot create FLER job folder: year or month is empty");
        return false;
    }

    QString folderPath = getJobFolderPath(year, month);
    if (!createDirectoryIfNotExists(folderPath)) {
        Logger::instance().error("Failed to create FLER job folder: " + folderPath);
        return false;
    }

    Logger::instance().info("Created FLER job folder: " + folderPath);
    return true;
}

bool TMFLERFileManager::openDataFolder() const
{
    QString dataPath = getDataPath();

    // Check if folder exists
    QFileInfo folderInfo(dataPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("FLER DATA folder does not exist: " + dataPath);
        return false;
    }

    // Open folder in Windows Explorer
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(dataPath));
    if (success) {
        Logger::instance().info("Opened FLER DATA folder: " + dataPath);
    } else {
        Logger::instance().error("Failed to open FLER DATA folder: " + dataPath);
    }

    return success;
}

bool TMFLERFileManager::openArchiveFolder() const
{
    QString archivePath = getArchivePath();

    // Check if folder exists
    QFileInfo folderInfo(archivePath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("FLER ARCHIVE folder does not exist: " + archivePath);
        return false;
    }

    // Open folder in Windows Explorer
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(archivePath));
    if (success) {
        Logger::instance().info("Opened FLER ARCHIVE folder: " + archivePath);
    } else {
        Logger::instance().error("Failed to open FLER ARCHIVE folder: " + archivePath);
    }

    return success;
}

bool TMFLERFileManager::openScriptsFolder() const
{
    QString scriptsPath = getScriptsPath();

    // Check if folder exists
    QFileInfo folderInfo(scriptsPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("FLER scripts folder does not exist: " + scriptsPath);
        return false;
    }

    // Open folder in Windows Explorer
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(scriptsPath));
    if (success) {
        Logger::instance().info("Opened FLER scripts folder: " + scriptsPath);
    } else {
        Logger::instance().error("Failed to open FLER scripts folder: " + scriptsPath);
    }

    return success;
}

bool TMFLERFileManager::openJobFolder(const QString& year, const QString& month) const
{
    QString jobFolderPath = getJobFolderPath(year, month);

    // Check if folder exists
    QFileInfo folderInfo(jobFolderPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("FLER job folder does not exist: " + jobFolderPath);
        return false;
    }

    // Open folder in Windows Explorer
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(jobFolderPath));
    if (success) {
        Logger::instance().info("Opened FLER job folder: " + jobFolderPath);
    } else {
        Logger::instance().error("Failed to open FLER job folder: " + jobFolderPath);
    }

    return success;
}
