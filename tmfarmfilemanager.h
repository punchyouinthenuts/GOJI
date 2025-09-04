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
 */
class TMFarmFileManager : public BaseFileSystemManager
{
public:
    explicit TMFarmFileManager(QSettings* settings);

    /** @return Base path, default: C:/Goji/TRACHMAR/FARMWORKERS */
    QString getBasePath() const override;

    /** @return DATA path, default: <Base>/DATA */
    QString getDataPath() const;

    /** @return ARCHIVE path, default: <Base>/ARCHIVE */
    QString getArchivePath() const;

    /** @return Scripts path, default: C:/Goji/scripts/TRACHMAR/FARMWORKERS */
    QString getScriptsPath() const;

    /** @return ARCHIVE/<job>_<quarter><year> (e.g., 12345_3RD2025) */
    QString getJobFolderPath(const QString& jobNumber, const QString& year, const QString& quarterCode) const;

    /** Deprecated: placeholder-only; prefer the overload with job number. */
    QString getJobFolderPath(const QString& year, const QString& quarterCode) const;

    /** @return Full path to the requested script (.py) by friendly key */
    QString getScriptPath(const QString& scriptName) const;

    /** Create base directories if missing */
    bool createBaseDirectories();

    /** Create ARCHIVE/<job>_<quarter><year> */
    bool createJobFolder(const QString& jobNumber, const QString& year, const QString& quarterCode);

    /** Open DATA in Explorer */
    bool openDataFolder() const;

    /** Open ARCHIVE/<job>_<quarter><year> in Explorer (falls back to ARCHIVE if missing) */
    bool openArchiveFolder(const QString& jobNumber, const QString& year, const QString& quarterCode) const;

    /** Remove all files from DATA */
    bool cleanDataFolder() const;

    /** Move all files from DATA → ARCHIVE/<job>_<quarter><year> */
    bool moveFilesToArchive(const QString& jobNumber, const QString& year, const QString& quarterCode);

    /** Expose settings if needed */
    QSettings* getSettings() const { return m_settings; }

private:
    QMap<QString, QString> m_scriptPaths;
    void initializeScriptPaths();
};

#endif // TMFARMFILEMANAGER_H
