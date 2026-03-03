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

    /**
     * Ensures the meter_rates table exists and is seeded.
     * Migrates from legacy meter_rate table if meter_rates is empty and
     * the legacy table is present. Drops the legacy meter_rate table unconditionally.
     */
    void ensureMeterRatesTableExists();

    /**
     * Returns the most recently inserted rate from meter_rates
     * (ORDER BY created_at DESC LIMIT 1), or defaultValue if the table is empty.
     */
    double getCurrentMeterRate(double defaultValue = 0.69);

    /**
     * Inserts a new row into meter_rates with the given rate_value.
     * Returns true on success.
     */
    bool updateMeterRateInDatabase(double newRate);

private:
    DatabaseManager* m_dbManager;
    QSqlDatabase database() const;
};

#endif // METERRATESERVICE_H
