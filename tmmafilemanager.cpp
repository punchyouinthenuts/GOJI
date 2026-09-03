#include "tmmafilemanager.h"

#include "fileutils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSaveFile>

TMMAFileManager::TMMAFileManager(QSettings* settings, const QString& basePathOverride)
    : BaseFileSystemManager(settings)
    , m_basePathOverride(basePathOverride)
{
}

QString TMMAFileManager::getBasePath() const
{
    if (!m_basePathOverride.isEmpty()) {
        return QDir::cleanPath(m_basePathOverride);
    }
    return QDir::cleanPath(FileUtils::resolveTrachmarBasePath(m_settings, QStringLiteral("TM MA"))
                           + QStringLiteral("/MA"));
}

QString TMMAFileManager::getDataPath() const
{
    return getBasePath() + QStringLiteral("/DATA");
}

QString TMMAFileManager::getRawInputPath() const
{
    return getBasePath() + QStringLiteral("/RAW INPUT");
}

QString TMMAFileManager::getArchivePath() const
{
    return getBasePath() + QStringLiteral("/ARCHIVE");
}

QString TMMAFileManager::getScriptsPath() const
{
    return QStringLiteral("C:/Goji/scripts/TRACHMAR/MA");
}

QString TMMAFileManager::getScriptPath(const QString& scriptName) const
{
    return getScriptsPath() + QLatin1Char('/') + scriptName + QStringLiteral(".py");
}

QString TMMAFileManager::monthAbbreviation(const QString& month)
{
    static const QMap<QString, QString> months = {
        {QStringLiteral("01"), QStringLiteral("JAN")},
        {QStringLiteral("02"), QStringLiteral("FEB")},
        {QStringLiteral("03"), QStringLiteral("MAR")},
        {QStringLiteral("04"), QStringLiteral("APR")},
        {QStringLiteral("05"), QStringLiteral("MAY")},
        {QStringLiteral("06"), QStringLiteral("JUN")},
        {QStringLiteral("07"), QStringLiteral("JUL")},
        {QStringLiteral("08"), QStringLiteral("AUG")},
        {QStringLiteral("09"), QStringLiteral("SEP")},
        {QStringLiteral("10"), QStringLiteral("OCT")},
        {QStringLiteral("11"), QStringLiteral("NOV")},
        {QStringLiteral("12"), QStringLiteral("DEC")}
    };
    return months.value(month);
}

QString TMMAFileManager::getArchiveJobPath(const QString& jobNumber,
                                           const QString& year,
                                           const QString& month) const
{
    return getArchivePath() + QLatin1Char('/') + year + QLatin1Char('/')
        + monthAbbreviation(month) + QLatin1Char(' ') + jobNumber;
}

QString TMMAFileManager::getArchiveDataPath(const QString& jobNumber,
                                            const QString& year,
                                            const QString& month) const
{
    return getArchiveJobPath(jobNumber, year, month) + QStringLiteral("/DATA");
}

QString TMMAFileManager::getArchiveRawInputPath(const QString& jobNumber,
                                                const QString& year,
                                                const QString& month) const
{
    return getArchiveJobPath(jobNumber, year, month) + QStringLiteral("/RAW INPUT");
}

bool TMMAFileManager::ensureDirectory(const QString& path, QString* errorMessage)
{
    QDir dir(path);
    if (dir.exists() || QDir().mkpath(path)) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("Could not create directory: %1").arg(QDir::toNativeSeparators(path));
    }
    return false;
}

bool TMMAFileManager::createBaseDirectories(QString* errorMessage) const
{
    return ensureDirectory(getDataPath(), errorMessage)
        && ensureDirectory(getRawInputPath(), errorMessage)
        && ensureDirectory(getArchivePath(), errorMessage);
}

bool TMMAFileManager::ensureArchiveDirectories(const QString& jobNumber,
                                               const QString& year,
                                               const QString& month,
                                               QString* errorMessage) const
{
    if (monthAbbreviation(month).isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot create TMMA archive directories: invalid month '%1'.").arg(month);
        }
        return false;
    }

    return createBaseDirectories(errorMessage)
        && ensureDirectory(getArchivePath() + QLatin1Char('/') + year, errorMessage)
        && ensureDirectory(getArchiveJobPath(jobNumber, year, month), errorMessage)
        && ensureDirectory(getArchiveDataPath(jobNumber, year, month), errorMessage)
        && ensureDirectory(getArchiveRawInputPath(jobNumber, year, month), errorMessage);
}

QList<QFileInfo> TMMAFileManager::filesIn(const QString& path)
{
    return QDir(path).entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
}

bool TMMAFileManager::containsDirectories(const QString& path, QString* directoryName)
{
    const QFileInfoList directories = QDir(path).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    if (directories.isEmpty()) {
        return false;
    }
    if (directoryName) {
        *directoryName = directories.constFirst().fileName();
    }
    return true;
}

bool TMMAFileManager::filesHaveSameContents(const QString& firstPath, const QString& secondPath)
{
    QFile first(firstPath);
    QFile second(secondPath);
    if (first.size() != second.size()
        || !first.open(QIODevice::ReadOnly)
        || !second.open(QIODevice::ReadOnly)) {
        return false;
    }

    constexpr qint64 chunkSize = 1024 * 1024;
    while (!first.atEnd() || !second.atEnd()) {
        if (first.read(chunkSize) != second.read(chunkSize)) {
            return false;
        }
    }
    return true;
}

bool TMMAFileManager::workingDirectoriesAreEmpty(QString* errorMessage) const
{
    const QStringList paths = {getDataPath(), getRawInputPath()};
    for (const QString& path : paths) {
        const QFileInfoList entries = QDir(path).entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
        if (!entries.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                    "Unsafe TMMA working-directory contamination: %1 contains '%2'. "
                    "Remove or archive unrelated working files before reopening a job.")
                    .arg(QDir::toNativeSeparators(path), entries.constFirst().fileName());
            }
            return false;
        }
    }
    return true;
}

bool TMMAFileManager::copyArchiveToWorking(const QString& jobNumber,
                                           const QString& year,
                                           const QString& month,
                                           QString* errorMessage) const
{
    if (!createBaseDirectories(errorMessage) || !workingDirectoriesAreEmpty(errorMessage)) {
        return false;
    }

    const QList<QPair<QString, QString>> pairs = {
        {getArchiveDataPath(jobNumber, year, month), getDataPath()},
        {getArchiveRawInputPath(jobNumber, year, month), getRawInputPath()}
    };

    for (const auto& pair : pairs) {
        QString childDirectory;
        if (QDir(pair.first).exists() && containsDirectories(pair.first, &childDirectory)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("TMMA archive restore cannot safely copy nested directory '%1' from %2.")
                                    .arg(childDirectory, QDir::toNativeSeparators(pair.first));
            }
            return false;
        }
        for (const QFileInfo& source : filesIn(pair.first)) {
            const QString destination = QDir(pair.second).filePath(source.fileName());
            if (QFileInfo::exists(destination)) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("TMMA archive restore collision: %1 already exists.")
                                        .arg(QDir::toNativeSeparators(destination));
                }
                return false;
            }
        }
    }

    QStringList createdFiles;
    for (const auto& pair : pairs) {
        for (const QFileInfo& source : filesIn(pair.first)) {
            const QString destination = QDir(pair.second).filePath(source.fileName());
            if (!QFile::copy(source.absoluteFilePath(), destination)
                || QFileInfo(destination).size() != source.size()) {
                for (const QString& created : createdFiles) {
                    QFile::remove(created);
                }
                QFile::remove(destination);
                if (errorMessage) {
                    *errorMessage = QStringLiteral("TMMA archive restore failed while copying %1 to %2.")
                                        .arg(QDir::toNativeSeparators(source.absoluteFilePath()),
                                             QDir::toNativeSeparators(destination));
                }
                return false;
            }
            createdFiles.append(destination);
        }
    }
    return true;
}

bool TMMAFileManager::moveWorkingToArchive(const QString& jobNumber,
                                           const QString& year,
                                           const QString& month,
                                           QString* errorMessage) const
{
    if (!ensureArchiveDirectories(jobNumber, year, month, errorMessage)) {
        return false;
    }

    const QList<QPair<QString, QString>> pairs = {
        {getDataPath(), getArchiveDataPath(jobNumber, year, month)},
        {getRawInputPath(), getArchiveRawInputPath(jobNumber, year, month)}
    };

    for (const auto& pair : pairs) {
        QString childDirectory;
        if (containsDirectories(pair.first, &childDirectory)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("TMMA close cannot safely move nested directory '%1' from %2.")
                                    .arg(childDirectory, QDir::toNativeSeparators(pair.first));
            }
            return false;
        }
        for (const QFileInfo& source : filesIn(pair.first)) {
            const QString destination = QDir(pair.second).filePath(source.fileName());
            if (QFileInfo::exists(destination)) {
                if (filesHaveSameContents(source.absoluteFilePath(), destination)) {
                    continue;
                }
                if (errorMessage) {
                    *errorMessage = QStringLiteral(
                        "TMMA archive collision: %1 already exists with different contents. "
                        "No working files were overwritten.")
                        .arg(QDir::toNativeSeparators(destination));
                }
                return false;
            }
        }
    }

    for (const auto& pair : pairs) {
        for (const QFileInfo& source : filesIn(pair.first)) {
            const QString destination = QDir(pair.second).filePath(source.fileName());
            if (QFileInfo::exists(destination)) {
                // Reopened jobs contain verified copies of their retained archive
                // snapshot. Removing an identical working duplicate completes the
                // move without overwriting or altering the archive original.
                if (!filesHaveSameContents(source.absoluteFilePath(), destination)
                    || !QFile::remove(source.absoluteFilePath())) {
                    if (errorMessage) {
                        *errorMessage = QStringLiteral(
                            "TMMA could not remove verified working duplicate %1; both copies were retained.")
                            .arg(QDir::toNativeSeparators(source.absoluteFilePath()));
                    }
                    return false;
                }
                continue;
            }
            if (QFile::rename(source.absoluteFilePath(), destination)) {
                continue;
            }

            if (!QFile::copy(source.absoluteFilePath(), destination)
                || QFileInfo(destination).size() != source.size()) {
                QFile::remove(destination);
                if (errorMessage) {
                    *errorMessage = QStringLiteral("TMMA archive move failed for %1; the source remains recoverable.")
                                        .arg(QDir::toNativeSeparators(source.absoluteFilePath()));
                }
                return false;
            }
            if (!QFile::remove(source.absoluteFilePath())) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral(
                        "TMMA archived a verified copy of %1 but could not remove the working source. "
                        "Both copies were retained for recovery.")
                        .arg(QDir::toNativeSeparators(source.absoluteFilePath()));
                }
                return false;
            }
        }
    }
    return true;
}

bool TMMAFileManager::archiveJobDirectoryExists(const QString& jobNumber,
                                                const QString& year,
                                                const QString& month) const
{
    return QDir(getArchiveJobPath(jobNumber, year, month)).exists();
}

bool TMMAFileManager::directoryTreeContainsFiles(const QString& path)
{
    QDirIterator iterator(path, QDir::Files | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    return iterator.hasNext();
}

bool TMMAFileManager::createNewArchiveSkeleton(const QString& jobNumber,
                                               const QString& year,
                                               const QString& month,
                                               QString* errorMessage) const
{
    const QString jobPath = getArchiveJobPath(jobNumber, year, month);
    if (QFileInfo::exists(jobPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("TMMA archive identity already exists: %1. No data was changed.")
                                .arg(QDir::toNativeSeparators(jobPath));
        }
        return false;
    }

    if (!createBaseDirectories(errorMessage)
        || !ensureDirectory(getArchivePath() + QLatin1Char('/') + year, errorMessage)
        || !ensureDirectory(getArchiveDataPath(jobNumber, year, month), errorMessage)
        || !ensureDirectory(getArchiveRawInputPath(jobNumber, year, month), errorMessage)) {
        QString cleanupError;
        removeArchiveJobDirectoryIfEmpty(jobNumber, year, month, &cleanupError);
        if (errorMessage && !cleanupError.isEmpty()) {
            *errorMessage += QStringLiteral(" Cleanup also failed: %1").arg(cleanupError);
        }
        return false;
    }
    return true;
}

bool TMMAFileManager::removeArchiveJobDirectoryIfEmpty(const QString& jobNumber,
                                                       const QString& year,
                                                       const QString& month,
                                                       QString* errorMessage) const
{
    const QString jobPath = getArchiveJobPath(jobNumber, year, month);
    const QFileInfo jobInfo(jobPath);
    if (jobInfo.exists() && !jobInfo.isDir()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Refusing to remove non-directory TMMA archive identity: %1")
                                .arg(QDir::toNativeSeparators(jobPath));
        }
        return false;
    }
    QDir jobDirectory(jobPath);
    if (!jobDirectory.exists()) {
        return true;
    }
    if (directoryTreeContainsFiles(jobPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Refusing to remove non-empty TMMA archive directory: %1")
                                .arg(QDir::toNativeSeparators(jobPath));
        }
        return false;
    }
    if (!jobDirectory.removeRecursively()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not remove incomplete TMMA archive directory: %1")
                                .arg(QDir::toNativeSeparators(jobPath));
        }
        return false;
    }
    return true;
}

bool TMMAFileManager::validateMigrationRecord(const TMMAIdentityMigrationRecord& record,
                                              QString* errorMessage) const
{
    const QString expectedOld = QDir::cleanPath(getArchiveJobPath(
        record.oldJobNumber, record.oldYear, record.oldMonth));
    const QString expectedNew = QDir::cleanPath(getArchiveJobPath(
        record.newJobNumber, record.newYear, record.newMonth));
    if (record.oldJobNumber.isEmpty() || record.oldYear.isEmpty() || record.oldMonth.isEmpty()
        || record.newJobNumber.isEmpty() || record.newYear.isEmpty() || record.newMonth.isEmpty()
        || monthAbbreviation(record.oldMonth).isEmpty()
        || monthAbbreviation(record.newMonth).isEmpty()
        || QDir::cleanPath(record.oldArchivePath) != expectedOld
        || QDir::cleanPath(record.newArchivePath) != expectedNew
        || expectedOld == expectedNew) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("TMMA identity migration marker is invalid or does not match the configured archive root.");
        }
        return false;
    }
    return true;
}

bool TMMAFileManager::preflightIdentityMigration(const TMMAIdentityMigrationRecord& record,
                                                 QString* errorMessage) const
{
    if (!validateMigrationRecord(record, errorMessage)) {
        return false;
    }
    if (!QFileInfo(record.oldArchivePath).isDir()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Expected TMMA archive directory is missing: %1. No data was changed.")
                                .arg(QDir::toNativeSeparators(record.oldArchivePath));
        }
        return false;
    }
    if (QFileInfo::exists(record.newArchivePath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("TMMA archive target already exists: %1. No data was changed.")
                                .arg(QDir::toNativeSeparators(record.newArchivePath));
        }
        return false;
    }
    return true;
}

bool TMMAFileManager::migrateArchiveIdentity(const TMMAIdentityMigrationRecord& record,
                                             QString* errorMessage) const
{
    if (!preflightIdentityMigration(record, errorMessage)) {
        return false;
    }
    if (!ensureDirectory(QFileInfo(record.newArchivePath).absolutePath(), errorMessage)) {
        return false;
    }
    if (!QDir().rename(record.oldArchivePath, record.newArchivePath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not migrate TMMA archive identity from %1 to %2. No data was changed.")
                                .arg(QDir::toNativeSeparators(record.oldArchivePath),
                                     QDir::toNativeSeparators(record.newArchivePath));
        }
        return false;
    }
    return true;
}

bool TMMAFileManager::rollbackArchiveIdentity(const TMMAIdentityMigrationRecord& record,
                                              QString* errorMessage) const
{
    if (!validateMigrationRecord(record, errorMessage)) {
        return false;
    }
    if (QDir(record.oldArchivePath).exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot roll back TMMA archive identity because the old path already exists: %1")
                                .arg(QDir::toNativeSeparators(record.oldArchivePath));
        }
        return false;
    }
    if (!QDir(record.newArchivePath).exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot roll back TMMA archive identity because the migrated path is missing: %1")
                                .arg(QDir::toNativeSeparators(record.newArchivePath));
        }
        return false;
    }
    if (!ensureDirectory(QFileInfo(record.oldArchivePath).absolutePath(), errorMessage)) {
        return false;
    }
    if (!QDir().rename(record.newArchivePath, record.oldArchivePath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not roll back TMMA archive identity from %1 to %2.")
                                .arg(QDir::toNativeSeparators(record.newArchivePath),
                                     QDir::toNativeSeparators(record.oldArchivePath));
        }
        return false;
    }
    return true;
}

QString TMMAFileManager::identityMigrationMarkerPath() const
{
    return getArchivePath() + QStringLiteral("/.tmma_identity_migration.json");
}

bool TMMAFileManager::identityMigrationMarkerExists() const
{
    return QFileInfo::exists(identityMigrationMarkerPath());
}

bool TMMAFileManager::writeIdentityMigrationMarker(const TMMAIdentityMigrationRecord& record,
                                                   QString* errorMessage) const
{
    if (!validateMigrationRecord(record, errorMessage)
        || !ensureDirectory(getArchivePath(), errorMessage)) {
        return false;
    }

    QJsonObject object;
    object.insert(QStringLiteral("version"), 1);
    object.insert(QStringLiteral("old_job_number"), record.oldJobNumber);
    object.insert(QStringLiteral("old_year"), record.oldYear);
    object.insert(QStringLiteral("old_month"), record.oldMonth);
    object.insert(QStringLiteral("new_job_number"), record.newJobNumber);
    object.insert(QStringLiteral("new_year"), record.newYear);
    object.insert(QStringLiteral("new_month"), record.newMonth);
    object.insert(QStringLiteral("old_archive_path"), QDir::cleanPath(record.oldArchivePath));
    object.insert(QStringLiteral("new_archive_path"), QDir::cleanPath(record.newArchivePath));

    QSaveFile marker(identityMigrationMarkerPath());
    if (!marker.open(QIODevice::WriteOnly)
        || marker.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) < 0
        || !marker.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not write durable TMMA identity recovery marker: %1")
                                .arg(QDir::toNativeSeparators(identityMigrationMarkerPath()));
        }
        return false;
    }
    return true;
}

bool TMMAFileManager::readIdentityMigrationMarker(TMMAIdentityMigrationRecord& record,
                                                  QString* errorMessage) const
{
    QFile marker(identityMigrationMarkerPath());
    if (!marker.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not read TMMA identity recovery marker: %1")
                                .arg(QDir::toNativeSeparators(identityMigrationMarkerPath()));
        }
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(marker.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("TMMA identity recovery marker is not valid JSON: %1")
                                .arg(parseError.errorString());
        }
        return false;
    }
    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("version")).toInt(-1) != 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("TMMA identity recovery marker has an unsupported version.");
        }
        return false;
    }
    record.oldJobNumber = object.value(QStringLiteral("old_job_number")).toString();
    record.oldYear = object.value(QStringLiteral("old_year")).toString();
    record.oldMonth = object.value(QStringLiteral("old_month")).toString();
    record.newJobNumber = object.value(QStringLiteral("new_job_number")).toString();
    record.newYear = object.value(QStringLiteral("new_year")).toString();
    record.newMonth = object.value(QStringLiteral("new_month")).toString();
    record.oldArchivePath = object.value(QStringLiteral("old_archive_path")).toString();
    record.newArchivePath = object.value(QStringLiteral("new_archive_path")).toString();
    return validateMigrationRecord(record, errorMessage);
}

bool TMMAFileManager::clearIdentityMigrationMarker(QString* errorMessage) const
{
    const QString markerPath = identityMigrationMarkerPath();
    if (!QFileInfo::exists(markerPath) || QFile::remove(markerPath)) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("Could not clear TMMA identity recovery marker: %1")
                            .arg(QDir::toNativeSeparators(markerPath));
    }
    return false;
}

bool TMMAFileManager::recoverIdentityMigration(const TMMAIdentityMigrationRecord& record,
                                               bool oldJobExists,
                                               bool newJobExists,
                                               bool oldLogExists,
                                               bool newLogExists,
                                               TMMAIdentityRecoveryOutcome& outcome,
                                               QString* errorMessage) const
{
    outcome = TMMAIdentityRecoveryOutcome::Blocked;
    if (!validateMigrationRecord(record, errorMessage)) {
        return false;
    }

    const bool oldArchiveExists = QDir(record.oldArchivePath).exists();
    const bool newArchiveExists = QDir(record.newArchivePath).exists();

    if (oldJobExists && !newJobExists) {
        if (!oldArchiveExists && newArchiveExists) {
            if (!rollbackArchiveIdentity(record, errorMessage)) {
                return false;
            }
        } else if (!oldArchiveExists || newArchiveExists) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("TMMA recovery is ambiguous: the database has the old identity, but the archive paths are not in a recoverable old-or-migrated state.");
            }
            return false;
        }
        if (!clearIdentityMigrationMarker(errorMessage)) {
            return false;
        }
        outcome = TMMAIdentityRecoveryOutcome::OriginalIdentity;
        return true;
    }

    if (!oldJobExists && newJobExists && !oldLogExists) {
        if (oldArchiveExists && !newArchiveExists) {
            if (!migrateArchiveIdentity(record, errorMessage)) {
                return false;
            }
        } else if (oldArchiveExists || !newArchiveExists) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("TMMA recovery is ambiguous: the database has the new identity, but the archive paths are not in a recoverable committed-or-old state.");
            }
            return false;
        }
        if (!clearIdentityMigrationMarker(errorMessage)) {
            return false;
        }
        outcome = TMMAIdentityRecoveryOutcome::NewIdentity;
        return true;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("TMMA recovery is ambiguous because the old/new database identities do not identify exactly one authoritative job.");
    }
    Q_UNUSED(newLogExists);
    return false;
}
