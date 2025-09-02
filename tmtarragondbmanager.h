#ifndef TMTARRAGONDBMANAGER_H
#define TMTARRAGONDBMANAGER_H

#include <QString>
#include <QList>
#include <QMap>
#include <QVariant>
#include "databasemanager.h"

class TMTarragonDBManager
{
public:
    // Singleton access
    static TMTarragonDBManager* instance();

    // Initialize tables
    bool initialize();

    // Job operations
    bool saveJob(const QString& jobNumber, const QString& year,
                 const QString& month, const QString& dropNumber);
    bool loadJob(const QString& year, const QString& month,
                 const QString& dropNumber, QString& jobNumber);
    bool deleteJob(const QString& year, const QString& month, const QString& dropNumber);
    bool jobExists(const QString& year, const QString& month, const QString& dropNumber);
    QList<QMap<QString, QString>> getAllJobs();

    // Job state operations (for UI state persistence)
    bool saveJobState(const QString& year, const QString& month, const QString& dropNumber,
                      int htmlDisplayState, bool jobDataLocked, bool postageDataLocked,
                      const QString& postage, const QString& count, const QString& lastExecutedScript = "");
    bool loadJobState(const QString& year, const QString& month, const QString& dropNumber,
                      int& htmlDisplayState, bool& jobDataLocked, bool& postageDataLocked,
                      QString& postage, QString& count, QString& lastExecutedScript);

    // Postage data operations (standardized structure)
    bool savePostageData(const QString& year, const QString& month, const QString& dropNumber,
                         const QString& postage, const QString& count, bool locked);
    bool loadPostageData(const QString& year, const QString& month, const QString& dropNumber,
                         QString& postage, QString& count, bool& locked);

    // Log operations - standardized 8-column format
    bool addLogEntry(const QString& jobNumber, const QString& description,
                     const QString& postage, const QString& count,
                     const QString& perPiece, const QString& mailClass,
                     const QString& shape, const QString& permit,
                     const QString& date);

    bool updateLogJobNumber(const QString& oldJobNumber, const QString& newJobNumber);

    QList<QMap<QString, QVariant>> getLog();

    // Terminal log specific to this tab
    bool saveTerminalLog(const QString& year, const QString& month,
                         const QString& dropNumber, const QString& message);
    QStringList getTerminalLogs(const QString& year, const QString& month, const QString& dropNumber);

private:
    // Private constructor for singleton
    TMTarragonDBManager();

    // Core database reference
    DatabaseManager* m_dbManager;

    // Singleton instance
    static TMTarragonDBManager* m_instance;

    // Table creation
    bool createTables();

    // Constants
    const QString TAB_NAME = "TM_TARRAGON";
};

#endif // TMTARRAGONDBMANAGER_H
