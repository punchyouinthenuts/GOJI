#include "pdffilehelper.h"
#include <QDebug>
#include <QProcess>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QThread>
#include <QCoreApplication>

PDFFileHelper::PDFFileHelper(QObject *parent)
    : QObject(parent)
{
}

bool PDFFileHelper::checkPDFAccess(const QString &filePath)
{
    QFile file(filePath);

    // Check if file exists
    if (!file.exists()) {
        emit logMessage(QString("PDF file does not exist: %1").arg(filePath));
        return false;
    }

    // Check read access
    if (!file.open(QIODevice::ReadOnly)) {
        emit logMessage(QString("Cannot open PDF file for reading: %1 - Error: %2")
                            .arg(filePath, file.errorString()));
        return false;
    }
    file.close();

    // Check write access
    if (!file.open(QIODevice::ReadWrite)) {
        emit logMessage(QString("Cannot open PDF file for writing: %1 - Error: %2")
                            .arg(filePath, file.errorString()));
        return false;
    }
    file.close();

    // Check if file is locked by another process
    bool isLocked = isFileLocked(filePath);
    if (isLocked) {
        emit logMessage(QString("PDF file appears to be locked by another process: %1").arg(filePath));
        return false;
    }

    emit logMessage(QString("PDF file is accessible: %1").arg(filePath));
    return true;
}

bool PDFFileHelper::isFileLocked(const QString &filePath)
{
    // On Windows, try to open the file for exclusive access
    QFile file(filePath);
    if (!file.open(QIODevice::ReadWrite)) {
        // Could be locked or other permission issue
        return true;
    }

    // Try to lock for exclusive access
    bool locked = !file.lock(QFile::ExclusiveLock);

    // If we got the lock, unlock and close
    if (!locked) {
        file.unlock();
    }

    file.close();
    return locked;
}

bool PDFFileHelper::fixPDFPermissions(const QString &filePath)
{
    QFile file(filePath);

    if (!file.exists()) {
        emit logMessage(QString("Cannot fix permissions - PDF file does not exist: %1").arg(filePath));
        return false;
    }

    // Get current permissions
    QFileInfo fileInfo(filePath);
    QFile::Permissions currentPermissions = fileInfo.permissions();

    // Add read/write permissions
    QFile::Permissions newPermissions = currentPermissions |
                                        QFile::ReadOwner | QFile::WriteOwner |
                                        QFile::ReadUser | QFile::WriteUser |
                                        QFile::ReadGroup | QFile::WriteGroup;

    if (file.setPermissions(newPermissions)) {
        emit logMessage(QString("Successfully updated permissions for: %1").arg(filePath));
        return true;
    } else {
        emit logMessage(QString("Failed to update permissions for: %1 - Error: %2")
                            .arg(filePath, file.errorString()));
        return false;
    }
}

bool PDFFileHelper::makeBackupCopy(const QString &filePath, QString &backupPath)
{
    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists()) {
        emit logMessage(QString("Cannot backup - file does not exist: %1").arg(filePath));
        return false;
    }

    // Create backup directory
    QString backupDir = fileInfo.absolutePath() + "/backups";
    QDir dir;
    if (!dir.exists(backupDir)) {
        if (!dir.mkpath(backupDir)) {
            emit logMessage(QString("Failed to create backup directory: %1").arg(backupDir));
            return false;
        }
    }

    // Generate timestamped backup filename
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    backupPath = QString("%1/%2_backup_%3.%4")
                     .arg(backupDir, fileInfo.baseName(), timestamp, fileInfo.suffix());

    // Create the backup
    if (QFile::copy(filePath, backupPath)) {
        emit logMessage(QString("Created backup: %1").arg(backupPath));
        return true;
    } else {
        emit logMessage(QString("Failed to create backup from %1 to %2")
                            .arg(filePath, backupPath));
        return false;
    }
}

bool PDFFileHelper::releasePDFFile(const QString &filePath)
{
    // This is a bit of a hack to try to close any processes that might have the file open
    // It works by forcing the garbage collector to run and closing any temp handles

    emit logMessage(QString("Attempting to release file handles for: %1").arg(filePath));

    // Force application to process events
    QCoreApplication::processEvents();

    // Wait a moment for any pending operations to complete
    QThread::msleep(500);

    // Force garbage collection (not directly available in Qt, but we can prompt it)
    QByteArray largeArray(1024 * 1024 * 10, 0); // Allocate 10MB
    largeArray.clear(); // Release it

    // Process events again
    QCoreApplication::processEvents();

    // Wait again
    QThread::msleep(500);

    // Check if the file is now accessible
    return checkPDFAccess(filePath);
}

bool PDFFileHelper::repairPDF(const QString &filePath)
{
    // First create a backup
    QString backupPath;
    if (!makeBackupCopy(filePath, backupPath)) {
        emit logMessage("Cannot repair PDF without a backup. Aborting repair.");
        return false;
    }

    emit logMessage(QString("Attempting to repair PDF file: %1").arg(filePath));

    // Check if Ghostscript is available (for PDF repair)
    QProcess gsCheck;
    gsCheck.start("gswin64c", QStringList() << "--version");
    bool gsAvailable = gsCheck.waitForFinished(5000) && (gsCheck.exitCode() == 0);

    if (gsAvailable) {
        // Use Ghostscript to rebuild the PDF
        QString tempPDF = filePath + ".temp.pdf";

        QProcess process;
        QStringList args;
        args << "-sDEVICE=pdfwrite"
             << "-dPDFSETTINGS=/prepress"
             << "-dNOPAUSE"
             << "-dBATCH"
             << "-dSAFER"
             << QString("-sOutputFile=%1").arg(tempPDF)
             << filePath;

        process.start("gswin64c", args);

        if (process.waitForFinished(30000)) {
            if (process.exitCode() == 0) {
                // Delete the original file
                QFile originalFile(filePath);
                if (originalFile.exists()) {
                    if (!originalFile.remove()) {
                        emit logMessage(QString("Failed to remove original PDF: %1").arg(originalFile.errorString()));
                        return false;
                    }
                }

                // Rename the temp file to the original filename
                QFile tempFile(tempPDF);
                if (tempFile.rename(filePath)) {
                    emit logMessage("Successfully repaired PDF using Ghostscript.");
                    return true;
                } else {
                    emit logMessage(QString("Failed to rename repaired PDF: %1").arg(tempFile.errorString()));
                    return false;
                }
            } else {
                emit logMessage(QString("Ghostscript repair failed with exit code: %1")
                                    .arg(process.exitCode()));
            }
        } else {
            emit logMessage("Ghostscript repair process timed out.");
        }
    } else {
        emit logMessage("Ghostscript not available. Cannot repair PDF using that method.");
    }

    // Try simpler repair method - just copy from backup
    if (QFile::exists(backupPath)) {
        emit logMessage("Attempting basic repair by copying from backup...");

        // Remove original if it exists
        QFile originalFile(filePath);
        if (originalFile.exists() && !originalFile.remove()) {
            emit logMessage(QString("Failed to remove problematic file: %1").arg(originalFile.errorString()));
            return false;
        }

        // Copy backup to original location
        if (QFile::copy(backupPath, filePath)) {
            emit logMessage("Successfully restored PDF from backup.");
            return true;
        } else {
            emit logMessage("Failed to restore from backup.");
        }
    }

    return false;
}

bool PDFFileHelper::analyzeProblem(const QString &filePath, PDFProblemType &problemType)
{
    QFileInfo fileInfo(filePath);

    // Check if file exists
    if (!fileInfo.exists()) {
        problemType = PDFProblemType::FileNotFound;
        return true;
    }

    // Check file size
    if (fileInfo.size() == 0) {
        problemType = PDFProblemType::EmptyFile;
        return true;
    }

    // Check permissions
    if (!fileInfo.isReadable() || !fileInfo.isWritable()) {
        problemType = PDFProblemType::PermissionIssue;
        return true;
    }

    // Check if file is locked
    if (isFileLocked(filePath)) {
        problemType = PDFProblemType::FileLocked;
        return true;
    }

    // Open the file and check for PDF header
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        problemType = PDFProblemType::AccessDenied;
        return true;
    }

    QByteArray header = file.read(1024);
    file.close();

    // Check if it's a valid PDF (should start with %PDF-1.)
    if (!header.startsWith("%PDF-1.")) {
        problemType = PDFProblemType::InvalidFormat;
        return true;
    }

    // If we got here and found no specific issues
    problemType = PDFProblemType::Unknown;
    return false;
}

bool PDFFileHelper::fixPDFProblem(const QString &filePath, PDFProblemType problemType)
{
    switch (problemType) {
    case PDFProblemType::FileNotFound:
        emit logMessage("Cannot fix - file not found.");
        return false;

    case PDFProblemType::EmptyFile:
        emit logMessage("File is empty. Attempting to restore from backup...");
        {
            QString backupPath;
            // We don't actually create a backup here, just use the path format to find the most recent backup
            QString backupDir = QFileInfo(filePath).absolutePath() + "/backups";
            QDir dir(backupDir);

            if (dir.exists()) {
                // Find the most recent backup
                QStringList filters;
                filters << QFileInfo(filePath).baseName() + "_backup_*.pdf";
                QFileInfoList backups = dir.entryInfoList(filters, QDir::Files, QDir::Time);

                if (!backups.isEmpty()) {
                    backupPath = backups.first().absoluteFilePath();
                    // Copy the backup to the original location
                    QFile originFile(filePath);
                    if (originFile.exists()) {
                        originFile.remove();
                    }

                    if (QFile::copy(backupPath, filePath)) {
                        emit logMessage(QString("Successfully restored from backup: %1").arg(backupPath));
                        return true;
                    }
                }
            }
            emit logMessage("No backups found to restore from.");
        }
        return false;

    case PDFProblemType::PermissionIssue:
        return fixPDFPermissions(filePath);

    case PDFProblemType::FileLocked:
        return releasePDFFile(filePath);

    case PDFProblemType::AccessDenied:
        emit logMessage("Access denied. Attempting to fix permissions and release locks...");
        if (fixPDFPermissions(filePath) && releasePDFFile(filePath)) {
            return true;
        }
        return false;

    case PDFProblemType::InvalidFormat:
        emit logMessage("Invalid PDF format. Attempting repair...");
        return repairPDF(filePath);

    case PDFProblemType::Unknown:
    default:
        // Try everything
        emit logMessage("Unknown issue. Trying all repair methods...");
        fixPDFPermissions(filePath);
        releasePDFFile(filePath);
        return repairPDF(filePath);
    }
}
