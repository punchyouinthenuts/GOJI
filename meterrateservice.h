#ifndef METERRATESERVICE_H
#define METERRATESERVICE_H

#include <QObject>
#include <QSqlDatabase>

class DatabaseManager;

class MeterRateService : public QObject
{
    Q_OBJECT

public:
    explicit MeterRateService(DatabaseManager* dbManager, QObject* parent = nullptr);

    void ensureMeterRatesTableExists();
    double getCurrentMeterRate(double defaultValue = 0.69);
    bool updateMeterRateInDatabase(double newRate);

private:
    DatabaseManager* m_dbManager;
    QSqlDatabase database() const;
};

#endif // METERRATESERVICE_H