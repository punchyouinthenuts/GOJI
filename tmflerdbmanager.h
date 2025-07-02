#ifndef TMFLERDBMANAGER_H
#define TMFLERDBMANAGER_H

#include "databasemanager.h"
#include <QSqlTableModel>
#include <QObject>

/**
 * @brief Database manager for TM FL ER tab operations
 *
 * This class manages all database operations specific to the TM FL ER tab,
 * including job saving/loading and tracker log entries. Follows the same
 * pattern as TMTermDBManager for consistency.
 */
class TMFLERDBManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Get singleton instance
     * @return Pointer to the singleton instance
     */
    static TMFLERDBManager* instance();

    /**
     * @brief Initialize database tables for TM FL ER
     * @return True if initialization successful
     */
    bool initializeTables();

    /**
     * @brief Save a job to the database
     * @param jobNumber Job number (5 digits)
     * @param year Year (YYYY format)
     * @param month Month (MM format)
     * @return True if save successful
     */
    bool saveJob(const QString& jobNumber, const QString& year, const QString& month);

    /**
     * @brief Load a job from the database
     * @param year Year (YYYY format)
     * @param month Month (MM format)
     * @param jobNumber Reference to store loaded job number
     * @return True if load successful
     */
    bool loadJob(const QString& year, const QString& month, QString& jobNumber);

    /**
     * @brief Get all saved jobs from database
     * @return List of job data maps containing job_number, year, month
     */
    QList<QMap<QString, QString>> getAllJobs();

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

private:
    explicit TMFLERDBManager(QObject *parent = nullptr);
    ~TMFLERDBManager();

    // Singleton instance
    static TMFLERDBManager* m_instance;

    // Database manager reference
    DatabaseManager* m_dbManager;

    // Tracker model
    QSqlTableModel* m_trackerModel;

    /**
     * @brief Create database tables for TM FL ER
     * @return True if tables created successfully
     */
    bool createTables();
};

#endif // TMFLERDBMANAGER_H
