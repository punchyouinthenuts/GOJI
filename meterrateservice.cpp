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
        "CREATE TABLE IF NOT EXISTS meter_rates ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "rate_value REAL NOT NULL,"
        "created_at TEXT NOT NULL DEFAULT (datetime('now','localtime')),"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now','localtime'))"
        ")"
    );

    // Migration: if meter_rates is empty, attempt to seed from legacy meter_rate table
    QSqlQuery countQuery(database());
    countQuery.exec("SELECT COUNT(*) FROM meter_rates");
    int count = 0;
    if (countQuery.next()) {
        count = countQuery.value(0).toInt();
    }

    if (count == 0) {
        // Check if legacy table exists
        QSqlQuery legacyCheck(database());
        legacyCheck.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='meter_rate'");
        if (legacyCheck.next()) {
            // Legacy table exists — attempt to read its rate
            QSqlQuery legacyRead(database());
            legacyRead.exec("SELECT rate FROM meter_rate WHERE id = 1");
            if (legacyRead.next()) {
                double legacyRate = legacyRead.value(0).toDouble();
                QSqlQuery insert(database());
                insert.prepare(
                    "INSERT INTO meter_rates (rate_value, created_at, updated_at) "
                    "VALUES (:rate, datetime('now','localtime'), datetime('now','localtime'))"
                );
                insert.bindValue(":rate", legacyRate);
                insert.exec();
            } else {
                // Legacy table exists but no row — seed default
                QSqlQuery insert(database());
                insert.prepare(
                    "INSERT INTO meter_rates (rate_value, created_at, updated_at) "
                    "VALUES (:rate, datetime('now','localtime'), datetime('now','localtime'))"
                );
                insert.bindValue(":rate", 0.69);
                insert.exec();
            }
        } else {
            // No legacy table — seed default
            QSqlQuery insert(database());
            insert.prepare(
                "INSERT INTO meter_rates (rate_value, created_at, updated_at) "
                "VALUES (:rate, datetime('now','localtime'), datetime('now','localtime'))"
            );
            insert.bindValue(":rate", 0.69);
            insert.exec();
        }
    }

    // Drop legacy table unconditionally after migration
    query.exec("DROP TABLE IF EXISTS meter_rate");
}

double MeterRateService::getCurrentMeterRate(double defaultValue)
{
    QSqlQuery query(database());
    query.prepare("SELECT rate_value FROM meter_rates ORDER BY created_at DESC LIMIT 1");

    if (query.exec() && query.next()) {
        return query.value(0).toDouble();
    }

    return defaultValue;
}

bool MeterRateService::updateMeterRateInDatabase(double newRate)
{
    QSqlQuery query(database());
    query.prepare(
        "INSERT INTO meter_rates (rate_value, created_at, updated_at) "
        "VALUES (:rate, datetime('now','localtime'), datetime('now','localtime'))"
    );
    query.bindValue(":rate", newRate);

    return query.exec();
}
