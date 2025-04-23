#ifndef FILESYSTEMMANAGER_H
#define FILESYSTEMMANAGER_H

#include <QSettings>
#include <QMap>
#include <QStringList>
#include <QDir>

class FileSystemManager
{
public:
    FileSystemManager(QSettings* settings);

    // Job folder operations
    bool createJobFolders(const QString& year, const QString& month, const QString& week);
    bool copyFilesFromHomeToWorking(const QString& month, const QString& week);
    bool moveFilesToHomeFolders(const QString& month, const QString& week);

    // File checking
    bool checkProofFiles(const QString& jobType, QStringList& missingFiles);
    bool checkPrintFiles(const QString& jobType, QStringList& missingFiles);

    // Path getters
    QString getBasePath() const;
    QString getIZPath() const;
    QString getProofFolderPath(const QString& jobType) const;
    QString getPrintFolderPath(const QString& jobType) const;

    // File maps for proof and print files
    const QMap<QString, QStringList>& getProofFiles() const;
    const QMap<QString, QStringList>& getPrintFiles() const;

private:
    QSettings* settings;
    QMap<QString, QStringList> proofFiles;
    QMap<QString, QStringList> printFiles;

    void initializeFileMaps();
};

#endif // FILESYSTEMMANAGER_H
