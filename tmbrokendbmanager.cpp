#include "tmbrokendbmanager.h"
#include "logger.h"
#include "DatabaseManager.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QApplication>
#include <QMutexLocker>

// Static member initialization
TMBrokenDBManager* TMBrokenDBManager::m_instance = nullptr;
QMutex TMBrokenDBManager::m_mutex;

// Table name constants
const QString TMBrokenDBManager::JOB_DATA_TABLE = "tm_broken_job_data";
const QString TMBrokenDBManager::LOG_TABLE = "tm_broken_log";

TMBrokenDBManager* TMBrokenDBManager::instance()
{
    QMutexLocker locker(&m_mutex);
    if (!m_instance) {
        m_instance = new TMBrokenDBManager();
    }
    return m_instance;
}

TMBrokenDBManager::TMBrokenDBManager(QObject *parent)
    : QObject(parent),
      m_initialized(false),
      m_dbManager(nullptr)
{
    // Constructor body - initialization happens in initializeDatabase()
}

TMBrokenDBManager::~TMBrokenDBManager()
{
    if (m_database.isOpen()) {
        m_database.close();
    }
}

bool TMBrokenDBManager::initializeDatabase()
{
    if (m_initialized) {
        return true;
    }

    // Use shared DatabaseManager that connects to goji.db
    m_dbManager = DatabaseManager::instance();

    if (!m_dbManager || !m_dbManager->isInitialized()) {
        m_lastError = "Failed to initialize shared DatabaseManager";
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return false;
    }

    // Use the shared database connection instead of opening a local file
    m_database = m_dbManager->getDatabase();

    // Tables must still be created if missing (only once)
    if (!createTables()) {
        m_lastError = "Failed to create database tables";
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return false;
    }

    if (!createIndexes()) {
        m_lastError = "Failed to create database indexes";
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return false;
    }

    m_initialized = true;
    Logger::instance().info("TMBrokenDBManager: Database initialized using shared goji.db");
    return true;
}

bool TMBrokenDBManager::isInitialized() const
{
    return m_initialized;
}

bool TMBrokenDBManager::createTables()
{
    if (!createJobDataTable()) {
        return false;
    }
    
    if (!createLogTable()) {
        return false;
    }
    
    return true;
}

bool TMBrokenDBManager::createJobDataTable()
{
    QString sql = QString(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "job_number VARCHAR(50) NOT NULL, "
        "year VARCHAR(4) NOT NULL, "
        "month VARCHAR(2) NOT NULL, "
        "postage TEXT, "
        "count TEXT, "
        "job_data_locked INTEGER DEFAULT 0, "
        "postage_data_locked INTEGER DEFAULT 0, "
        "html_display_state TEXT, "
        "last_executed_script TEXT, "
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
        "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
        "UNIQUE(year, month)"
        ")"
    ).arg(JOB_DATA_TABLE);

    QSqlQuery query(m_dbManager->getDatabase());

    if (!query.exec(sql)) {
        m_lastError = "Failed to create job data table: " + query.lastError().text();
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return false;
    }

    return true;
}

bool TMBrokenDBManager::createLogTable()
{
    QString sql = QString(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "job_number VARCHAR(50), "
        "description TEXT, "
        "postage TEXT, "
        "count TEXT, "
        "per_piece TEXT, "
        "mail_class VARCHAR(50), "
        "shape VARCHAR(50), "
        "permit VARCHAR(50), "
        "date DATE, "
        "year VARCHAR(4), "
        "month VARCHAR(2), "
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")"
    ).arg(LOG_TABLE);

    QSqlQuery query(m_dbManager->getDatabase());

    if (!query.exec(sql)) {
        m_lastError = "Failed to create log table: " + query.lastError().text();
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return false;
    }

    // Add idempotent ALTER TABLE migrations for year/month columns
    // These will fail silently if columns already exist
    QSqlQuery alterQuery(m_dbManager->getDatabase());
    alterQuery.exec(QString("ALTER TABLE %1 ADD COLUMN year VARCHAR(4)").arg(LOG_TABLE));
    alterQuery.exec(QString("ALTER TABLE %1 ADD COLUMN month VARCHAR(2)").arg(LOG_TABLE));

    return true;
}

bool TMBrokenDBManager::createIndexes()
{
    QStringList indexQueries = {
        QString("CREATE INDEX IF NOT EXISTS idx_%1_year_month ON %1(year, month)").arg(JOB_DATA_TABLE),
        QString("CREATE INDEX IF NOT EXISTS idx_%1_job_number ON %1(job_number)").arg(JOB_DATA_TABLE),
        QString("CREATE INDEX IF NOT EXISTS idx_%1_date ON %1(date)").arg(LOG_TABLE),
        QString("CREATE INDEX IF NOT EXISTS idx_%1_job_number ON %1(job_number)").arg(LOG_TABLE)
    };

    QSqlQuery query(m_dbManager->getDatabase());

    for (const QString& indexSql : indexQueries) {
        if (!query.exec(indexSql)) {
            m_lastError = "Failed to create index: " + query.lastError().text();
            Logger::instance().error("TMBrokenDBManager: " + m_lastError);
            return false;
        }
    }

    return true;
}

bool TMBrokenDBManager::saveJob(const QString& jobNumber, const QString& year, const QString& month)
{
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());

    // Use non-destructive UPSERT to preserve other columns (postage, count, locks, etc.)
    QString sql = QString(
        "INSERT INTO %1 (job_number, year, month, updated_at) "
        "VALUES (?, ?, ?, ?) "
        "ON CONFLICT(year, month) DO UPDATE SET "
        "  updated_at = excluded.updated_at, "
        "  job_number = excluded.job_number"
    ).arg(JOB_DATA_TABLE);
    
    query.prepare(sql);
    query.addBindValue(jobNumber);
    query.addBindValue(year);
    query.addBindValue(month);
    query.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    if (!query.exec()) {
        // Fallback for SQLite < 3.24: Use INSERT OR IGNORE + UPDATE pattern
        query.clear();
        
        // First, try to insert if not exists
        QString insertSql = QString(
            "INSERT OR IGNORE INTO %1 (job_number, year, month, updated_at) "
            "VALUES (?, ?, ?, ?)"
        ).arg(JOB_DATA_TABLE);
        
        query.prepare(insertSql);
        query.addBindValue(jobNumber);
        query.addBindValue(year);
        query.addBindValue(month);
        query.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
        
        if (!query.exec()) {
            m_lastError = "Failed to insert job: " + query.lastError().text();
            Logger::instance().error("TMBrokenDBManager: " + m_lastError);
            return false;
        }
        
        // Then update the record (this won't affect other columns)
        QString updateSql = QString(
            "UPDATE %1 SET updated_at=?, job_number=? WHERE year=? AND month=?"
        ).arg(JOB_DATA_TABLE);
        
        query.prepare(updateSql);
        query.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
        query.addBindValue(jobNumber);
        query.addBindValue(year);
        query.addBindValue(month);
        
        if (!query.exec()) {
            m_lastError = "Failed to update job: " + query.lastError().text();
            Logger::instance().error("TMBrokenDBManager: " + m_lastError);
            return false;
        }
    }

    Logger::instance().info(QString("TMBroken job saved: %1 for %2/%3").arg(jobNumber, year, month));
    return true;
}

bool TMBrokenDBManager::saveJobData(const QVariantMap& jobData)
{
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());

    QString sql = QString(
        "INSERT OR REPLACE INTO %1 "
        "(job_number, year, month, postage, count, job_data_locked, postage_data_locked, "
        "html_display_state, last_executed_script, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
    ).arg(JOB_DATA_TABLE);

    query.prepare(sql);
    query.addBindValue(jobData["job_number"]);
    query.addBindValue(jobData["year"]);
    query.addBindValue(jobData["month"]);
    query.addBindValue(jobData["postage"]);
    query.addBindValue(jobData["count"]);
    query.addBindValue(jobData["job_data_locked"].toBool() ? 1 : 0);
    query.addBindValue(jobData["postage_data_locked"].toBool() ? 1 : 0);
    query.addBindValue(jobData["html_display_state"]);
    query.addBindValue(jobData["last_executed_script"]);
    query.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    if (!query.exec()) {
        m_lastError = "Failed to save job data: " + query.lastError().text();
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return false;
    }

    return true;
}

QVariantMap TMBrokenDBManager::loadJobData(const QString& year, const QString& month)
{
    QVariantMap result;
    
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return result;
    }

    QSqlQuery query(m_dbManager->getDatabase());

    QString sql = QString("SELECT * FROM %1 WHERE year = ? AND month = ?").arg(JOB_DATA_TABLE);
    
    query.prepare(sql);
    query.addBindValue(year);
    query.addBindValue(month);

    if (!query.exec()) {
        m_lastError = "Failed to load job data: " + query.lastError().text();
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return result;
    }

    if (query.next()) {
        QSqlRecord record = query.record();
        for (int i = 0; i < record.count(); ++i) {
            result[record.fieldName(i)] = record.value(i);
        }
    }

    return result;
}

bool TMBrokenDBManager::deleteJobData(const QString& year, const QString& month)
{
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());

    QString sql = QString("DELETE FROM %1 WHERE year = ? AND month = ?").arg(JOB_DATA_TABLE);
    
    query.prepare(sql);
    query.addBindValue(year);
    query.addBindValue(month);

    if (!query.exec()) {
        m_lastError = "Failed to delete job data: " + query.lastError().text();
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return false;
    }

    return true;
}

bool TMBrokenDBManager::addLogEntry(const QVariantMap& logEntry)
{
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return false;
    }

    QString jobNumber = logEntry["job_number"].toString();
    QString description = logEntry["description"].toString();
    QString date = logEntry["date"].toString();

    QSqlQuery query(m_dbManager->getDatabase());

    // First, try to update an existing entry with the same job identifier
    QString updateSql = QString(
                            "UPDATE %1 SET postage = ?, count = ?, per_piece = ?, "
                            "mail_class = ?, shape = ?, permit = ?, date = ?, year = ?, month = ? "
                            "WHERE job_number = ? AND description = ?"
                            ).arg(LOG_TABLE);

    query.prepare(updateSql);
    query.addBindValue(logEntry["postage"]);
    query.addBindValue(logEntry["count"]);
    query.addBindValue(logEntry["per_piece"]);
    query.addBindValue(logEntry["mail_class"]);
    query.addBindValue(logEntry["shape"]);
    query.addBindValue(logEntry["permit"]);
    query.addBindValue(logEntry["date"]);
    query.addBindValue(logEntry["year"]);
    query.addBindValue(logEntry["month"]);
    query.addBindValue(jobNumber);
    query.addBindValue(description);

    if (!query.exec()) {
        m_lastError = "Failed to update log entry: " + query.lastError().text();
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return false;
    }

    // Check if any rows were updated
    if (query.numRowsAffected() > 0) {
        Logger::instance().info(QString("TMBroken log entry updated: Job %1").arg(jobNumber));
        return true;
    }

    // No existing record found, insert new one
    QString insertSql = QString(
                            "INSERT INTO %1 (job_number, description, postage, count, per_piece, mail_class, shape, permit, date, year, month) "
                            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
                            ).arg(LOG_TABLE);

    query.prepare(insertSql);
    query.addBindValue(logEntry["job_number"]);
    query.addBindValue(logEntry["description"]);
    query.addBindValue(logEntry["postage"]);
    query.addBindValue(logEntry["count"]);
    query.addBindValue(logEntry["per_piece"]);
    query.addBindValue(logEntry["mail_class"]);
    query.addBindValue(logEntry["shape"]);
    query.addBindValue(logEntry["permit"]);
    query.addBindValue(logEntry["date"]);
    query.addBindValue(logEntry["year"]);
    query.addBindValue(logEntry["month"]);

    if (!query.exec()) {
        m_lastError = "Failed to add log entry: " + query.lastError().text();
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return false;
    }

    Logger::instance().info(QString("TMBroken log entry added: Job %1").arg(jobNumber));
    return true;
}

bool TMBrokenDBManager::updateLogEntry(int id, const QVariantMap& logEntry)
{
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());

    QString sql = QString(
        "UPDATE %1 SET job_number = ?, description = ?, postage = ?, count = ?, "
        "per_piece = ?, mail_class = ?, shape = ?, permit = ?, date = ? WHERE id = ?"
    ).arg(LOG_TABLE);

    query.prepare(sql);
    query.addBindValue(logEntry["job_number"]);
    query.addBindValue(logEntry["description"]);
    query.addBindValue(logEntry["postage"]);
    query.addBindValue(logEntry["count"]);
    query.addBindValue(logEntry["per_piece"]);
    query.addBindValue(logEntry["mail_class"]);
    query.addBindValue(logEntry["shape"]);
    query.addBindValue(logEntry["permit"]);
    query.addBindValue(logEntry["date"]);
    query.addBindValue(id);

    if (!query.exec()) {
        m_lastError = "Failed to update log entry: " + query.lastError().text();
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return false;
    }

    return true;
}

bool TMBrokenDBManager::deleteLogEntry(int id)
{
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());

    QString sql = QString("DELETE FROM %1 WHERE id = ?").arg(LOG_TABLE);
    
    query.prepare(sql);
    query.addBindValue(id);

    if (!query.exec()) {
        m_lastError = "Failed to delete log entry: " + query.lastError().text();
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return false;
    }

    return true;
}

QList<QVariantMap> TMBrokenDBManager::getAllLogEntries()
{
    QList<QVariantMap> result;
    
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return result;
    }

    QSqlQuery query(m_dbManager->getDatabase());

    QString sql = QString("SELECT * FROM %1 ORDER BY date DESC").arg(LOG_TABLE);

    if (!query.exec(sql)) {
        m_lastError = "Failed to get log entries: " + query.lastError().text();
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return result;
    }

    while (query.next()) {
        QVariantMap entry;
        QSqlRecord record = query.record();
        for (int i = 0; i < record.count(); ++i) {
            entry[record.fieldName(i)] = record.value(i);
        }
        result.append(entry);
    }

    return result;
}

QList<QVariantMap> TMBrokenDBManager::getLogEntriesByDateRange(const QDate& startDate, const QDate& endDate)
{
    QList<QVariantMap> result;
    
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return result;
    }

    QSqlQuery query(m_dbManager->getDatabase());

    QString sql = QString("SELECT * FROM %1 WHERE date >= ? AND date <= ? ORDER BY date DESC").arg(LOG_TABLE);
    
    query.prepare(sql);
    query.addBindValue(startDate.toString("yyyy-MM-dd"));
    query.addBindValue(endDate.toString("yyyy-MM-dd"));

    if (!query.exec()) {
        m_lastError = "Failed to get log entries by date range: " + query.lastError().text();
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return result;
    }

    while (query.next()) {
        QVariantMap entry;
        QSqlRecord record = query.record();
        for (int i = 0; i < record.count(); ++i) {
            entry[record.fieldName(i)] = record.value(i);
        }
        result.append(entry);
    }

    return result;
}

QVariantMap TMBrokenDBManager::getJobStatistics(const QString& year, const QString& month)
{
    QVariantMap result;
    
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return result;
    }

    QSqlQuery query(m_dbManager->getDatabase());

    QString sql = QString(
        "SELECT COUNT(*) as total_entries, SUM(CAST(REPLACE(REPLACE(postage, '$', ''), ',', '') AS REAL)) as total_postage, "
        "SUM(CAST(REPLACE(count, ',', '') AS INTEGER)) as total_count "
        "FROM %1 WHERE date LIKE ?"
    ).arg(LOG_TABLE);
    
    query.prepare(sql);
    query.addBindValue(year + "-" + month + "%");

    if (!query.exec()) {
        m_lastError = "Failed to get job statistics: " + query.lastError().text();
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return result;
    }

    if (query.next()) {
        result["total_entries"] = query.value(0);
        result["total_postage"] = query.value(1);
        result["total_count"] = query.value(2);
    }

    return result;
}

QStringList TMBrokenDBManager::getAvailableYears()
{
    QStringList result;
    
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return result;
    }

    QSqlQuery query(m_dbManager->getDatabase());

    QString sql = QString("SELECT DISTINCT year FROM %1 ORDER BY year DESC").arg(JOB_DATA_TABLE);

    if (!query.exec(sql)) {
        m_lastError = "Failed to get available years: " + query.lastError().text();
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return result;
    }

    while (query.next()) {
        result.append(query.value(0).toString());
    }

    return result;
}

QStringList TMBrokenDBManager::getAvailableMonths(const QString& year)
{
    QStringList result;
    
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return result;
    }

    QSqlQuery query(m_dbManager->getDatabase());

    QString sql = QString("SELECT DISTINCT month FROM %1 WHERE year = ? ORDER BY month").arg(JOB_DATA_TABLE);
    
    query.prepare(sql);
    query.addBindValue(year);

    if (!query.exec()) {
        m_lastError = "Failed to get available months: " + query.lastError().text();
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return result;
    }

    while (query.next()) {
        result.append(query.value(0).toString());
    }

    return result;
}

QList<QMap<QString, QString>> TMBrokenDBManager::getAllJobs()
{
    QList<QMap<QString, QString>> jobs;
    
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return jobs;
    }

    QSqlQuery query(m_dbManager->getDatabase());

    QString sql = QString("SELECT job_number, year, month FROM %1 ORDER BY year DESC, month DESC").arg(JOB_DATA_TABLE);
    
    if (!query.exec(sql)) {
        m_lastError = "Failed to get all jobs: " + query.lastError().text();
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return jobs;
    }

    while (query.next()) {
        QMap<QString, QString> job;
        job["job_number"] = query.value("job_number").toString();
        job["year"] = query.value("year").toString();
        job["month"] = query.value("month").toString();
        jobs.append(job);
    }

    return jobs;
}

bool TMBrokenDBManager::backupDatabase(const QString& backupPath)
{
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return false;
    }

    // This is a simplified backup - copy the database file
    QFile dbFile(m_databasePath);
    if (!dbFile.exists()) {
        m_lastError = "Database file does not exist";
        return false;
    }

    if (!dbFile.copy(backupPath)) {
        m_lastError = "Failed to copy database file: " + dbFile.errorString();
        return false;
    }

    return true;
}

bool TMBrokenDBManager::restoreDatabase(const QString& backupPath)
{
    if (!QFile::exists(backupPath)) {
        m_lastError = "Backup file does not exist";
        return false;
    }

    // Close current database connection
    if (m_database.isOpen()) {
        m_database.close();
    }

    // Remove current database file
    if (QFile::exists(m_databasePath)) {
        QFile::remove(m_databasePath);
    }

    // Copy backup file to database location
    if (!QFile::copy(backupPath, m_databasePath)) {
        m_lastError = "Failed to restore database from backup";
        return false;
    }

    // Reinitialize database
    m_initialized = false;
    return initializeDatabase();
}

bool TMBrokenDBManager::cleanupOldEntries(int daysOld)
{
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return false;
    }

    QDate cutoffDate = QDate::currentDate().addDays(-daysOld);
    
    QSqlQuery query(m_dbManager->getDatabase());

    QString sql = QString("DELETE FROM %1 WHERE date < ?").arg(LOG_TABLE);
    
    query.prepare(sql);
    query.addBindValue(cutoffDate.toString("yyyy-MM-dd"));

    if (!query.exec()) {
        m_lastError = "Failed to cleanup old entries: " + query.lastError().text();
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return false;
    }

    return true;
}

QString TMBrokenDBManager::getLastError() const
{
    return m_lastError;
}

bool TMBrokenDBManager::executeQuery(const QString& query, const QVariantMap& params)
{
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery sqlQuery(m_database);
    sqlQuery.prepare(query);
    
    if (!bindParameters(sqlQuery, params)) {
        m_lastError = "Failed to bind parameters";
        return false;
    }

    if (!sqlQuery.exec()) {
        m_lastError = "Query execution failed: " + sqlQuery.lastError().text();
        Logger::instance().error("TMBrokenDBManager: " + m_lastError);
        return false;
    }

    return true;
}

QString TMBrokenDBManager::formatSqlValue(const QVariant& value) const
{
    if (value.isNull()) {
        return "NULL";
    }
    
    switch (value.typeId()) {
        case QMetaType::QString:
            return "'" + value.toString().replace("'", "''") + "'";
        case QMetaType::Int:
        case QMetaType::Double:
            return value.toString();
        case QMetaType::Bool:
            return value.toBool() ? "1" : "0";
        default:
            return "'" + value.toString().replace("'", "''") + "'";
    }
}

bool TMBrokenDBManager::bindParameters(QSqlQuery& query, const QVariantMap& params) const
{
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        query.bindValue(":" + it.key(), it.value());
    }
    return true;
}
