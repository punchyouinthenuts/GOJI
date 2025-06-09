#include "tmtermfilemanager.h"
#include "logger.h"
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>

TMTermFileManager::TMTermFileManager(QSettings* settings)
    : BaseFileSystemManager(settings)
{
    initializeScriptPaths();
}

QString TMTermFileManager::getBasePath() const
{
    // Check for configured path in settings or use default
    return m_settings->value("TMTERM/BasePath", "C:/Goji/TRACHMAR/TERM").toString();
}

QString TMTermFileManager::getDataPath() const
{
    return m_settings->value("TMTERM/DataPath", getBasePath() + "/DATA").toString();
}

QString TMTermFileManager::getArchivePath() const
{
    return m_settings->value("TMTERM/ArchivePath", getBasePath() + "/ARCHIVE").toString();
}

QString TMTermFileManager::getScriptsPath() const
{
    return m_settings->value("TMTERM/ScriptsPath", "C:/Goji/Scripts/TRACHMAR/TERM").toString();
}

QString TMTermFileManager::getJobFolderPath(const QString& year, const QString& month) const
{
    if (year.isEmpty() || month.isEmpty()) {
        Logger::instance().warning("Year or month is empty when getting TERM job folder path");
        return QString();
    }

    // Convert month number to abbreviation for folder name
    static const QMap<QString, QString> monthMap = {
        {"01", "JAN"}, {"02", "FEB"}, {"03", "MAR"}, {"04", "APR"},
        {"05", "MAY"}, {"06", "JUN"}, {"07", "JUL"}, {"08", "AUG"},
        {"09", "SEP"}, {"10", "OCT"}, {"11", "NOV"}, {"12", "DEC"}
    };

    QString monthAbbrev = monthMap.value(month, month); // Use original if not found
    QString jobNumber = "00000"; // Placeholder - in real use this would come from UI

    return getArchivePath() + "/" + jobNumber + " " + monthAbbrev + " " + year;
}

QString TMTermFileManager::getScriptPath(const QString& scriptName) const
{
    if (m_scriptPaths.contains(scriptName)) {
        return m_scriptPaths[scriptName];
    }

    // If not in the map, try to resolve using the scripts path
    return getScriptsPath() + "/" + scriptName + ".py";
}

bool TMTermFileManager::createBaseDirectories()
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
            Logger::instance().error("Failed to create TERM directory: " + dir);
        }
    }

    if (allCreated) {
        Logger::instance().info("All TERM base directories created successfully");
    }

    return allCreated;
}

bool TMTermFileManager::createJobFolder(const QString& year, const QString& month)
{
    if (year.isEmpty() || month.isEmpty()) {
        Logger::instance().error("Cannot create TERM job folder: year or month is empty");
        return false;
    }

    QString folderPath = getJobFolderPath(year, month);
    if (!createDirectoryIfNotExists(folderPath)) {
        Logger::instance().error("Failed to create TERM job folder: " + folderPath);
        return false;
    }

    Logger::instance().info("Created TERM job folder: " + folderPath);
    return true;
}

bool TMTermFileManager::openDataFolder() const
{
    QString dataPath = getDataPath();

    // Check if folder exists
    QFileInfo folderInfo(dataPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("TERM DATA folder does not exist: " + dataPath);

        // Try to create it
        if (!QDir().mkpath(dataPath)) {
            Logger::instance().error("Failed to create TERM DATA folder: " + dataPath);
            return false;
        }
    }

    // Open the folder using QDesktopServices
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(dataPath));

    if (success) {
        Logger::instance().info("Opened TERM DATA folder: " + dataPath);
    } else {
        Logger::instance().error("Failed to open TERM DATA folder: " + dataPath);
    }

    return success;
}

bool TMTermFileManager::openArchiveFolder(const QString& year, const QString& month) const
{
    if (year.isEmpty() || month.isEmpty()) {
        Logger::instance().error("Cannot open TERM archive folder: year or month is empty");
        return false;
    }

    QString folderPath = getJobFolderPath(year, month);

    // Check if folder exists
    QFileInfo folderInfo(folderPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        Logger::instance().warning("TERM archive folder does not exist: " + folderPath);

        // Open the parent archive directory instead
        folderPath = getArchivePath();
    }

    // Open the folder using QDesktopServices
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));

    if (success) {
        Logger::instance().info("Opened TERM archive folder: " + folderPath);
    } else {
        Logger::instance().error("Failed to open TERM archive folder: " + folderPath);
    }

    return success;
}

bool TMTermFileManager::cleanDataFolder() const
{
    QString dataPath = getDataPath();
    QDir dataDir(dataPath);

    if (!dataDir.exists()) {
        Logger::instance().warning("TERM DATA folder does not exist, nothing to clean: " + dataPath);
        return true; // Consider this success since the goal is achieved
    }

    // Get list of all files in the directory
    QStringList files = dataDir.entryList(QDir::Files | QDir::NoDotAndDotDot);

    bool allRemoved = true;
    int removedCount = 0;

    for (const QString& file : files) {
        QString filePath = dataDir.absoluteFilePath(file);
        if (QFile::remove(filePath)) {
            removedCount++;
            Logger::instance().info("Removed file from TERM DATA: " + file);
        } else {
            allRemoved = false;
            Logger::instance().error("Failed to remove file from TERM DATA: " + file);
        }
    }

    if (allRemoved) {
        Logger::instance().info(QString("Successfully cleaned TERM DATA folder: %1 files removed").arg(removedCount));
    } else {
        Logger::instance().warning(QString("Partially cleaned TERM DATA folder: %1 files removed").arg(removedCount));
    }

    return allRemoved;
}

bool TMTermFileManager::moveFilesToArchive(const QString& year, const QString& month) const
{
    if (year.isEmpty() || month.isEmpty()) {
        Logger::instance().error("Cannot move files to archive: year or month is empty");
        return false;
    }

    QString dataPath = getDataPath();
    QString archivePath = getJobFolderPath(year, month);

    QDir dataDir(dataPath);
    if (!dataDir.exists()) {
        Logger::instance().warning("TERM DATA folder does not exist: " + dataPath);
        return true; // Nothing to move
    }

    // Create archive folder if it doesn't exist
    if (!createJobFolder(year, month)) {
        Logger::instance().error("Failed to create archive folder for move operation");
        return false;
    }

    // Get list of all files in the DATA directory
    QStringList files = dataDir.entryList(QDir::Files | QDir::NoDotAndDotDot);

    bool allMoved = true;
    int movedCount = 0;

    for (const QString& file : files) {
        QString sourcePath = dataDir.absoluteFilePath(file);
        QString destPath = archivePath + "/" + file;

        // Check if destination file already exists
        if (QFile::exists(destPath)) {
            // Create a unique filename
            QFileInfo fileInfo(file);
            QString baseName = fileInfo.baseName();
            QString extension = fileInfo.suffix();
            int counter = 1;

            do {
                QString newFileName = QString("%1_copy%2.%3").arg(baseName).arg(counter).arg(extension);
                destPath = archivePath + "/" + newFileName;
                counter++;
            } while (QFile::exists(destPath));
        }

        if (QFile::rename(sourcePath, destPath)) {
            movedCount++;
            Logger::instance().info("Moved file to TERM archive: " + file);
        } else {
            allMoved = false;
            Logger::instance().error("Failed to move file to TERM archive: " + file);
        }
    }

    if (allMoved) {
        Logger::instance().info(QString("Successfully moved all files to TERM archive: %1 files moved").arg(movedCount));
    } else {
        Logger::instance().warning(QString("Partially moved files to TERM archive: %1 files moved").arg(movedCount));
    }

    return allMoved;
}

void TMTermFileManager::initializeScriptPaths()
{
    Logger::instance().info("Initializing TERM script paths...");

    QString scriptsDir = getScriptsPath();

    // Map script names to their full paths
    m_scriptPaths["01TERMFIRSTSTEP"] = scriptsDir + "/01TERMFIRSTSTEP.py";
    m_scriptPaths["02TERMFINALSTEP"] = scriptsDir + "/02TERMFINALSTEP.py";

    // Log the script paths for debugging
    for (auto it = m_scriptPaths.constBegin(); it != m_scriptPaths.constEnd(); ++it) {
        Logger::instance().info(QString("TERM script mapped: %1 -> %2").arg(it.key(), it.value()));
    }

    Logger::instance().info("TERM script paths initialization complete");
}
