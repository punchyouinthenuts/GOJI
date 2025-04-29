#include "filesystemmanager.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>

FileSystemManager::FileSystemManager(QSettings* settings)
    : settings(settings)
{
    initializeFileMaps();
}

void FileSystemManager::initializeFileMaps()
{
    // Initialize file mappings for proof files
    proofFiles = {
        {"CBC", {"/RAC/CBC/ART/CBC2 PROOF.indd", "/RAC/CBC/ART/CBC3 PROOF.indd"}},
        {"EXC", {"/RAC/EXC/ART/EXC PROOF.indd"}},
        {"INACTIVE", {"/RAC/INACTIVE/ART/A-PU PROOF.indd", "/RAC/INACTIVE/ART/FZA-PO PROOF.indd",
                      "/RAC/INACTIVE/ART/FZA-PU PROOF.indd", "/RAC/INACTIVE/ART/PR-PO PROOF.indd",
                      "/RAC/INACTIVE/ART/PR-PU PROOF.indd", "/RAC/INACTIVE/ART/A-PO PROOF.indd"}},
        {"NCWO", {"/RAC/NCWO/ART/NCWO 2-PR PROOF.indd", "/RAC/NCWO/ART/NCWO 1 extremely long line-A PROOF.indd",
                  "/RAC/NCWO/ART/NCWO 1-AP PROOF.indd", "/RAC/INACTIVE/ART/NCWO 1-APPR PROOF.indd",
                  "/RAC/NCWO/ART/NCWO 1-PR PROOF.indd", "/RAC/NCWO/ART/NCWO 2-A PROOF.indd",
                  "/RAC/NCWO/ART/NCWO 2-AP PROOF.indd", "/RAC/NCWO/ART/NCWO 2-APPR PROOF.indd"}},
        {"PREPIF", {"/RAC/PREPIF/ART/PREPIF US PROOF.indd", "/RAC/PREPIF/ART/PREPIF PR PROOF.indd"}}
    };

    // Initialize file mappings for print files
    printFiles = {
        {"CBC", {"/RAC/CBC/ART/CBC2 PRINT.indd", "/RAC/CBC/ART/CBC3 PRINT.indd"}},
        {"EXC", {"/RAC/EXC/ART/EXC PRINT.indd"}},
        {"NCWO", {"/RAC/NCWO/ART/NCWO 2-PR PRINT.indd", "/RAC/NCWO/ART/NCWO 1-A PRINT.indd",
                  "/RAC/NCWO/ART/NCWO 1-AP PRINT.indd", "/RAC/NCWO/ART/NCWO 1-APPR PRINT.indd",
                  "/RAC/NCWO/ART/NCWO 1-PR PRINT.indd", "/RAC/NCWO/ART/NCWO 2-A PRINT.indd",
                  "/RAC/NCWO/ART/NCWO 2-AP PRINT.indd", "/RAC/NCWO/ART/NCWO 2-APPR PRINT.indd"}},
        {"PREPIF", {"/RAC/PREPIF/ART/PREPIF US PRINT.indd", "/RAC/PREPIF/ART/PREPIF PR PRINT.indd"}}
    };
}

bool FileSystemManager::createJobFolders(const QString& /* year */, const QString& month, const QString& week)
{
    QString basePath = getBasePath();
    QStringList jobTypes = {"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"};
    QString homeFolder = month + "." + week;
    bool success = true;

    for (const QString& jobType : jobTypes) {
        QString fullPath = basePath + "/" + jobType + "/" + homeFolder;
        QDir dir(fullPath);
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                qDebug() << "Failed to create home folder:" << fullPath;
                success = false;
                continue;
            }

            for (const QString& subDir : QStringList{"INPUT", "OUTPUT", "PRINT", "PROOF"}) {
                QDir subDirPath(fullPath + "/" + subDir);
                if (!subDirPath.exists()) {
                    if (!subDirPath.mkdir(".")) {
                        qDebug() << "Failed to create subdirectory:" << subDirPath.path();
                        success = false;
                    }
                }
            }
        }
    }

    return success;
}

bool FileSystemManager::copyFilesFromHomeToWorking(const QString& month, const QString& week)
{
    QString basePath = getBasePath();
    QStringList jobTypes = {"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"};
    QString homeFolder = month + "." + week;
    bool success = true;

    for (const QString& jobType : jobTypes) {
        QString homeDir = basePath + "/" + jobType + "/" + homeFolder;
        QString workingDir = basePath + "/" + jobType + "/JOB";

        for (const QString& subDir : QStringList{"INPUT", "OUTPUT", "PRINT", "PROOF"}) {
            QDir homeSubDir(homeDir + "/" + subDir);
            QDir workingSubDir(workingDir + "/" + subDir);
            if (!workingSubDir.exists()) {
                if (!workingSubDir.mkpath(".")) {
                    qDebug() << "Failed to create working directory:" << workingSubDir.path();
                    success = false;
                    continue;
                }
            }

            const QStringList& files = homeSubDir.entryList(QDir::Files);
            for (const QString& file : files) {
                QString src = homeSubDir.filePath(file);
                QString dest = workingSubDir.filePath(file);
                if (QFile::exists(dest)) {
                    if (!QFile::remove(dest)) {
                        qDebug() << "Failed to remove existing file:" << dest;
                        success = false;
                        continue;
                    }
                }
                if (!QFile::copy(src, dest)) {
                    qDebug() << "Failed to copy" << src << "to" << dest;
                    success = false;
                }
            }
        }
    }

    return success;
}

bool FileSystemManager::moveFilesToHomeFolders(const QString& month, const QString& week)
{
    QString basePath = getBasePath();
    QStringList jobTypes = {"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"};
    QString homeFolder = month + "." + week;
    bool success = true;

    for (const QString& jobType : jobTypes) {
        QString homeDir = basePath + "/" + jobType + "/" + homeFolder;
        QString workingDir = basePath + "/" + jobType + "/JOB";

        for (const QString& subDir : QStringList{"INPUT", "OUTPUT", "PRINT", "PROOF"}) {
            QDir workingSubDir(workingDir + "/" + subDir);
            QDir homeSubDir(homeDir + "/" + subDir);
            if (!homeSubDir.exists()) {
                if (!homeSubDir.mkpath(".")) {
                    qDebug() << "Failed to create home subdirectory:" << homeSubDir.path();
                    success = false;
                    continue;
                }
            }

            const QStringList& files = workingSubDir.entryList(QDir::Files);
            for (const QString& file : files) {
                QString src = workingSubDir.filePath(file);
                QString dest = homeSubDir.filePath(file);
                if (QFile::exists(dest)) {
                    if (!QFile::remove(dest)) {
                        qDebug() << "Failed to remove existing file:" << dest;
                        success = false;
                        continue;
                    }
                }
                if (!QFile::rename(src, dest)) {
                    qDebug() << "Failed to move" << src << "to" << dest;
                    success = false;
                }
            }
        }
    }

    return success;
}

bool FileSystemManager::checkProofFiles(const QString& jobType, QStringList& missingFiles)
{
    missingFiles.clear();

    if (!proofFiles.contains(jobType)) {
        qDebug() << "No proof files defined for" << jobType;
        return false;
    }

    QString proofPath = getProofFolderPath(jobType);
    QDir proofDir(proofPath);
    if (!proofDir.exists()) {
        qDebug() << "Proof directory does not exist:" << proofPath;
        return false;
    }

    QStringList expectedFiles = proofFiles[jobType];
    bool allFilesPresent = true;
    for (const QString& file : expectedFiles) {
        QString fileName = QFileInfo(file).fileName();
        if (!proofDir.exists(fileName)) {
            missingFiles.append(fileName);
            allFilesPresent = false;
        }
    }

    return allFilesPresent;
}

bool FileSystemManager::checkPrintFiles(const QString& jobType, QStringList& missingFiles)
{
    missingFiles.clear();

    if (!printFiles.contains(jobType)) {
        qDebug() << "No print files defined for" << jobType;
        return false;
    }

    QString printPath = getPrintFolderPath(jobType);
    QDir printDir(printPath);
    if (!printDir.exists()) {
        qDebug() << "Print directory does not exist:" << printPath;
        return false;
    }

    QStringList expectedFiles = printFiles[jobType];
    bool allFilesPresent = true;
    for (const QString& file : expectedFiles) {
        QString fileName = QFileInfo(file).fileName();
        if (!printDir.exists(fileName)) {
            missingFiles.append(fileName);
            allFilesPresent = false;
        }
    }

    return allFilesPresent;
}

QString FileSystemManager::getBasePath() const
{
    return settings->value("BasePath", "C:/Goji/RAC").toString();
}

QString FileSystemManager::getIZPath() const
{
    return settings->value("IZPath", getBasePath() + "/WEEKLY/INPUTZIP").toString();
}

QString FileSystemManager::getProofFolderPath(const QString& jobType) const
{
    return settings->value("ProofPath", getBasePath() + "/" + jobType + "/JOB/PROOF").toString();
}

QString FileSystemManager::getPrintFolderPath(const QString& jobType) const
{
    return settings->value("PrintPath", getBasePath() + "/" + jobType + "/JOB/PRINT").toString();
}

const QMap<QString, QStringList>& FileSystemManager::getProofFiles() const
{
    return proofFiles;
}

const QMap<QString, QStringList>& FileSystemManager::getPrintFiles() const
{
    return printFiles;
}

QString FileSystemManager::getArtFolderPath(const QString& jobType) const
{
    return getBasePath() + "/" + jobType + "/ART";
}

bool FileSystemManager::openInddFiles(const QString& jobType, const QString& pattern) const
{
    QString artPath = getArtFolderPath(jobType);
    QDir dir(artPath);

    if (!dir.exists()) {
        qDebug() << "ART directory does not exist:" << artPath;
        return false;
    }

    QStringList filters;
    filters << "*" + pattern + "*.indd";
    QFileInfoList fileInfoList = dir.entryInfoList(filters, QDir::Files);

    if (fileInfoList.isEmpty()) {
        qDebug() << "No" << pattern << "INDD files found in:" << artPath;
        return false;
    }

    bool anyFileOpened = false;
    for (const QFileInfo& fileInfo : fileInfoList) {
        QString filePath = fileInfo.absoluteFilePath();
        if (QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
            qDebug() << "Opened" << pattern << "INDD file:" << filePath;
            anyFileOpened = true;
        } else {
            qDebug() << "Failed to open" << pattern << "INDD file:" << filePath;
        }
    }

    return anyFileOpened;
}
