#include "tmfarmdbmanager.h"
#include "logger.h"
#include "databasemanager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QVariant>
#include <QDir>

namespace {
    TMFarmDBManager* g_instance = nullptr;
}

TMFarmDBManager* TMFarmDBManager::instance()
{
    if (!g_instance) {
        g_instance = new TMFarmDBManager(nullptr);
    }
    return g_instance;
}

TMFarmDBManager::TMFarmDBManager(QObject *parent)
    : QObject(parent), m_initialized(false)
{
    DatabaseManager* dbm = DatabaseManager::instance();
    if (!dbm || !dbm->isInitialized()) {
        Logger::instance().warning("DatabaseManager not initialized; attempting local SQLite for FARMWORKERS");
        QSqlDatabase db = QSqlDatabase::database();
        if (!db.isValid()) {
            db = QSqlDatabase::addDatabase("QSQLITE");
            QString fallbackPath = "C:/Goji/TRACHMAR/FARMWORKERS/farmworkers.sqlite";
            QDir().mkpath("C:/Goji/TRACHMAR/FARMWORKERS");
            db.setDatabaseName(fallbackPath);
        }
        if (!db.isOpen()) {
            if (!db.open()) {
                Logger::instance().error("Failed to open SQLite database for FARMWORKERS: " + db.lastError().text());
                m_initialized = false;
                return;
            }
        }
        m_db = db;
    } else {
        m_db = dbm->getDatabase();
    }

    m_initialized = ensureTables();
    if (m_initialized) {
        Logger::instance().info("TM FARMWORKERS database initialized");
    } else {
        Logger::instance().error("TM FARMWORKERS database failed to initialize");
    }
}

bool TMFarmDBManager::isInitialized() const { return m_initialized; }
QSqlDatabase TMFarmDBManager::getDatabase() const { return m_db; }

static bool execQuery(QSqlQuery& q, const QString& sql)
{
    if (!q.exec(sql)) {
        Logger::instance().error("SQL error: " + q.lastError().text() + " | SQL: " + sql);
        return false;
    }
    return true;
}

bool TMFarmDBManager::ensureTables()
{
    QSqlQuery q(m_db);
    bool ok = true;

    ok &= execQuery(q, R"SQL(
        CREATE TABLE IF NOT EXISTS tm_farm_job (
            year TEXT NOT NULL,
            quarter TEXT NOT NULL,
            job_number TEXT NOT NULL,
            PRIMARY KEY (year, quarter)
        )
    )SQL");

    ok &= execQuery(q, R"SQL(
        CREATE TABLE IF NOT EXISTS tm_farm_state (
            year TEXT NOT NULL,
            quarter TEXT NOT NULL,
            html_state INTEGER NOT NULL,
            job_locked INTEGER NOT NULL,
            postage_locked INTEGER NOT NULL,
            postage TEXT,
            count TEXT,
            last_script TEXT,
            PRIMARY KEY (year, quarter)
        )
    )SQL");

    ok &= execQuery(q, R"SQL(
        CREATE TABLE IF NOT EXISTS tm_farm_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            job TEXT,
            description TEXT,
            postage TEXT,
            count TEXT,
            avg_rate TEXT,
            mail_class TEXT,
            shape TEXT,
            permit TEXT,
            date TEXT,
            year TEXT,
            quarter TEXT
        )
    )SQL");

    return ok;
}

bool TMFarmDBManager::saveJob(const QString& jobNumber, const QString& year, const QString& quarter)
{
    QSqlQuery q(m_db);
    q.prepare(R"SQL(
        INSERT INTO tm_farm_job (year, quarter, job_number)
        VALUES (:year, :quarter, :job)
        ON CONFLICT(year, quarter) DO UPDATE SET job_number=excluded.job_number
    )SQL");
    q.bindValue(":year", year);
    q.bindValue(":quarter", quarter);
    q.bindValue(":job", jobNumber);
    if (!q.exec()) {
        Logger::instance().error("saveJob failed: " + q.lastError().text());
        return false;
    }
    Logger::instance().info(QString("Saved FARMWORKERS job %1 for %2/%3").arg(jobNumber, year, quarter));
    return true;
}

bool TMFarmDBManager::loadJob(const QString& year, const QString& quarter, QString& outJobNumber)
{
    QSqlQuery q(m_db);
    q.prepare(R"SQL(
        SELECT job_number FROM tm_farm_job WHERE year=:year AND quarter=:quarter
    )SQL");
    q.bindValue(":year", year);
    q.bindValue(":quarter", quarter);
    if (!q.exec()) {
        Logger::instance().error("loadJob failed: " + q.lastError().text());
        return false;
    }
    if (q.next()) {
        outJobNumber = q.value(0).toString();
        return true;
    }
    return false;
}

bool TMFarmDBManager::saveJobState(const QString& year, const QString& quarter,
                                   int htmlState, bool jobLocked, bool postageLocked,
                                   const QString& postage, const QString& count,
                                   const QString& lastExecutedScript)
{
    QSqlQuery q(m_db);
    q.prepare(R"SQL(
        INSERT INTO tm_farm_state (year, quarter, html_state, job_locked, postage_locked, postage, count, last_script)
        VALUES (:year, :quarter, :html_state, :job_locked, :postage_locked, :postage, :count, :last_script)
        ON CONFLICT(year, quarter) DO UPDATE SET
            html_state=excluded.html_state,
            job_locked=excluded.job_locked,
            postage_locked=excluded.postage_locked,
            postage=excluded.postage,
            count=excluded.count,
            last_script=excluded.last_script
    )SQL");
    q.bindValue(":year", year);
    q.bindValue(":quarter", quarter);
    q.bindValue(":html_state", htmlState);
    q.bindValue(":job_locked", jobLocked ? 1 : 0);
    q.bindValue(":postage_locked", postageLocked ? 1 : 0);
    q.bindValue(":postage", postage);
    q.bindValue(":count", count);
    q.bindValue(":last_script", lastExecutedScript);
    if (!q.exec()) {
        Logger::instance().error("saveJobState failed: " + q.lastError().text());
        return false;
    }
    return true;
}

bool TMFarmDBManager::loadJobState(const QString& year, const QString& quarter,
                                   int& htmlState, bool& jobLocked, bool& postageLocked,
                                   QString& postage, QString& count, QString& lastExecutedScript)
{
    QSqlQuery q(m_db);
    q.prepare(R"SQL(
        SELECT html_state, job_locked, postage_locked, postage, count, last_script
        FROM tm_farm_state
        WHERE year=:year AND quarter=:quarter
    )SQL");
    q.bindValue(":year", year);
    q.bindValue(":quarter", quarter);
    if (!q.exec()) {
        Logger::instance().error("loadJobState failed: " + q.lastError().text());
        return false;
    }
    if (!q.next()) {
        return false;
    }
    htmlState = q.value(0).toInt();
    jobLocked = q.value(1).toInt() != 0;
    postageLocked = q.value(2).toInt() != 0;
    postage = q.value(3).toString();
    count = q.value(4).toString();
    lastExecutedScript = q.value(5).toString();
    return true;
}

bool TMFarmDBManager::addLogEntry(const QString& jobNumber, const QString& description,
                                  const QString& formattedPostage, const QString& formattedCount,
                                  const QString& formattedAvgRate, const QString& mailClass,
                                  const QString& shape, const QString& permit, const QString& date,
                                  const QString& year, const QString& quarter)
{
    QSqlQuery q(m_db);
    q.prepare(R"SQL(
        INSERT INTO tm_farm_log
            (job, description, postage, count, avg_rate, mail_class, shape, permit, date, year, quarter)
        VALUES
            (:job, :description, :postage, :count, :avg_rate, :mail_class, :shape, :permit, :date, :year, :quarter)
    )SQL");
    q.bindValue(":job", jobNumber);
    q.bindValue(":description", description);
    q.bindValue(":postage", formattedPostage);
    q.bindValue(":count", formattedCount);
    q.bindValue(":avg_rate", formattedAvgRate);
    q.bindValue(":mail_class", mailClass);
    q.bindValue(":shape", shape);
    q.bindValue(":permit", permit);
    q.bindValue(":date", date);
    q.bindValue(":year", year);
    q.bindValue(":quarter", quarter);
    if (!q.exec()) {
        Logger::instance().error("addLogEntry failed: " + q.lastError().text());
        return false;
    }
    return true;
}

bool TMFarmDBManager::updateLogEntryForJob(const QString& jobNumber, const QString& description,
                                           const QString& formattedPostage, const QString& formattedCount,
                                           const QString& formattedAvgRate, const QString& mailClass,
                                           const QString& shape, const QString& permit, const QString& date,
                                           const QString& year, const QString& quarter)
{
    QSqlQuery q(m_db);
    q.prepare(R"SQL(
        UPDATE tm_farm_log
           SET description=:description,
               postage=:postage,
               count=:count,
               avg_rate=:avg_rate,
               mail_class=:mail_class,
               shape=:shape,
               permit=:permit,
               date=:date
         WHERE job=:job AND year=:year AND quarter=:quarter
    )SQL");
    q.bindValue(":description", description);
    q.bindValue(":postage", formattedPostage);
    q.bindValue(":count", formattedCount);
    q.bindValue(":avg_rate", formattedAvgRate);
    q.bindValue(":mail_class", mailClass);
    q.bindValue(":shape", shape);
    q.bindValue(":permit", permit);
    q.bindValue(":date", date);
    q.bindValue(":job", jobNumber);
    q.bindValue(":year", year);
    q.bindValue(":quarter", quarter);
    if (!q.exec()) {
        Logger::instance().error("updateLogEntryForJob failed: " + q.lastError().text());
        return false;
    }
    return q.numRowsAffected() > 0;
}

bool TMFarmDBManager::updateLogJobNumber(const QString& oldJobNumber, const QString& newJobNumber)
{
    QSqlQuery q(m_db);
    q.prepare(R"SQL(
        UPDATE tm_farm_log SET job=:newJob WHERE job=:oldJob
    )SQL");
    q.bindValue(":newJob", newJobNumber);
    q.bindValue(":oldJob", oldJobNumber);
    if (!q.exec()) {
        Logger::instance().error("updateLogJobNumber (log) failed: " + q.lastError().text());
        return false;
    }
    QSqlQuery q2(m_db);
    q2.prepare(R"SQL(
        UPDATE tm_farm_job SET job_number=:newJob WHERE job_number=:oldJob
    )SQL");
    q2.bindValue(":newJob", newJobNumber);
    q2.bindValue(":oldJob", oldJobNumber);
    if (!q2.exec()) {
        Logger::instance().error("updateLogJobNumber (job table) failed: " + q2.lastError().text());
        return false;
    }
    return true;
}


QList<QMap<QString, QString>> TMFarmDBManager::getAllJobs() const
{
    QList<QMap<QString, QString>> rows;
    if (!m_db.isOpen()) return rows;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT year, quarter, job_number FROM tm_farm_job ORDER BY year DESC, quarter DESC"))) {
        Logger::instance().error("getAllJobs failed: " + q.lastError().text());
        return rows;
    }
    while (q.next()) {
        QMap<QString, QString> m;
        m["year"] = q.value(0).toString();
        m["quarter"] = q.value(1).toString();
        m["job_number"] = q.value(2).toString();
        rows.append(m);
    }
    return rows;
}
