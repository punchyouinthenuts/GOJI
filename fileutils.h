#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <QString>
#include <QStringList>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>
#include <functional>
#include <memory>

namespace FileUtils {

/**
 * @brief Result class for file operations with error information
 */
class FileResult {
public:
    bool success;            ///< Whether the operation succeeded
    QString errorMessage;    ///< Error message or data (e.g., file content, file list) if operation succeeded
    QString path;            ///< Path involved in the operation

    FileResult() : success(true) {}

    FileResult(bool success, const QString& errorMessage = QString(), const QString& path = QString())
        : success(success), errorMessage(errorMessage), path(path) {}

    /**
     * @brief Check if operation succeeded
     * @return True if operation succeeded
     */
    operator bool() const { return success; }

    /**
     * @brief Get formatted error message
     * @return Formatted error message with path information
     */
    QString formatError() const {
        if (success) return QString();

        if (path.isEmpty()) {
            return errorMessage;
        } else {
            return QString("%1: %2").arg(errorMessage, path);
        }
    }
};

/**
 * @brief Validate whether a file operation will succeed
 * @param operation The operation type ("copy", "move", "delete")
 * @param sourcePath The source file path
 * @param destPath The destination file path (optional for delete)
 * @return Result with success status and error information
 */
FileResult validateFileOperation(const QString& operation, const QString& sourcePath, const QString& destPath = QString());

/**
 * @brief Create a backup of a file
 * @param filePath The path to the file to back up
 * @param backupDir Optional custom backup directory
 * @throws FileOperationException on failure
 */
void createBackup(const QString& filePath, const QString& backupDir = QString());

/**
 * @brief Safely removes a file with proper error handling
 * @param filePath The path to the file to remove
 * @param createBackup Whether to create a backup before removing
 * @throws FileOperationException on failure
 */
void safeRemoveFile(const QString& filePath, bool createBackup = false);

/**
 * @brief Safely copies a file with verification
 * @param sourcePath The source file path
 * @param destPath The destination file path
 * @param overwrite Whether to overwrite an existing destination file
 * @throws FileOperationException on failure
 */
void safeCopyFile(const QString& sourcePath, const QString& destPath, bool overwrite = true);

/**
 * @brief Safely moves a file with fallback to copy+delete
 * @param sourcePath The source file path
 * @param destPath The destination file path
 * @param overwrite Whether to overwrite an existing destination file
 * @throws FileOperationException on failure
 */
void safeMoveFile(const QString& sourcePath, const QString& destPath, bool overwrite = true);

/**
 * @brief Creates all necessary directories in a path
 * @param dirPath The directory path to create
 * @throws FileOperationException on failure
 */
void ensureDirectoryExists(const QString& dirPath);

/**
 * @brief Safely read the entire contents of a file
 * @param filePath The path to the file to read
 * @param maxSize Maximum size in bytes to read (0 = no limit)
 * @return Result with success status; file content in errorMessage field if successful
 * @throws FileOperationException on file open failure
 */
FileResult readTextFile(const QString& filePath, qint64 maxSize = 0);

/**
 * @brief Safely write text content to a file
 * @param filePath The path to the file to write
 * @param content The text content to write
 * @param append Whether to append to existing file
 * @throws FileOperationException on failure
 */
void writeTextFile(const QString& filePath, const QString& content, bool append = false);

/**
 * @brief Get a list of files matching a pattern
 * @param dirPath The directory to search
 * @param filters File filters (e.g., "*.txt")
 * @param recursive Whether to search subdirectories
 * @return Result with success status; file list in errorMessage field if successful
 */
FileResult findFiles(const QString& dirPath, const QStringList& filters, bool recursive = false);

/**
 * @brief Check if a file is locked by another process
 * @param filePath Path to the file to check
 * @return Result with success=true if file is NOT locked
 * @throws FileOperationException on file operation failure
 */
FileResult isFileLocked(const QString& filePath);

/**
 * @brief Attempt to release locks on a file
 * @param filePath Path to the file
 * @return Result with success status and error information
 */
FileResult releaseFileLock(const QString& filePath);

/**
 * @brief Calculate hash for a file
 * @param filePath Path to the file
 * @param method Hash method (e.g., "md5", "sha1", "sha256")
 * @return Result with success status; hash in errorMessage field if successful
 * @throws FileOperationException on file open failure
 */
FileResult calculateFileHash(const QString& filePath, const QString& method = "sha256");

/**
 * @brief Get formatted file size with appropriate units
 * @param sizeInBytes The size in bytes
 * @return Formatted size (e.g., "1.23 MB")
 */
QString formatFileSize(qint64 sizeInBytes);

/**
 * @brief Get the MIME type of a file based on its extension and (optionally) content
 * @param filePath Path to the file
 * @param checkContent Whether to check file content if extension is ambiguous
 * @return MIME type string
 * @throws FileOperationException if checkContent is true and file cannot be opened
 */
QString getMimeType(const QString& filePath, bool checkContent = false);

/**
 * @brief Try to create a unique file name in a directory
 * @param baseDir The directory to create the file in
 * @param baseName The base file name without extension
 * @param extension The file extension (with dot)
 * @return Unique file name (baseDir + baseName + counter + extension)
 * @throws FileOperationException on directory creation failure
 */
QString createUniqueFileName(const QString& baseDir, const QString& baseName, const QString& extension);

/**
 * @brief Create temporary file with specific content
 * @param content The content to write
 * @param prefix The prefix for the temporary file name
 * @param extension The file extension (with dot)
 * @throws FileOperationException on failure
 */
void createTempFile(const QString& content, const QString& prefix = "temp", const QString& extension = ".tmp");

/**
 * @brief Clean up old temporary files
 * @param tempDir Directory containing temporary files
 * @param prefix Prefix of files to clean up
 * @param maxAgeHours Maximum age in hours before deletion
 * @throws FileOperationException on file removal failure
 */
void cleanupTempFiles(const QString& tempDir = QString(), const QString& prefix = "temp", int maxAgeHours = 24);

} // namespace FileUtils

#endif // FILEUTILS_H
