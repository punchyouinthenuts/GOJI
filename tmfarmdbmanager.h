#ifndef TMFARMDBMANAGER_H
#define TMFARMDBMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QVariant>
#include <QList>
#include <QMap>
#include <QString>

class TMFarmDBManager : public QObject
{
    Q_OBJECT
public:
    static TMFarmDBManager* instance();

    explicit TMFarmDBManager(QObject* parent = nullptr);
    ~TMFarmDBManager() override = default;

    bool isInitialized() const;
    QSqlDatabase getDatabase() const;
    bool ensureTables();
    bool createTables(); // kept public to match .cpp usage

    // Job table
    bool saveJob(const QString& jobNumber, const QString& year, const QString& quarter);
    bool loadJob(const QString& year, const QString& quarter, QString& outJobNumber);

    // State table
    bool saveJobState(const QString& year, const QString& quarter,
                      int htmlState, bool jobLocked, bool postageLocked,
                      const QString& postage, const QString& count,
                      const QString& lastExecutedScript);

    bool loadJobState(const QString& year, const QString& quarter,
                      int& htmlState, bool& jobLocked, bool& postageLocked,
                      QString& postage, QString& count, QString& lastExecutedScript);

    // Tracker log table
    bool addLogEntry(const QString& jobNumber, const QString& description,
                     const QString& formattedPostage, const QString& formattedCount,
                     const QString& formattedAvgRate, const QString& mailClass,
                     const QString& shape, const QString& permit, const QString& date,
                     const QString& year, const QString& quarter);

    bool updateLogEntryForJob(const QString& jobNumber, const QString& description,
                              const QString& formattedPostage, const QString& formattedCount,
                              const QString& formattedAvgRate, const QString& mailClass,
                              const QString& shape, const QString& permit, const QString& date,
                              const QString& year, const QString& quarter);

    bool updateLogJobNumber(const QString& oldJobNumber, const QString& newJobNumber);

    // Open Job menu helper
    QList<QMap<QString, QString>> getAllJobs() const;

private:
    QSqlDatabase m_db;
    bool m_initialized{false};
};

#endif // TMFARMDBMANAGER_H
