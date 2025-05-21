#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QString>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QSqlQuery>

class DatabaseManager
{
public:
    // Singleton access
    static DatabaseManager* instance();

    // Core database functions
    bool initialize(const QString& dbPath);
    bool initializeAlt(const QString& dbPath);  // Add this line here
    bool isInitialized() const;
    QSqlDatabase& getDatabase() { return m_db; }

    // Create tables for new modules
    bool createTable(const QString& tableName, const QString& tableDefinition);

    // Generic query execution
    bool executeQuery(const QString& queryStr);
    bool executeQuery(QSqlQuery& query);

    // Generic data retrieval
    QList<QMap<QString, QVariant>> executeSelectQuery(const QString& queryStr);

    // Terminal logs (shared functionality)
    bool saveTerminalLog(const QString& tabName, const QString& year,
                         const QString& month, const QString& week,
                         const QString& message);
    QStringList getTerminalLogs(const QString& tabName, const QString& year,
                                const QString& month, const QString& week);

    // Validation helper
    bool validateInput(const QString& value, bool allowEmpty = false);

private:
    // Private constructor for singleton
    DatabaseManager();
    ~DatabaseManager();

    // Core database objects
    QSqlDatabase m_db;
    bool m_initialized;

    // Singleton instance
    static DatabaseManager* m_instance;

    // Core table creation
    bool createCoreTables();
};

#endif // DATABASEMANAGER_H
