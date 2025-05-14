#ifndef BASEFILESYSTEMMANAGER_H
#define BASEFILESYSTEMMANAGER_H

#include <QString>
#include <QStringList>
#include <QSettings>
#include <QDir>
#include <QPair>
#include <QList>

/**
 * @brief Abstract base class for file system operations
 *
 * This class defines the common interface for file system operations
 * across different tabs/modules in the application.
 */
class BaseFileSystemManager
{
public:
    /**
     * @brief Constructor
     * @param settings Application settings
     */
    explicit BaseFileSystemManager(QSettings* settings) : m_settings(settings) {}

    /**
     * @brief Virtual destructor
     */
    virtual ~BaseFileSystemManager() {}

    /**
     * @brief Get the base path for the module
     * @return The base path
     */
    virtual QString getBasePath() const = 0;

    /**
     * @brief Create a directory if it doesn't exist
     * @param path Path to create
     * @return True if the directory exists or was created successfully
     */
    virtual bool createDirectoryIfNotExists(const QString& path);

    /**
     * @brief Copy a file from source to destination
     * @param source Source file path
     * @param destination Destination file path
     * @return True if the copy was successful
     */
    virtual bool copyFile(const QString& source, const QString& destination);

    /**
     * @brief Move a file from source to destination
     * @param source Source file path
     * @param destination Destination file path
     * @return True if the move was successful
     */
    virtual bool moveFile(const QString& source, const QString& destination);

    /**
     * @brief Check if a file exists
     * @param path File path to check
     * @return True if the file exists
     */
    virtual bool fileExists(const QString& path) const;

    /**
     * @brief Get a list of files in a directory that match a pattern
     * @param dirPath Directory path
     * @param filter File name filter (e.g., "*.pdf")
     * @return List of matching file paths
     */
    virtual QStringList getFilesInDirectory(const QString& dirPath, const QString& filter = "*") const;

    /**
     * @brief Open a file with the default application
     * @param filePath File path to open
     * @return True if the file was opened successfully
     */
    virtual bool openFile(const QString& filePath) const;

protected:
    QSettings* m_settings;
    QList<QPair<QString, QString>> m_completedOperations; // Tracks operations for logging or rollback
};

#endif // BASEFILESYSTEMMANAGER_H
