#ifndef TMMAFILEMANAGER_H
#define TMMAFILEMANAGER_H

#include "basefilesystemmanager.h"

#include <QFileInfo>

struct TMMAIdentityMigrationRecord
{
    QString oldJobNumber;
    QString oldYear;
    QString oldMonth;
    QString newJobNumber;
    QString newYear;
    QString newMonth;
    QString oldArchivePath;
    QString newArchivePath;
};

enum class TMMAIdentityRecoveryOutcome
{
    OriginalIdentity,
    NewIdentity,
    Blocked
};

class TMMAFileManager : public BaseFileSystemManager
{
public:
    explicit TMMAFileManager(QSettings* settings, const QString& basePathOverride = QString());

    QString getBasePath() const override;
    QString getDataPath() const;
    QString getRawInputPath() const;
    QString getArchivePath() const;
    QString getScriptsPath() const;
    QString getScriptPath(const QString& scriptName) const;

    QString getArchiveJobPath(const QString& jobNumber,
                              const QString& year,
                              const QString& month) const;
    QString getArchiveDataPath(const QString& jobNumber,
                               const QString& year,
                               const QString& month) const;
    QString getArchiveRawInputPath(const QString& jobNumber,
                                   const QString& year,
                                   const QString& month) const;

    bool createBaseDirectories(QString* errorMessage = nullptr) const;
    bool ensureArchiveDirectories(const QString& jobNumber,
                                  const QString& year,
                                  const QString& month,
                                  QString* errorMessage = nullptr) const;
    bool workingDirectoriesAreEmpty(QString* errorMessage = nullptr) const;
    bool copyArchiveToWorking(const QString& jobNumber,
                              const QString& year,
                              const QString& month,
                              QString* errorMessage = nullptr) const;
    bool moveWorkingToArchive(const QString& jobNumber,
                              const QString& year,
                              const QString& month,
                              QString* errorMessage = nullptr) const;

    bool archiveJobDirectoryExists(const QString& jobNumber,
                                   const QString& year,
                                   const QString& month) const;
    bool createNewArchiveSkeleton(const QString& jobNumber,
                                  const QString& year,
                                  const QString& month,
                                  QString* errorMessage = nullptr) const;
    bool removeArchiveJobDirectoryIfEmpty(const QString& jobNumber,
                                          const QString& year,
                                          const QString& month,
                                          QString* errorMessage = nullptr) const;
    bool preflightIdentityMigration(const TMMAIdentityMigrationRecord& record,
                                    QString* errorMessage = nullptr) const;
    bool migrateArchiveIdentity(const TMMAIdentityMigrationRecord& record,
                                QString* errorMessage = nullptr) const;
    bool rollbackArchiveIdentity(const TMMAIdentityMigrationRecord& record,
                                 QString* errorMessage = nullptr) const;

    QString identityMigrationMarkerPath() const;
    bool identityMigrationMarkerExists() const;
    bool writeIdentityMigrationMarker(const TMMAIdentityMigrationRecord& record,
                                      QString* errorMessage = nullptr) const;
    bool readIdentityMigrationMarker(TMMAIdentityMigrationRecord& record,
                                     QString* errorMessage = nullptr) const;
    bool clearIdentityMigrationMarker(QString* errorMessage = nullptr) const;
    bool recoverIdentityMigration(const TMMAIdentityMigrationRecord& record,
                                  bool oldJobExists,
                                  bool newJobExists,
                                  bool oldLogExists,
                                  bool newLogExists,
                                  TMMAIdentityRecoveryOutcome& outcome,
                                  QString* errorMessage = nullptr) const;

    static QString monthAbbreviation(const QString& month);

private:
    static bool containsDirectories(const QString& path, QString* directoryName = nullptr);
    static QList<QFileInfo> filesIn(const QString& path);
    static bool ensureDirectory(const QString& path, QString* errorMessage);
    static bool filesHaveSameContents(const QString& firstPath, const QString& secondPath);
    static bool directoryTreeContainsFiles(const QString& path);
    bool validateMigrationRecord(const TMMAIdentityMigrationRecord& record,
                                 QString* errorMessage) const;

    QString m_basePathOverride;
};

#endif // TMMAFILEMANAGER_H
