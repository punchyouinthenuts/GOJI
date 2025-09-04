#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariantMap>

class TMFarmDBManager : public QObject {
    Q_OBJECT
public:
    // Singleton accessor
    static TMFarmDBManager* instance(QObject* parent = nullptr);

    // Lifecycle
    explicit TMFarmDBManager(QObject* parent = nullptr);
    ~TMFarmDBManager() override;

    // Initialization / connection
    bool isInitialized() const;
    QSqlDatabase getDatabase() const;
    bool ensureTables();

    // Public API used by controllers
    bool upsertJob(const QString& year, const QString& month, const QString& jobNumber);
    bool saveJobState(const QString& year, const QString& month, const QString& jobNumber,
                      bool jobLocked, const QString& htmlState);
    QVariantMap loadJobState(const QString& year, const QString& month, const QString& jobNumber);
    bool addLogEntry(const QString& year, const QString& month, const QString& jobNumber,
                     const QString& description);
    bool addTerminalLog(const QString& message, int type);

private:
    static TMFarmDBManager* s_instance;
    QSqlDatabase m_db;
    bool m_initialized{false};
};
