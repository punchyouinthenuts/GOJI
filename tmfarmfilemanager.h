#ifndef TMFARMFILEMANAGER_H
#define TMFARMFILEMANAGER_H

#include "basefilesystemmanager.h"
#include <QMap>
#include <QSettings>

/**
 * @brief File system manager for TM FARMWORKERS
 *
 * Quarter-based jobs (1ST/2ND/3RD/4TH). Paths, logs, and docs are FARMWORKERS-only.
 * Script mapping uses the real scripts:
 *   C:\Goji\scripts\TRACHMAR\FARMWORKERS\01 INITIAL.py
 *   C:\Goji\scripts\TRACHMAR\FARMWORKERS\02 POST PROCESS.py
 * 
 * Job folder naming: jobNumber_yearquarter (e.g., 12345_20253RD)
 * HOME folder = ARCHIVE folder for consistency with other TRACHMAR modules
 */
class TMFarmFileManager : public BaseFileSystemManager
{
public:
    explicit TMFarmFileManager(QSettings* settings);

    /** @return Base path, default: C:/Goji/TRACHMAR/FARMWORKERS */
    QString getBasePath() const override;

    /** @return DATA path, default: <Base>/DATA */
    QString getDataPath() const;

    /** @return ARCHIVE path (HOME), default: <Base>/ARCHIVE */
    QString getArchivePath() const;

    /** @return Scripts path, default: C:/Goji/scripts/TRACHMAR/FARMWORKERS */
    QString getScriptsPath() const;

    /** 
     * @return ARCHIVE/jobNumber_yearquarter (e.g., ARCHIVE/12345_20253RD) 
     * @param jobNumber 5-digit job number
     * @param year 4-digit year (e.g., "2025")
     * @param quarterCode Quarter code: "1ST", "2ND", "3RD", or "4TH"
     */
    QString getJobFolderPath(const QString& jobNumber, const QString& year, const QString& quarterCode) const;

    /** Deprecated: placeholder-only; prefer the overload with job number. */
    QString getJobFolderPath(const QString& year, const QString& quarterCode) const;

    /** @return Full path to the requested script (.py) by friendly key */
    QString getScriptPath(const QString& scriptName) const;

    /** Create base directories if missing */
    bool createBaseDirectories();

    /** Create ARCHIVE/jobNumber_yearquarter */
    bool createJobFolder(const QString& jobNumber, const QString& year, const QString& quarterCode);

    /** Open DATA in Explorer */
    bool openDataFolder() const;

    /** Open ARCHIVE/jobNumber_yearquarter in Explorer (falls back to ARCHIVE if missing) */
    bool openArchiveFolder(const QString& jobNumber, const QString& year, const QString& quarterCode) const;

    /** Remove all files from DATA */
    bool cleanDataFolder() const;

    /** Move all files from DATA → ARCHIVE/jobNumber_yearquarter */
    bool moveFilesToArchive(const QString& jobNumber, const QString& year, const QString& quarterCode);

    /** Copy all files from ARCHIVE/jobNumber_yearquarter → DATA (for job reopening) */
    bool copyFilesFromArchive(const QString& jobNumber, const QString& year, const QString& quarterCode);

    /** Expose settings if needed */
    QSettings* getSettings() const { return m_settings; }

private:
    QMap<QString, QString> m_scriptPaths;
    void initializeScriptPaths();
};

#endif // TMFARMFILEMANAGER_H
