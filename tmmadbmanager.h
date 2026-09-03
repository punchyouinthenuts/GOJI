#ifndef TMMADBMANAGER_H
#define TMMADBMANAGER_H

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

class DatabaseManager;

struct TMMAJobState
{
    int htmlDisplayState = 0;
    bool jobDataLocked = false;
    bool postageDataLocked = false;
    QString postage;
    QString count;
    QString mailClass;
    QString permit;
    QString lastExecutedScript;
};

class TMMADBManager : public QObject
{
    Q_OBJECT

public:
    static TMMADBManager* instance();

    bool initializeTables();
    bool saveJob(const QString& jobNumber, const QString& year, const QString& month);
    bool jobExists(const QString& jobNumber, const QString& year, const QString& month) const;
    bool jobIdentityExists(const QString& jobNumber,
                           const QString& year,
                           const QString& month,
                           bool& exists) const;
    bool logIdentityExists(const QString& jobNumber,
                           const QString& year,
                           const QString& month,
                           bool& exists) const;
    bool createJobWithState(const QString& jobNumber,
                            const QString& year,
                            const QString& month,
                            const TMMAJobState& state);
    bool migrateJobIdentity(const QString& oldJobNumber,
                            const QString& oldYear,
                            const QString& oldMonth,
                            const QString& newJobNumber,
                            const QString& newYear,
                            const QString& newMonth,
                            const TMMAJobState& state);
    bool saveJobState(const QString& jobNumber,
                      const QString& year,
                      const QString& month,
                      const TMMAJobState& state);
    bool loadJobState(const QString& jobNumber,
                      const QString& year,
                      const QString& month,
                      TMMAJobState& state) const;
    QList<QMap<QString, QString>> getAllJobs() const;
    bool deleteJob(const QString& jobNumber, const QString& year, const QString& month);

    bool upsertLogEntry(const QString& jobNumber,
                        const QString& year,
                        const QString& month,
                        const QString& description,
                        const QString& postage,
                        const QString& count,
                        const QString& perPiece,
                        const QString& mailClass,
                        const QString& shape,
                        const QString& permit,
                        const QString& date);

    QString lastError() const;

private:
    explicit TMMADBManager(QObject* parent = nullptr);
    bool execPrepared(class QSqlQuery& query, const QString& operation) const;
    bool identityCount(const QString& table,
                       const QString& jobNumber,
                       const QString& year,
                       const QString& month,
                       int& count,
                       class QSqlDatabase& database) const;
    void setLastError(const QString& error) const;

    static TMMADBManager* s_instance;
    DatabaseManager* m_dbManager;
    mutable QString m_lastError;
};

#endif // TMMADBMANAGER_H
