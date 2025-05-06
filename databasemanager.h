#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QString>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QJsonObject>
#include "jobdata.h"

class DatabaseManager
{
public:
    DatabaseManager(const QString& dbPath);
    ~DatabaseManager();

    // Database setup
    bool initialize();
    bool isInitialized() const;

    // Job operations
    bool saveJob(const JobData& job);
    bool loadJob(const QString& year, const QString& month, const QString& week, JobData& job);
    bool deleteJob(const QString& year, const QString& month, const QString& week);
    bool jobExists(const QString& year, const QString& month, const QString& week);
    QList<QMap<QString, QString>> getAllJobs();

    // Proof versions
    int getNextProofVersion(const QString& filePath);
    bool updateProofVersion(const QString& filePath, int version);
    QMap<QString, int> getAllProofVersions(const QString& jobPrefix = QString());

    // Post-proof counts
    bool savePostProofCounts(const QJsonObject& countsData);
    bool clearPostProofCounts(const QString& week);
    QList<QMap<QString, QVariant>> getPostProofCounts(const QString& week = QString());
    QList<QMap<QString, QVariant>> getCountComparison();

    // Terminal logs
    bool saveTerminalLog(const QString& year, const QString& month, const QString& week, const QString& message);
    QStringList getTerminalLogs(const QString& year, const QString& month, const QString& week);

private:
    QSqlDatabase db;
    bool initialized;

    bool createTables();
};

#endif // DATABASEMANAGER_H
