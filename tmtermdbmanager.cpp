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

// FIXED: Enhanced database table creation with proper schema
bool TMTermDBManager::createTables()
{
    if (!m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized");
        return false;
    }

    // Create main jobs table with all required columns
    QSqlQuery createJobsTable(m_dbManager->getDatabase());
    if (!createJobsTable.exec(
            "CREATE TABLE IF NOT EXISTS tm_term_jobs ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "job_number TEXT DEFAULT '', "
            "year TEXT NOT NULL, "
            "month TEXT NOT NULL, "
            "html_display_state INTEGER DEFAULT 0, "
            "job_data_locked INTEGER DEFAULT 0, "
            "postage_data_locked INTEGER DEFAULT 0, "
            "postage TEXT DEFAULT '', "
            "count TEXT DEFAULT '', "
            "last_executed_script TEXT DEFAULT '', "
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
            "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
            "UNIQUE(year, month)"
            ")")) {
        Logger::instance().error("Failed to create tm_term_jobs table: " +
                                 createJobsTable.lastError().text());
        return false;
    }

    // Create log table
    QSqlQuery createLogTable(m_dbManager->getDatabase());
    if (!createLogTable.exec(
            "CREATE TABLE IF NOT EXISTS tm_term_log ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "job_number TEXT NOT NULL, "
            "description TEXT NOT NULL, "
            "count TEXT NOT NULL, "
            "postage TEXT NOT NULL, "
            "per_piece TEXT NOT NULL, "
            "mail_class TEXT NOT NULL, "
            "permit TEXT NOT NULL, "
            "date TEXT NOT NULL, "
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ")")) {
        Logger::instance().error("Failed to create tm_term_log table: " +
                                 createLogTable.lastError().text());
        return false;
    }

    // Verify schema integrity
    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("PRAGMA table_info(tm_term_jobs)");
    if (!query.exec()) {
        Logger::instance().error("Failed to verify tm_term_jobs schema: " + query.lastError().text());
        return false;
    }

    bool hasPostage = false, hasCount = false, hasPostageLocked = false;
    while (query.next()) {
        QString column = query.value("name").toString();
        if (column == "postage") hasPostage = true;
        if (column == "count") hasCount = true;
        if (column == "postage_data_locked") hasPostageLocked = true;
    }

    if (!hasPostage || !hasCount || !hasPostageLocked) {
        Logger::instance().error("tm_term_jobs schema missing required columns for postage persistence");
        return false;
    }

    Logger::instance().info("TMTerm database tables created successfully");
    return true;
}

// FIXED: Simplified saveJob method
bool TMTermDBManager::saveJob(const QString& jobNumber, const QString& year, const QString& month)
{
    if (!m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for TMTerm saveJob");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());

    // First try to update existing record
    query.prepare("UPDATE tm_term_jobs SET "
                  "job_number = :job_number, "
                  "updated_at = :updated_at "
                  "WHERE year = :year AND month = :month");
    query.bindValue(":job_number", jobNumber);
    query.bindValue(":updated_at", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    if (!m_dbManager->executeQuery(query)) {
        Logger::instance().error(QString("Failed to update TMTerm job: %1 for %2/%3: %4")
                                     .arg(jobNumber, year, month, query.lastError().text()));
        return false;
    }

    // Check if any rows were affected (updated)
    if (query.numRowsAffected() == 0) {
        // No existing record found, insert new one
        query.prepare("INSERT INTO tm_term_jobs "
                      "(job_number, year, month, created_at, updated_at) "
                      "VALUES (:job_number, :year, :month, :created_at, :updated_at)");
        query.bindValue(":job_number", jobNumber);
        query.bindValue(":year", year);
        query.bindValue(":month", month);
        QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        query.bindValue(":created_at", currentTime);
        query.bindValue(":updated_at", currentTime);

        if (!m_dbManager->executeQuery(query)) {
            Logger::instance().error(QString("Failed to insert TMTerm job: %1 for %2/%3: %4")
                                         .arg(jobNumber, year, month, query.lastError().text()));
            return false;
        }
    }

    Logger::instance().info(QString("TMTerm job saved: %1 for %2/%3").arg(jobNumber, year, month));
    return true;
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
                                   int htmlDisplayState, bool jobDataLocked, bool postageDataLocked,
                                   const QString& postage, const QString& count, const QString& lastExecutedScript)
{
    if (!m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for TMTerm saveJobState");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());

    // FIXED: Simplified query without problematic subqueries - use UPDATE OR INSERT approach
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    // First try to update existing record
    query.prepare("UPDATE tm_term_jobs SET "
                  "html_display_state = :html_display_state, "
                  "job_data_locked = :job_data_locked, "
                  "postage_data_locked = :postage_data_locked, "
                  "postage = :postage, "
                  "count = :count, "
                  "last_executed_script = :last_executed_script, "
                  "updated_at = :updated_at "
                  "WHERE year = :year AND month = :month");

    query.bindValue(":html_display_state", htmlDisplayState);
    query.bindValue(":job_data_locked", jobDataLocked ? 1 : 0);
    query.bindValue(":postage_data_locked", postageDataLocked ? 1 : 0);
    query.bindValue(":postage", postage);
    query.bindValue(":count", count);
    query.bindValue(":last_executed_script", lastExecutedScript);
    query.bindValue(":updated_at", currentTime);
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    if (!m_dbManager->executeQuery(query)) {
        Logger::instance().error(QString("Failed to update TMTerm job state for %1/%2: %3")
                                     .arg(year, month, query.lastError().text()));
        return false;
    }

    // Check if any rows were affected (updated)
    if (query.numRowsAffected() == 0) {
        // No existing record found, insert new one
        query.prepare("INSERT INTO tm_term_jobs "
                      "(year, month, job_number, html_display_state, job_data_locked, "
                      "postage_data_locked, postage, count, last_executed_script, "
                      "created_at, updated_at) "
                      "VALUES (:year, :month, '', :html_display_state, :job_data_locked, "
                      ":postage_data_locked, :postage, :count, :last_executed_script, "
                      ":created_at, :updated_at)");

        query.bindValue(":year", year);
        query.bindValue(":month", month);
        query.bindValue(":html_display_state", htmlDisplayState);
        query.bindValue(":job_data_locked", jobDataLocked ? 1 : 0);
        query.bindValue(":postage_data_locked", postageDataLocked ? 1 : 0);
        query.bindValue(":postage", postage);
        query.bindValue(":count", count);
        query.bindValue(":last_executed_script", lastExecutedScript);
        query.bindValue(":created_at", currentTime);
        query.bindValue(":updated_at", currentTime);

        if (!m_dbManager->executeQuery(query)) {
            Logger::instance().error(QString("Failed to insert TMTerm job state for %1/%2: %3")
                                         .arg(year, month, query.lastError().text()));
            return false;
        }
    }

    Logger::instance().info(QString("TMTerm job state saved for %1/%2: postage=%3, count=%4, locked=%5")
                                .arg(year, month, postage, count, postageDataLocked ? "true" : "false"));
    return true;
}

bool TMTermDBManager::loadJobState(const QString& year, const QString& month,
                                   int& htmlDisplayState, bool& jobDataLocked, bool& postageDataLocked,
                                   QString& postage, QString& count, QString& lastExecutedScript)
{
    if (!m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for TMTerm loadJobState");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT html_display_state, job_data_locked, postage_data_locked, "
                  "postage, count, last_executed_script FROM tm_term_jobs "
                  "WHERE year = :year AND month = :month");
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    if (!m_dbManager->executeQuery(query)) {
        Logger::instance().error(QString("Failed to execute TMTerm loadJobState query for %1/%2: %3")
                                     .arg(year, month, query.lastError().text()));
        return false;
    }

    if (!query.next()) {
        // Set defaults if no record found
        htmlDisplayState = 0;
        jobDataLocked = false;
        postageDataLocked = false;
        postage = "";
        count = "";
        lastExecutedScript = "";
        Logger::instance().info(QString("No TMTerm job state found for %1/%2, using defaults").arg(year, month));
        return false;
    }

    // Load values from database
    htmlDisplayState = query.value("html_display_state").toInt();
    jobDataLocked = query.value("job_data_locked").toInt() == 1;
    postageDataLocked = query.value("postage_data_locked").toInt() == 1;
    postage = query.value("postage").toString();
    count = query.value("count").toString();
    lastExecutedScript = query.value("last_executed_script").toString();

    Logger::instance().info(QString("TMTerm job state loaded for %1/%2: postage=%3, count=%4, locked=%5")
                                .arg(year, month, postage, count, postageDataLocked ? "true" : "false"));
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
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(R"(
        INSERT INTO tm_term_log
        (job_number, description, postage, count, per_piece, mail_class, shape, permit, date, created_at)
        VALUES (:job_number, :description, :postage, :count, :per_piece, :mail_class, :shape, :permit, :date, :created_at)
    )");

    query.bindValue(":job_number", jobNumber);
    query.bindValue(":description", description);
    query.bindValue(":postage", postage);
    query.bindValue(":count", count);
    query.bindValue(":per_piece", perPiece);
    query.bindValue(":mail_class", mailClass);
    query.bindValue(":shape", shape);
    query.bindValue(":permit", permit);
    query.bindValue(":date", date);
    query.bindValue(":created_at", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    bool success = query.exec();
    if (!success) {
        qDebug() << "Failed to add log entry:" << query.lastError().text();
        Logger::instance().error("Failed to add TERM log entry: " + query.lastError().text());
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
