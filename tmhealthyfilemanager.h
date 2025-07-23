#ifndef TMHEALTHYFILEMANAGER_H
#define TMHEALTHYFILEMANAGER_H

#include "basefilesystemmanager.h"
#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QHash>
#include <QMap>

class TMHealthyFileManager : public QObject, public BaseFileSystemManager
{
    Q_OBJECT

public:
    explicit TMHealthyFileManager(QSettings* settings, QObject *parent = nullptr);
    ~TMHealthyFileManager();

    // Base directory paths
    QString getBasePath() const override;
    QString getBaseDirectory() const;
    QString getInputDirectory() const;
    QString getOutputDirectory() const;
    QString getProcessedDirectory() const;
    QString getArchiveDirectory() const;
    QString getScriptsDirectory() const;
    QString getHomeDirectory() const;
    QString getDataDirectory() const;
    
    // Job-specific directories
    QString getJobDirectory(const QString& year, const QString& month) const;
    QString getJobInputDirectory(const QString& year, const QString& month) const;
    QString getJobOutputDirectory(const QString& year, const QString& month) const;
    QString getJobArchiveDirectory(const QString& year, const QString& month) const;
    
    // File operations
    bool createJobStructure(const QString& year, const QString& month);
    bool copyFilesToJobDirectory(const QString& year, const QString& month);
    bool moveFilesToHomeDirectory(const QString& year, const QString& month);
    bool archiveJobFiles(const QString& year, const QString& month);
    bool cleanupJobDirectory(const QString& year, const QString& month);
    
    // File validation
    bool validateInputFile(const QString& filePath) const;
    bool validateOutputFile(const QString& filePath) const;
    QStringList getSupportedInputFormats() const;
    QStringList getSupportedOutputFormats() const;
    
    // Directory monitoring
    void startDirectoryMonitoring();
    void stopDirectoryMonitoring();
    bool isMonitoringActive() const;
    
    // File listing and management
    QStringList getInputFiles() const;
    QStringList getOutputFiles() const;
    QStringList getProcessedFiles() const;
    QStringList getJobFiles(const QString& year, const QString& month) const;
    QStringList getArchivedFiles(const QString& year, const QString& month) const;
    
    // File information
    QFileInfo getFileInfo(const QString& filePath) const;
    QString getFileChecksum(const QString& filePath) const;
    qint64 getDirectorySize(const QString& directoryPath) const;
    
    // Backup and restore
    bool backupJobData(const QString& year, const QString& month, const QString& backupPath);
    bool restoreJobData(const QString& year, const QString& month, const QString& backupPath);
    
    // Cleanup operations
    bool cleanupOldFiles(int daysOld);
    bool cleanupTemporaryFiles();
    bool cleanupProcessedFiles(int daysOld);
    
signals:
    void fileAdded(const QString& filePath);
    void fileRemoved(const QString& filePath);
    void fileModified(const QString& filePath);
    void directoryChanged(const QString& directoryPath);
    void processingStarted(const QString& jobId);
    void processingCompleted(const QString& jobId);
    void processingFailed(const QString& jobId, const QString& error);

private slots:
    void onDirectoryChanged(const QString& path);
    void onFileChanged(const QString& path);

private:
    // Directory structure management
    void initializeDirectoryStructure();
    bool createDirectory(const QString& path);
    bool ensureDirectoryExists(const QString& path);
    
    // File operations helpers
    bool copyFileWithBackup(const QString& source, const QString& destination);
    bool moveFileWithBackup(const QString& source, const QString& destination);
    QString generateBackupFileName(const QString& filePath) const;
    
    // Path utilities
    QString normalizePath(const QString& path) const;
    QString makeRelativePath(const QString& path) const;
    bool isPathValid(const QString& path) const;
    
    // File validation helpers
    bool isValidCSVFile(const QString& filePath) const;
    bool isValidZIPFile(const QString& filePath) const;
    bool isValidExcelFile(const QString& filePath) const;
    QString detectFileFormat(const QString& filePath) const;
    
    // Directory monitoring
    void setupFileWatchers();
    void updateFileWatchers();
    void removeFileWatchers();
    
    // Script path management
    QString getScriptPath(const QString& scriptName) const;
    
    // Script path management
    void initializeScriptPaths();
    
    // Member variables
    QSettings* m_settings;
    QString m_baseDirectory;
    QString m_homeDirectory;
    QString m_dataDirectory;
    QString m_inputDirectory;
    QString m_outputDirectory;
    QString m_processedDirectory;
    QString m_archiveDirectory;
    QString m_scriptsDirectory;
    
    // Script paths mapping
    QMap<QString, QString> m_scriptPaths;
    
    // Monitoring
    QFileSystemWatcher* m_inputWatcher;
    QFileSystemWatcher* m_outputWatcher;
    QFileSystemWatcher* m_processedWatcher;
    bool m_monitoringActive;
    
    // File tracking
    QStringList m_watchedDirectories;
    QStringList m_watchedFiles;
    QHash<QString, QDateTime> m_fileTimestamps;
    
    // Constants
    static const QStringList SUPPORTED_INPUT_FORMATS;
    static const QStringList SUPPORTED_OUTPUT_FORMATS;
    static const QString BASE_PATH;
    static const QString HOME_FOLDER;
    static const QString DATA_FOLDER;
    static const QString INPUT_FOLDER;
    static const QString OUTPUT_FOLDER;
    static const QString PROCESSED_FOLDER;
    static const QString ARCHIVE_FOLDER;
    static const QString SCRIPTS_FOLDER;
};

#endif // TMHEALTHYFILEMANAGER_H