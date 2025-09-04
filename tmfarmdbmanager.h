#ifndef TMFARMDBMANAGER_H
#define TMFARMDBMANAGER_H

#include <QString>
#include <QList>
#include <QMap>
#include <QVariant>
#include "databasemanager.h"

/**
 * @brief Database manager for TM FARMWORKERS tab
 *
 * Handles job persistence, job state, and log entries.
 * FARMWORKERS uses quarter-based jobs (e.g., 12345_3RD2025).
 */
class TMFarmDBManager
{
public:
    static TMFarmDBManager* instance();

    bool initialize();

    // Job operations (quarter-based)
    bool saveJob(const QString& jobNumber, const QString& year, const QString& quarter);
    bool loadJob(const QString& year, const QString& quarter, QString& jobNumber);
    bool deleteJob(const QString& year, const QString& quarter);
    bool jobExists(const QString& year, const QString& quarter);
    QList<QMap<QString, QString>> getAllJobs();

    // Job state persistence (includes postage data)
    bool saveJobState(const QString& year, const QString& quarter,
                      int htmlDisplayState, bool jobDataLocked, bool postageDataLocked,
                      const QString& postage, const QString& count, const QString& lastExecutedScript = "");
    bool loadJobState(const QString& year, const QString& quarter,
                      int& htmlDisplayState, bool& jobDataLocked, bool& postageDataLocked,
                      QString& postage, QString& count, QString& lastExecutedScript);

    // Log operations - standardized 8-column format
    bool addLogEntry(const QString& jobNumber, const QString& description,
                     const QString& postage, const QString& count,
                     const QString& perPiece, const QString& mailClass,
                     const QString& shape, const QString& permit,
                     const QString& date,
                     const QString& year, const QString& quarter);

    bool updateLogEntryForJob(const QString& jobNumber, const QString& description,
                              const QString& postage, const QString& count,
                              const QString& avgRate, const QString& mailClass,
                              const QString& shape, const QString& permit,
                              const QString& date,
                              const QString& year, const QString& quarter);

    bool updateLogJobNumber(const QString& oldJobNumber, const QString& newJobNumber);

    QList<QMap<QString, QVariant>> getLog();

    bool saveTerminalLog(const QString& year, const QString& quarter,
                         const QString& message);
    QStringList getTerminalLogs(const QString& year, const QString& quarter);

private:
    TMFarmDBManager();
    DatabaseManager* m_dbManager;
    static TMFarmDBManager* m_instance;

    bool createTables();

    const QString TAB_NAME = "TM_FARMWORKERS";
};

#endif // TMFARMDBMANAGER_H
