#include "tmweeklypcdbmanager.h"
#include <QDateTime>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>

// Initialize static member
TMWeeklyPCDBManager* TMWeeklyPCDBManager::m_instance = nullptr;

TMWeeklyPCDBManager* TMWeeklyPCDBManager::instance()
{
    if (!m_instance) {
        m_instance = new TMWeeklyPCDBManager();
    }
    return m_instance;
}

TMWeeklyPCDBManager::TMWeeklyPCDBManager()
{
    m_dbManager = DatabaseManager::instance();
}

bool TMWeeklyPCDBManager::initialize()
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Core database manager not initialized";
        return false;
    }

    return createTables();
}

bool TMWeeklyPCDBManager::createTables()
{
    // Create tm_weekly_jobs table
    if (!m_dbManager->createTable("tm_weekly_jobs",
                                  "("
                                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                  "job_number TEXT NOT NULL, "
                                  "year TEXT NOT NULL, "
                                  "month TEXT NOT NULL, "
                                  "week TEXT NOT NULL, "
                                  "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                                  "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                                  "UNIQUE(year, month, week)"
                                  ")")) {
        qDebug() << "Error creating tm_weekly_jobs table";
        return false;
    }

    // Create tm_weekly_log table with updated schema including shape column
    if (!m_dbManager->createTable("tm_weekly_log",
                                  "("
                                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                  "job_number TEXT NOT NULL, "
                                  "description TEXT NOT NULL, "
                                  "postage TEXT, "
                                  "count TEXT, "
                                  "per_piece TEXT, "
                                  "class TEXT, "
                                  "shape TEXT, "
                                  "permit TEXT, "
                                  "date TEXT, "
                                  "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                                  ")")) {
        qDebug() << "Error creating tm_weekly_log table";
        return false;
    }

    return true;
}

bool TMWeeklyPCDBManager::saveJob(const QString& jobNumber, const QString& year,
                                  const QString& month, const QString& week)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    // Validate inputs
    if (!m_dbManager->validateInput(jobNumber) ||
        !m_dbManager->validateInput(year) ||
        !m_dbManager->validateInput(month) ||
        !m_dbManager->validateInput(week)) {
        qDebug() << "Invalid input data";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("INSERT OR REPLACE INTO tm_weekly_jobs "
                  "(job_number, year, month, week, updated_at) "
                  "VALUES (:job_number, :year, :month, :week, :updated_at)");
    query.bindValue(":job_number", jobNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", week);
    query.bindValue(":updated_at", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    return m_dbManager->executeQuery(query);
}

bool TMWeeklyPCDBManager::loadJob(const QString& year, const QString& month,
                                  const QString& week, QString& jobNumber)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT job_number FROM tm_weekly_jobs "
                  "WHERE year = :year AND month = :month AND week = :week");
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", week);

    if (!m_dbManager->executeQuery(query)) {
        return false;
    }

    if (!query.next()) {
        qDebug() << "No job found for" << year << month << week;
        return false;
    }

    jobNumber = query.value("job_number").toString();
    return true;
}

bool TMWeeklyPCDBManager::deleteJob(const QString& year, const QString& month, const QString& week)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("DELETE FROM tm_weekly_jobs "
                  "WHERE year = :year AND month = :month AND week = :week");
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", week);

    return m_dbManager->executeQuery(query);
}

bool TMWeeklyPCDBManager::jobExists(const QString& year, const QString& month, const QString& week)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT COUNT(*) FROM tm_weekly_jobs "
                  "WHERE year = :year AND month = :month AND week = :week");
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", week);

    if (!m_dbManager->executeQuery(query) || !query.next()) {
        return false;
    }

    return query.value(0).toInt() > 0;
}

QList<QMap<QString, QString>> TMWeeklyPCDBManager::getAllJobs()
{
    QList<QMap<QString, QString>> result;

    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return result;
    }

    QList<QMap<QString, QVariant>> queryResult = m_dbManager->executeSelectQuery(
        "SELECT year, month, week, job_number FROM tm_weekly_jobs "
        "ORDER BY year DESC, month DESC, week DESC"
        );

    for (const QMap<QString, QVariant>& row : queryResult) {
        QMap<QString, QString> job;
        job["year"] = row["year"].toString();
        job["month"] = row["month"].toString();
        job["week"] = row["week"].toString();
        job["job_number"] = row["job_number"].toString();
        result.append(job);
    }

    return result;
}

bool TMWeeklyPCDBManager::addLogEntry(const QString& jobNumber, const QString& description,
                                      const QString& postage, const QString& count,
                                      const QString& perPiece, const QString& mailClass,
                                      const QString& shape, const QString& permit,
                                      const QString& date)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("INSERT INTO tm_weekly_log "
                  "(job_number, description, postage, count, per_piece, class, shape, permit, date) "
                  "VALUES (:job_number, :description, :postage, :count, :per_piece, :class, :shape, :permit, :date)");
    query.bindValue(":job_number", jobNumber);
    query.bindValue(":description", description);
    query.bindValue(":postage", postage);
    query.bindValue(":count", count);
    query.bindValue(":per_piece", perPiece);
    query.bindValue(":class", mailClass);
    query.bindValue(":shape", shape);
    query.bindValue(":permit", permit);
    query.bindValue(":date", date);

    return m_dbManager->executeQuery(query);
}

QList<QMap<QString, QVariant>> TMWeeklyPCDBManager::getLog()
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return QList<QMap<QString, QVariant>>();
    }

    return m_dbManager->executeSelectQuery(
        "SELECT * FROM tm_weekly_log ORDER BY id DESC"
        );
}

bool TMWeeklyPCDBManager::saveTerminalLog(const QString& year, const QString& month,
                                          const QString& week, const QString& message)
{
    return m_dbManager->saveTerminalLog(TAB_NAME, year, month, week, message);
}

QStringList TMWeeklyPCDBManager::getTerminalLogs(const QString& year, const QString& month,
                                                 const QString& week)
{
    return m_dbManager->getTerminalLogs(TAB_NAME, year, month, week);
}
