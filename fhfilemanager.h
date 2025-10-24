#ifndef FHFILEMANAGER_H
#define FHFILEMANAGER_H

#include "basefilesystemmanager.h"
#include <QMap>

/**
 * @brief File system manager for FOUR HANDS tab
 *
 * This class implements file system operations specific to the FOUR HANDS tab,
 * including directory structures, file paths, and specialized operations.
 * Follows the same pattern as TMFLERFileManager but for FOUR HANDS directory structure.
 */
class FHFileManager : public BaseFileSystemManager
{
public:
    /**
     * @brief Constructor
     * @param settings Application settings
     */
    explicit FHFileManager(QSettings* settings);

    /**
     * @brief Get the base path for FOUR HANDS
     * @return The base path
     */
    QString getBasePath() const override;

    /**
     * @brief Get the path to the ORIGINAL directory
     * @return The ORIGINAL directory path
     */
    QString getOriginalPath() const;

    /**
     * @brief Get the path to the INPUT directory
     * @return The INPUT directory path
     */
    QString getInputPath() const;

    /**
     * @brief Get the path to the OUTPUT directory
     * @return The OUTPUT directory path
     */
    QString getOutputPath() const;

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
     * @brief Get the path to a specific job folder in ARCHIVE with job number
     * @param jobNumber Job number (5 digits)
     * @param year Year for the job (YYYY format)
     * @param month Month for the job (MM format)
     * @return The job folder path
     */
    QString getJobFolderPath(const QString& jobNumber, const QString& year, const QString& month) const;

    /**
     * @brief Get the path to a specific script file
     * @param scriptName Name of the script file (without extension)
     * @return The script file path
     */
    QString getScriptPath(const QString& scriptName) const;

    /**
     * @brief Create base directories for FOUR HANDS
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
     * @brief Open the ORIGINAL folder in Windows Explorer
     * @return True if the folder was opened successfully
     */
    bool openOriginalFolder() const;

    /**
     * @brief Open the INPUT folder in Windows Explorer
     * @return True if the folder was opened successfully
     */
    bool openInputFolder() const;

    /**
     * @brief Open the OUTPUT folder in Windows Explorer
     * @return True if the folder was opened successfully
     */
    bool openOutputFolder() const;

    /**
     * @brief Open the ARCHIVE folder in Windows Explorer
     * @return True if the folder was opened successfully
     */
    bool openArchiveFolder() const;

    /**
     * @brief Open the scripts folder in Windows Explorer
     * @return True if the folder was opened successfully
     */
    bool openScriptsFolder() const;

    /**
     * @brief Open a specific job folder in Windows Explorer
     * @param year Year for the job
     * @param month Month for the job
     * @return True if the folder was opened successfully
     */
    bool openJobFolder(const QString& year, const QString& month) const;

private:
    /**
     * @brief Map of script names to their full paths
     */
    QMap<QString, QString> m_scriptPaths;

    /**
     * @brief Initialize script paths
     */
    void initializeScriptPaths();
};

#endif // FHFILEMANAGER_H
