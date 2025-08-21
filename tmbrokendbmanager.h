#ifndef TMBROKENDBMANAGER_H
#define TMBROKENDBMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariantMap>
#include <QString>
#include <QStringList>
#include <QMutex>

// ✅ Forward declaration outside the class
class DatabaseManager;

class TMBrokenDBManager : public QObject
{
    Q_OBJECT

public:
    static TMBrokenDBManager* instance();

    // Database initialization
    bool initializeDatabase();
    bool isInitialized() const;

    // Job data management
    bool saveJob(const QString& jobNumber, const QString& year, const QString& month);
    bool saveJobData(const QVariantMap& jobData);
    QVariantMap loadJobData(const QString& year, const QString& month);
    bool deleteJobData(const QString& year, const QString& month);

    // Log management
    bool addLogEntry(const QVariantMap& logEntry);
    bool updateLogEntry(int id, const QVariantMap& logEntry);
    bool deleteLogEntry(int id);
    QList<QVariantMap> getAllLogEntries();
    QList<QVariantMap> getLogEntriesByDateRange(const QDate& startDate, const QDate& endDate);

    // Statistics and queries
    QVariantMap getJobStatistics(const QString& year, const QString& month);
    QStringList getAvailableYears();
    QStringList getAvailableMonths(const QString& year);
    QList<QMap<QString, QString>> getAllJobs();

    // Database maintenance
    bool backupDatabase(const QString& backupPath);
    bool restoreDatabase(const QString& backupPath);
    bool cleanupOldEntries(int daysOld);

    // Utility methods
    QString getLastError() const;
    bool executeQuery(const QString& query, const QVariantMap& params = QVariantMap());

private:
    explicit TMBrokenDBManager(QObject *parent = nullptr);
    ~TMBrokenDBManager();

    // Singleton pattern
    static TMBrokenDBManager* m_instance;
    static QMutex m_mutex;

    // Database setup
    bool createTables();
    bool createJobDataTable();
    bool createLogTable();
    bool createIndexes();

    // Helper methods
    QString formatSqlValue(const QVariant& value) const;
    bool bindParameters(QSqlQuery& query, const QVariantMap& params) const;

    // Member variables
    QSqlDatabase m_database;
    bool m_initialized;
    QString m_lastError;
    QString m_databasePath;

    // ✅ Correct: only the pointer lives here
    DatabaseManager* m_dbManager;

    // Table names
    static const QString JOB_DATA_TABLE;
    static const QString LOG_TABLE;
};

#endif // TMBROKENDBMANAGER_H
