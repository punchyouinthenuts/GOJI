#ifndef TMTERMFILEMANAGER_H
#define TMTERMFILEMANAGER_H

#include "basefilesystemmanager.h"
#include <QMap>

/**
 * @brief File system manager for TM TERM tab
 *
 * This class implements file system operations specific to the TM TERM tab,
 * including directory structures, file paths, and specialized operations.
 */
class TMTermFileManager : public BaseFileSystemManager
{
public:
    /**
     * @brief Constructor
     * @param settings Application settings
     */
    explicit TMTermFileManager(QSettings* settings);

    /**
     * @brief Get the base path for TM TERM
     * @return The base path
     */
    QString getBasePath() const override;

    /**
     * @brief Get the path to the DATA directory
     * @return The DATA directory path
     */
    QString getDataPath() const;

    /**
     * @brief Get the path to the ARCHIVE directory
     * @return The ARCHIVE directory path
     */
    QString getArchivePath() const;

    /**
     * @brief Get the path to the scripts directory
     * @return The scripts directory path
     */
    QString getScriptsPath() const;

    /**
     * @brief Get the path to a specific job folder in ARCHIVE
     * @param year Year for the job (YYYY format)
     * @param month Month for the job (MM format)
     * @return The job folder path
     */
    QString getJobFolderPath(const QString& year, const QString& month) const;

    /**
     * @brief Get the path to a specific script file
     * @param scriptName Name of the script file (without extension)
     * @return The script file path
     */
    QString getScriptPath(const QString& scriptName) const;

    /**
     * @brief Create base directories for TM TERM
     * @return True if all directories were created successfully
     */
    bool createBaseDirectories();

    /**
     * @brief Create a job folder for a specific year and month in ARCHIVE
     * @param year Year for the job (YYYY format)
     * @param month Month for the job (MM format)
     * @return True if the folder was created successfully
     */
    bool createJobFolder(const QString& year, const QString& month);

    /**
     * @brief Open the DATA folder in Windows Explorer
     * @return True if the folder was opened successfully
     */
    bool openDataFolder() const;

    /**
     * @brief Open a specific archive folder in Windows Explorer
     * @param year Year for the job (YYYY format)
     * @param month Month for the job (MM format)
     * @return True if the folder was opened successfully
     */
    bool openArchiveFolder(const QString& year, const QString& month) const;

    /**
     * @brief Get access to the settings object
     * @return Pointer to the QSettings object
     */
    QSettings* getSettings() const { return m_settings; }

    /**
     * @brief Clean the DATA folder (remove all files)
     * @return True if cleaning was successful
     */
    bool cleanDataFolder() const;

    /**
     * @brief Move files from DATA to ARCHIVE folder
     * @param year Year for the job (YYYY format)
     * @param month Month for the job (MM format)
     * @return True if files were moved successfully
     */
    bool moveFilesToArchive(const QString& year, const QString& month) const;

private:
    // Map of script names to file paths
    QMap<QString, QString> m_scriptPaths;

    /**
     * @brief Initialize script paths map
     */
    void initializeScriptPaths();
};

#endif // TMTERMFILEMANAGER_H
