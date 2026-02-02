#include "fhfilemanager.h"
#include "logger.h"
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>

FHFileManager::FHFileManager(QSettings* settings)
    : BaseFileSystemManager(settings)
{
    initializeScriptPaths();
}

QString FHFileManager::getBasePath() const
{
    return "C:/Goji/AUTOMATION/FOUR HANDS";
}

QString FHFileManager::getOriginalPath() const
{
    return getBasePath() + "/ORIGINAL";
}

QString FHFileManager::getInputPath() const
{
    return getBasePath() + "/INPUT";
}

QString FHFileManager::getOutputPath() const
{
    return getBasePath() + "/OUTPUT";
}

QString FHFileManager::getArchivePath() const
{
    return getBasePath() + "/ARCHIVE";
}

QString FHFileManager::getScriptsPath() const
{
    return "C:/Goji/scripts/FOUR HANDS";
}

void FHFileManager::initializeScriptPaths()
{
    QString scriptsDir = getScriptsPath();

    // Map script names to their full paths
    m_scriptPaths["01INITIAL"] = scriptsDir + "/01 INITIAL.py";
    m_scriptPaths["02FINALPROCESS"] = scriptsDir + "/02 FINAL PROCESS.py";

    Logger::instance().info("FOUR HANDS script paths initialized");
}

QString FHFileManager::getJobFolderPath(const QString& year, const QString& month) const
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

QString FHFileManager::getJobFolderPath(const QString& jobNumber, const QString& dropNumber,
                                        const QString& year, const QString& month) const
{
    QMap<QString, QString> monthMap = {
        {"01","JAN"},{"02","FEB"},{"03","MAR"},{"04","APR"},
        {"05","MAY"},{"06","JUN"},{"07","JUL"},{"08","AUG"},
        {"09","SEP"},{"10","OCT"},{"11","NOV"},{"12","DEC"}
    };
    const QString monthAbbrev = monthMap.value(month, month);
    const QString dn = dropNumber.isEmpty() ? "1" : dropNumber;
    return getArchivePath() + "/" + jobNumber + " D" + dn + " " + monthAbbrev + " " + year;
}

bool FHFileManager::createJobFolder(const QString& jobNumber, const QString& dropNumber,
                                    const QString& year, const QString& month)
{
    const QString base = getJobFolderPath(jobNumber, dropNumber, year, month);
    if (!createDirectoryIfNotExists(base)) return false;

    const QStringList subs = {"INPUT", "ORIGINAL", "OUTPUT"};
    for (const QString& s : subs) {
        if (!createDirectoryIfNotExists(base + "/" + s)) return false;
    }
    return true;
}

QString FHFileManager::getScriptPath(const QString& scriptName) const
{
    if (m_scriptPaths.contains(scriptName)) {
        return m_scriptPaths[scriptName];
    }

    // If not in the map, try to resolve using the scripts path
    return getScriptsPath() + "/" + scriptName + ".py";
}

bool FHFileManager::createBaseDirectories()
{
    QStringList directories = {
        "C:/Goji",
        "C:/Goji/AUTOMATION",
        getBasePath(),
        getOriginalPath(),
        getInputPath(),
        getOutputPath(),
        getArchivePath(),
        getScriptsPath()
    };

    bool allCreated = true;
    for (const QString& dir : directories) {
        if (!createDirectoryIfNotExists(dir)) {
            allCreated = false;
            Logger::instance().error("Failed to create FOUR HANDS directory: " + dir);
        }
    }

    if (allCreated) {
        Logger::instance().info("All FOUR HANDS base directories created successfully");
    }

    return allCreated;
}

bool FHFileManager::createJobFolder(const QString& year, const QString& month)
{
    if (year.isEmpty() || month.isEmpty()) {
        Logger::instance().error("Cannot create FOUR HANDS job folder: year or month is empty");
        return false;
    }

    QString folderPath = getJobFolderPath(year, month);
    if (!createDirectoryIfNotExists(folderPath)) {
        Logger::instance().error("Failed to create FOUR HANDS job folder: " + folderPath);
        return false;
    }

    Logger::instance().info("Created FOUR HANDS job folder: " + folderPath);
    return true;
}

bool FHFileManager::openOriginalFolder() const
{
    QString originalPath = getOriginalPath();

    // Check if folder exists
    QFileInfo folderInfo(originalPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("FOUR HANDS ORIGINAL folder does not exist: " + originalPath);
        return false;
    }

    // Open folder in Windows Explorer
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(originalPath));
    if (success) {
        Logger::instance().info("Opened FOUR HANDS ORIGINAL folder: " + originalPath);
    } else {
        Logger::instance().error("Failed to open FOUR HANDS ORIGINAL folder: " + originalPath);
    }

    return success;
}

bool FHFileManager::openInputFolder() const
{
    QString inputPath = getInputPath();

    // Check if folder exists
    QFileInfo folderInfo(inputPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("FOUR HANDS INPUT folder does not exist: " + inputPath);
        return false;
    }

    // Open folder in Windows Explorer
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(inputPath));
    if (success) {
        Logger::instance().info("Opened FOUR HANDS INPUT folder: " + inputPath);
    } else {
        Logger::instance().error("Failed to open FOUR HANDS INPUT folder: " + inputPath);
    }

    return success;
}

bool FHFileManager::openOutputFolder() const
{
    QString outputPath = getOutputPath();

    // Check if folder exists
    QFileInfo folderInfo(outputPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("FOUR HANDS OUTPUT folder does not exist: " + outputPath);
        return false;
    }

    // Open folder in Windows Explorer
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));
    if (success) {
        Logger::instance().info("Opened FOUR HANDS OUTPUT folder: " + outputPath);
    } else {
        Logger::instance().error("Failed to open FOUR HANDS OUTPUT folder: " + outputPath);
    }

    return success;
}

bool FHFileManager::openArchiveFolder() const
{
    QString archivePath = getArchivePath();

    // Check if folder exists
    QFileInfo folderInfo(archivePath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("FOUR HANDS ARCHIVE folder does not exist: " + archivePath);
        return false;
    }

    // Open folder in Windows Explorer
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(archivePath));
    if (success) {
        Logger::instance().info("Opened FOUR HANDS ARCHIVE folder: " + archivePath);
    } else {
        Logger::instance().error("Failed to open FOUR HANDS ARCHIVE folder: " + archivePath);
    }

    return success;
}

bool FHFileManager::openScriptsFolder() const
{
    QString scriptsPath = getScriptsPath();

    // Check if folder exists
    QFileInfo folderInfo(scriptsPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("FOUR HANDS scripts folder does not exist: " + scriptsPath);
        return false;
    }

    // Open folder in Windows Explorer
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(scriptsPath));
    if (success) {
        Logger::instance().info("Opened FOUR HANDS scripts folder: " + scriptsPath);
    } else {
        Logger::instance().error("Failed to open FOUR HANDS scripts folder: " + scriptsPath);
    }

    return success;
}

bool FHFileManager::openJobFolder(const QString& year, const QString& month) const
{
    QString jobFolderPath = getJobFolderPath(year, month);

    // Check if folder exists
    QFileInfo folderInfo(jobFolderPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("FOUR HANDS job folder does not exist: " + jobFolderPath);
        return false;
    }

    // Open folder in Windows Explorer
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(jobFolderPath));
    if (success) {
        Logger::instance().info("Opened FOUR HANDS job folder: " + jobFolderPath);
    } else {
        Logger::instance().error("Failed to open FOUR HANDS job folder: " + jobFolderPath);
    }

    return success;
}
