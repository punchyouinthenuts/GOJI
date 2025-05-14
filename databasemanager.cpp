#include "databasemanager.h"
#include <QDebug>
#include <QSqlError>
#include <QSqlRecord>  // Add this include for QSqlRecord
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
