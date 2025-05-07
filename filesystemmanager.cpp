#include "filesystemmanager.h"
#include "fileutils.h"
#include "errorhandling.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>
#include <QThread>
#include <QtConcurrent>

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
    QStringList failedFiles; // Track failures

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
                        failedFiles << src;
                        success = false;
                        continue;
                    }
                }

                // Try rename first (fast move operation)
                if (QFile::rename(src, dest)) {
                    qDebug() << "Moved" << src << "to" << dest;
                } else {
                    // If rename fails, try copy+delete as fallback
                    if (QFile::copy(src, dest)) {
                        // Verify the copy was successful
                        if (QFileInfo(dest).size() == QFileInfo(src).size()) {
                            if (QFile::remove(src)) {
                                qDebug() << "Copy+Delete successful for" << src << "to" << dest;
                            } else {
                                qDebug() << "Warning: Copied but failed to delete source:" << src;
                                failedFiles << src;
                                success = false;
                            }
                        } else {
                            qDebug() << "Warning: Copy size mismatch for" << src;
                            failedFiles << src;
                            success = false;
                        }
                    } else {
                        qDebug() << "Failed to move or copy" << src << "to" << dest;
                        failedFiles << src;
                        success = false;
                    }
                }
            }
        }
    }

    // Log failures for debugging
    if (!failedFiles.isEmpty()) {
        qDebug() << "Failed to move the following files:";
        for (const QString& file : failedFiles) {
            qDebug() << "  " << file;
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

    QString printPath = getPrintFolderPath(jobType); // e.g., C:/Goji/RAC/CBC/JOB/PRINT
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
        qDebug() << "No PDF keywords defined for" << jobType;
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

    QString outputPath = getBasePath() + "/INACTIVE/JOB/OUTPUT"; // C:/Goji/RAC/INACTIVE/JOB/OUTPUT
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

    // Open the first file and wait longer for InDesign to start
    if (!fileInfoList.isEmpty()) {
        QString filePath = fileInfoList.first().absoluteFilePath();
        if (QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
            qDebug() << "Opened first" << pattern << "INDD file:" << filePath;

            // Wait 10 seconds for InDesign to start
            QThread::sleep(10);

            // Now open the remaining files with shorter delays
            for (int i = 1; i < fileInfoList.size(); i++) {
                QString nextFilePath = fileInfoList.at(i).absoluteFilePath();
                if (QDesktopServices::openUrl(QUrl::fromLocalFile(nextFilePath))) {
                    qDebug() << "Opened additional" << pattern << "INDD file:" << nextFilePath;

                    // Wait 3 seconds between subsequent files
                    QThread::sleep(3);
                } else {
                    qDebug() << "Failed to open" << pattern << "INDD file:" << nextFilePath;
                }
            }

            return true;
        } else {
            qDebug() << "Failed to open first" << pattern << "INDD file:" << filePath;
        }
    }

    return false;
}
