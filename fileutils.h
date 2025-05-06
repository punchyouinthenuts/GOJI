#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <QString>
#include <QStringList>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>

namespace FileUtils {

/**
 * @brief Validates whether a file operation will succeed
 * @param operation The operation type ("copy", "move", "delete")
 * @param sourcePath The source file path
 * @param destPath The destination file path (optional for delete)
 * @return True if the operation is likely to succeed, false otherwise
 */
bool validateFileOperation(const QString& operation, const QString& sourcePath, const QString& destPath = QString()) {
    // Validate source path
    if (sourcePath.isEmpty()) {
        qDebug() << "Invalid source path (empty)";
        return false;
    }

    // For operations that require source file
    if (operation != "create") {
        QFileInfo sourceInfo(sourcePath);
        if (!sourceInfo.exists()) {
            qDebug() << "Source file does not exist:" << sourcePath;
            return false;
        }

        if (!sourceInfo.isReadable()) {
            qDebug() << "Source file is not readable:" << sourcePath;
            return false;
        }
    }

    // For operations that require destination path
    if (operation == "copy" || operation == "move") {
        if (destPath.isEmpty()) {
            qDebug() << "Invalid destination path (empty)";
            return false;
        }

        QFileInfo destInfo(destPath);
        QDir destDir = destInfo.dir();

        if (!destDir.exists()) {
            if (!destDir.mkpath(".")) {
                qDebug() << "Cannot create destination directory:" << destDir.path();
                return false;
            }
        }

        if (destInfo.exists() && !destInfo.isWritable()) {
            qDebug() << "Destination file exists but is not writable:" << destPath;
            return false;
        }
    }

    return true;
}

/**
 * @brief Creates a backup of a file
 * @param filePath The path to the file to back up
 * @param backupDir Optional custom backup directory
 * @return The path to the created backup file, or empty string on failure
 */
QString createBackup(const QString& filePath, const QString& backupDir = QString()) {
    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists() || !fileInfo.isReadable()) {
        qDebug() << "Cannot backup non-existent or unreadable file:" << filePath;
        return QString();
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
            qDebug() << "Failed to create backup directory:" << backupPath;
            return QString();
        }
    }

    // Create timestamped backup filename
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString backupFile = QString("%1/%2_backup_%3.%4")
                             .arg(backupPath, fileInfo.baseName(), timestamp, fileInfo.suffix());

    // Perform the backup
    if (QFile::copy(filePath, backupFile)) {
        qDebug() << "Created backup:" << backupFile;
        return backupFile;
    } else {
        qDebug() << "Failed to create backup from" << filePath << "to" << backupFile;
        return QString();
    }
}

/**
 * @brief Safely removes a file with proper error handling
 * @param filePath The path to the file to remove
 * @param createBackup Whether to create a backup before removing
 * @return True if removal was successful, false otherwise
 */
bool safeRemoveFile(const QString& filePath, bool createBackup = false) {
    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists()) {
        // File already doesn't exist, consider it successful
        return true;
    }

    if (!fileInfo.isWritable()) {
        // Try to make the file writable
        QFile file(filePath);
        if (!file.setPermissions(file.permissions() | QFile::WriteOwner | QFile::WriteUser)) {
            qDebug() << "Failed to make file writable:" << filePath;
            return false;
        }
    }

    // Create backup if requested
    if (createBackup) {
        QString backupFile = FileUtils::createBackup(filePath);
        if (backupFile.isEmpty()) {
            qDebug() << "Failed to create backup before removal:" << filePath;
            // Continue anyway - backup is optional
        }
    }

    // Attempt to remove the file
    QFile file(filePath);
    if (!file.remove()) {
        qDebug() << "Failed to remove file:" << filePath << "-" << file.errorString();
        return false;
    }

    return true;
}

/**
 * @brief Safely copies a file with verification
 * @param sourcePath The source file path
 * @param destPath The destination file path
 * @param overwrite Whether to overwrite an existing destination file
 * @return True if copy was successful and verified, false otherwise
 */
bool safeCopyFile(const QString& sourcePath, const QString& destPath, bool overwrite = true) {
    // Validate operation
    if (!validateFileOperation("copy", sourcePath, destPath)) {
        return false;
    }

    // Handle existing destination file
    QFileInfo destInfo(destPath);
    if (destInfo.exists()) {
        if (!overwrite) {
            qDebug() << "Destination file exists and overwrite is disabled:" << destPath;
            return false;
        }

        if (!safeRemoveFile(destPath)) {
            qDebug() << "Failed to remove existing destination file:" << destPath;
            return false;
        }
    }

    // Perform the copy
    if (!QFile::copy(sourcePath, destPath)) {
        qDebug() << "Failed to copy" << sourcePath << "to" << destPath;
        return false;
    }

    // Verify the copy was successful by comparing file sizes
    QFileInfo sourceInfo(sourcePath);
    QFileInfo newDestInfo(destPath);

    if (newDestInfo.size() != sourceInfo.size()) {
        qDebug() << "Copy verification failed - size mismatch:"
                 << sourcePath << "(" << sourceInfo.size() << "bytes) vs "
                 << destPath << "(" << newDestInfo.size() << "bytes)";

        // Try to remove the corrupt copy
        QFile::remove(destPath);
        return false;
    }

    return true;
}

/**
 * @brief Safely moves a file with fallback to copy+delete
 * @param sourcePath The source file path
 * @param destPath The destination file path
 * @param overwrite Whether to overwrite an existing destination file
 * @return True if move was successful, false otherwise
 */
bool safeMoveFile(const QString& sourcePath, const QString& destPath, bool overwrite = true) {
    // Validate operation
    if (!validateFileOperation("move", sourcePath, destPath)) {
        return false;
    }

    // Handle existing destination file
    QFileInfo destInfo(destPath);
    if (destInfo.exists()) {
        if (!overwrite) {
            qDebug() << "Destination file exists and overwrite is disabled:" << destPath;
            return false;
        }

        if (!safeRemoveFile(destPath)) {
            qDebug() << "Failed to remove existing destination file:" << destPath;
            return false;
        }
    }

    // Try to use rename (fast move) first
    if (QFile::rename(sourcePath, destPath)) {
        qDebug() << "Moved" << sourcePath << "to" << destPath;
        return true;
    }

    // If rename fails, try copy+delete
    qDebug() << "Direct rename failed, falling back to copy+delete for" << sourcePath;

    if (safeCopyFile(sourcePath, destPath, true)) {
        // Verify the copy succeeded before deleting source
        if (safeRemoveFile(sourcePath)) {
            qDebug() << "Successfully moved (copy+delete)" << sourcePath << "to" << destPath;
            return true;
        } else {
            qDebug() << "Warning: Copied but failed to delete source:" << sourcePath;
            // Still return true because the data was successfully transferred
            return true;
        }
    } else {
        qDebug() << "Failed to copy during move operation:" << sourcePath << "to" << destPath;
        return false;
    }
}

/**
 * @brief Creates all necessary directories in a path
 * @param dirPath The directory path to create
 * @return True if successful, false otherwise
 */
bool ensureDirectoryExists(const QString& dirPath) {
    QDir dir(dirPath);
    if (dir.exists()) {
        return true;
    }

    return dir.mkpath(".");
}

} // namespace FileUtils

#endif // FILEUTILS_H
