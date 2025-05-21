#include "databasemanager.h"
#include <QDebug>
#include <QSqlError>
#include <QSqlRecord>  // Added this include for QSqlRecord
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

    // Try to open the database
    qDebug() << "Setting up database connection to:" << dbPath;

    // Check if the connection name already exists
    QString connectionName = "main_connection";
    if (QSqlDatabase::contains(connectionName)) {
        qDebug() << "Removing existing database connection";
        QSqlDatabase::removeDatabase(connectionName);
    }

    // Create a new database connection
    m_db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    m_db.setDatabaseName(dbPath);

    // Try to open with detailed error reporting
    qDebug() << "Opening database connection...";
    if (!m_db.open()) {
        qDebug() << "Failed to open database connection";
        qDebug() << "Error details:" << m_db.lastError().text();

        // Try to get more specific error information
        qDebug() << "Error Type:" << m_db.lastError().type();
        qDebug() << "Driver Text:" << m_db.lastError().driverText();
        qDebug() << "Database Text:" << m_db.lastError().databaseText();

        // Try a different approach
        qDebug() << "Attempting alternative connection approach";
        m_db = QSqlDatabase::addDatabase("QSQLITE"); // Default connection
        m_db.setDatabaseName(dbPath);

        if (!m_db.open()) {
            qDebug() << "Alternative approach also failed:" << m_db.lastError().text();
            return false;
        }
        qDebug() << "Alternative approach succeeded";
    }

    qDebug() << "Database connection opened successfully";

    // Create core tables
    if (!createCoreTables()) {
        qDebug() << "Failed to create core database tables";
        m_db.close();
        return false;
    }

    m_initialized = true;
    qDebug() << "Database initialized successfully";
    return true;
}

// Add this method to DatabaseManager and call it from main instead of initialize

bool DatabaseManager::initializeAlt(const QString& dbPath)
{
    qDebug() << "Trying alternative database initialization approach";

    // Ensure directory exists
    QFileInfo fileInfo(dbPath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "Failed to create database directory:" << dir.path();
            return false;
        }
        qDebug() << "Created directory:" << dir.path();
    }

    // Remove connection if it exists
    if (QSqlDatabase::contains("qt_sql_default_connection")) {
        QSqlDatabase::removeDatabase("qt_sql_default_connection");
    }

    // Create a new database connection without a specific name
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);

    // Try to open the database
    qDebug() << "Opening database at:" << dbPath;
    if (!m_db.open()) {
        qDebug() << "Failed to open database:" << m_db.lastError().text();
        return false;
    }

    qDebug() << "Database opened successfully";

    // Create a simplified version of the core tables
    QSqlQuery query;
    QString createTableSQL =
        "CREATE TABLE IF NOT EXISTS terminal_logs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "tab_name TEXT NOT NULL, "
        "year TEXT, "
        "month TEXT, "
        "week TEXT, "
        "timestamp TEXT, "
        "message TEXT)";

    qDebug() << "Creating table with SQL:" << createTableSQL;

    if (!query.exec(createTableSQL)) {
        qDebug() << "Failed to create terminal_logs table:" << query.lastError().text();
        qDebug() << "Native Error Code:" << query.lastError().nativeErrorCode();
        qDebug() << "Driver Text:" << query.lastError().driverText();
        m_db.close();
        return false;
    }

    qDebug() << "Table created successfully";

    // Test inserting a record
    QString insertSQL =
        "INSERT INTO terminal_logs (tab_name, year, month, week, timestamp, message) "
        "VALUES ('TEST', '2025', '05', '1', datetime('now'), 'Database initialized')";

    if (!query.exec(insertSQL)) {
        qDebug() << "Failed to insert test record:" << query.lastError().text();
        // Continue anyway - this is just a test
    } else {
        qDebug() << "Test record inserted successfully";
    }

    m_initialized = true;
    qDebug() << "Database initialized successfully using alternative approach";
    return true;
}

bool DatabaseManager::isInitialized() const
{
    return m_initialized && m_db.isOpen();
}

bool DatabaseManager::createCoreTables()
{
    QSqlQuery query(m_db);

    // Try to create the terminal_logs table with more detailed error reporting
    QString createTableSQL = "CREATE TABLE IF NOT EXISTS terminal_logs ("
                             "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                             "tab_name TEXT NOT NULL, "
                             "year TEXT, "
                             "month TEXT, "
                             "week TEXT, "
                             "timestamp TEXT, "
                             "message TEXT)";

    qDebug() << "Executing SQL:" << createTableSQL;

    if (!query.exec(createTableSQL)) {
        // Detailed error reporting for Qt 6
        qDebug() << "SQL Error:" << query.lastError().text();

        // Qt 6 compatible error reporting
        qDebug() << "Error Type:" << static_cast<int>(query.lastError().type());
        qDebug() << "Native Error Code:" << query.lastError().nativeErrorCode();
        qDebug() << "Driver Text:" << query.lastError().driverText();
        qDebug() << "Database Text:" << query.lastError().databaseText();

        // Try a simpler query to test basic database functionality
        qDebug() << "Testing simple query to verify database connection";
        QSqlQuery testQuery(m_db);
        if (!testQuery.exec("SELECT 1")) {
            qDebug() << "Even simple query failed:" << testQuery.lastError().text();
        } else {
            qDebug() << "Simple query succeeded, problem is specific to table creation";
        }

        return false;
    }

    qDebug() << "Terminal_logs table created successfully";
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
