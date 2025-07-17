(backupPath)) {
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