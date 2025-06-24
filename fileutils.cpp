#include "fileutils.h"
#include "errorhandling.h"
#include "logger.h"
#include <QTemporaryFile>
#include <QCryptographicHash>
#include <QMimeDatabase>
#include <QProcess>
#include <QCoreApplication>
#include <QThread>
#include <QStandardPaths>
#include <QMutex>

namespace FileUtils {
// Static mutex for thread-safe file operations
Q_GLOBAL_STATIC(QMutex, fileOperationMutex)

FileResult validateFileOperation(const QString& operation, const QString& sourcePath, const QString& destPath) {
    // Validate source path
    if (sourcePath.isEmpty()) {
        return FileResult(false, "Invalid source path (empty)");
    }

    // For operations that require source file
    if (operation != "create") {
        QFileInfo sourceInfo(sourcePath);
        if (!sourceInfo.exists()) {
            return FileResult(false, "Source file does not exist", sourcePath);
        }

        if (!sourceInfo.isReadable()) {
            return FileResult(false, "Source file is not readable", sourcePath);
        }
    }

    // For operations that require destination path
    if (operation == "copy" || operation == "move") {
        if (destPath.isEmpty()) {
            return FileResult(false, "Invalid destination path (empty)");
        }

        QFileInfo destInfo(destPath);
        QDir destDir = destInfo.dir();

        if (!destDir.exists()) {
            if (!destDir.mkpath(".")) {
                return FileResult(false, "Cannot create destination directory", destDir.path());
            }
        }

        if (destInfo.exists() && !destInfo.isWritable()) {
            return FileResult(false, "Destination file exists but is not writable", destPath);
        }
    }

    return FileResult(true);
}

void createBackup(const QString& filePath, const QString& backupDir) {
    QMutexLocker locker(fileOperationMutex()); // Lock for thread safety

    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists() || !fileInfo.isReadable()) {
        THROW_FILE_ERROR("Cannot backup non-existent or unreadable file: " + filePath, filePath);
    }

    // Determine backup directory
    QString backupPath;
    if (backupDir.isEmpty()) {
        // Use default - create 'backups' subdirectory
        backupPath = fileInfo.absolutePath() + "/backups";
    } else {
        backupPath = backupDir;
    }

    // Ensure backup directory exists
    QDir dir;
    if (!dir.exists(backupPath)) {
        if (!dir.mkpath(backupPath)) {
            THROW_FILE_ERROR("Failed to create backup directory: " + backupPath, backupPath);
        }
    }

    // Create timestamped backup filename
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString backupFile = QString("%1/%2_backup_%3.%4")
                             .arg(backupPath, fileInfo.baseName(), timestamp, fileInfo.suffix());

    // Perform the backup
    if (!QFile::copy(filePath, backupFile)) {
        THROW_FILE_ERROR("Failed to create backup: " + filePath, filePath);
    }
    LOG_INFO(QString("Created backup: %1").arg(backupFile));
}

void safeRemoveFile(const QString& filePath, bool createBackupFirst) {
    QMutexLocker locker(fileOperationMutex()); // Lock for thread safety

    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists()) {
        // File already doesn't exist, consider it successful
        return;
    }

    if (!fileInfo.isWritable()) {
        // Try to make the file writable
        QFile file(filePath);
        if (!file.setPermissions(file.permissions() | QFile::WriteOwner | QFile::WriteUser)) {
            THROW_FILE_ERROR("Failed to make file writable: " + filePath, filePath);
        }
    }

    // Create backup if requested
    if (createBackupFirst) {
        try {
            createBackup(filePath);
        } catch (const FileOperationException& e) {
            LOG_WARNING(QString("Failed to create backup before removal: %1").arg(filePath));
            // Continue anyway - backup is optional
        }
    }

    // Attempt to remove the file
    QFile file(filePath);
    if (!file.remove()) {
        THROW_FILE_ERROR("Failed to remove file: " + file.errorString(), filePath);
    }
}

void safeCopyFile(const QString& sourcePath, const QString& destPath, bool overwrite) {
    QMutexLocker locker(fileOperationMutex()); // Lock for thread safety

    // Validate operation
    FileResult validation = validateFileOperation("copy", sourcePath, destPath);
    if (!validation) {
        THROW_FILE_ERROR(validation.errorMessage, validation.path);
    }

    // Handle existing destination file
    QFileInfo destInfo(destPath);
    if (destInfo.exists()) {
        if (!overwrite) {
            THROW_FILE_ERROR("Destination file exists and overwrite is disabled: " + destPath, destPath);
        }
        safeRemoveFile(destPath);
    }

    // Perform the copy
    if (!QFile::copy(sourcePath, destPath)) {
        THROW_FILE_ERROR("Failed to copy file: " + sourcePath, sourcePath);
    }

    // Verify the copy was successful by comparing file sizes
    QFileInfo sourceInfo(sourcePath);
    QFileInfo newDestInfo(destPath);

    if (newDestInfo.size() != sourceInfo.size()) {
        // Size mismatch - remove corrupted file
        try {
            safeRemoveFile(destPath);
        } catch (const FileOperationException& e) {
            LOG_WARNING(QString("Failed to clean up corrupted destination file: %1").arg(destPath));
        }
        THROW_FILE_ERROR("Copy verification failed - size mismatch: " + sourcePath, sourcePath);
    }
}

void safeMoveFile(const QString& sourcePath, const QString& destPath, bool overwrite) {
    QMutexLocker locker(fileOperationMutex()); // Lock for thread safety

    // Validate operation
    FileResult validation = validateFileOperation("move", sourcePath, destPath);
    if (!validation) {
        THROW_FILE_ERROR(validation.errorMessage, validation.path);
    }

    // Handle existing destination file
    QFileInfo destInfo(destPath);
    if (destInfo.exists()) {
        if (!overwrite) {
            THROW_FILE_ERROR("Destination file exists and overwrite is disabled: " + destPath, destPath);
        }
        safeRemoveFile(destPath);
    }

    // Try to use rename (fast move) first
    if (QFile::rename(sourcePath, destPath)) {
        LOG_INFO(QString("Moved %1 to %2").arg(sourcePath, destPath));
        return;
    }

    // If rename fails, try copy+delete
    LOG_INFO(QString("Direct rename failed, falling back to copy+delete for %1").arg(sourcePath));
    safeCopyFile(sourcePath, destPath, true);
    safeRemoveFile(sourcePath);
}

void ensureDirectoryExists(const QString& dirPath) {
    QMutexLocker locker(fileOperationMutex()); // Lock for thread safety

    QDir dir(dirPath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            THROW_FILE_ERROR("Failed to create directory: " + dirPath, dirPath);
        }
    }
}

FileResult readTextFile(const QString& filePath, qint64 maxSize) {
    QMutexLocker locker(fileOperationMutex()); // Lock for thread safety

    QFile file(filePath);

    if (!file.exists()) {
        return FileResult(false, "File does not exist", filePath);
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        THROW_FILE_ERROR("Failed to open file: " + file.errorString(), filePath);
    }

    // Check file size if limit is specified
    if (maxSize > 0 && file.size() > maxSize) {
        file.close();
        return FileResult(false, QString("File size exceeds limit of %1 bytes").arg(maxSize), filePath);
    }

    QTextStream stream(&file);
    QString content = stream.readAll();
    file.close();

    return FileResult(true, content);
}

void writeTextFile(const QString& filePath, const QString& content, bool append) {
    QMutexLocker locker(fileOperationMutex()); // Lock for thread safety

    // Create directory if it doesn't exist
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            THROW_FILE_ERROR("Failed to create directory: " + dir.path(), dir.path());
        }
    }

    QFile file(filePath);
    QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Text;
    if (append) {
        mode |= QIODevice::Append;
    }

    if (!file.open(mode)) {
        THROW_FILE_ERROR("Failed to open file for writing: " + file.errorString(), filePath);
    }

    QTextStream stream(&file);
    stream << content;

    // Check for errors
    if (stream.status() != QTextStream::Ok) {
        file.close();
        THROW_FILE_ERROR("Error writing to file: " + filePath, filePath);
    }

    file.close();
}

FileResult findFiles(const QString& dirPath, const QStringList& filters, bool recursive) {
    QDir dir(dirPath);

    if (!dir.exists()) {
        return FileResult(false, "Directory does not exist", dirPath);
    }

    QDir::Filters dirFilters = QDir::Files | QDir::NoDotAndDotDot;
    if (recursive) {
        dirFilters |= QDir::AllDirs;
    }

    QStringList fileList;
    QFileInfoList entries = dir.entryInfoList(filters, dirFilters);

    for (const auto& entry : std::as_const(entries)) {
        if (entry.isFile()) {
            fileList << entry.absoluteFilePath();
        } else if (recursive && entry.isDir()) {
            // Recursively search subdirectory
            FileResult subResult = findFiles(entry.absoluteFilePath(), filters, true);
            if (subResult) {
                fileList.append(subResult.errorMessage.split('\n', Qt::SkipEmptyParts));
            }
        }
    }

    // Return the file list in the errorMessage field
    return FileResult(true, fileList.join('\n'));
}

FileResult isFileLocked(const QString& filePath) {
    QMutexLocker locker(fileOperationMutex()); // Lock for thread safety

    // Try to open the file for exclusive ReadWrite access
    QFile file(filePath);
    if (!file.open(QIODevice::ReadWrite)) {
        return FileResult(false, "File is locked or inaccessible", filePath);
    }

    // Additional check: Try to rename the file temporarily
    QString tempPath = filePath + ".locktest";
    QFile tempFile(tempPath);

    // Remove any existing temp file first
    if (tempFile.exists()) {
        try {
            safeRemoveFile(tempPath);
        } catch (const FileOperationException& e) {
            file.close();
            return FileResult(false, "Failed to remove temporary lock test file: " + tempPath, tempPath);
        }
    }

    // If we can rename the file, it's not locked
    bool canRename = file.copy(tempPath);
    file.close();

    // Clean up the temp file if it was created
    if (canRename) {
        try {
            safeRemoveFile(tempPath);
        } catch (const FileOperationException& e) {
            return FileResult(false, "Failed to clean up temporary lock test file: " + tempPath, tempPath);
        }
        return FileResult(true);
    }

    return FileResult(false, "File is locked by another process", filePath);
}

FileResult releaseFileLock(const QString& filePath) {
    QMutexLocker locker(fileOperationMutex()); // Lock for thread safety

    // This is a bit of a hack to try to close any processes that might have the file open
    LOG_INFO(QString("Attempting to release file handles for: %1").arg(filePath));

    // Force application to process events
    QCoreApplication::processEvents();

    // Wait a moment for any pending operations to complete
    QThread::msleep(500);

    // Force garbage collection (not directly available in Qt, but we can prompt it)
    QByteArray largeArray(1024 * 1024 * 10, 0); // Allocate 10MB
    largeArray.clear(); // Release it

    // Process events again
    QCoreApplication::processEvents();

    // Wait again
    QThread::msleep(500);

    // Check if the file is now accessible
    FileResult result = isFileLocked(filePath);
    if (result) {
        return FileResult(true);
    } else {
        return FileResult(false, "Failed to release file lock: " + filePath, filePath);
    }
}

FileResult calculateFileHash(const QString& filePath, const QString& method) {
    QMutexLocker locker(fileOperationMutex()); // Lock for thread safety

    QFile file(filePath);

    if (!file.exists()) {
        return FileResult(false, "File does not exist", filePath);
    }

    if (!file.open(QIODevice::ReadOnly)) {
        THROW_FILE_ERROR("Failed to open file: " + file.errorString(), filePath);
    }

    // Determine hash algorithm
    QCryptographicHash::Algorithm hashAlgorithm;
    if (method.toLower() == "md5") {
        hashAlgorithm = QCryptographicHash::Md5;
    } else if (method.toLower() == "sha1") {
        hashAlgorithm = QCryptographicHash::Sha1;
    } else if (method.toLower() == "sha256") {
        hashAlgorithm = QCryptographicHash::Sha256;
    } else if (method.toLower() == "sha512") {
        hashAlgorithm = QCryptographicHash::Sha512;
    } else {
        file.close();
        return FileResult(false, "Unsupported hash algorithm", method);
    }

    QCryptographicHash hash(hashAlgorithm);
    if (hash.addData(&file)) {
        QString hashResult = hash.result().toHex();
        file.close();
        return FileResult(true, hashResult);
    }

    file.close();
    THROW_FILE_ERROR("Failed to calculate hash: " + filePath, filePath);
}

QString formatFileSize(qint64 sizeInBytes) {
    const qint64 kb = 1024;
    const qint64 mb = kb * 1024;
    const qint64 gb = mb * 1024;

    if (sizeInBytes >= gb) {
        return QString("%1 GB").arg(QString::number(sizeInBytes / static_cast<double>(gb), 'f', 2));
    } else if (sizeInBytes >= mb) {
        return QString("%1 MB").arg(QString::number(sizeInBytes / static_cast<double>(mb), 'f', 2));
    } else if (sizeInBytes >= kb) {
        return QString("%1 KB").arg(QString::number(sizeInBytes / static_cast<double>(kb), 'f', 2));
    } else {
        return QString("%1 bytes").arg(sizeInBytes);
    }
}

QString getMimeType(const QString& filePath, bool checkContent) {
    QMimeDatabase db;
    QMimeType mime;

    if (checkContent) {
        QMutexLocker locker(fileOperationMutex()); // Lock for thread safety
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            mime = db.mimeTypeForData(&file);
            file.close();
        } else {
            THROW_FILE_ERROR("Failed to open file for MIME type detection: " + file.errorString(), filePath);
        }
    } else {
        mime = db.mimeTypeForFile(filePath);
    }

    return mime.name();
}

QString createUniqueFileName(const QString& baseDir, const QString& baseName, const QString& extension) {
    QMutexLocker locker(fileOperationMutex()); // Lock for thread safety

    QDir dir(baseDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            THROW_FILE_ERROR("Failed to create directory for unique filename: " + baseDir, baseDir);
        }
    }

    // Try original name first
    QString fileName = baseName + extension;
    QString filePath = dir.filePath(fileName);

    if (!QFile::exists(filePath)) {
        return filePath;
    }

    // If exists, try with counter
    int counter = 1;
    do {
        fileName = QString("%1_%2%3").arg(baseName).arg(counter++).arg(extension);
        filePath = dir.filePath(fileName);
    } while (QFile::exists(filePath) && counter < 1000); // Avoid infinite loop

    return filePath;
}

void createTempFile(const QString& content, const QString& prefix, const QString& extension) {
    QMutexLocker locker(fileOperationMutex()); // Lock for thread safety

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir dir(tempDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            THROW_FILE_ERROR("Failed to create temporary directory: " + tempDir, tempDir);
        }
    }

    // Create unique file name
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    QString fileName = prefix + "_" + timestamp + extension;
    QString filePath = dir.filePath(fileName);

    // Write content to file
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        THROW_FILE_ERROR("Failed to create temporary file: " + file.errorString(), filePath);
    }

    QTextStream stream(&file);
    stream << content;
    file.close();
}

void cleanupTempFiles(const QString& tempDir, const QString& prefix, int maxAgeHours) {
    QMutexLocker locker(fileOperationMutex()); // Lock for thread safety

    QString dirPath = tempDir;
    if (dirPath.isEmpty()) {
        dirPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }

    QDir dir(dirPath);
    if (!dir.exists()) {
        return; // No directory, no files to clean
    }

    QStringList filters;
    filters << prefix + "*";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    QDateTime currentTime = QDateTime::currentDateTime();
    for (const auto& fileInfo : std::as_const(files)) {
        // Calculate file age in hours
        qint64 fileAge = fileInfo.lastModified().secsTo(currentTime) / 3600;

        if (fileAge > maxAgeHours) {
            if (!QFile::remove(fileInfo.absoluteFilePath())) {
                THROW_FILE_ERROR("Failed to remove temporary file: " + fileInfo.absoluteFilePath(), fileInfo.absoluteFilePath());
            }
        }
    }
}

} // namespace FileUtils
