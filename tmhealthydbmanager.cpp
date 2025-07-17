#include "tmhealthydbmanager.h"
#include "logger.h"
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
TMHealthyDBManager* TMHealthyDBManager::m_instance = nullptr;
QMutex TMHealthyDBManager::m_mutex;

// Table name constants
const QString TMHealthyDBManager::JOB_DATA_TABLE = "tmhealthy_job_data";
const QString TMHealthyDBManager::LOG_TABLE = "tmhealthy_log";

TMHealthyDBManager* TMHealthyDBManager::instance()
{
    QMutexLocker locker(&m_mutex);
    if (!m_instance) {
        m_instance = new TMHealthyDBManager();
    }
    return m_instance;
}

TMHealthyDBManager::TMHealthyDBManager(QObject *parent)
    : QObject(parent),
      m_initialized(false),
      m_databasePath("C:/Goji/database/tmhealthy.db")
{
    // Constructor body - initialization happens in initializeDatabase()
}

TMHealthyDBManager::~TMHealthyDBManager()
{
    if (m_database.isOpen()) {
        m_database.close();
    }
}

bool TMHealthyDBManager::initializeDatabase()
{
    if (m_initialized) {
        return true;
    }

    // Ensure database directory exists
    QFileInfo dbFileInfo(m_databasePath);
    QDir dbDir = dbFileInfo.dir();
    if (!dbDir.exists()) {
        if (!dbDir.mkpath(".")) {
            m_lastError = "Failed to create database directory: " + dbDir.path();
            Logger::instance().error("TMHealthyDBManager: " + m_lastError);
            return false;
        }
    }

    // Setup database connection
    m_database = QSqlDatabase::addDatabase("QSQLITE", "tmhealthy_connection");
    m_database.setDatabaseName(m_databasePath);

    if (!m_database.open()) {
        m_lastError = "Failed to open database: " + m_database.lastError().text();
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
        return false;
    }

    // Create tables
    if (!createTables()) {
        m_lastError = "Failed to create database tables";
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
        return false;
    }

    // Create indexes for better performance
    if (!createIndexes()) {
        m_lastError = "Failed to create database indexes";
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
        return false;
    }

    m_initialized = true;
    Logger::instance().info("TMHealthyDBManager: Database initialized successfully");
    return true;
}

bool TMHealthyDBManager::isInitialized() const
{
    return m_initialized;
}

bool TMHealthyDBManager::createTables()
{
    if (!createJobDataTable()) {
        return false;
    }
    
    if (!createLogTable()) {
        return false;
    }
    
    return true;
}

bool TMHealthyDBManager::createJobDataTable()
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

    QSqlQuery query(m_database);
    if (!query.exec(sql)) {
        m_lastError = "Failed to create job data table: " + query.lastError().text();
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
        return false;
    }

    return true;
}

bool TMHealthyDBManager::createLogTable()
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
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")"
    ).arg(LOG_TABLE);

    QSqlQuery query(m_database);
    if (!query.exec(sql)) {
        m_lastError = "Failed to create log table: " + query.lastError().text();
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
        return false;
    }

    return true;
}

bool TMHealthyDBManager::createIndexes()
{
    QStringList indexQueries = {
        QString("CREATE INDEX IF NOT EXISTS idx_%1_year_month ON %1(year, month)").arg(JOB_DATA_TABLE),
        QString("CREATE INDEX IF NOT EXISTS idx_%1_job_number ON %1(job_number)").arg(JOB_DATA_TABLE),
        QString("CREATE INDEX IF NOT EXISTS idx_%1_date ON %1(date)").arg(LOG_TABLE),
        QString("CREATE INDEX IF NOT EXISTS idx_%1_job_number ON %1(job_number)").arg(LOG_TABLE)
    };

    QSqlQuery query(m_database);
    for (const QString& indexSql : indexQueries) {
        if (!query.exec(indexSql)) {
            m_lastError = "Failed to create index: " + query.lastError().text();
            Logger::instance().error("TMHealthyDBManager: " + m_lastError);
            return false;
        }
    }

    return true;
}

bool TMHealthyDBManager::saveJob(const QString& jobNumber, const QString& year, const QString& month)
{
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_database);
    QString sql = QString(
        "INSERT OR REPLACE INTO %1 (job_number, year, month, updated_at) "
        "VALUES (?, ?, ?, ?)"
    ).arg(JOB_DATA_TABLE);
    
    query.prepare(sql);
    query.addBindValue(jobNumber);
    query.addBindValue(year);
    query.addBindValue(month);
    query.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    if (!query.exec()) {
        m_lastError = "Failed to save job: " + query.lastError().text();
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
        return false;
    }

    Logger::instance().info(QString("TMHealthy job saved: %1 for %2/%3").arg(jobNumber, year, month));
    return true;
}

bool TMHealthyDBManager::saveJobData(const QVariantMap& jobData)
{
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_database);
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
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
        return false;
    }

    return true;
}

QVariantMap TMHealthyDBManager::loadJobData(const QString& year, const QString& month)
{
    QVariantMap result;
    
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return result;
    }

    QSqlQuery query(m_database);
    QString sql = QString("SELECT * FROM %1 WHERE year = ? AND month = ?").arg(JOB_DATA_TABLE);
    
    query.prepare(sql);
    query.addBindValue(year);
    query.addBindValue(month);

    if (!query.exec()) {
        m_lastError = "Failed to load job data: " + query.lastError().text();
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
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

bool TMHealthyDBManager::deleteJobData(const QString& year, const QString& month)
{
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_database);
    QString sql = QString("DELETE FROM %1 WHERE year = ? AND month = ?").arg(JOB_DATA_TABLE);
    
    query.prepare(sql);
    query.addBindValue(year);
    query.addBindValue(month);

    if (!query.exec()) {
        m_lastError = "Failed to delete job data: " + query.lastError().text();
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
        return false;
    }

    return true;
}

bool TMHealthyDBManager::addLogEntry(const QVariantMap& logEntry)
{
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_database);
    QString sql = QString(
        "INSERT INTO %1 (job_number, description, postage, count, per_piece, mail_class, shape, permit, date) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"
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

    if (!query.exec()) {
        m_lastError = "Failed to add log entry: " + query.lastError().text();
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
        return false;
    }

    return true;
}

bool TMHealthyDBManager::updateLogEntry(int id, const QVariantMap& logEntry)
{
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_database);
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
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
        return false;
    }

    return true;
}

bool TMHealthyDBManager::deleteLogEntry(int id)
{
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_database);
    QString sql = QString("DELETE FROM %1 WHERE id = ?").arg(LOG_TABLE);
    
    query.prepare(sql);
    query.addBindValue(id);

    if (!query.exec()) {
        m_lastError = "Failed to delete log entry: " + query.lastError().text();
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
        return false;
    }

    return true;
}

QList<QVariantMap> TMHealthyDBManager::getAllLogEntries()
{
    QList<QVariantMap> result;
    
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return result;
    }

    QSqlQuery query(m_database);
    QString sql = QString("SELECT * FROM %1 ORDER BY date DESC").arg(LOG_TABLE);

    if (!query.exec(sql)) {
        m_lastError = "Failed to get log entries: " + query.lastError().text();
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
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

QList<QVariantMap> TMHealthyDBManager::getLogEntriesByDateRange(const QDate& startDate, const QDate& endDate)
{
    QList<QVariantMap> result;
    
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return result;
    }

    QSqlQuery query(m_database);
    QString sql = QString("SELECT * FROM %1 WHERE date >= ? AND date <= ? ORDER BY date DESC").arg(LOG_TABLE);
    
    query.prepare(sql);
    query.addBindValue(startDate.toString("yyyy-MM-dd"));
    query.addBindValue(endDate.toString("yyyy-MM-dd"));

    if (!query.exec()) {
        m_lastError = "Failed to get log entries by date range: " + query.lastError().text();
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
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

QVariantMap TMHealthyDBManager::getJobStatistics(const QString& year, const QString& month)
{
    QVariantMap result;
    
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return result;
    }

    QSqlQuery query(m_database);
    QString sql = QString(
        "SELECT COUNT(*) as total_entries, SUM(CAST(REPLACE(REPLACE(postage, '$', ''), ',', '') AS REAL)) as total_postage, "
        "SUM(CAST(REPLACE(count, ',', '') AS INTEGER)) as total_count "
        "FROM %1 WHERE date LIKE ?"
    ).arg(LOG_TABLE);
    
    query.prepare(sql);
    query.addBindValue(year + "-" + month + "%");

    if (!query.exec()) {
        m_lastError = "Failed to get job statistics: " + query.lastError().text();
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
        return result;
    }

    if (query.next()) {
        result["total_entries"] = query.value(0);
        result["total_postage"] = query.value(1);
        result["total_count"] = query.value(2);
    }

    return result;
}

QStringList TMHealthyDBManager::getAvailableYears()
{
    QStringList result;
    
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return result;
    }

    QSqlQuery query(m_database);
    QString sql = QString("SELECT DISTINCT year FROM %1 ORDER BY year DESC").arg(JOB_DATA_TABLE);

    if (!query.exec(sql)) {
        m_lastError = "Failed to get available years: " + query.lastError().text();
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
        return result;
    }

    while (query.next()) {
        result.append(query.value(0).toString());
    }

    return result;
}

QStringList TMHealthyDBManager::getAvailableMonths(const QString& year)
{
    QStringList result;
    
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return result;
    }

    QSqlQuery query(m_database);
    QString sql = QString("SELECT DISTINCT month FROM %1 WHERE year = ? ORDER BY month").arg(JOB_DATA_TABLE);
    
    query.prepare(sql);
    query.addBindValue(year);

    if (!query.exec()) {
        m_lastError = "Failed to get available months: " + query.lastError().text();
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
        return result;
    }

    while (query.next()) {
        result.append(query.value(0).toString());
    }

    return result;
}

QList<QMap<QString, QString>> TMHealthyDBManager::getAllJobs()
{
    QList<QMap<QString, QString>> jobs;
    
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return jobs;
    }

    QSqlQuery query(m_database);
    QString sql = QString("SELECT job_number, year, month FROM %1 ORDER BY year DESC, month DESC").arg(JOB_DATA_TABLE);
    
    if (!query.exec(sql)) {
        m_lastError = "Failed to get all jobs: " + query.lastError().text();
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
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

bool TMHealthyDBManager::backupDatabase(const QString& backupPath)
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

bool TMHealthyDBManager::restoreDatabase(const QString& backupPath)
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

bool TMHealthyDBManager::cleanupOldEntries(int daysOld)
{
    if (!m_initialized) {
        m_lastError = "Database not initialized";
        return false;
    }

    QDate cutoffDate = QDate::currentDate().addDays(-daysOld);
    
    QSqlQuery query(m_database);
    QString sql = QString("DELETE FROM %1 WHERE date < ?").arg(LOG_TABLE);
    
    query.prepare(sql);
    query.addBindValue(cutoffDate.toString("yyyy-MM-dd"));

    if (!query.exec()) {
        m_lastError = "Failed to cleanup old entries: " + query.lastError().text();
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
        return false;
    }

    return true;
}

QString TMHealthyDBManager::getLastError() const
{
    return m_lastError;
}

bool TMHealthyDBManager::executeQuery(const QString& query, const QVariantMap& params)
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
        Logger::instance().error("TMHealthyDBManager: " + m_lastError);
        return false;
    }

    return true;
}

QString TMHealthyDBManager::formatSqlValue(const QVariant& value) const
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

bool TMHealthyDBManager::bindParameters(QSqlQuery& query, const QVariantMap& params) const
{
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        query.bindValue(":" + it.key(), it.value());
    }
    return true;
}
