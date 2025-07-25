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
                      bool proofApprovalChecked, int htmlDisplayState,
                      bool jobDataLocked, bool postageDataLocked,
                      const QString& postage, const QString& count,
                      const QString& mailClass, const QString& permit);
    bool loadJobState(const QString& year, const QString& month, const QString& week,
                      bool& proofApprovalChecked, int& htmlDisplayState,
                      bool& jobDataLocked, bool& postageDataLocked,
                      QString& postage, QString& count,
                      QString& mailClass, QString& permit);

    // Postage data operations (for persistent postage field storage)
    bool savePostageData(const QString& year, const QString& month, const QString& week,
                         const QString& postage, const QString& count, const QString& mailClass,
                         const QString& permit, bool locked);
    bool loadPostageData(const QString& year, const QString& month, const QString& week,
                         QString& postage, QString& count, QString& mailClass,
                         QString& permit, bool& locked);

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

    // Debug function to examine database contents
    void debugDatabaseContents(const QString& year, const QString& month) const;
    
    // Debug function to populate test data for troubleshooting
    bool populateTestData(const QString& year, const QString& month, const QString& week,
                         const QString& postage, const QString& count, 
                         const QString& mailClass, const QString& permit);
                         
    // Function to load postage data from log table (fallback method)
    bool loadPostageDataFromLog(const QString& year, const QString& month, const QString& week,
                               QString& postage, QString& count, QString& mailClass,
                               QString& permit);

    // NEW FUNCTION: Load log entry by job number, month, and week
    bool loadLogEntry(const QString& jobNumber, const QString& month, const QString& week,
                     QString& postage, QString& count, QString& mailClass, QString& permit);

private:
    // Private constructor for singleton
    TMWeeklyPCDBManager();

    // Core database reference
    DatabaseManager* m_dbManager;

    // Singleton instance
    static TMWeeklyPCDBManager* m_instance;

    // Table creation
    bool createTables();

    // Helper functions for week format handling
    QString normalizeWeekFormat(const QString& week, bool zeroPadded = true) const;
    bool tryLoadWithBothWeekFormats(const QString& tableName, const QString& selectClause, 
                                   const QString& year, const QString& month, const QString& week,
                                   QSqlQuery& resultQuery);

    // Constants
    const QString TAB_NAME = "TM_WEEKLY_PC";
};

#endif // TMWEEKLYPCDBMANAGER_H
