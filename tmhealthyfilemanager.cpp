#include "tmhealthyfilemanager.h"
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
const QStringList TMHealthyFileManager::SUPPORTED_INPUT_FORMATS = {"*.csv", "*.txt", "*.xlsx", "*.xls"};
const QStringList TMHealthyFileManager::SUPPORTED_OUTPUT_FORMATS = {"*.pdf", "*.csv", "*.xlsx"};
const QString TMHealthyFileManager::BASE_PATH = "C:/Goji/TRACHMAR/HEALTHY BEGINNINGS";
const QString TMHealthyFileManager::HOME_FOLDER = "HOME";
const QString TMHealthyFileManager::DATA_FOLDER = "DATA";
const QString TMHealthyFileManager::INPUT_FOLDER = "INPUT";
const QString TMHealthyFileManager::OUTPUT_FOLDER = "OUTPUT";
const QString TMHealthyFileManager::PROCESSED_FOLDER = "PROCESSED";
const QString TMHealthyFileManager::ARCHIVE_FOLDER = "ARCHIVE";
const QString TMHealthyFileManager::SCRIPTS_FOLDER = "SCRIPTS";

TMHealthyFileManager::TMHealthyFileManager(QSettings* settings, QObject* parent)
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

    Logger::instance().info("TMHealthyFileManager initialized with base path: " + m_baseDirectory);
}

TMHealthyFileManager::~TMHealthyFileManager()
{
    stopDirectoryMonitoring();
    Logger::instance().info("TMHealthyFileManager destroyed");
}

QString TMHealthyFileManager::getBasePath() const
{
    return m_baseDirectory;
}

QString TMHealthyFileManager::getBaseDirectory() const
{
    return m_baseDirectory;
}

QString TMHealthyFileManager::getInputDirectory() const
{
    return m_inputDirectory;
}

QString TMHealthyFileManager::getOutputDirectory() const
{
    return m_outputDirectory;
}

QString TMHealthyFileManager::getProcessedDirectory() const
{
    return m_processedDirectory;
}

QString TMHealthyFileManager::getArchiveDirectory() const
{
    return m_archiveDirectory;
}

QString TMHealthyFileManager::getScriptsDirectory() const
{
    return m_scriptsDirectory;
}

QString TMHealthyFileManager::getHomeDirectory() const
{
    return m_homeDirectory;
}

QString TMHealthyFileManager::getDataDirectory() const
{
    return m_dataDirectory;
}

QString TMHealthyFileManager::getJobDirectory(const QString& year, const QString& month) const
{
    return m_baseDirectory + "/" + year + "/" + month;
}

QString TMHealthyFileManager::getJobInputDirectory(const QString& year, const QString& month) const
{
    return getJobDirectory(year, month) + "/" + INPUT_FOLDER;
}

QString TMHealthyFileManager::getJobOutputDirectory(const QString& year, const QString& month) const
{
    return getJobDirectory(year, month) + "/" + OUTPUT_FOLDER;
}

QString TMHealthyFileManager::getJobArchiveDirectory(const QString& year, const QString& month) const
{
    return getJobDirectory(year, month) + "/" + ARCHIVE_FOLDER;
}

bool TMHealthyFileManager::createJobStructure(const QString& year, const QString& month)
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

bool TMHealthyFileManager::copyFilesToJobDirectory(const QString& year, const QString& month)
{
    QString jobInputDir = getJobInputDirectory(year, month);
    
    if (!ensureDirectoryExists(jobInputDir)) {
        return false;
    }

    // Copy files from main input directory to job input directory
    QDir inputDir(m_inputDirectory);
    QStringList files = inputDir.entryList(QDir::Files);
    
    for (const QString& fileName : files) {
        QString sourcePath = m_inputDirectory + "/" + fileName;
        QString destPath = jobInputDir + "/" + fileName;
        
        if (!copyFileWithBackup(sourcePath, destPath)) {
            Logger::instance().error("Failed to copy file: " + sourcePath);
            return false;
        }
    }

    Logger::instance().info("Copied files to job directory for " + year + "-" + month);
    return true;
}

bool TMHealthyFileManager::moveFilesToHomeDirectory(const QString& year, const QString& month)
{
    QString jobOutputDir = getJobOutputDirectory(year, month);
    
    if (!QDir(jobOutputDir).exists()) {
        Logger::instance().warning("Job output directory does not exist: " + jobOutputDir);
        return true; // Nothing to move
    }

    if (!ensureDirectoryExists(m_homeDirectory)) {
        return false;
    }

    QDir outputDir(jobOutputDir);
    QStringList files = outputDir.entryList(QDir::Files);
    
    for (const QString& fileName : files) {
        QString sourcePath = jobOutputDir + "/" + fileName;
        QString destPath = m_homeDirectory + "/" + fileName;
        
        if (!moveFileWithBackup(sourcePath, destPath)) {
            Logger::instance().error("Failed to move file: " + sourcePath);
            return false;
        }
    }

    Logger::instance().info("Moved files to HOME directory for " + year + "-" + month);
    return true;
}

bool TMHealthyFileManager::archiveJobFiles(const QString& year, const QString& month)
{
    QString jobDir = getJobDirectory(year, month);
    QString archiveDir = getJobArchiveDirectory(year, month);
    
    if (!QDir(jobDir).exists()) {
        Logger::instance().warning("Job directory does not exist: " + jobDir);
        return true; // Nothing to archive
    }

    if (!ensureDirectoryExists(archiveDir)) {
        return false;
    }

    QDir jobDirectory(jobDir);
    QStringList files = jobDirectory.entryList(QDir::Files);
    
    for (const QString& fileName : files) {
        QString sourcePath = jobDir + "/" + fileName;
        QString destPath = archiveDir + "/" + fileName;
        
        if (!moveFileWithBackup(sourcePath, destPath)) {
            Logger::instance().error("Failed to archive file: " + sourcePath);
            return false;
        }
    }

    Logger::instance().info("Archived job files for " + year + "-" + month);
    return true;
}

bool TMHealthyFileManager::cleanupJobDirectory(const QString& year, const QString& month)
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

bool TMHealthyFileManager::validateInputFile(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    QString suffix = "*." + fileInfo.suffix().toLower();
    return SUPPORTED_INPUT_FORMATS.contains(suffix);
}

bool TMHealthyFileManager::validateOutputFile(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    QString suffix = "*." + fileInfo.suffix().toLower();
    return SUPPORTED_OUTPUT_FORMATS.contains(suffix);
}

QStringList TMHealthyFileManager::getSupportedInputFormats() const
{
    return SUPPORTED_INPUT_FORMATS;
}

QStringList TMHealthyFileManager::getSupportedOutputFormats() const
{
    return SUPPORTED_OUTPUT_FORMATS;
}

void TMHealthyFileManager::startDirectoryMonitoring()
{
    if (m_monitoringActive) {
        return;
    }

    setupFileWatchers();
    m_monitoringActive = true;
    Logger::instance().info("Directory monitoring started");
}

void TMHealthyFileManager::stopDirectoryMonitoring()
{
    if (!m_monitoringActive) {
        return;
    }

    removeFileWatchers();
    m_monitoringActive = false;
    Logger::instance().info("Directory monitoring stopped");
}

bool TMHealthyFileManager::isMonitoringActive() const
{
    return m_monitoringActive;
}

QStringList TMHealthyFileManager::getInputFiles() const
{
    QDir dir(m_inputDirectory);
    return dir.entryList(SUPPORTED_INPUT_FORMATS, QDir::Files);
}

QStringList TMHealthyFileManager::getOutputFiles() const
{
    QDir dir(m_outputDirectory);
    return dir.entryList(SUPPORTED_OUTPUT_FORMATS, QDir::Files);
}

QStringList TMHealthyFileManager::getProcessedFiles() const
{
    QDir dir(m_processedDirectory);
    return dir.entryList(QDir::Files);
}

QStringList TMHealthyFileManager::getJobFiles(const QString& year, const QString& month) const
{
    QString jobDir = getJobDirectory(year, month);
    QDir dir(jobDir);
    return dir.entryList(QDir::Files);
}

QStringList TMHealthyFileManager::getArchivedFiles(const QString& year, const QString& month) const
{
    QString archiveDir = getJobArchiveDirectory(year, month);
    QDir dir(archiveDir);
    return dir.entryList(QDir::Files);
}

QFileInfo TMHealthyFileManager::getFileInfo(const QString& filePath) const
{
    return QFileInfo(filePath);
}

QString TMHealthyFileManager::getFileChecksum(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return hash.result().toHex();
}

qint64 TMHealthyFileManager::getDirectorySize(const QString& directoryPath) const
{
    qint64 size = 0;
    QDirIterator it(directoryPath, QDir::Files, QDirIterator::Subdirectories);
    
    while (it.hasNext()) {
        it.next();
        size += it.fileInfo().size();
    }
    
    return size;
}

bool TMHealthyFileManager::backupJobData(const QString& year, const QString& month, const QString& backupPath)
{
    QString jobDir = getJobDirectory(year, month);
    
    if (!QDir(jobDir).exists()) {
        Logger::instance().error("Job directory does not exist: " + jobDir);
        return false;
    }
    
    if (!ensureDirectoryExists(backupPath)) {
        return false;
    }
    
    // Copy all files from job directory to backup location
    QDir sourceDir(jobDir);
    QStringList files = sourceDir.entryList(QDir::Files);
    
    for (const QString& fileName : files) {
        QString sourcePath = jobDir + "/" + fileName;
        QString destPath = backupPath + "/" + fileName;
        
        if (!QFile::copy(sourcePath, destPath)) {
            Logger::instance().error("Failed to backup file: " + sourcePath);
            return false;
        }
    }
    
    Logger::instance().info("Backed up job data for " + year + "-" + month + " to " + backupPath);
    return true;
}

bool TMHealthyFileManager::restoreJobData(const QString& year, const QString& month, const QString& backupPath)
{
    if (!QDir(backupPath).exists()) {
        Logger::instance().error("Backup directory does not exist: " + backupPath);
        return false;
    }
    
    QString jobDir = getJobDirectory(year, month);
    if (!createJobStructure(year, month)) {
        return false;
    }
    
    // Copy all files from backup location to job directory
    QDir backupDir(backupPath);
    QStringList files = backupDir.entryList(QDir::Files);
    
    for (const QString& fileName : files) {
        QString sourcePath = backupPath + "/" + fileName;
        QString destPath = jobDir + "/" + fileName;
        
        if (!QFile::copy(sourcePath, destPath)) {
            Logger::instance().error("Failed to restore file: " + sourcePath);
            return false;
        }
    }
    
    Logger::instance().info("Restored job data for " + year + "-" + month + " from " + backupPath);
    return true;
}

bool TMHealthyFileManager::cleanupOldFiles(int daysOld)
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

bool TMHealthyFileManager::cleanupTemporaryFiles()
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

bool TMHealthyFileManager::cleanupProcessedFiles(int daysOld)
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

void TMHealthyFileManager::onDirectoryChanged(const QString& path)
{
    emit directoryChanged(path);
    Logger::instance().info("Directory changed: " + path);
}

void TMHealthyFileManager::onFileChanged(const QString& path)
{
    emit fileModified(path);
    Logger::instance().info("File changed: " + path);
}

void TMHealthyFileManager::initializeDirectoryStructure()
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

bool TMHealthyFileManager::createDirectory(const QString& path)
{
    QDir dir;
    return dir.mkpath(path);
}

bool TMHealthyFileManager::ensureDirectoryExists(const QString& path)
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

bool TMHealthyFileManager::copyFileWithBackup(const QString& source, const QString& destination)
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

bool TMHealthyFileManager::moveFileWithBackup(const QString& source, const QString& destination)
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

QString TMHealthyFileManager::generateBackupFileName(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    return fileInfo.dir().path() + "/" + fileInfo.baseName() + "_backup_" + timestamp + "." + fileInfo.suffix();
}

QString TMHealthyFileManager::normalizePath(const QString& path) const
{
    return QDir::cleanPath(path);
}

QString TMHealthyFileManager::makeRelativePath(const QString& path) const
{
    QDir baseDir(m_baseDirectory);
    return baseDir.relativeFilePath(path);
}

bool TMHealthyFileManager::isPathValid(const QString& path) const
{
    return !path.isEmpty() && QDir::isAbsolutePath(path);
}

bool TMHealthyFileManager::isValidCSVFile(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    return fileInfo.suffix().toLower() == "csv" && fileInfo.exists();
}

bool TMHealthyFileManager::isValidZIPFile(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    return fileInfo.suffix().toLower() == "zip" && fileInfo.exists();
}

bool TMHealthyFileManager::isValidExcelFile(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    return (suffix == "xlsx" || suffix == "xls") && fileInfo.exists();
}

QString TMHealthyFileManager::detectFileFormat(const QString& filePath) const
{
    QMimeDatabase mimeDB;
    QMimeType mimeType = mimeDB.mimeTypeForFile(filePath);
    return mimeType.name();
}

void TMHealthyFileManager::setupFileWatchers()
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
    connect(m_inputWatcher, &QFileSystemWatcher::directoryChanged, this, &TMHealthyFileManager::onDirectoryChanged);
    connect(m_outputWatcher, &QFileSystemWatcher::directoryChanged, this, &TMHealthyFileManager::onDirectoryChanged);
    connect(m_processedWatcher, &QFileSystemWatcher::directoryChanged, this, &TMHealthyFileManager::onDirectoryChanged);
}

void TMHealthyFileManager::updateFileWatchers()
{
    removeFileWatchers();
    setupFileWatchers();
}

void TMHealthyFileManager::removeFileWatchers()
{
    if (m_inputWatcher) {
        delete m_inputWatcher;
        m_inputWatcher = nullptr;
    }
    if (m_outputWatcher) {
        delete m_outputWatcher;
        m_outputWatcher = nullptr;
    }
    if (m_processedWatcher) {
        delete m_processedWatcher;
        m_processedWatcher = nullptr;
    }
}

QString TMHealthyFileManager::getScriptPath(const QString& scriptName) const
{
    return m_scriptPaths.value(scriptName, QString());
}

void TMHealthyFileManager::initializeScriptPaths()
{
    Logger::instance().info("Initializing HEALTHY script paths...");

    QString scriptsDir = "C:/Goji/scripts/TRACHMAR/HEALTHY BEGINNINGS";

    // Map script names to their full paths
    m_scriptPaths["01INITIAL"] = scriptsDir + "/01 INITIAL.py";
    m_scriptPaths["02FINALPROCESS"] = scriptsDir + "/02 FINAL PROCESS.py";

    // Log the script paths for debugging
    for (auto it = m_scriptPaths.constBegin(); it != m_scriptPaths.constEnd(); ++it) {
        Logger::instance().info(QString("HEALTHY script mapped: %1 -> %2").arg(it.key(), it.value()));
    }

    Logger::instance().info("HEALTHY script paths initialization complete");
}
