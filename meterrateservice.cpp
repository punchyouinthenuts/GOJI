#include "meterrateservice.h"
#include "databasemanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

MeterRateService::MeterRateService(DatabaseManager* dbManager, QObject* parent)
    : QObject(parent),
      m_dbManager(dbManager)
{
}

QSqlDatabase MeterRateService::database() const
{
    return m_dbManager->getDatabase();
}

void MeterRateService::ensureMeterRatesTableExists()
{
    QSqlQuery query(database());

    query.exec(
        "CREATE TABLE IF NOT EXISTS meter_rate ("
        "id INTEGER PRIMARY KEY CHECK (id = 1),"
        "rate REAL NOT NULL"
        ")"
    );

    // Ensure single row exists
    query.exec("INSERT OR IGNORE INTO meter_rate (id, rate) VALUES (1, 0.69)");
}

double MeterRateService::getCurrentMeterRate(double defaultValue)
{
    QSqlQuery query(database());
    query.prepare("SELECT rate FROM meter_rate WHERE id = 1");

    if (query.exec() && query.next()) {
        return query.value(0).toDouble();
    }

    return defaultValue;
}

bool MeterRateService::updateMeterRateInDatabase(double newRate)
{
    QSqlQuery query(database());
    query.prepare("UPDATE meter_rate SET rate = :rate WHERE id = 1");
    query.bindValue(":rate", newRate);

    return query.exec();
}
