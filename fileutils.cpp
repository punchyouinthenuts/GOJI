#include "FileUtils.h"
#include "errorhandling.h"
#include "logger.h"
#include <QTemporaryFile>
#include <QCryptographicHash>
#include <QMimeDatabase>
#include <QProcess>
#include <QCoreApplication>
#include <QThread>
#include <QStandardPaths>

namespace FileUtils {

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

FileResult createBackup(const QString& filePath, const QString& backupDir) {
    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists() || !fileInfo.isReadable()) {
        return FileResult(false, "Cannot backup non-existent or unreadable file", filePath);
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
            return FileResult(false, "Failed to create backup directory", backupPath);
        }
    }

    // Create timestamped backup filename
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString backupFile = QString("%1/%2_backup_%3.%4")
                             .arg(backupPath, fileInfo.baseName(), timestamp, fileInfo.suffix());

    // Perform the backup
    if (QFile::copy(filePath, backupFile)) {
        LOG_INFO(QString("Created backup: %1").arg(backupFile));
        return FileResult(true, backupFile);
    } else {
        return FileResult(false, "Failed to create backup", filePath);
    }
}

FileResult safeRemoveFile(const QString& filePath, bool createBackupFirst) {
    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists()) {
        // File already doesn't exist, consider it successful
        return FileResult(true);
    }

    if (!fileInfo.isWritable()) {
        // Try to make the file writable
        QFile file(filePath);
        if (!file.setPermissions(file.permissions() | QFile::WriteOwner | QFile::WriteUser)) {
            return FileResult(false, "Failed to make file writable", filePath);
        }
    }

    // Create backup if requested
    if (createBackupFirst) {
        FileResult backupResult = createBackup(filePath);
        if (!backupResult) {
            LOG_WARNING(QString("Failed to create backup before removal: %1").arg(filePath));
            // Continue anyway - backup is optional
        }
    }

    // Attempt to remove the file
    QFile file(filePath);
    if (!file.remove()) {
        return FileResult(false, QString("Failed to remove file: %1").arg(file.errorString()), filePath);
    }

    return FileResult(true);
}

FileResult safeCopyFile(const QString& sourcePath, const QString& destPath, bool overwrite) {
    // Validate operation
    FileResult validation = validateFileOperation("copy", sourcePath, destPath);
    if (!validation) {
        return validation;
    }

    // Handle existing destination file
    QFileInfo destInfo(destPath);
    if (destInfo.exists()) {
        if (!overwrite) {
            return FileResult(false, "Destination file exists and overwrite is disabled", destPath);
        }

        FileResult removeResult = safeRemoveFile(destPath);
        if (!removeResult) {
            return FileResult(false, "Failed to remove existing destination file", destPath);
        }
    }

    // Perform the copy
    if (!QFile::copy(sourcePath, destPath)) {
        return FileResult(false, "Failed to copy file", sourcePath);
    }

    // Verify the copy was successful by comparing file sizes
    QFileInfo sourceInfo(sourcePath);
    QFileInfo newDestInfo(destPath);

    if (newDestInfo.size() != sourceInfo.size()) {
        // Size mismatch - remove corrupted file and report error
        QFile::remove(destPath);
        return FileResult(false, "Copy verification failed - size mismatch", sourcePath);
    }

    return FileResult(true);
}

FileResult safeMoveFile(const QString& sourcePath, const QString& destPath, bool overwrite) {
    // Validate operation
    FileResult validation = validateFileOperation("move", sourcePath, destPath);
    if (!validation) {
        return validation;
    }

    // Handle existing destination file
    QFileInfo destInfo(destPath);
    if (destInfo.exists()) {
        if (!overwrite) {
            return FileResult(false, "Destination file exists and overwrite is disabled", destPath);
        }

        FileResult removeResult = safeRemoveFile(destPath);
        if (!removeResult) {
            return FileResult(false, "Failed to remove existing destination file", destPath);
        }
    }

    // Try to use rename (fast move) first
    if (QFile::rename(sourcePath, destPath)) {
        LOG_INFO(QString("Moved %1 to %2").arg(sourcePath, destPath));
        return FileResult(true);
    }

    // If rename fails, try copy+delete
    LOG_INFO(QString("Direct rename failed, falling back to copy+delete for %1").arg(sourcePath));

    FileResult copyResult = safeCopyFile(sourcePath, destPath, true);
    if (!copyResult) {
        return copyResult;
    }

    // Verify the copy succeeded before deleting source
    FileResult removeResult = safeRemoveFile(sourcePath);
    if (!removeResult) {
        LOG_WARNING(QString("Warning: Copied but failed to delete source: %1").arg(sourcePath));
        // Still return true because the data was successfully transferred
    }

    return FileResult(true);
}

FileResult ensureDirectoryExists(const QString& dirPath) {
    QDir dir(dirPath);
    if (dir.exists()) {
        return FileResult(true);
    }

    if (dir.mkpath(".")) {
        return FileResult(true);
    }

    return FileResult(false, "Failed to create directory", dirPath);
}

FileResult readTextFile(const QString& filePath, qint64 maxSize) {
    QFile file(filePath);

    if (!file.exists()) {
        return FileResult(false, "File does not exist", filePath);
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return FileResult(false, QString("Failed to open file: %1").arg(file.errorString()), filePath);
    }

    // Check file size if limit is specified
    if (maxSize > 0 && file.size() > maxSize) {
        return FileResult(false, QString("File size exceeds limit of %1 bytes").arg(maxSize), filePath);
    }

    QTextStream stream(&file);
    QString content = stream.readAll();
    file.close();

    return FileResult(true, content);
}

FileResult writeTextFile(const QString& filePath, const QString& content, bool append) {
    // Create directory if it doesn't exist
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            return FileResult(false, "Failed to create directory", dir.path());
        }
    }

    QFile file(filePath);
    QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Text;
    if (append) {
        mode |= QIODevice::Append;
    }

    if (!file.open(mode)) {
        return FileResult(false, QString("Failed to open file for writing: %1").arg(file.errorString()), filePath);
    }

    QTextStream stream(&file);
    stream << content;

    // Check for errors
    if (stream.status() != QTextStream::Ok) {
        file.close();
        return FileResult(false, "Error writing to file", filePath);
    }

    file.close();
    return FileResult(true);
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

    for (const QFileInfo& entry : entries) {
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

    // Return the file list in the error message field
    return FileResult(true, fileList.join('\n'));
}

FileResult isFileLocked(const QString& filePath) {
    // Try to open the file for exclusive ReadWrite access
    // If we can't open it, it might be locked by another process
    QFile file(filePath);
    if (!file.open(QIODevice::ReadWrite)) {
        return FileResult(false, "File is locked or inaccessible", filePath);
    }

    // Additional check: Try to rename the file temporarily
    // (This is a common way to check if a file is locked on Windows)
    QString tempPath = filePath + ".locktest";
    QFile tempFile(tempPath);

    // Remove any existing temp file first
    if (tempFile.exists()) {
        tempFile.remove();
    }

    // If we can rename the file, it's not locked
    bool canRename = file.copy(tempPath);
    file.close();

    // Clean up the temp file if it was created
    if (canRename) {
        tempFile.remove();
        return FileResult(true);
    }

    return FileResult(false, "File is locked by another process", filePath);
}

FileResult releaseFileLock(const QString& filePath) {
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
        return FileResult(false, "Failed to release file lock", filePath);
    }
}

FileResult calculateFileHash(const QString& filePath, const QString& method) {
    QFile file(filePath);

    if (!file.exists()) {
        return FileResult(false, "File does not exist", filePath);
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return FileResult(false, QString("Failed to open file: %1").arg(file.errorString()), filePath);
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
    return FileResult(false, "Failed to calculate hash", filePath);
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
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            mime = db.mimeTypeForData(&file);
            file.close();
        } else {
            mime = db.mimeTypeForFile(filePath);
        }
    } else {
        mime = db.mimeTypeForFile(filePath);
    }

    return mime.name();
}

QString createUniqueFileName(const QString& baseDir, const QString& baseName, const QString& extension) {
    QDir dir(baseDir);
    if (!dir.exists()) {
        dir.mkpath(".");
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

FileResult createTempFile(const QString& content, const QString& prefix, const QString& extension) {
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir dir(tempDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Create unique file name
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    QString fileName = prefix + "_" + timestamp + extension;
    QString filePath = dir.filePath(fileName);

    // Write content to file
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return FileResult(false, QString("Failed to create temporary file: %1").arg(file.errorString()));
    }

    QTextStream stream(&file);
    stream << content;
    file.close();

    return FileResult(true, filePath);
}

FileResult cleanupTempFiles(const QString& tempDir, const QString& prefix, int maxAgeHours) {
    QString dirPath = tempDir;
    if (dirPath.isEmpty()) {
        dirPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }

    QDir dir(dirPath);
    if (!dir.exists()) {
        return FileResult(false, "Temporary directory does not exist", dirPath);
    }

    QStringList filters;
    filters << prefix + "*";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    QDateTime currentTime = QDateTime::currentDateTime();
    int deletedCount = 0;

    for (const QFileInfo& fileInfo : files) {
        // Calculate file age in hours
        qint64 fileAge = fileInfo.lastModified().secsTo(currentTime) / 3600;

        if (fileAge > maxAgeHours) {
            if (QFile::remove(fileInfo.absoluteFilePath())) {
                deletedCount++;
            }
        }
    }

    return FileResult(true, QString::number(deletedCount));
}

} // namespace FileUtils
