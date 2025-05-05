#ifndef PDFFILEHELPER_H
#define PDFFILEHELPER_H

#include <QObject>
#include <QString>

/**
 * @brief Enumeration of possible PDF file problems
 */
enum class PDFProblemType {
    FileNotFound,    ///< The PDF file does not exist
    EmptyFile,       ///< The PDF file exists but is empty
    PermissionIssue, ///< The PDF file has incorrect permissions
    FileLocked,      ///< The PDF file is locked by another process
    AccessDenied,    ///< Cannot access the PDF file
    InvalidFormat,   ///< The file is not a valid PDF
    Unknown          ///< Cannot determine the specific issue
};

/**
 * @brief Helper class for diagnosing and fixing PDF file issues
 *
 * This class provides utilities for checking PDF file accessibility,
 * diagnosing common problems, and attempting to fix issues that
 * might occur during PDF regeneration.
 */
class PDFFileHelper : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent The parent object
     */
    explicit PDFFileHelper(QObject *parent = nullptr);

    /**
     * @brief Check if a PDF file is accessible
     * @param filePath The path to the PDF file
     * @return True if the file is accessible, false otherwise
     */
    bool checkPDFAccess(const QString &filePath);

    /**
     * @brief Fix permissions for a PDF file
     * @param filePath The path to the PDF file
     * @return True if permissions were successfully updated, false otherwise
     */
    bool fixPDFPermissions(const QString &filePath);

    /**
     * @brief Create a backup copy of a PDF file
     * @param filePath The path to the PDF file
     * @param backupPath Output parameter - path to the created backup
     * @return True if backup was successfully created, false otherwise
     */
    bool makeBackupCopy(const QString &filePath, QString &backupPath);

    /**
     * @brief Attempt to release any locks on a PDF file
     * @param filePath The path to the PDF file
     * @return True if the file is now accessible, false otherwise
     */
    bool releasePDFFile(const QString &filePath);

    /**
     * @brief Attempt to repair a corrupted PDF file
     * @param filePath The path to the PDF file
     * @return True if repair was successful, false otherwise
     */
    bool repairPDF(const QString &filePath);

    /**
     * @brief Analyze the specific problem with a PDF file
     * @param filePath The path to the PDF file
     * @param problemType Output parameter - the detected problem type
     * @return True if a specific problem was identified, false otherwise
     */
    bool analyzeProblem(const QString &filePath, PDFProblemType &problemType);

    /**
     * @brief Attempt to fix a PDF problem based on its type
     * @param filePath The path to the PDF file
     * @param problemType The type of problem to fix
     * @return True if the problem was fixed, false otherwise
     */
    bool fixPDFProblem(const QString &filePath, PDFProblemType problemType);

signals:
    /**
     * @brief Signal emitted to log messages
     * @param message The log message
     */
    void logMessage(const QString &message);

private:
    /**
     * @brief Check if a file is locked by another process
     * @param filePath The path to the file
     * @return True if the file is locked, false otherwise
     */
    bool isFileLocked(const QString &filePath);
};

#endif // PDFFILEHELPER_H
