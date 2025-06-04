#ifndef TMWEEKLYPCDBMANAGER_H
#define TMWEEKLYPCDBMANAGER_H

#include <QString>
#include <QList>
#include <QMap>
#include <QVariant>
#include "databasemanager.h"

class TMWeeklyPCDBManager
{
public:
    // Singleton access
    static TMWeeklyPCDBManager* instance();

    // Initialize tables
    bool initialize();

    // Job operations
    bool saveJob(const QString& jobNumber, const QString& year,
                 const QString& month, const QString& week);
    bool loadJob(const QString& year, const QString& month,
                 const QString& week, QString& jobNumber);
    bool deleteJob(const QString& year, const QString& month, const QString& week);
    bool jobExists(const QString& year, const QString& month, const QString& week);
    QList<QMap<QString, QString>> getAllJobs();

    // Job state operations (for UI state persistence)
    bool saveJobState(const QString& year, const QString& month, const QString& week,
                      bool proofApprovalChecked, int htmlDisplayState);
    bool loadJobState(const QString& year, const QString& month, const QString& week,
                      bool& proofApprovalChecked, int& htmlDisplayState);

    // Log operations - updated signature to match implementation
    bool addLogEntry(const QString& jobNumber, const QString& description,
                     const QString& postage, const QString& count,
                     const QString& perPiece, const QString& mailClass,
                     const QString& shape, const QString& permit,
                     const QString& date);

    QList<QMap<QString, QVariant>> getLog();

    // Terminal log specific to this tab
    bool saveTerminalLog(const QString& year, const QString& month,
                         const QString& week, const QString& message);
    QStringList getTerminalLogs(const QString& year, const QString& month, const QString& week);

private:
    // Private constructor for singleton
    TMWeeklyPCDBManager();

    // Core database reference
    DatabaseManager* m_dbManager;

    // Singleton instance
    static TMWeeklyPCDBManager* m_instance;

    // Table creation
    bool createTables();

    // Constants
    const QString TAB_NAME = "TM_WEEKLY_PC";
};

#endif // TMWEEKLYPCDBMANAGER_H
