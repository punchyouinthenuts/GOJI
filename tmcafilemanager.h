#ifndef TMCAFILEMANAGER_H
#define TMCAFILEMANAGER_H

#include "basefilesystemmanager.h"
#include <QMap>

/**
 * @brief File system manager for TM CA (CA EDR/BA) tab
 *
 * This class implements file system operations specific to the TM CA tab,
 * including directory structures and file paths.
 *
 * Base path: C:/Goji/TRACHMAR/CA
 * Required minimum dirs:
 *   DATA, ARCHIVE, DROP, BA/INPUT, EDR/INPUT
 */
class TMCAFileManager : public BaseFileSystemManager
{
public:
    /**
     * @brief Constructor
     * @param settings Application settings
     */
    explicit TMCAFileManager(QSettings* settings);

    /**
     * @brief Get the base path for TM CA
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
     * @brief Get the path to the DROP directory
     * @return The DROP directory path
     */
    QString getDropPath() const;

    /**
     * @brief Get the path to the BA INPUT directory
     * @return The BA INPUT directory path
     */
    QString getBAInputPath() const;

    /**
     * @brief Get the path to the EDR INPUT directory
     * @return The EDR INPUT directory path
     */
    QString getEDRInputPath() const;

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
     * @brief Create base directories for TM CA
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
     * @brief Open the DROP folder in Windows Explorer
     * @return True if the folder was opened successfully
     */
    bool openDropFolder() const;

    /**
     * @brief Open the BA/INPUT folder in Windows Explorer
     * @return True if the folder was opened successfully
     */
    bool openBAInputFolder() const;

    /**
     * @brief Open the EDR/INPUT folder in Windows Explorer
     * @return True if the folder was opened successfully
     */
    bool openEDRInputFolder() const;

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

#endif // TMCAFILEMANAGER_H
