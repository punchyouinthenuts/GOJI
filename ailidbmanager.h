#ifndef AILIDBMANAGER_H
#define AILIDBMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QVariant>

class DatabaseManager;

/**
 * @brief Database manager for AILI tab operations.
 *
 * AILI jobs are uniquely identified by the combination of:
 * - job number
 * - issue number
 * - version
 * - month
 * - year
 *
 * This manager owns persistence for:
 * - locked job metadata
 * - locked postage/count metadata
 * - UI state needed to restore an in-progress job
 */
class AILIDBManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Returns the singleton instance.
     */
    static AILIDBManager* instance();

    /**
     * @brief Initializes the AILI database tables.
     * @return true on success.
     */
    bool initializeTables();

    /**
     * @brief Inserts or updates the core AILI job metadata row.
     *
     * This should be called when lockButtonAILI is successfully pressed.
     */
    bool saveJob(const QString& jobNumber,
                 const QString& issueNumber,
                 const QString& year,
                 const QString& month,
                 const QString& version,
                 const QString& pageCount);

    /**
     * @brief Loads a saved AILI job by its uniqueness key.
     * @return true if a matching row exists and was loaded.
     */
    bool loadJob(const QString& jobNumber,
                 const QString& issueNumber,
                 const QString& year,
                 const QString& month,
                 const QString& version,
                 QString& pageCountOut,
                 QString& postageOut,
                 QString& countOut);

    /**
     * @brief Deletes a saved AILI job by its uniqueness key.
     */
    bool deleteJob(const QString& jobNumber,
                   const QString& issueNumber,
                   const QString& year,
                   const QString& month,
                   const QString& version);

    /**
     * @brief Returns true if a job exists for the supplied uniqueness key.
     */
    bool jobExists(const QString& jobNumber,
                   const QString& issueNumber,
                   const QString& year,
                   const QString& month,
                   const QString& version) const;

    /**
     * @brief Returns all saved AILI jobs.
     *
     * Each map is expected to contain at least:
     * - job_number
     * - issue_number
     * - year
     * - month
     * - version
     * - page_count
     * - postage
     * - count
     */
    QList<QMap<QString, QString>> getAllJobs() const;

    /**
     * @brief Persists the current UI state for an in-progress or completed job.
     *
     * This should be called whenever lock state or script progress changes.
     */
    bool saveJobState(const QString& jobNumber,
                      const QString& issueNumber,
                      const QString& year,
                      const QString& month,
                      const QString& version,
                      const QString& pageCount,
                      int htmlDisplayState,
                      bool jobDataLocked,
                      bool postageDataLocked,
                      const QString& postage,
                      const QString& count,
                      const QString& lastExecutedScript);

    /**
     * @brief Restores the saved UI state for a job.
     */
    bool loadJobState(const QString& jobNumber,
                      const QString& issueNumber,
                      const QString& year,
                      const QString& month,
                      const QString& version,
                      QString& pageCountOut,
                      int& htmlDisplayState,
                      bool& jobDataLocked,
                      bool& postageDataLocked,
                      QString& postageOut,
                      QString& countOut,
                      QString& lastExecutedScriptOut);

    /**
     * @brief Updates locked postage/count data on the existing job row.
     *
     * This should be called when postageLockAILI is successfully pressed.
     */
    bool savePostageData(const QString& jobNumber,
                         const QString& issueNumber,
                         const QString& year,
                         const QString& month,
                         const QString& version,
                         const QString& postage,
                         const QString& count,
                         bool locked);

    /**
     * @brief Loads postage/count data from the existing job row.
     */
    bool loadPostageData(const QString& jobNumber,
                         const QString& issueNumber,
                         const QString& year,
                         const QString& month,
                         const QString& version,
                         QString& postageOut,
                         QString& countOut,
                         bool& lockedOut);

private:
    explicit AILIDBManager(QObject* parent = nullptr);
    ~AILIDBManager() override = default;

    AILIDBManager(const AILIDBManager&) = delete;
    AILIDBManager& operator=(const AILIDBManager&) = delete;

    /**
     * @brief Creates all database tables required by AILI.
     */
    bool createTables();

    static AILIDBManager* m_instance;
    DatabaseManager* m_dbManager;
};

#endif // AILIDBMANAGER_H

