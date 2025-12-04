#include "tmfarmfilemanager.h"
#include "logger.h"
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <utility> // For std::as_const in Qt 6

TMFarmFileManager::TMFarmFileManager(QSettings* settings)
    : BaseFileSystemManager(settings)
{
    initializeScriptPaths();
}

QString TMFarmFileManager::getBasePath() const
{
    return m_settings->value("TMFW/BasePath", "C:/Goji/TRACHMAR/FARMWORKERS").toString();
}

QString TMFarmFileManager::getDataPath() const
{
    return m_settings->value("TMFW/DataPath", getBasePath() + "/DATA").toString();
}

QString TMFarmFileManager::getArchivePath() const
{
    return m_settings->value("TMFW/ArchivePath", getBasePath() + "/ARCHIVE").toString();
}

QString TMFarmFileManager::getScriptsPath() const
{
    return m_settings->value("TMFW/ScriptsPath", "C:/Goji/scripts/TRACHMAR/FARMWORKERS").toString();
}

QString TMFarmFileManager::getJobFolderPath(const QString& year, const QString& quarter) const
{
    Logger::instance().warning("Deprecated getJobFolderPath(year, quarter) used. Provide jobNumber too.");
    return getJobFolderPath("00000", year, quarter);
}

QString TMFarmFileManager::getJobFolderPath(const QString& jobNumber, const QString& year, const QString& quarterCode) const
{
    if (jobNumber.isEmpty() || year.isEmpty() || quarterCode.isEmpty()) {
        Logger::instance().warning("Missing job number/year/quarter for FARMWORKERS job folder path");
        return QString();
    }

    // Format: jobNumber_yearquarter (e.g., 12345_20253RD)
    // quarterCode expected as "1ST", "2ND", "3RD", or "4TH"
    return getArchivePath() + "/" + jobNumber + "_" + year + quarterCode;
}

QString TMFarmFileManager::getScriptPath(const QString& scriptName) const
{
    if (m_scriptPaths.contains(scriptName)) {
        return m_scriptPaths[scriptName];
    }
    return getScriptsPath() + "/" + scriptName + ".py";
}

bool TMFarmFileManager::createBaseDirectories()
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
            Logger::instance().error("Failed to create FARMWORKERS directory: " + dir);
        }
    }

    if (allCreated) {
        Logger::instance().info("All FARMWORKERS base directories created successfully");
    }

    return allCreated;
}

bool TMFarmFileManager::createJobFolder(const QString& jobNumber, const QString& year, const QString& quarterCode)
{
    if (jobNumber.isEmpty() || year.isEmpty() || quarterCode.isEmpty()) {
        Logger::instance().error("Cannot create FARMWORKERS job folder: jobNumber/year/quarter missing");
        return false;
    }

    QString folderPath = getJobFolderPath(jobNumber, year, quarterCode);
    if (!createDirectoryIfNotExists(folderPath)) {
        Logger::instance().error("Failed to create FARMWORKERS job folder: " + folderPath);
        return false;
    }

    Logger::instance().info("Created FARMWORKERS job folder: " + folderPath);
    return true;
}

bool TMFarmFileManager::openDataFolder() const
{
    QString dataPath = getDataPath();
    QFileInfo folderInfo(dataPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("FARMWORKERS DATA folder does not exist: " + dataPath);
        if (!QDir().mkpath(dataPath)) {
            Logger::instance().error("Failed to create FARMWORKERS DATA folder: " + dataPath);
            return false;
        }
    }
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(dataPath));
    if (success)
        Logger::instance().info("Opened FARMWORKERS DATA folder: " + dataPath);
    else
        Logger::instance().error("Failed to open FARMWORKERS DATA folder: " + dataPath);
    return success;
}

bool TMFarmFileManager::openArchiveFolder(const QString& jobNumber, const QString& year, const QString& quarterCode) const
{
    if (jobNumber.isEmpty() || year.isEmpty() || quarterCode.isEmpty()) {
        Logger::instance().error("Cannot open FARMWORKERS archive folder: missing jobNumber/year/quarter");
        return false;
    }
    QString folderPath = getJobFolderPath(jobNumber, year, quarterCode);
    QFileInfo folderInfo(folderPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("FARMWORKERS archive folder does not exist: " + folderPath);
        folderPath = getArchivePath();
    }
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
    if (success)
        Logger::instance().info("Opened FARMWORKERS archive folder: " + folderPath);
    else
        Logger::instance().error("Failed to open FARMWORKERS archive folder: " + folderPath);
    return success;
}

bool TMFarmFileManager::cleanDataFolder() const
{
    QString dataPath = getDataPath();
    QDir dataDir(dataPath);
    if (!dataDir.exists()) {
        Logger::instance().warning("FARMWORKERS DATA folder does not exist, nothing to clean: " + dataPath);
        return true;
    }
    QStringList files = dataDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    bool allRemoved = true;
    int removedCount = 0;
    for (const QString& file : std::as_const(files)) {
        QString filePath = dataDir.absoluteFilePath(file);
        if (QFile::remove(filePath)) {
            removedCount++;
            Logger::instance().info("Removed file from FARMWORKERS DATA: " + file);
        } else {
            allRemoved = false;
            Logger::instance().error("Failed to remove file from FARMWORKERS DATA: " + file);
        }
    }
    if (allRemoved)
        Logger::instance().info(QString("Successfully cleaned FARMWORKERS DATA folder: %1 files removed").arg(removedCount));
    else
        Logger::instance().warning(QString("Partially cleaned FARMWORKERS DATA folder: %1 files removed").arg(removedCount));
    return allRemoved;
}

bool TMFarmFileManager::moveFilesToArchive(const QString& jobNumber, const QString& year, const QString& quarterCode)
{
    if (jobNumber.isEmpty() || year.isEmpty() || quarterCode.isEmpty()) {
        Logger::instance().error("Cannot move files: missing jobNumber/year/quarter");
        return false;
    }
    QString dataPath = getDataPath();
    QString archivePath = getJobFolderPath(jobNumber, year, quarterCode);
    QDir dataDir(dataPath);
    if (!dataDir.exists()) {
        Logger::instance().warning("FARMWORKERS DATA folder does not exist: " + dataPath);
        return true;
    }
    if (!createJobFolder(jobNumber, year, quarterCode)) {
        Logger::instance().error("Failed to create archive folder for move");
        return false;
    }
    QStringList files = dataDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    bool allMoved = true;
    int movedCount = 0;
    for (const QString& file : std::as_const(files)) {
        QString sourcePath = dataDir.absoluteFilePath(file);
        QString destPath = archivePath + "/" + file;
        if (QFile::exists(destPath)) QFile::remove(destPath);
        if (QFile::rename(sourcePath, destPath)) {
            movedCount++;
            Logger::instance().info("Moved file to FARMWORKERS archive: " + file);
        } else {
            allMoved = false;
            Logger::instance().error("Failed to move file to FARMWORKERS archive: " + file);
        }
    }
    if (allMoved)
        Logger::instance().info(QString("Successfully moved all files to FARMWORKERS archive: %1").arg(movedCount));
    else
        Logger::instance().warning(QString("Partially moved files to FARMWORKERS archive: %1").arg(movedCount));
    return allMoved;
}

bool TMFarmFileManager::copyFilesFromArchive(const QString& jobNumber, const QString& year, const QString& quarterCode)
{
    if (jobNumber.isEmpty() || year.isEmpty() || quarterCode.isEmpty()) {
        Logger::instance().error("Cannot copy files: missing jobNumber/year/quarter");
        return false;
    }

    QString archivePath = getJobFolderPath(jobNumber, year, quarterCode);
    QString dataPath = getDataPath();

    QDir archiveDir(archivePath);
    if (!archiveDir.exists()) {
        Logger::instance().warning("FARMWORKERS archive folder does not exist: " + archivePath);
        return false;
    }

    QDir dataDir(dataPath);
    if (!dataDir.exists()) {
        if (!QDir().mkpath(dataPath)) {
            Logger::instance().error("Failed to create FARMWORKERS DATA folder: " + dataPath);
            return false;
        }
    }

    QStringList files = archiveDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    bool allCopied = true;
    int copiedCount = 0;

    for (const QString& file : std::as_const(files)) {
        QString sourcePath = archiveDir.absoluteFilePath(file);
        QString destPath = dataDir.absoluteFilePath(file);

        // Remove existing file in DATA if it exists
        if (QFile::exists(destPath)) {
            QFile::remove(destPath);
        }

        if (QFile::copy(sourcePath, destPath)) {
            copiedCount++;
            Logger::instance().info("Copied file from FARMWORKERS archive to DATA: " + file);
        } else {
            allCopied = false;
            Logger::instance().error("Failed to copy file from FARMWORKERS archive: " + file);
        }
    }

    if (allCopied) {
        Logger::instance().info(QString("Successfully copied all files from FARMWORKERS archive: %1").arg(copiedCount));
    } else {
        Logger::instance().warning(QString("Partially copied files from FARMWORKERS archive: %1").arg(copiedCount));
    }

    return allCopied;
}

void TMFarmFileManager::initializeScriptPaths()
{
    Logger::instance().info("Initializing FARMWORKERS script paths...");
    QString scriptsDir = getScriptsPath();
    m_scriptPaths.clear();
    m_scriptPaths["01 INITIAL"]      = scriptsDir + "/01 INITIAL.py";
    m_scriptPaths["02 POST PROCESS"] = scriptsDir + "/02 POST PROCESS.py";
    for (auto it = m_scriptPaths.constBegin(); it != m_scriptPaths.constEnd(); ++it) {
        Logger::instance().info(QString("FARMWORKERS script mapped: %1 -> %2").arg(it.key(), it.value()));
    }
    Logger::instance().info("FARMWORKERS script paths initialization complete");
}
