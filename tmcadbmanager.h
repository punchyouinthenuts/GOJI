#ifndef TMCADBMANGER_H
#define TMCADBMANGER_H

#include "databasemanager.h"
#include <QSqlTableModel>
#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QVariant>

/**
 * @brief Database manager for TM CA (CA EDR/BA)
 *
 * Mirrors the prevailing TRACHMAR TM* DB manager pattern (TMFLERDBManager/TMTermDBManager):
 * - Uses the shared DatabaseManager singleton connection
 * - Creates a jobs table (one job per job_number+year+month via UNIQUE(job_number, year, month))
 * - Creates a job_state table for UI persistence (job/postage locks + html state + last script + postage/count)
 * - Creates an 8-column tracker log table consistent with other TM trackers
 * - Creates a terminal_log table for per-period terminal history
 */
class TMCADBManager : public QObject
{
    Q_OBJECT

public:
    static TMCADBManager* instance();

    bool initializeTables();

    // Job operations
    bool saveJob(const QString& jobNumber, const QString& year, const QString& month);
    bool loadJob(const QString& jobNumber, const QString& year, const QString& month);
    bool loadJob(const QString& year, const QString& month, QString& jobNumber);
    bool deleteJob(int year, int month);
    bool deleteJob(const QString& year, const QString& month);
    bool jobExists(const QString& year, const QString& month);
    QList<QMap<QString, QString>> getAllJobs();

    // Job state operations (UI persistence) - keyed by job_number + year + month
    bool saveJobState(const QString& jobNumber, const QString& year, const QString& month,
                      int htmlDisplayState, bool jobDataLocked, bool postageDataLocked,
                      const QString& postage, const QString& count,
                      const QString& lastExecutedScript = "");
    bool loadJobState(const QString& jobNumber, const QString& year, const QString& month,
                      int& htmlDisplayState, bool& jobDataLocked, bool& postageDataLocked,
                      QString& postage, QString& count, QString& lastExecutedScript);

    // Tracker log operations - standardized 8-column format
    bool addLogEntry(const QString& jobNumber, const QString& description,
                     const QString& postage, const QString& count,
                     const QString& avgRate, const QString& mailClass,
                     const QString& shape, const QString& permit,
                     const QString& date,
                     const QString& year, const QString& month);

    // Alias used by TMCAController — year/month stored in tm_ca_log but not displayed in UI
    bool insertLogRow(const QString& jobNumber, const QString& description,
                      const QString& postage, const QString& count,
                      const QString& avgRate, const QString& mailClass,
                      const QString& shape, const QString& permit,
                      const QString& date,
                      const QString& year, const QString& month);

    QList<QMap<QString, QVariant>> getLog();

    // Terminal log operations - per period
    bool saveTerminalLog(const QString& year, const QString& month, const QString& message);
    QStringList getTerminalLogs(const QString& year, const QString& month);

    // Optional access to a tracker model (some modules expose this)
    QSqlTableModel* getTrackerModel();

private:
    explicit TMCADBManager(QObject *parent = nullptr);

    static TMCADBManager* m_instance;

    DatabaseManager* m_dbManager;
    QSqlTableModel* m_trackerModel;

    bool createTables();

    const QString TAB_NAME = "TM_CA";
};

#endif // TMCADBMANGER_H
