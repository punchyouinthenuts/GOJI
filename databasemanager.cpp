#include "databasemanager.h"
#include <QDebug>
#include <QSqlError>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>

// Initialize static member
DatabaseManager* DatabaseManager::m_instance = nullptr;

DatabaseManager* DatabaseManager::instance()
{
    if (!m_instance) {
        m_instance = new DatabaseManager();
    }
    return m_instance;
}

DatabaseManager::DatabaseManager()
    : m_initialized(false)
{
}

DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::initialize(const QString& dbPath)
{
    // Create the database directory if it doesn't exist
    QFileInfo fileInfo(dbPath);
    QDir dir = fileInfo.dir();

    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "Failed to create database directory:" << dir.path();
            return false;
        }
    }

    // Open the database
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qDebug() << "Failed to open database:" << m_db.lastError().text();
        return false;
    }

    // Create core tables
    if (!createCoreTables()) {
        qDebug() << "Failed to create core database tables";
        m_db.close();
        return false;
    }

    m_initialized = true;
    return true;
}

bool DatabaseManager::isInitialized() const
{
    return m_initialized && m_db.isOpen();
}

bool DatabaseManager::createCoreTables()
{
    // Create terminal_logs table (shared across all tabs)
    if (!executeQuery("CREATE TABLE IF NOT EXISTS terminal_logs ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "tab_name TEXT NOT NULL, "
                      "year TEXT, "
                      "month TEXT, "
                      "week TEXT, "
                      "timestamp TEXT, "
                      "message TEXT)")) {
        return false;
    }

    return true;
}

bool DatabaseManager::createTable(const QString& tableName, const QString& tableDefinition)
{
    if (!isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QString query = QString("CREATE TABLE IF NOT EXISTS %1 %2").arg(tableName, tableDefinition);
    return executeQuery(query);
}

bool DatabaseManager::executeQuery(const QString& queryStr)
{
    if (!isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.exec(queryStr)) {
        qDebug() << "Query failed:" << query.lastError().text();
        qDebug() << "Query was:" << queryStr;
        return false;
    }

    return true;
}

bool DatabaseManager::executeQuery(QSqlQuery& query)
{
    if (!isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    if (!query.exec()) {
        qDebug() << "Query failed:" << query.lastError().text();
        qDebug() << "Query was:" << query.lastQuery();
        return false;
    }

    return true;
}

QList<QMap<QString, QVariant>> DatabaseManager::executeSelectQuery(const QString& queryStr)
{
    QList<QMap<QString, QVariant>> result;

    if (!isInitialized()) {
        qDebug() << "Database not initialized";
        return result;
    }

    QSqlQuery query(m_db);
    if (!query.exec(queryStr)) {
        qDebug() << "Select query failed:" << query.lastError().text();
        qDebug() << "Query was:" << queryStr;
        return result;
    }

    while (query.next()) {
        QMap<QString, QVariant> row;
        QSqlRecord record = query.record();

        for (int i = 0; i < record.count(); i++) {
            row[record.fieldName(i)] = query.value(i);
        }

        result.append(row);
    }

    return result;
}

bool DatabaseManager::saveTerminalLog(const QString& tabName, const QString& year,
                                      const QString& month, const QString& week,
                                      const QString& message)
{
    if (!isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO terminal_logs (tab_name, year, month, week, timestamp, message) "
                  "VALUES (:tab_name, :year, :month, :week, :timestamp, :message)");
    query.bindValue(":tab_name", tabName);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", week);
    query.bindValue(":timestamp", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    query.bindValue(":message", message);

    return executeQuery(query);
}

QStringList DatabaseManager::getTerminalLogs(const QString& tabName, const QString& year,
                                             const QString& month, const QString& week)
{
    QStringList logs;

    if (!isInitialized()) {
        qDebug() << "Database not initialized";
        return logs;
    }

    QSqlQuery query(m_db);
    query.prepare("SELECT timestamp, message FROM terminal_logs "
                  "WHERE tab_name = :tab_name AND year = :year AND month = :month AND week = :week "
                  "ORDER BY timestamp");
    query.bindValue(":tab_name", tabName);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", week);

    if (!executeQuery(query)) {
        return logs;
    }

    while (query.next()) {
        QString log = QString("[%1] %2").arg(query.value("timestamp").toString(),
                                             query.value("message").toString());
        logs.append(log);
    }

    return logs;
}

bool DatabaseManager::validateInput(const QString& value, bool allowEmpty)
{
    if (value.isEmpty()) {
        return allowEmpty;
    }

    // Check for SQL injection attempts
    QStringList dangerousPatterns = {"--", ";", "DROP", "DELETE", "INSERT", "UPDATE", "UNION"};
    for (const QString& pattern : dangerousPatterns) {
        if (value.toUpper().contains(pattern)) {
            qDebug() << "Potentially dangerous input detected:" << value;
            return false;
        }
    }

    return true;
}
