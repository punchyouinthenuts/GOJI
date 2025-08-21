#include "tmbrokenfilemanager.h"
#include "logger.h"
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QStandardPaths>
#include <QFile>
#include <QIODevice>
#include <QFileSystemWatcher>
#include <QCryptographicHash>
#include <QMimeDatabase>
#include <QMimeType>

// Static member initialization
const QStringList TMBrokenFileManager::SUPPORTED_INPUT_FORMATS = {"*.csv", "*.txt", "*.xlsx", "*.xls"};
const QStringList TMBrokenFileManager::SUPPORTED_OUTPUT_FORMATS = {"*.pdf", "*.csv", "*.xlsx"};
const QString TMBrokenFileManager::BASE_PATH = "C:/Goji/TRACHMAR/BROKEN APPOINTMENTS";
const QString TMBrokenFileManager::HOME_FOLDER = "HOME";
const QString TMBrokenFileManager::DATA_FOLDER = "DATA";
const QString TMBrokenFileManager::INPUT_FOLDER = "INPUT";
const QString TMBrokenFileManager::OUTPUT_FOLDER = "OUTPUT";
const QString TMBrokenFileManager::PROCESSED_FOLDER = "PROCESSED";
const QString TMBrokenFileManager::ARCHIVE_FOLDER = "ARCHIVE";
const QString TMBrokenFileManager::SCRIPTS_FOLDER = "SCRIPTS";

TMBrokenFileManager::TMBrokenFileManager(QSettings* settings, QObject* parent)
    : QObject(parent), BaseFileSystemManager(settings),
      m_settings(settings),
      m_inputWatcher(nullptr),
      m_outputWatcher(nullptr),
      m_processedWatcher(nullptr),
      m_monitoringActive(false)
{
    // Initialize directory paths
    m_baseDirectory = BASE_PATH;
    m_homeDirectory = m_baseDirectory + "/" + HOME_FOLDER;
    m_dataDirectory = m_baseDirectory + "/" + DATA_FOLDER;
    m_inputDirectory = m_dataDirectory + "/" + INPUT_FOLDER;
    m_outputDirectory = m_dataDirectory + "/" + OUTPUT_FOLDER;
    m_processedDirectory = m_dataDirectory + "/" + PROCESSED_FOLDER;
    m_archiveDirectory = m_baseDirectory + "/" + ARCHIVE_FOLDER;
    m_scriptsDirectory = m_baseDirectory + "/" + SCRIPTS_FOLDER;

    // Initialize directory structure
    initializeDirectoryStructure();

    // Initialize script paths
    initializeScriptPaths();

    Logger::instance().info("TMBrokenFileManager initialized with base path: " + m_baseDirectory);
}

TMBrokenFileManager::~TMBrokenFileManager()
{
    stopDirectoryMonitoring();
    Logger::instance().info("TMBrokenFileManager destroyed");
}

QString TMBrokenFileManager::getBasePath() const
{
    return m_baseDirectory;
}

QString TMBrokenFileManager::getBaseDirectory() const
{
    return m_baseDirectory;
}

QString TMBrokenFileManager::getInputDirectory() const
{
    return m_inputDirectory;
}

QString TMBrokenFileManager::getOutputDirectory() const
{
    return m_outputDirectory;
}

QString TMBrokenFileManager::getProcessedDirectory() const
{
    return m_processedDirectory;
}

QString TMBrokenFileManager::getArchiveDirectory() const
{
    return m_archiveDirectory;
}

QString TMBrokenFileManager::getScriptsDirectory() const
{
    return m_scriptsDirectory;
}

QString TMBrokenFileManager::getHomeDirectory() const
{
    return m_homeDirectory;
}

QString TMBrokenFileManager::getDataDirectory() const
{
    return m_dataDirectory;
}

QString TMBrokenFileManager::getJobDirectory(const QString& year, const QString& month) const
{
    return m_baseDirectory + "/" + year + "/" + month;
}

QString TMBrokenFileManager::getJobInputDirectory(const QString& year, const QString& month) const
{
    return getJobDirectory(year, month) + "/" + INPUT_FOLDER;
}

QString TMBrokenFileManager::getJobOutputDirectory(const QString& year, const QString& month) const
{
    return getJobDirectory(year, month) + "/" + OUTPUT_FOLDER;
}

QString TMBrokenFileManager::getJobArchiveDirectory(const QString& year, const QString& month) const
{
    return getJobDirectory(year, month) + "/" + ARCHIVE_FOLDER;
}

bool TMBrokenFileManager::createJobStructure(const QString& year, const QString& month)
{
    QString jobDir = getJobDirectory(year, month);
    
    QStringList directories = {
        jobDir,
        getJobInputDirectory(year, month),
        getJobOutputDirectory(year, month),
        getJobArchiveDirectory(year, month)
    };

    for (const QString& dir : directories) {
        if (!ensureDirectoryExists(dir)) {
            Logger::instance().error("Failed to create job directory: " + dir);
            return false;
        }
    }

    Logger::instance().info("Created job structure for " + year + "-" + month);
    return true;
}

bool TMBrokenFileManager::copyFilesToJobDirectory(const QString& year, const QString& month)
{
    QString jobInputDir = getJobInputDirectory(year, month);
    
    if (!ensureDirectoryExists(jobInputDir)) {
        Logger::instance().error("Failed to create job input directory: " + jobInputDir);
        return false;
    }

    // Copy files from main input directory to job input directory
    QDir inputDir(m_inputDirectory);
    QStringList files = inputDir.entryList(QDir::Files);
    
    for (const QString& fileName : files) {
        QString sourcePath = m_inputDirectory + "/" + fileName;
        QString destPath = jobInputDir + "/" + fileName;
        
        if (!copyFileWithBackup(sourcePath, destPath)) {
            Logger::instance().error("Failed to copy file from " + sourcePath + " to " + destPath);
            return false;
        }
    }

    Logger::instance().info("Copied files to job directory for " + year + "-" + month);
    return true;
}

bool TMBrokenFileManager::moveFilesToHomeDirectory(const QString& year, const QString& month)
{
    QString jobOutputDir = getJobOutputDirectory(year, month);
    
    if (!QDir(jobOutputDir).exists()) {
        Logger::instance().warning("Job output directory does not exist: " + jobOutputDir);
        return true; // Nothing to move
    }

    if (!ensureDirectoryExists(m_homeDirectory)) {
        Logger::instance().error("Failed to create home directory: " + m_homeDirectory);
        return false;
    }

    QDir outputDir(jobOutputDir);
    QStringList files = outputDir.entryList(QDir::Files);
    
    for (const QString& fileName : files) {
        QString sourcePath = jobOutputDir + "/" + fileName;
        QString destPath = m_homeDirectory + "/" + fileName;
        
        if (!moveFileWithBackup(sourcePath, destPath)) {
            Logger::instance().error("Failed to move file from " + sourcePath + " to " + destPath);
            return false;
        }
    }

    Logger::instance().info("Moved files to HOME directory for " + year + "-" + month);
    return true;
}

bool TMBrokenFileManager::archiveJobFiles(const QString& year, const QString& month)
{
    QString jobDir = getJobDirectory(year, month);
    QString archiveDir = getJobArchiveDirectory(year, month);
    
    if (!QDir(jobDir).exists()) {
        Logger::instance().warning("Job directory does not exist: " + jobDir);
        return true; // Nothing to archive
    }

    if (!ensureDirectoryExists(archiveDir)) {
        Logger::instance().error("Failed to create archive directory: " + archiveDir);
        return false;
    }

    QDir jobDirectory(jobDir);
    QStringList files = jobDirectory.entryList(QDir::Files);
    
    for (const QString& fileName : files) {
        QString sourcePath = jobDir + "/" + fileName;
        QString destPath = archiveDir + "/" + fileName;
        
        if (!moveFileWithBackup(sourcePath, destPath)) {
            Logger::instance().error("Failed to archive file from " + sourcePath + " to " + destPath);
            return false;
        }
    }

    Logger::instance().info("Archived job files for " + year + "-" + month);
    return true;
}

bool TMBrokenFileManager::cleanupJobDirectory(const QString& year, const QString& month)
{
    QString jobDir = getJobDirectory(year, month);
    
    if (!QDir(jobDir).exists()) {
        return true; // Already clean
    }

    QDir directory(jobDir);
    if (!directory.removeRecursively()) {
        Logger::instance().error("Failed to cleanup job directory: " + jobDir);
        return false;
    }

    Logger::instance().info("Cleaned up job directory for " + year + "-" + month);
    return true;
}

bool TMBrokenFileManager::validateInputFile(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    QString suffix = "*." + fileInfo.suffix().toLower();
    return SUPPORTED_INPUT_FORMATS.contains(suffix);
}

bool TMBrokenFileManager::validateOutputFile(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    QString suffix = "*." + fileInfo.suffix().toLower();
    return SUPPORTED_OUTPUT_FORMATS.contains(suffix);
}

QString TMBrokenFileManager::getOriginalDirectory() const
{
    return m_dataDirectory + "/ORIGINAL";
}

QString TMBrokenFileManager::getMergedDirectory() const
{
    return m_dataDirectory + "/MERGED";
}

QString TMBrokenFileManager::getFallbackDirectory() const
{
    return "C:/Users/JCox/Desktop/MOVE TO NETWORK DRIVE";
}

bool TMBrokenFileManager::createOriginalDirectory()
{
    QString originalDir = getOriginalDirectory();
    return ensureDirectoryExists(originalDir);
}

bool TMBrokenFileManager::createMergedDirectory()
{
    QString mergedDir = getMergedDirectory();
    return ensureDirectoryExists(mergedDir);
}

QStringList TMBrokenFileManager::getSupportedInputFormats() const
{
    return SUPPORTED_INPUT_FORMATS;
}

QStringList TMBrokenFileManager::getSupportedOutputFormats() const
{
    return SUPPORTED_OUTPUT_FORMATS;
}

void TMBrokenFileManager::startDirectoryMonitoring()
{
    if (m_monitoringActive) {
        return;
    }

    setupFileWatchers();
    m_monitoringActive = true;
    Logger::instance().info("Directory monitoring started");
}

void TMBrokenFileManager::stopDirectoryMonitoring()
{
    if (!m_monitoringActive) {
        return;
    }

    removeFileWatchers();
    m_monitoringActive = false;
    Logger::instance().info("Directory monitoring stopped");
}

bool TMBrokenFileManager::isMonitoringActive() const
{
    return m_monitoringActive;
}

QStringList TMBrokenFileManager::getInputFiles() const
{
    QDir dir(m_inputDirectory);
    return dir.entryList(SUPPORTED_INPUT_FORMATS, QDir::Files);
}

QStringList TMBrokenFileManager::getOutputFiles() const
{
    QDir dir(m_outputDirectory);
    return dir.entryList(SUPPORTED_OUTPUT_FORMATS, QDir::Files);
}

QStringList TMBrokenFileManager::getProcessedFiles() const
{
    QDir dir(m_processedDirectory);
    return dir.entryList(QDir::Files);
}

QStringList TMBrokenFileManager::getJobFiles(const QString& year, const QString& month) const
{
    QString jobDir = getJobDirectory(year, month);
    QDir dir(jobDir);
    return dir.entryList(QDir::Files);
}

QStringList TMBrokenFileManager::getArchivedFiles(const QString& year, const QString& month) const
{
    QString archiveDir = getJobArchiveDirectory(year, month);
    QDir dir(archiveDir);
    return dir.entryList(QDir::Files);
}

QFileInfo TMBrokenFileManager::getFileInfo(const QString& filePath) const
{
    return QFileInfo(filePath);
}

QString TMBrokenFileManager::getFileChecksum(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return hash.result().toHex();
}

qint64 TMBrokenFileManager::getDirectorySize(const QString& directoryPath) const
{
    qint64 size = 0;
    QDirIterator it(directoryPath, QDir::Files, QDirIterator::Subdirectories);
    
    while (it.hasNext()) {
        it.next();
        size += it.fileInfo().size();
    }
    
    return size;
}

bool TMBrokenFileManager::backupJobData(const QString& year, const QString& month, const QString& backupPath)
{
    QString jobDir = getJobDirectory(year, month);
    
    if (!QDir(jobDir).exists()) {
        Logger::instance().error("Job directory does not exist: " + jobDir);
        return false;
    }
    
    if (!ensureDirectoryExists(backupPath)) {
        Logger::instance().error("Failed to create backup directory: " + backupPath);
        return false;
    }
    
    // Copy all files from job directory to backup location
    QDir sourceDir(jobDir);
    QStringList files = sourceDir.entryList(QDir::Files);
    
    for (const QString& fileName : files) {
        QString sourcePath = jobDir + "/" + fileName;
        QString destPath = backupPath + "/" + fileName;
        
        if (!QFile::copy(sourcePath, destPath)) {
            Logger::instance().error("Failed to backup file from " + sourcePath + " to " + destPath);
            return false;
        }
    }
    
    Logger::instance().info("Backed up job data for " + year + "-" + month + " to " + backupPath);
    return true;
}

bool TMBrokenFileManager::restoreJobData(const QString& year, const QString& month, const QString& backupPath)
{
    if (!QDir(backupPath).exists()) {
        Logger::instance().error("Backup directory does not exist: " + backupPath);
        return false;
    }
    
    QString jobDir = getJobDirectory(year, month);
    if (!createJobStructure(year, month)) {
        Logger::instance().error("Failed to create job structure for restore operation: " + year + "/" + month);
        return false;
    }
    
    // Copy all files from backup location to job directory
    QDir backupDir(backupPath);
    QStringList files = backupDir.entryList(QDir::Files);
    
    for (const QString& fileName : files) {
        QString sourcePath = backupPath + "/" + fileName;
        QString destPath = jobDir + "/" + fileName;
        
        if (!QFile::copy(sourcePath, destPath)) {
            Logger::instance().error("Failed to restore file from " + sourcePath + " to " + destPath);
            return false;
        }
    }
    
    Logger::instance().info("Restored job data for " + year + "-" + month + " from " + backupPath);
    return true;
}

bool TMBrokenFileManager::cleanupOldFiles(int daysOld)
{
    if (daysOld <= 0) {
        return false;
    }
    
    QDateTime cutoffDate = QDateTime::currentDateTime().addDays(-daysOld);
    QStringList directories = {m_processedDirectory, m_archiveDirectory};
    
    bool allCleaned = true;
    
    for (const QString& dir : directories) {
        QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
        
        while (it.hasNext()) {
            it.next();
            QFileInfo fileInfo = it.fileInfo();
            
            if (fileInfo.lastModified() < cutoffDate) {
                if (!QFile::remove(fileInfo.absoluteFilePath())) {
                    Logger::instance().error("Failed to remove old file: " + fileInfo.absoluteFilePath());
                    allCleaned = false;
                } else {
                    Logger::instance().info("Removed old file: " + fileInfo.fileName());
                }
            }
        }
    }
    
    return allCleaned;
}

bool TMBrokenFileManager::cleanupTemporaryFiles()
{
    QStringList tempPatterns = {"*.tmp", "*.temp", "*~", "*.bak"};
    QStringList directories = {m_baseDirectory, m_dataDirectory, m_processedDirectory};
    
    bool allCleaned = true;
    
    for (const QString& dir : directories) {
        QDir directory(dir);
        
        for (const QString& pattern : tempPatterns) {
            QStringList tempFiles = directory.entryList(QStringList() << pattern, QDir::Files);
            
            for (const QString& tempFile : tempFiles) {
                QString filePath = dir + "/" + tempFile;
                if (!QFile::remove(filePath)) {
                    Logger::instance().error("Failed to remove temporary file: " + filePath);
                    allCleaned = false;
                } else {
                    Logger::instance().info("Removed temporary file: " + tempFile);
                }
            }
        }
    }
    
    return allCleaned;
}

bool TMBrokenFileManager::cleanupProcessedFiles(int daysOld)
{
    if (daysOld <= 0) {
        return false;
    }
    
    QDateTime cutoffDate = QDateTime::currentDateTime().addDays(-daysOld);
    QDir processedDir(m_processedDirectory);
    
    if (!processedDir.exists()) {
        return true; // Nothing to clean
    }
    
    QStringList files = processedDir.entryList(QDir::Files);
    bool allCleaned = true;
    
    for (const QString& fileName : files) {
        QString filePath = m_processedDirectory + "/" + fileName;
        QFileInfo fileInfo(filePath);
        
        if (fileInfo.lastModified() < cutoffDate) {
            if (!QFile::remove(filePath)) {
                Logger::instance().error("Failed to remove processed file: " + filePath);
                allCleaned = false;
            } else {
                Logger::instance().info("Removed processed file: " + fileName);
            }
        }
    }
    
    return allCleaned;
}

void TMBrokenFileManager::onDirectoryChanged(const QString& path)
{
    emit directoryChanged(path);
    Logger::instance().info("Directory changed: " + path);
}

void TMBrokenFileManager::onFileChanged(const QString& path)
{
    emit fileModified(path);
    Logger::instance().info("File changed: " + path);
}

void TMBrokenFileManager::initializeDirectoryStructure()
{
    QStringList directories = {
        m_baseDirectory,
        m_homeDirectory,
        m_dataDirectory,
        m_inputDirectory,
        m_outputDirectory,
        m_processedDirectory,
        m_archiveDirectory,
        m_scriptsDirectory
    };

    for (const QString& dir : directories) {
        ensureDirectoryExists(dir);
    }
}

bool TMBrokenFileManager::createDirectory(const QString& path)
{
    QDir dir;
    return dir.mkpath(path);
}

bool TMBrokenFileManager::ensureDirectoryExists(const QString& path)
{
    QDir dir(path);
    if (!dir.exists()) {
        if (!createDirectory(path)) {
            Logger::instance().error("Failed to create directory: " + path);
            return false;
        }
        Logger::instance().info("Created directory: " + path);
    }
    return true;
}

bool TMBrokenFileManager::copyFileWithBackup(const QString& source, const QString& destination)
{
    // If destination exists, create backup
    if (QFile::exists(destination)) {
        QString backupPath = generateBackupFileName(destination);
        if (!QFile::copy(destination, backupPath)) {
            Logger::instance().warning("Failed to create backup of: " + destination);
        }
    }

    return QFile::copy(source, destination);
}

bool TMBrokenFileManager::moveFileWithBackup(const QString& source, const QString& destination)
{
    // If destination exists, create backup
    if (QFile::exists(destination)) {
        QString backupPath = generateBackupFileName(destination);
        if (!QFile::copy(destination, backupPath)) {
            Logger::instance().warning("Failed to create backup of: " + destination);
        }
        QFile::remove(destination);
    }

    return QFile::rename(source, destination);
}

QString TMBrokenFileManager::generateBackupFileName(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    return fileInfo.dir().path() + "/" + fileInfo.baseName() + "_backup_" + timestamp + "." + fileInfo.suffix();
}

QString TMBrokenFileManager::normalizePath(const QString& path) const
{
    return QDir::cleanPath(path);
}

QString TMBrokenFileManager::makeRelativePath(const QString& path) const
{
    QDir baseDir(m_baseDirectory);
    return baseDir.relativeFilePath(path);
}

bool TMBrokenFileManager::isPathValid(const QString& path) const
{
    return !path.isEmpty() && QDir::isAbsolutePath(path);
}

bool TMBrokenFileManager::isValidCSVFile(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    return fileInfo.suffix().toLower() == "csv" && fileInfo.exists();
}

bool TMBrokenFileManager::isValidZIPFile(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    return fileInfo.suffix().toLower() == "zip" && fileInfo.exists();
}

bool TMBrokenFileManager::isValidExcelFile(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    return (suffix == "xlsx" || suffix == "xls") && fileInfo.exists();
}

QString TMBrokenFileManager::detectFileFormat(const QString& filePath) const
{
    QMimeDatabase mimeDB;
    QMimeType mimeType = mimeDB.mimeTypeForFile(filePath);
    return mimeType.name();
}

void TMBrokenFileManager::setupFileWatchers()
{
    // Create watchers
    m_inputWatcher = new QFileSystemWatcher(this);
    m_outputWatcher = new QFileSystemWatcher(this);
    m_processedWatcher = new QFileSystemWatcher(this);

    // Add directories to watch
    if (QDir(m_inputDirectory).exists()) {
        m_inputWatcher->addPath(m_inputDirectory);
    }
    if (QDir(m_outputDirectory).exists()) {
        m_outputWatcher->addPath(m_outputDirectory);
    }
    if (QDir(m_processedDirectory).exists()) {
        m_processedWatcher->addPath(m_processedDirectory);
    }

    // Connect signals
    connect(m_inputWatcher, &QFileSystemWatcher::directoryChanged, this, &TMBrokenFileManager::onDirectoryChanged);
    connect(m_outputWatcher, &QFileSystemWatcher::directoryChanged, this, &TMBrokenFileManager::onDirectoryChanged);
    connect(m_processedWatcher, &QFileSystemWatcher::directoryChanged, this, &TMBrokenFileManager::onDirectoryChanged);
}

void TMBrokenFileManager::updateFileWatchers()
{
    removeFileWatchers();
    setupFileWatchers();
}

void TMBrokenFileManager::removeFileWatchers()
{
    if (m_inputWatcher) {
        if (!m_inputWatcher->directories().isEmpty()) {
            m_inputWatcher->removePaths(m_inputWatcher->directories());
        }
        if (!m_inputWatcher->files().isEmpty()) {
            m_inputWatcher->removePaths(m_inputWatcher->files());
        }
        delete m_inputWatcher;
        m_inputWatcher = nullptr;
    }
    if (m_outputWatcher) {
        if (!m_outputWatcher->directories().isEmpty()) {
            m_outputWatcher->removePaths(m_outputWatcher->directories());
        }
        if (!m_outputWatcher->files().isEmpty()) {
            m_outputWatcher->removePaths(m_outputWatcher->files());
        }
        delete m_outputWatcher;
        m_outputWatcher = nullptr;
    }
    if (m_processedWatcher) {
        if (!m_processedWatcher->directories().isEmpty()) {
            m_processedWatcher->removePaths(m_processedWatcher->directories());
        }
        if (!m_processedWatcher->files().isEmpty()) {
            m_processedWatcher->removePaths(m_processedWatcher->files());
        }
        delete m_processedWatcher;
        m_processedWatcher = nullptr;
    }
}

QString TMBrokenFileManager::getScriptPath(const QString& scriptName) const
{
    return m_scriptPaths.value(scriptName, QString());
}

void TMBrokenFileManager::initializeScriptPaths()
{
    Logger::instance().info("Initializing BROKEN APPOINTMENTS script paths...");

    QString scriptsDir = "C:/Goji/scripts/TRACHMAR/BROKEN APPOINTMENTS";

    // Map script names to their full paths
    m_scriptPaths["01INITIAL"] = scriptsDir + "/01 INITIAL.py";
    m_scriptPaths["02FINALPROCESS"] = scriptsDir + "/02 FINAL PROCESS.py";

    // Log the script paths for debugging
    for (auto it = m_scriptPaths.constBegin(); it != m_scriptPaths.constEnd(); ++it) {
        Logger::instance().info(QString("BROKEN APPOINTMENTS script mapped: %1 -> %2").arg(it.key(), it.value()));
    }

    Logger::instance().info("BROKEN APPOINTMENTS script paths initialization complete");
}
