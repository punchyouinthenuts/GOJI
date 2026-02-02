#ifndef FHDBMANAGER_H
#define FHDBMANAGER_H

#include "databasemanager.h"
#include <QSqlTableModel>
#include <QObject>

/**
* @brief Database manager for FOUR HANDS tab operations
*
* This class manages all database operations specific to the FOUR HANDS tab,
* including job saving/loading and tracker log entries. Follows the same
* pattern as TMFLERDBManager for consistency.
*/
class FHDBManager : public QObject
{
    Q_OBJECT

public:
    /**
    * @brief Get singleton instance
    * @return Pointer to the singleton instance
    */
    static FHDBManager* instance();

    /**
    * @brief Initialize database tables for FOUR HANDS
    * @return True if initialization successful
    */
    bool initializeTables();

    /**
    * @brief Save a job to the database
    * @param jobNumber Job number (5 digits)
    * @param dropNumber Drop number (defaults to "1" when blank)
    * @param year Year (YYYY format)
    * @param month Month (MM format)
    * @return True if save successful
    */
    bool saveJob(const QString& jobNumber, const QString& dropNumber, const QString& year, const QString& month);

    /**
    * @brief Load a job from the database
    * @param jobNumber Job number (5 digits)
    * @param dropNumber Drop number (defaults to "1" when blank)
    * @param yearOut Reference to store loaded year
    * @param monthOut Reference to store loaded month
    * @return True if load successful
    */
    bool loadJob(const QString& jobNumber, const QString& dropNumber, QString& yearOut, QString& monthOut);

    /**
    * @brief Delete a job from the database
    * @param year Year as integer
    * @param month Month as integer
    * @return True if deletion successful
    */
    bool deleteJob(int year, int month);

    /**
    * @brief Get all saved jobs from database
    * @return List of job data maps containing job_number, drop_number, year, month
    */
    QList<QMap<QString, QString>> getAllJobs();

    /**
    * @brief Job state operations (for UI state persistence)
    */
    bool saveJobState(const QString& jobNumber, const QString& dropNumber, const QString& year, const QString& month,
                      int htmlDisplayState, bool jobDataLocked, bool postageDataLocked,
                      const QString& postage, const QString& count, const QString& lastExecutedScript);

    bool loadJobState(const QString& jobNumber, const QString& dropNumber, const QString& year, const QString& month,
                      int& htmlDisplayState, bool& jobDataLocked, bool& postageDataLocked,
                      QString& postage, QString& count, QString& lastExecutedScript);

    /**
    * @brief Get tracker model for displaying log entries
    * @return Pointer to QSqlTableModel for tracker table
    */
    QSqlTableModel* getTrackerModel();

    /**
    * @brief Add a log entry to the tracker
    * @param jobNumber Job number
    * @param description Job description
    * @param postage Postage amount
    * @param count Piece count
    * @param perPiece Per piece rate
    * @param mailClass Mail class (STD, FC, etc.)
    * @param shape Shape (LTR, etc.)
    * @param permit Permit type
    * @param date Date string
    * @return True if entry added successfully
    */
    bool addLogEntry(const QString& jobNumber, const QString& description,
                     const QString& postage, const QString& count,
                     const QString& perPiece, const QString& mailClass,
                     const QString& shape, const QString& permit,
                     const QString& date);

    /**
    * @brief Delete a log entry by ID
    * @param id Record ID to delete
    * @return True if deletion successful
    */
    bool deleteLogEntry(int id);

    /**
    * @brief Update a log entry
    * @param id Record ID to update
    * @param jobNumber Job number
    * @param description Job description
    * @param postage Postage amount
    * @param count Piece count
    * @param perPiece Per piece rate
    * @param mailClass Mail class
    * @param shape Shape
    * @param permit Permit type
    * @param date Date string
    * @return True if update successful
    */
    bool updateLogEntry(int id, const QString& jobNumber, const QString& description,
                        const QString& postage, const QString& count,
                        const QString& perPiece, const QString& mailClass,
                        const QString& shape, const QString& permit,
                        const QString& date);

    /**
    * @brief Update existing log entry for specific job
    * @param jobNumber Job number
    * @param description Job description
    * @param postage Postage amount
    * @param count Piece count
    * @param avgRate Average rate
    * @param mailClass Mail class
    * @param shape Shape
    * @param permit Permit type
    * @param date Date string
    * @return True if update successful
    */
    bool updateLogEntryForJob(const QString& jobNumber, const QString& description,
                              const QString& postage, const QString& count,
                              const QString& avgRate, const QString& mailClass,
                              const QString& shape, const QString& permit,
                              const QString& date);

    /**
    * @brief Update log entry job number
    * @param oldJobNumber Old job number
    * @param newJobNumber New job number
    * @return True if update successful
    */
    bool updateLogJobNumber(const QString& oldJobNumber, const QString& newJobNumber);

private:
    explicit FHDBManager(QObject *parent = nullptr);
    ~FHDBManager();

    // Singleton instance
    static FHDBManager* m_instance;

    // Database manager reference
    DatabaseManager* m_dbManager;

    // Tracker model
    QSqlTableModel* m_trackerModel;

    /**
    * @brief Create database tables for FOUR HANDS
    * @return True if tables created successfully
    */
    bool createTables();
};

#endif // FHDBMANAGER_H
