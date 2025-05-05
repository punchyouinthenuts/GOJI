#ifndef PDFREPAIRINTEGRATION_H
#define PDFREPAIRINTEGRATION_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>

// Forward declarations
class JobController;
class PDFFileHelper;
enum class PDFProblemType;

/**
 * @brief Integration class to provide PDF repair functionality in the job workflow
 *
 * This class connects the PDF file helper with the job controller to provide
 * automatic repair capabilities for PDF files during proof regeneration.
 */
class PDFRepairIntegration : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param jobController The job controller to integrate with
     * @param parent The parent object
     */
    explicit PDFRepairIntegration(JobController* jobController, QObject* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~PDFRepairIntegration();

    /**
     * @brief Check and repair all PDF files for a specific job type
     * @param jobType The job type (CBC, EXC, etc.)
     * @return True if all PDFs were successfully repaired, false otherwise
     */
    bool checkAndRepairPDFs(const QString& jobType);

    /**
     * @brief Verify PDFs before script execution
     * @param filePaths List of PDF file paths to verify
     * @return True if all PDFs are valid, false otherwise
     */
    bool verifyPDFsBeforeScript(const QStringList& filePaths);

    /**
     * @brief Repair a single PDF file
     * @param filePath The path to the PDF file
     * @return True if repair was successful, false otherwise
     */
    bool repairSinglePDF(const QString& filePath);

    /**
     * @brief Monitor the creation of a PDF file
     * @param filePath The path to the PDF file
     * @param timeoutSeconds The maximum time to wait for the file (in seconds)
     * @return True if the file was created successfully, false otherwise
     */
    bool monitorPDFCreation(const QString& filePath, int timeoutSeconds = 30);

    /**
     * @brief Get the proof folder path for a job type
     * @param jobType The job type (CBC, EXC, etc.)
     * @return The proof folder path
     */
    QString getProofFolderPath(const QString& jobType);

    /**
     * @brief Check if PDF repair is in progress
     * @return True if repair is in progress, false otherwise
     */
    bool isRepairing() const;

signals:
    /**
     * @brief Signal emitted when repair operation starts
     */
    void repairStarted();

    /**
     * @brief Signal emitted when repair operation finishes
     * @param hasErrors True if there were errors during repair, false otherwise
     */
    void repairFinished(bool hasErrors);

    /**
     * @brief Signal emitted to log messages
     * @param message The log message
     */
    void logMessage(const QString& message);

private:
    JobController* m_jobController;     ///< The job controller
    PDFFileHelper* m_pdfHelper;         ///< The PDF file helper
    bool m_isRepairing;                 ///< Flag indicating if repair is in progress
};

#endif // PDFREPAIRINTEGRATION_H
