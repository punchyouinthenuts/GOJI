#ifndef TMWEEKLYPCFILEMANAGER_H
#define TMWEEKLYPCFILEMANAGER_H

#include "basefilesystemmanager.h"
#include <QMap>

/**
 * @brief File system manager for TM WEEKLY PC tab
 *
 * This class implements file system operations specific to the TM WEEKLY PC tab,
 * including directory structures, file paths, and specialized operations.
 */
class TMWeeklyPCFileManager : public BaseFileSystemManager
{
public:
    /**
     * @brief Constructor
     * @param settings Application settings
     */
    explicit TMWeeklyPCFileManager(QSettings* settings);

    /**
     * @brief Get the base path for TM WEEKLY PC
     * @return The base path
     */
    QString getBasePath() const override;

    /**
     * @brief Get the path to the JOB input directory
     * @return The input directory path
     */
    QString getInputPath() const;

    /**
     * @brief Get the path to the JOB output directory
     * @return The output directory path
     */
    QString getOutputPath() const;

    /**
     * @brief Get the path to the JOB proof directory
     * @return The proof directory path
     */
    QString getProofPath() const;

    /**
     * @brief Get the path to the JOB print directory
     * @return The print directory path
     */
    QString getPrintPath() const;

    /**
     * @brief Get the path to the ART directory
     * @return The ART directory path
     */
    QString getArtPath() const;

    /**
     * @brief Get the path to the scripts directory
     * @return The scripts directory path
     */
    QString getScriptsPath() const;

    /**
     * @brief Get the path to a specific job folder
     * @param month Month for the job (MM format)
     * @param week Week for the job (D format)
     * @return The job folder path
     */
    QString getJobFolderPath(const QString& month, const QString& week) const;

    /**
     * @brief Get the path to a specific script file
     * @param scriptName Name of the script file
     * @return The script file path
     */
    QString getScriptPath(const QString& scriptName) const;

    /**
     * @brief Get the path to a proof file in the ART directory
     * @param variant File variant (e.g., "SORTED" or "UNSORTED")
     * @return The proof file path
     */
    QString getProofFilePath(const QString& variant) const;

    /**
     * @brief Get the path to a print file in the ART directory
     * @param variant File variant (e.g., "SORTED" or "UNSORTED")
     * @return The print file path
     */
    QString getPrintFilePath(const QString& variant) const;

    /**
     * @brief Create base directories for TM WEEKLY PC
     * @return True if all directories were created successfully
     */
    bool createBaseDirectories();

    /**
     * @brief Create a job folder for a specific month and week
     * @param month Month for the job (MM format)
     * @param week Week for the job (D format)
     * @return True if the folder was created successfully
     */
    bool createJobFolder(const QString& month, const QString& week);

    /**
     * @brief Open a proof file with the specified variant
     * @param variant File variant (e.g., "SORTED" or "UNSORTED")
     * @return True if the file was opened successfully
     */
    bool openProofFile(const QString& variant) const;

    /**
     * @brief Open a print file with the specified variant
     * @param variant File variant (e.g., "SORTED" or "UNSORTED")
     * @return True if the file was opened successfully
     */
    bool openPrintFile(const QString& variant) const;

    /**
     * @brief Get access to the settings object
     * @return Pointer to the QSettings object
     */
    QSettings* getSettings() const { return m_settings; }

private:
    // Map of script names to file paths
    QMap<QString, QString> m_scriptPaths;

    /**
     * @brief Initialize script paths map
     */
    void initializeScriptPaths();
};

#endif // TMWEEKLYPCFILEMANAGER_H
