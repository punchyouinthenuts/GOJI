#include "filesystemmanager.h"
#include "errorhandling.h"
#include "logger.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>
#include <QThread>

FileSystemManager::FileSystemManager(QSettings* settings)
    : settings(settings)
{
    initializeFileMaps();
}

FileSystemManager::~FileSystemManager()
{
    // No resources to clean up
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

    // Print files map is no longer used for .indd files; kept for compatibility
    printFiles = {
        {"CBC", {}},
        {"EXC", {}},
        {"NCWO", {}},
        {"PREPIF", {}}
    };
}

bool FileSystemManager::createJobFolders(const QString& /* year */, const QString& month, const QString& week)
{
    QString basePath = getBasePath();
    QStringList jobTypes = {"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"};
    QString homeFolder = month + "." + week;

    for (const QString& jobType : jobTypes) {
        QString fullPath = basePath + "/" + jobType + "/" + homeFolder;
        try {
            QDir dir(fullPath);
            if (!dir.exists() && !dir.mkpath(".")) {
                THROW_FILE_ERROR("Failed to create home folder", fullPath);
            }
        } catch (const FileOperationException& e) {
            Logger::instance().error(QString("Failed to create home folder: %1 - %2").arg(e.message(), e.path()));
            return false;
        }

        for (const QString& subDir : QStringList{"INPUT", "OUTPUT", "PRINT", "PROOF"}) {
            QString subDirPath = fullPath + "/" + subDir;
            try {
                QDir dir(subDirPath);
                if (!dir.exists() && !dir.mkpath(".")) {
                    THROW_FILE_ERROR("Failed to create subdirectory", subDirPath);
                }
            } catch (const FileOperationException& e) {
                Logger::instance().error(QString("Failed to create subdirectory: %1 - %2").arg(e.message(), e.path()));
                return false;
            }
        }
    }

    return true;
}

bool FileSystemManager::copyFilesFromHomeToWorking(const QString& month, const QString& week)
{
    QString basePath = getBasePath();
    QStringList jobTypes = {"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"};
    QString homeFolder = month + "." + week;

    for (const QString& jobType : jobTypes) {
        QString homeDir = basePath + "/" + jobType + "/" + homeFolder;
        QString workingDir = basePath + "/" + jobType + "/JOB";

        for (const QString& subDir : QStringList{"INPUT", "OUTPUT", "PRINT", "PROOF"}) {
            QDir homeSubDir(homeDir + "/" + subDir);
            QString workingSubDirPath = workingDir + "/" + subDir;
            try {
                QDir workingSubDir(workingSubDirPath);
                if (!workingSubDir.exists() && !workingSubDir.mkpath(".")) {
                    THROW_FILE_ERROR("Failed to create working directory", workingSubDirPath);
                }
            } catch (const FileOperationException& e) {
                Logger::instance().error(QString("Failed to create working directory: %1 - %2").arg(e.message(), e.path()));
                return false;
            }

            if (homeSubDir.exists()) {
                const QStringList& files = homeSubDir.entryList(QDir::Files);
                for (const QString& file : files) {
                    QString src = homeSubDir.filePath(file);
                    QString dest = workingSubDirPath + "/" + file;
                    try {
                        if (QFile::exists(dest)) {
                            if (!QFile::remove(dest)) {
                                THROW_FILE_ERROR("Failed to remove existing file at destination", dest);
                            }
                        }
                        if (!QFile::copy(src, dest)) {
                            THROW_FILE_ERROR("Failed to copy file", src);
                        }
                    } catch (const FileOperationException& e) {
                        Logger::instance().error(QString("Failed to copy file from %1 to %2: %3").arg(src, dest, e.message()));
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

bool FileSystemManager::moveFilesToHomeFolders(const QString& month, const QString& week)
{
    QString basePath = getBasePath();
    QStringList jobTypes = {"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"};
    QString homeFolder = month + "." + week;
    completedCopies.clear(); // Clear tracked operations

    for (const QString& jobType : jobTypes) {
        QString homeDir = basePath + "/" + jobType + "/" + homeFolder;
        QString workingDir = basePath + "/" + jobType + "/JOB";

        for (const QString& subDir : QStringList{"INPUT", "OUTPUT", "PRINT", "PROOF"}) {
            QDir workingSubDir(workingDir + "/" + subDir);
            QString homeSubDirPath = homeDir + "/" + subDir;
            try {
                QDir homeSubDir(homeSubDirPath);
                if (!homeSubDir.exists() && !homeSubDir.mkpath(".")) {
                    THROW_FILE_ERROR("Failed to create home subdirectory", homeSubDirPath);
                }
            } catch (const FileOperationException& e) {
                Logger::instance().error(QString("Failed to create home subdirectory: %1 - %2").arg(e.message(), e.path()));
                return false;
            }

            if (workingSubDir.exists()) {
                const QStringList& files = workingSubDir.entryList(QDir::Files);
                for (const QString& file : files) {
                    QString srcPath = workingSubDir.filePath(file);
                    QString destPath = homeSubDirPath + "/" + file;
                    try {
                        if (QFile::exists(destPath)) {
                            if (!QFile::remove(destPath)) {
                                THROW_FILE_ERROR("Failed to remove existing file at destination", destPath);
                            }
                        }
                        if (!QFile::rename(srcPath, destPath)) {
                            // If rename fails, try copy+delete
                            if (!QFile::copy(srcPath, destPath)) {
                                THROW_FILE_ERROR("Failed to copy file during move operation", srcPath);
                            }
                            if (!QFile::remove(srcPath)) {
                                THROW_FILE_ERROR("Failed to remove source file after copy", srcPath);
                            }
                        }
                        // Track the operation
                        completedCopies.append(qMakePair(srcPath, destPath));
                    } catch (const FileOperationException& e) {
                        Logger::instance().error(QString("Failed to move file from %1 to %2: %3").arg(srcPath, destPath, e.message()));
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

bool FileSystemManager::checkProofFiles(const QString& jobType, QStringList& missingFiles)
{
    missingFiles.clear();

    if (!proofFiles.contains(jobType)) {
        Logger::instance().warning("No proof files defined for " + jobType);
        return false;
    }

    QString proofPath = getProofFolderPath(jobType);
    QDir proofDir(proofPath);
    if (!proofDir.exists()) {
        Logger::instance().warning("Proof directory does not exist: " + proofPath);
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

    QString printPath = getPrintFolderPath(jobType);
    QDir printDir(printPath);
    if (!printDir.exists()) {
        missingFiles.append("Directory missing: " + printPath);
        return false;
    }

    // Define expected PDF keywords
    QMap<QString, QStringList> expectedKeywords = {
        {"CBC", {"CBC2 PRINT", "CBC3 PRINT"}},
        {"EXC", {"EXC PRINT"}},
        {"NCWO", {"NCWO 1-A PRINT", "NCWO 1-AP PRINT", "NCWO 1-APPR PRINT", "NCWO 1-PR PRINT",
                  "NCWO 2-A PRINT", "NCWO 2-AP PRINT", "NCWO 2-APPR PRINT", "NCWO 2-PR PRINT"}},
        {"PREPIF", {"PREPIF PR PRINT", "PREPIF US PRINT"}}
    };

    if (!expectedKeywords.contains(jobType)) {
        Logger::instance().warning("No PDF keywords defined for " + jobType);
        return false;
    }

    QStringList pdfFiles = printDir.entryList(QStringList() << "*.pdf", QDir::Files);
    bool allFilesPresent = true;
    QStringList keywords = expectedKeywords[jobType];

    for (const QString& keyword : keywords) {
        bool found = false;
        for (const QString& pdfFile : pdfFiles) {
            if (pdfFile.contains(keyword, Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        if (!found) {
            // Extract the variant name for display (e.g., "CBC2" from "CBC2 PRINT")
            QString variant = keyword.split(" ")[0];
            if (jobType == "PREPIF") {
                variant = keyword.split(" ")[0] + " " + keyword.split(" ")[1]; // e.g., "PREPIF PR"
            }
            missingFiles.append(variant);
            allFilesPresent = false;
        }
    }

    return allFilesPresent;
}

bool FileSystemManager::checkInactiveCsvFiles(QStringList& missingFiles)
{
    missingFiles.clear();

    QString outputPath = getBasePath() + "/INACTIVE/JOB/OUTPUT";
    QDir outputDir(outputPath);
    if (!outputDir.exists()) {
        missingFiles.append("Directory missing: " + outputPath);
        return false;
    }

    QStringList expectedCsvFiles = {
        "A-PO.csv", "A-PU.csv", "AT-PO.csv", "AT-PU.csv", "PR-PO.csv", "PR-PU.csv"
    };

    bool allFilesPresent = true;
    for (const QString& csvFile : expectedCsvFiles) {
        if (!outputDir.exists(csvFile)) {
            missingFiles.append(csvFile);
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
        Logger::instance().warning("ART directory does not exist: " + artPath);
        return false;
    }

    QStringList filters;
    filters << "*" + pattern + "*.indd";
    QDir::Filters dirFilters = QDir::Files | QDir::NoDotAndDotDot | QDir::Readable;
    QFileInfoList fileInfoList = dir.entryInfoList(filters, dirFilters);

    if (fileInfoList.isEmpty()) {
        Logger::instance().warning("No " + pattern + " INDD files found in: " + artPath);
        return false;
    }

    // Open the first file and wait longer for InDesign to start
    if (!fileInfoList.isEmpty()) {
        QString filePath = fileInfoList.first().absoluteFilePath();
        if (QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
            Logger::instance().info("Opened first " + pattern + " INDD file: " + filePath);

            // Wait for InDesign to start
            QThread::sleep(5);

            // Now open the remaining files with shorter delays
            for (int i = 1; i < fileInfoList.size(); i++) {
                QString nextFilePath = fileInfoList.at(i).absoluteFilePath();
                if (QDesktopServices::openUrl(QUrl::fromLocalFile(nextFilePath))) {
                    Logger::instance().info("Opened additional " + pattern + " INDD file: " + nextFilePath);

                    // Wait between files
                    QThread::sleep(2);
                } else {
                    Logger::instance().warning("Failed to open " + pattern + " INDD file: " + nextFilePath);
                }
            }

            return true;
        } else {
            Logger::instance().warning("Failed to open first " + pattern + " INDD file: " + filePath);
        }
    }

    return false;
}

const QList<QPair<QString, QString>>& FileSystemManager::getCompletedCopies() const {
    return completedCopies;
}
