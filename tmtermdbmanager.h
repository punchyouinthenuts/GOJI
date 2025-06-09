#ifndef TMTERMDBMANAGER_H
#define TMTERMDBMANAGER_H

#include <QString>
#include <QList>
#include <QMap>
#include <QVariant>
#include "databasemanager.h"

class TMTermDBManager
{
public:
    // Singleton access
    static TMTermDBManager* instance();

    // Initialize tables
    bool initialize();

    // Job operations (no week parameter for TERM)
    bool saveJob(const QString& jobNumber, const QString& year, const QString& month);
    bool loadJob(const QString& year, const QString& month, QString& jobNumber);
    bool deleteJob(const QString& year, const QString& month);
    bool jobExists(const QString& year, const QString& month);
    QList<QMap<QString, QString>> getAllJobs();

    // Job state operations (for UI state persistence)
    bool saveJobState(const QString& year, const QString& month,
                      int htmlDisplayState, bool jobDataLocked, bool postageDataLocked);
    bool loadJobState(const QString& year, const QString& month,
                      int& htmlDisplayState, bool& jobDataLocked, bool& postageDataLocked);

    // Log operations - same signature as TMWPC for consistency
    bool addLogEntry(const QString& jobNumber, const QString& description,
                     const QString& postage, const QString& count,
                     const QString& perPiece, const QString& mailClass,
                     const QString& shape, const QString& permit,
                     const QString& date);

    QList<QMap<QString, QVariant>> getLog();

    // Terminal log specific to this tab
    bool saveTerminalLog(const QString& year, const QString& month,
                         const QString& message);
    QStringList getTerminalLogs(const QString& year, const QString& month);

private:
    // Private constructor for singleton
    TMTermDBManager();

    // Core database reference
    DatabaseManager* m_dbManager;

    // Singleton instance
    static TMTermDBManager* m_instance;

    // Table creation
    bool createTables();

    // Constants
    const QString TAB_NAME = "TM_TERM";
};

#endif // TMTERMDBMANAGER_H
