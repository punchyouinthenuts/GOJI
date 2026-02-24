#include "tmcafilemanager.h"
#include "logger.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>

TMCAFileManager::TMCAFileManager(QSettings* settings)
    : BaseFileSystemManager(settings)
{
    initializeScriptPaths();
}

QString TMCAFileManager::getBasePath() const
{
    return "C:/Goji/TRACHMAR/CA";
}

QString TMCAFileManager::getDataPath() const
{
    return getBasePath() + "/DATA";
}

QString TMCAFileManager::getArchivePath() const
{
    return getBasePath() + "/ARCHIVE";
}

QString TMCAFileManager::getDropPath() const
{
    return getBasePath() + "/DROP";
}

QString TMCAFileManager::getBAInputPath() const
{
    return getBasePath() + "/BA/INPUT";
}

QString TMCAFileManager::getEDRInputPath() const
{
    return getBasePath() + "/EDR/INPUT";
}

QString TMCAFileManager::getScriptsPath() const
{
    return "C:/Goji/scripts/TRACHMAR/CA";
}

void TMCAFileManager::initializeScriptPaths()
{
    QString scriptsDir = getScriptsPath();

    // Map script names to their full paths
    m_scriptPaths["01INITIAL"] = scriptsDir + "/01 INITIAL.py";
    m_scriptPaths["02FINALPROCESS"] = scriptsDir + "/02 FINAL PROCESS.py";

    Logger::instance().info("TMCA script paths initialized");
}

QString TMCAFileManager::getJobFolderPath(const QString& year, const QString& month) const
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

QString TMCAFileManager::getJobFolderPath(const QString& jobNumber, const QString& year, const QString& month) const
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

QString TMCAFileManager::getScriptPath(const QString& scriptName) const
{
    if (m_scriptPaths.contains(scriptName)) {
        return m_scriptPaths[scriptName];
    }

    // If not in the map, try to resolve using the scripts path
    return getScriptsPath() + "/" + scriptName + ".py";
}

bool TMCAFileManager::createBaseDirectories()
{
    QStringList directories = {
        "C:/Goji",
        "C:/Goji/TRACHMAR",
        getBasePath(),
        getDataPath(),
        getArchivePath(),
        getDropPath(),
        getBAInputPath(),
        getEDRInputPath(),
        getScriptsPath()
    };

    bool allCreated = true;
    for (const QString& dir : directories) {
        if (!createDirectoryIfNotExists(dir)) {
            allCreated = false;
            Logger::instance().error("Failed to create TMCA directory: " + dir);
        }
    }

    if (allCreated) {
        Logger::instance().info("All TMCA base directories created successfully");
    }

    return allCreated;
}

bool TMCAFileManager::createJobFolder(const QString& year, const QString& month)
{
    if (year.isEmpty() || month.isEmpty()) {
        Logger::instance().error("Cannot create TMCA job folder: year or month is empty");
        return false;
    }

    QString folderPath = getJobFolderPath(year, month);
    if (!createDirectoryIfNotExists(folderPath)) {
        Logger::instance().error("Failed to create TMCA job folder: " + folderPath);
        return false;
    }

    Logger::instance().info("Created TMCA job folder: " + folderPath);
    return true;
}

bool TMCAFileManager::openDataFolder() const
{
    QString dataPath = getDataPath();

    QFileInfo folderInfo(dataPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("TMCA DATA folder does not exist: " + dataPath);
        return false;
    }

    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(dataPath));
    if (success) {
        Logger::instance().info("Opened TMCA DATA folder: " + dataPath);
    } else {
        Logger::instance().error("Failed to open TMCA DATA folder: " + dataPath);
    }

    return success;
}

bool TMCAFileManager::openArchiveFolder() const
{
    QString archivePath = getArchivePath();

    QFileInfo folderInfo(archivePath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("TMCA ARCHIVE folder does not exist: " + archivePath);
        return false;
    }

    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(archivePath));
    if (success) {
        Logger::instance().info("Opened TMCA ARCHIVE folder: " + archivePath);
    } else {
        Logger::instance().error("Failed to open TMCA ARCHIVE folder: " + archivePath);
    }

    return success;
}

bool TMCAFileManager::openScriptsFolder() const
{
    QString scriptsPath = getScriptsPath();

    QFileInfo folderInfo(scriptsPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("TMCA scripts folder does not exist: " + scriptsPath);
        return false;
    }

    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(scriptsPath));
    if (success) {
        Logger::instance().info("Opened TMCA scripts folder: " + scriptsPath);
    } else {
        Logger::instance().error("Failed to open TMCA scripts folder: " + scriptsPath);
    }

    return success;
}

bool TMCAFileManager::openDropFolder() const
{
    QString dropPath = getDropPath();

    QFileInfo folderInfo(dropPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("TMCA DROP folder does not exist: " + dropPath);
        return false;
    }

    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(dropPath));
    if (success) {
        Logger::instance().info("Opened TMCA DROP folder: " + dropPath);
    } else {
        Logger::instance().error("Failed to open TMCA DROP folder: " + dropPath);
    }

    return success;
}

bool TMCAFileManager::openBAInputFolder() const
{
    QString baInputPath = getBAInputPath();

    QFileInfo folderInfo(baInputPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("TMCA BA INPUT folder does not exist: " + baInputPath);
        return false;
    }

    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(baInputPath));
    if (success) {
        Logger::instance().info("Opened TMCA BA INPUT folder: " + baInputPath);
    } else {
        Logger::instance().error("Failed to open TMCA BA INPUT folder: " + baInputPath);
    }

    return success;
}

bool TMCAFileManager::openEDRInputFolder() const
{
    QString edrInputPath = getEDRInputPath();

    QFileInfo folderInfo(edrInputPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("TMCA EDR INPUT folder does not exist: " + edrInputPath);
        return false;
    }

    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(edrInputPath));
    if (success) {
        Logger::instance().info("Opened TMCA EDR INPUT folder: " + edrInputPath);
    } else {
        Logger::instance().error("Failed to open TMCA EDR INPUT folder: " + edrInputPath);
    }

    return success;
}

bool TMCAFileManager::openJobFolder(const QString& year, const QString& month) const
{
    QString jobFolderPath = getJobFolderPath(year, month);

    QFileInfo folderInfo(jobFolderPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("TMCA job folder does not exist: " + jobFolderPath);
        return false;
    }

    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(jobFolderPath));
    if (success) {
        Logger::instance().info("Opened TMCA job folder: " + jobFolderPath);
    } else {
        Logger::instance().error("Failed to open TMCA job folder: " + jobFolderPath);
    }

    return success;
}
