#ifndef FILESYSTEMMANAGER_H
#define FILESYSTEMMANAGER_H

#include <QSettings>
#include <QMap>
#include <QStringList>
#include <QDir>
#include <QList>
#include <QPair>
#include <QString>

class FileSystemManager
{
public:
    FileSystemManager(QSettings* settings);
    virtual ~FileSystemManager();

    // Job folder operations
    bool createJobFolders(const QString& year, const QString& month, const QString& week);
    bool copyFilesFromHomeToWorking(const QString& month, const QString& week);
    bool moveFilesToHomeFolders(const QString& month, const QString& week);

    // File checking
    bool checkProofFiles(const QString& jobType, QStringList& missingFiles);
    bool checkPrintFiles(const QString& jobType, QStringList& missingFiles);
    bool checkInactiveCsvFiles(QStringList& missingFiles);

    // Path getters
    QString getBasePath() const;
    QString getIZPath() const;
    QString getProofFolderPath(const QString& jobType) const;
    QString getPrintFolderPath(const QString& jobType) const;

    // File maps for proof and print files
    const QMap<QString, QStringList>& getProofFiles() const;
    const QMap<QString, QStringList>& getPrintFiles() const;

    // Get the path to the ART folder for a specific job type
    QString getArtFolderPath(const QString& jobType) const;

    // Open INDD files in the ART folder that match a pattern (PROOF or PRINT)
    bool openInddFiles(const QString& jobType, const QString& pattern) const;

    // Get completed file moves
    const QList<QPair<QString, QString>>& getCompletedCopies() const;

private:
    QSettings* settings;
    QMap<QString, QStringList> proofFiles;
    QMap<QString, QStringList> printFiles;
    QList<QPair<QString, QString>> completedCopies; // Tracks moved files for rollback or reference

    void initializeFileMaps();
};

#endif // FILESYSTEMMANAGER_H
