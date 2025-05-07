#include "pdfrepairintegration.h"
#include "pdffilehelper.h"
#include "filesystemmanager.h"  // Include FileSystemManager header
#include <QDebug>
#include <QDirIterator>
#include <QThread>

PDFRepairIntegration::PDFRepairIntegration(JobController* jobController, FileSystemManager* fileManager, QObject* parent)
    : QObject(parent),
    m_jobController(jobController),
    m_fileManager(fileManager),
    m_pdfHelper(new PDFFileHelper(this)),
    m_isRepairing(false)
{
    // Connect the PDF helper's log messages to our log messages
    connect(m_pdfHelper, &PDFFileHelper::logMessage, this, &PDFRepairIntegration::logMessage);
}

PDFRepairIntegration::~PDFRepairIntegration()
{
    // PDFFileHelper will be deleted automatically through parent-child relationship
}

bool PDFRepairIntegration::checkAndRepairPDFs(const QString& jobType)
{
    if (m_isRepairing) {
        emit logMessage("PDF repair already in progress. Please wait.");
        return false;
    }

    if (!m_fileManager) {
        emit logMessage("File system manager not available.");
        return false;
    }

    QString proofFolderPath = m_fileManager->getProofFolderPath(jobType);
    if (proofFolderPath.isEmpty()) {
        emit logMessage(QString("Cannot determine proof folder path for job type: %1").arg(jobType));
        return false;
    }

    m_isRepairing = true;
    emit repairStarted();
    emit logMessage(QString("Starting PDF check and repair for %1...").arg(jobType));

    // List of issues found
    QMap<QString, PDFProblemType> problemPDFs;
    QStringList repaired;
    QStringList failed;

    // Find all PDF files in the proof folder
    QDirIterator it(proofFolderPath, QStringList() << "*.pdf", QDir::Files, QDirIterator::Subdirectories);
    int fileCount = 0;

    while (it.hasNext()) {
        QString filePath = it.next();
        fileCount++;

        emit logMessage(QString("Checking PDF file: %1").arg(filePath));

        // Analyze the PDF file for problems
        PDFProblemType problemType;
        if (m_pdfHelper->analyzeProblem(filePath, problemType)) {
            problemPDFs.insert(filePath, problemType);

            emit logMessage(QString("Found issue with PDF: %1").arg(filePath));

            // Attempt to fix the problem
            if (m_pdfHelper->fixPDFProblem(filePath, problemType)) {
                repaired.append(filePath);
                emit logMessage(QString("Successfully repaired PDF: %1").arg(filePath));
            } else {
                failed.append(filePath);
                emit logMessage(QString("Failed to repair PDF: %1").arg(filePath));
            }
        }
    }

    // Summary of operation
    if (problemPDFs.isEmpty()) {
        emit logMessage(QString("No PDF issues found in %1 files.").arg(fileCount));
    } else {
        emit logMessage(QString("Found %1 problematic PDFs. Repaired: %2, Failed: %3")
                            .arg(problemPDFs.size())
                            .arg(repaired.size())
                            .arg(failed.size()));

        // Provide details on failed repairs
        if (!failed.isEmpty()) {
            emit logMessage("Failed repairs:");
            for (const QString& file : failed) {
                QString problemDesc;
                switch (problemPDFs[file]) {
                case PDFProblemType::FileNotFound:
                    problemDesc = "File not found";
                    break;
                case PDFProblemType::EmptyFile:
                    problemDesc = "Empty file";
                    break;
                case PDFProblemType::PermissionIssue:
                    problemDesc = "Permission issue";
                    break;
                case PDFProblemType::FileLocked:
                    problemDesc = "File locked by another process";
                    break;
                case PDFProblemType::AccessDenied:
                    problemDesc = "Access denied";
                    break;
                case PDFProblemType::InvalidFormat:
                    problemDesc = "Invalid PDF format";
                    break;
                default:
                    problemDesc = "Unknown issue";
                    break;
                }
                emit logMessage(QString("  - %1: %2").arg(file, problemDesc));
            }
        }
    }

    m_isRepairing = false;
    emit repairFinished(!failed.isEmpty());

    return repaired.size() == problemPDFs.size();
}

bool PDFRepairIntegration::verifyPDFsBeforeScript(const QStringList& filePaths)
{
    bool allGood = true;
    QStringList problemFiles;

    for (const QString& filePath : filePaths) {
        PDFProblemType problemType;

        // Check if there's a problem with the file
        if (m_pdfHelper->analyzeProblem(filePath, problemType)) {
            problemFiles.append(filePath);
            allGood = false;

            // Try to fix it automatically
            if (m_pdfHelper->fixPDFProblem(filePath, problemType)) {
                emit logMessage(QString("Auto-repaired PDF issue with: %1").arg(filePath));
            } else {
                emit logMessage(QString("Could not auto-repair PDF issue with: %1").arg(filePath));
            }
        }
    }

    if (!allGood) {
        emit logMessage(QString("Found %1 problem PDF files before script execution").arg(problemFiles.size()));
    }

    return allGood;
}

bool PDFRepairIntegration::repairSinglePDF(const QString& filePath)
{
    if (m_isRepairing) {
        emit logMessage("PDF repair already in progress. Please wait.");
        return false;
    }

    m_isRepairing = true;
    emit repairStarted();

    emit logMessage(QString("Starting repair for single PDF: %1").arg(filePath));

    // Analyze and fix the problem
    PDFProblemType problemType;
    if (m_pdfHelper->analyzeProblem(filePath, problemType)) {
        bool success = m_pdfHelper->fixPDFProblem(filePath, problemType);

        if (success) {
            emit logMessage(QString("Successfully repaired PDF: %1").arg(filePath));
        } else {
            emit logMessage(QString("Failed to repair PDF: %1").arg(filePath));
        }

        m_isRepairing = false;
        emit repairFinished(!success);
        return success;
    }

    emit logMessage(QString("No issues detected with PDF: %1").arg(filePath));
    m_isRepairing = false;
    emit repairFinished(false);
    return true;
}

bool PDFRepairIntegration::monitorPDFCreation(const QString& filePath, int timeoutSeconds)
{
    emit logMessage(QString("Monitoring PDF creation: %1").arg(filePath));

    // Wait for the file to be created
    for (int i = 0; i < timeoutSeconds * 2; i++) {
        QFile file(filePath);
        if (file.exists()) {
            // File exists, now make sure it's valid
            PDFProblemType problemType;
            if (!m_pdfHelper->analyzeProblem(filePath, problemType)) {
                // No problems detected
                emit logMessage(QString("PDF created successfully: %1").arg(filePath));
                return true;
            }

            // Problem detected - try to fix it
            if (m_pdfHelper->fixPDFProblem(filePath, problemType)) {
                emit logMessage(QString("PDF created with issues but repaired: %1").arg(filePath));
                return true;
            } else {
                emit logMessage(QString("PDF created with issues that could not be repaired: %1").arg(filePath));
                return false;
            }
        }

        // Wait 500ms before checking again
        QThread::msleep(500);
    }

    emit logMessage(QString("Timeout waiting for PDF creation: %1").arg(filePath));
    return false;
}

QString PDFRepairIntegration::getProofFolderPath(const QString& jobType)
{
    if (!m_fileManager) {
        return QString();
    }

    return m_fileManager->getProofFolderPath(jobType);
}

bool PDFRepairIntegration::isRepairing() const
{
    return m_isRepairing;
}
