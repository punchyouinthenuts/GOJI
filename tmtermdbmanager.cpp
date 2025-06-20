#include "tmtermdbmanager.h"
#include <QDateTime>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include "logger.h"

// Initialize static member
TMTermDBManager* TMTermDBManager::m_instance = nullptr;

TMTermDBManager* TMTermDBManager::instance()
{
    if (!m_instance) {
        m_instance = new TMTermDBManager();
    }
    return m_instance;
}

TMTermDBManager::TMTermDBManager()
{
    m_dbManager = DatabaseManager::instance();
}

bool TMTermDBManager::initialize()
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Core database manager not initialized";
        Logger::instance().error("Core database manager not initialized for TMTerm");
        return false;
    }

    return createTables();
}

bool TMTermDBManager::createTables()
{
    // Create tm_term_jobs table (no week column compared to TMWPC)
    if (!m_dbManager->createTable("tm_term_jobs",
                                  "("
                                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                  "job_number TEXT NOT NULL, "
                                  "year TEXT NOT NULL, "
                                  "month TEXT NOT NULL, "
                                  "html_display_state INTEGER DEFAULT 0, "
                                  "job_data_locked INTEGER DEFAULT 0, "
                                  "postage_data_locked INTEGER DEFAULT 0, "
                                  "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                                  "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                                  "UNIQUE(year, month)"
                                  ")")) {
        qDebug() << "Error creating tm_term_jobs table";
        Logger::instance().error("Failed to create tm_term_jobs table");
        return false;
    }

    // Create tm_term_log table (same structure as tm_weekly_log for consistency)
    if (!m_dbManager->createTable("tm_term_log",
                                  "("
                                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                  "job_number TEXT NOT NULL, "
                                  "description TEXT NOT NULL, "
                                  "postage TEXT NOT NULL, "
                                  "count TEXT NOT NULL, "
                                  "per_piece TEXT NOT NULL, "
                                  "class TEXT NOT NULL, "
                                  "shape TEXT NOT NULL, "
                                  "permit TEXT NOT NULL, "
                                  "date TEXT NOT NULL, "
                                  "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                                  ")")) {
        qDebug() << "Error creating tm_term_log table";
        Logger::instance().error("Failed to create tm_term_log table");
        return false;
    }

    Logger::instance().info("TMTerm database tables created successfully");
    return true;
}

bool TMTermDBManager::saveJob(const QString& jobNumber, const QString& year, const QString& month)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for TMTerm saveJob");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("INSERT OR REPLACE INTO tm_term_jobs "
                  "(job_number, year, month, updated_at) "
                  "VALUES (:job_number, :year, :month, :updated_at)");
    query.bindValue(":job_number", jobNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":updated_at", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    bool success = m_dbManager->executeQuery(query);
    if (success) {
        Logger::instance().info(QString("TMTerm job saved: %1 for %2/%3").arg(jobNumber, year, month));
    } else {
        Logger::instance().error(QString("Failed to save TMTerm job: %1 for %2/%3").arg(jobNumber, year, month));
    }
    return success;
}

bool TMTermDBManager::loadJob(const QString& year, const QString& month, QString& jobNumber)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for TMTerm loadJob");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT job_number FROM tm_term_jobs "
                  "WHERE year = :year AND month = :month");
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    if (!m_dbManager->executeQuery(query)) {
        Logger::instance().error(QString("Failed to execute TMTerm loadJob query for %1/%2").arg(year, month));
        return false;
    }

    if (!query.next()) {
        Logger::instance().warning(QString("No TMTerm job found for %1/%2").arg(year, month));
        return false;
    }

    jobNumber = query.value("job_number").toString();
    Logger::instance().info(QString("TMTerm job loaded: %1 for %2/%3").arg(jobNumber, year, month));
    return true;
}

bool TMTermDBManager::deleteJob(const QString& year, const QString& month)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for TMTerm deleteJob");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("DELETE FROM tm_term_jobs "
                  "WHERE year = :year AND month = :month");
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    bool success = m_dbManager->executeQuery(query);
    if (success) {
        Logger::instance().info(QString("TMTerm job deleted for %1/%2").arg(year, month));
    } else {
        Logger::instance().error(QString("Failed to delete TMTerm job for %1/%2").arg(year, month));
    }
    return success;
}

bool TMTermDBManager::jobExists(const QString& year, const QString& month)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT COUNT(*) FROM tm_term_jobs "
                  "WHERE year = :year AND month = :month");
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    if (!m_dbManager->executeQuery(query)) {
        return false;
    }

    if (!query.next()) {
        return false;
    }

    return query.value(0).toInt() > 0;
}

QList<QMap<QString, QString>> TMTermDBManager::getAllJobs()
{
    QList<QMap<QString, QString>> jobs;

    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for TMTerm getAllJobs");
        return jobs;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT job_number, year, month FROM tm_term_jobs "
                  "ORDER BY year DESC, month DESC");

    if (!m_dbManager->executeQuery(query)) {
        Logger::instance().error("Failed to execute TMTerm getAllJobs query");
        return jobs;
    }

    while (query.next()) {
        QMap<QString, QString> job;
        job["job_number"] = query.value("job_number").toString();
        job["year"] = query.value("year").toString();
        job["month"] = query.value("month").toString();
        jobs.append(job);
    }

    Logger::instance().info(QString("Retrieved %1 TMTerm jobs from database").arg(jobs.size()));
    return jobs;
}

bool TMTermDBManager::saveJobState(const QString& year, const QString& month,
                                   int htmlDisplayState, bool jobDataLocked, bool postageDataLocked)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for TMTerm saveJobState");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("UPDATE tm_term_jobs SET "
                  "html_display_state = :html_display_state, "
                  "job_data_locked = :job_data_locked, "
                  "postage_data_locked = :postage_data_locked, "
                  "updated_at = :updated_at "
                  "WHERE year = :year AND month = :month");
    query.bindValue(":html_display_state", htmlDisplayState);
    query.bindValue(":job_data_locked", jobDataLocked ? 1 : 0);
    query.bindValue(":postage_data_locked", postageDataLocked ? 1 : 0);
    query.bindValue(":updated_at", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    bool success = m_dbManager->executeQuery(query);
    if (success) {
        Logger::instance().info(QString("TMTerm job state saved for %1/%2").arg(year, month));
    } else {
        Logger::instance().error(QString("Failed to save TMTerm job state for %1/%2").arg(year, month));
    }
    return success;
}

bool TMTermDBManager::loadJobState(const QString& year, const QString& month,
                                   int& htmlDisplayState, bool& jobDataLocked, bool& postageDataLocked)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for TMTerm loadJobState");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT html_display_state, job_data_locked, postage_data_locked FROM tm_term_jobs "
                  "WHERE year = :year AND month = :month");
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    if (!m_dbManager->executeQuery(query)) {
        Logger::instance().error(QString("Failed to execute TMTerm loadJobState query for %1/%2").arg(year, month));
        return false;
    }

    if (!query.next()) {
        // No job found, set defaults
        htmlDisplayState = 0; // DefaultState
        jobDataLocked = false;
        postageDataLocked = false;
        Logger::instance().warning(QString("No TMTerm job state found for %1/%2, using defaults").arg(year, month));
        return false;
    }

    htmlDisplayState = query.value("html_display_state").toInt();
    jobDataLocked = query.value("job_data_locked").toInt() == 1;
    postageDataLocked = query.value("postage_data_locked").toInt() == 1;

    Logger::instance().info(QString("TMTerm job state loaded for %1/%2").arg(year, month));
    return true;
}

bool TMTermDBManager::addLogEntry(const QString& jobNumber, const QString& description,
                                  const QString& postage, const QString& count,
                                  const QString& perPiece, const QString& mailClass,
                                  const QString& shape, const QString& permit,
                                  const QString& date)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for TMTerm addLogEntry");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("INSERT INTO tm_term_log "
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

    bool success = m_dbManager->executeQuery(query);
    if (success) {
        Logger::instance().info(QString("TMTerm log entry added for job %1").arg(jobNumber));
    } else {
        Logger::instance().error(QString("Failed to add TMTerm log entry for job %1").arg(jobNumber));
    }
    return success;
}

QList<QMap<QString, QVariant>> TMTermDBManager::getLog()
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for TMTerm getLog");
        return QList<QMap<QString, QVariant>>();
    }

    QList<QMap<QString, QVariant>> logs = m_dbManager->executeSelectQuery(
        "SELECT * FROM tm_term_log ORDER BY id DESC"
        );

    Logger::instance().info(QString("Retrieved %1 TMTerm log entries").arg(logs.size()));
    return logs;
}

bool TMTermDBManager::saveTerminalLog(const QString& year, const QString& month, const QString& message)
{
    // Use empty string for week since TERM doesn't have weeks
    bool success = m_dbManager->saveTerminalLog(TAB_NAME, year, month, "", message);
    if (success) {
        Logger::instance().info(QString("TMTerm terminal log saved for %1/%2").arg(year, month));
    } else {
        Logger::instance().error(QString("Failed to save TMTerm terminal log for %1/%2").arg(year, month));
    }
    return success;
}

QStringList TMTermDBManager::getTerminalLogs(const QString& year, const QString& month)
{
    // Use empty string for week since TERM doesn't have weeks
    QStringList logs = m_dbManager->getTerminalLogs(TAB_NAME, year, month, "");
    Logger::instance().info(QString("Retrieved %1 TMTerm terminal log entries for %2/%3").arg(logs.size()).arg(year, month));
    return logs;
}
