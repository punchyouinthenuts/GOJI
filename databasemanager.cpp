#include "databasemanager.h"
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QDir>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonDocument>

DatabaseManager::DatabaseManager(const QString& dbPath)
    : initialized(false)
{
    // Initialize database with the provided path
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);
}

DatabaseManager::~DatabaseManager()
{
    if (db.isOpen()) {
        db.close();
    }
}

bool DatabaseManager::initialize()
{
    // Create directory if it doesn't exist
    QFileInfo fileInfo(db.databaseName());
    QDir dir = fileInfo.dir();

    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "Failed to create database directory:" << dir.path();
            return false;
        }
    }

    // Open database
    if (!db.open()) {
        qDebug() << "Failed to open database:" << db.lastError().text();
        return false;
    }

    // Create tables
    if (!createTables()) {
        qDebug() << "Failed to create database tables";
        db.close();
        return false;
    }

    initialized = true;
    return true;
}

bool DatabaseManager::isInitialized() const
{
    return initialized && db.isOpen();
}

bool DatabaseManager::createTables()
{
    QSqlQuery query(db);

    // Create jobs_rac_weekly table
    if (!query.exec("CREATE TABLE IF NOT EXISTS jobs_rac_weekly ("
                    "year INTEGER, "
                    "month INTEGER, "
                    "week INTEGER, "
                    "cbc_job_number TEXT, "
                    "ncwo_job_number TEXT, "
                    "inactive_job_number TEXT, "
                    "prepif_job_number TEXT, "
                    "exc_job_number TEXT, "
                    "cbc2_postage TEXT, "
                    "cbc3_postage TEXT, "
                    "exc_postage TEXT, "
                    "inactive_po_postage TEXT, "
                    "inactive_pu_postage TEXT, "
                    "ncwo1_a_postage TEXT, "
                    "ncwo2_a_postage TEXT, "
                    "ncwo1_ap_postage TEXT, "
                    "ncwo2_ap_postage TEXT, "
                    "prepif_postage TEXT, "
                    "progress TEXT, "
                    "step0_complete INTEGER DEFAULT 0, "
                    "step1_complete INTEGER DEFAULT 0, "
                    "step2_complete INTEGER DEFAULT 0, "
                    "step3_complete INTEGER DEFAULT 0, "
                    "step4_complete INTEGER DEFAULT 0, "
                    "step5_complete INTEGER DEFAULT 0, "
                    "step6_complete INTEGER DEFAULT 0, "
                    "step7_complete INTEGER DEFAULT 0, "
                    "step8_complete INTEGER DEFAULT 0, "
                    "PRIMARY KEY (year, month, week)"
                    ")")) {
        qDebug() << "Error creating jobs_rac_weekly table:" << query.lastError().text();
        return false;
    }

    // Create proof_versions table
    if (!query.exec("CREATE TABLE IF NOT EXISTS proof_versions ("
                    "file_path TEXT PRIMARY KEY, "
                    "version INTEGER DEFAULT 1"
                    ")")) {
        qDebug() << "Error creating proof_versions table:" << query.lastError().text();
        return false;
    }

    // Create post_proof_counts table
    if (!query.exec("CREATE TABLE IF NOT EXISTS post_proof_counts ("
                    "job_number TEXT, "
                    "week TEXT, "
                    "project TEXT, "
                    "pr_count INTEGER, "
                    "canc_count INTEGER, "
                    "us_count INTEGER, "
                    "postage TEXT)")) {
        qDebug() << "Error creating post_proof_counts table:" << query.lastError().text();
        return false;
    }

    // Create count_comparison table
    if (!query.exec("CREATE TABLE IF NOT EXISTS count_comparison ("
                    "group_name TEXT, "
                    "input_count INTEGER, "
                    "output_count INTEGER, "
                    "difference INTEGER)")) {
        qDebug() << "Error creating count_comparison table:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::saveJob(const JobData& job)
{
    if (!isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    // Update steps from flags before saving
    JobData jobCopy = job;
    jobCopy.updateStepsFromFlags();

    QSqlQuery query(db);
    query.prepare("INSERT OR REPLACE INTO jobs_rac_weekly (year, month, week, cbc_job_number, ncwo_job_number, inactive_job_number, "
                  "prepif_job_number, exc_job_number, cbc2_postage, cbc3_postage, exc_postage, inactive_po_postage, "
                  "inactive_pu_postage, ncwo1_a_postage, ncwo2_a_postage, ncwo1_ap_postage, ncwo2_ap_postage, prepif_postage, "
                  "progress, step0_complete, step1_complete, step2_complete, step3_complete, step4_complete, "
                  "step5_complete, step6_complete, step7_complete, step8_complete) "
                  "VALUES (:year, :month, :week, :cbc, :ncwo, :inactive, :prepif, :exc, :cbc2, :cbc3, :exc_p, :in_po, "
                  ":in_pu, :nc1a, :nc2a, :nc1ap, :nc2ap, :prepif_p, :progress, :s0, :s1, :s2, :s3, :s4, :s5, :s6, :s7, :s8)");

    query.bindValue(":year", jobCopy.year.toInt());
    query.bindValue(":month", jobCopy.month.toInt());
    query.bindValue(":week", jobCopy.week.toInt());
    query.bindValue(":cbc", jobCopy.cbcJobNumber);
    query.bindValue(":ncwo", jobCopy.ncwoJobNumber);
    query.bindValue(":inactive", jobCopy.inactiveJobNumber);
    query.bindValue(":prepif", jobCopy.prepifJobNumber);
    query.bindValue(":exc", jobCopy.excJobNumber);
    query.bindValue(":cbc2", jobCopy.cbc2Postage);
    query.bindValue(":cbc3", jobCopy.cbc3Postage);
    query.bindValue(":exc_p", jobCopy.excPostage);
    query.bindValue(":in_po", jobCopy.inactivePOPostage);
    query.bindValue(":in_pu", jobCopy.inactivePUPostage);
    query.bindValue(":nc1a", jobCopy.ncwo1APostage);
    query.bindValue(":nc2a", jobCopy.ncwo2APostage);
    query.bindValue(":nc1ap", jobCopy.ncwo1APPostage);
    query.bindValue(":nc2ap", jobCopy.ncwo2APPostage);
    query.bindValue(":prepif_p", jobCopy.prepifPostage);
    query.bindValue(":progress", "updated");
    query.bindValue(":s0", jobCopy.step0_complete);
    query.bindValue(":s1", jobCopy.step1_complete);
    query.bindValue(":s2", jobCopy.step2_complete);
    query.bindValue(":s3", jobCopy.step3_complete);
    query.bindValue(":s4", jobCopy.step4_complete);
    query.bindValue(":s5", jobCopy.step5_complete);
    query.bindValue(":s6", jobCopy.step6_complete);
    query.bindValue(":s7", jobCopy.step7_complete);
    query.bindValue(":s8", jobCopy.step8_complete);

    if (!query.exec()) {
        qDebug() << "Failed to save job:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::loadJob(const QString& year, const QString& month, const QString& week, JobData& job)
{
    if (!isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("SELECT * FROM jobs_rac_weekly WHERE year = :year AND month = :month AND week = :week");
    query.bindValue(":year", year.toInt());
    query.bindValue(":month", month.toInt());
    query.bindValue(":week", week.toInt());

    if (!query.exec() || !query.next()) {
        qDebug() << "Failed to load job:" << query.lastError().text();
        return false;
    }

    // Load data into job object
    job.year = year;
    job.month = month;
    job.week = week;
    job.cbcJobNumber = query.value("cbc_job_number").toString();
    job.excJobNumber = query.value("exc_job_number").toString();
    job.inactiveJobNumber = query.value("inactive_job_number").toString();
    job.ncwoJobNumber = query.value("ncwo_job_number").toString();
    job.prepifJobNumber = query.value("prepif_job_number").toString();

    job.cbc2Postage = query.value("cbc2_postage").toString();
    job.cbc3Postage = query.value("cbc3_postage").toString();
    job.excPostage = query.value("exc_postage").toString();
    job.inactivePOPostage = query.value("inactive_po_postage").toString();
    job.inactivePUPostage = query.value("inactive_pu_postage").toString();
    job.ncwo1APostage = query.value("ncwo1_a_postage").toString();
    job.ncwo2APostage = query.value("ncwo2_a_postage").toString();
    job.ncwo1APPostage = query.value("ncwo1_ap_postage").toString();
    job.ncwo2APPostage = query.value("ncwo2_ap_postage").toString();
    job.prepifPostage = query.value("prepif_postage").toString();

    job.step0_complete = query.value("step0_complete").toInt();
    job.step1_complete = query.value("step1_complete").toInt();
    job.step2_complete = query.value("step2_complete").toInt();
    job.step3_complete = query.value("step3_complete").toInt();
    job.step4_complete = query.value("step4_complete").toInt();
    job.step5_complete = query.value("step5_complete").toInt();
    job.step6_complete = query.value("step6_complete").toInt();
    job.step7_complete = query.value("step7_complete").toInt();
    job.step8_complete = query.value("step8_complete").toInt();

    // Update flags from steps
    job.updateFlagsFromSteps();

    return true;
}

bool DatabaseManager::deleteJob(const QString& year, const QString& month, const QString& week)
{
    if (!isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM jobs_rac_weekly WHERE year = :year AND month = :month AND week = :week");
    query.bindValue(":year", year.toInt());
    query.bindValue(":month", month.toInt());
    query.bindValue(":week", week.toInt());

    if (!query.exec()) {
        qDebug() << "Failed to delete job:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::jobExists(const QString& year, const QString& month, const QString& week)
{
    if (!isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM jobs_rac_weekly WHERE year = :year AND month = :month AND week = :week");
    query.bindValue(":year", year.toInt());
    query.bindValue(":month", month.toInt());
    query.bindValue(":week", week.toInt());

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

QList<QMap<QString, QString>> DatabaseManager::getAllJobs()
{
    QList<QMap<QString, QString>> result;

    if (!isInitialized()) {
        qDebug() << "Database not initialized";
        return result;
    }

    QSqlQuery query("SELECT year, month, week FROM jobs_rac_weekly ORDER BY year DESC, month DESC, week DESC", db);

    while (query.next()) {
        QMap<QString, QString> job;
        job["year"] = query.value(0).toString();
        job["month"] = query.value(1).toString();
        job["week"] = query.value(2).toString();
        result.append(job);
    }

    return result;
}

int DatabaseManager::getNextProofVersion(const QString& filePath)
{
    if (!isInitialized()) {
        qDebug() << "Database not initialized";
        return 1;
    }

    QSqlQuery query(db);
    query.prepare("SELECT version FROM proof_versions WHERE file_path = :filePath");
    query.bindValue(":filePath", filePath);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() + 1;
    }

    return 2; // First version is 1, so next version is 2
}

bool DatabaseManager::updateProofVersion(const QString& filePath, int version)
{
    if (!isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("INSERT OR REPLACE INTO proof_versions (file_path, version) VALUES (:filePath, :version)");
    query.bindValue(":filePath", filePath);
    query.bindValue(":version", version);

    if (!query.exec()) {
        qDebug() << "Failed to update proof version:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::savePostProofCounts(const QJsonObject& countsData)
{
    if (!isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    // Begin transaction
    db.transaction();

    // Process counts data
    QJsonArray counts = countsData["counts"].toArray();
    QSqlQuery countsQuery(db);
    countsQuery.prepare("INSERT INTO post_proof_counts (job_number, week, project, pr_count, canc_count, us_count, postage) "
                        "VALUES (:job, :week, :project, :pr, :canc, :us, :postage)");

    for (const QJsonValue& value : counts) {
        QJsonObject count = value.toObject();
        countsQuery.bindValue(":job", count["job_number"].toString());
        countsQuery.bindValue(":week", count["week"].toString());
        countsQuery.bindValue(":project", count["project"].toString());
        countsQuery.bindValue(":pr", count["pr_count"].toInt());
        countsQuery.bindValue(":canc", count["canc_count"].toInt());
        countsQuery.bindValue(":us", count["us_count"].toInt());
        countsQuery.bindValue(":postage", QString::number(count["postage"].toDouble(), 'f', 2));

        if (!countsQuery.exec()) {
            qDebug() << "Failed to insert post-proof count:" << countsQuery.lastError().text();
            db.rollback();
            return false;
        }
    }

    // Process comparison data
    QJsonArray comparison = countsData["comparison"].toArray();
    QSqlQuery comparisonQuery(db);
    comparisonQuery.prepare("INSERT INTO count_comparison (group_name, input_count, output_count, difference) "
                            "VALUES (:group, :input, :output, :diff)");

    for (const QJsonValue& value : comparison) {
        QJsonObject comp = value.toObject();
        comparisonQuery.bindValue(":group", comp["group"].toString());
        comparisonQuery.bindValue(":input", comp["input_count"].toInt());
        comparisonQuery.bindValue(":output", comp["output_count"].toInt());
        comparisonQuery.bindValue(":diff", comp["difference"].toInt());

        if (!comparisonQuery.exec()) {
            qDebug() << "Failed to insert comparison count:" << comparisonQuery.lastError().text();
            db.rollback();
            return false;
        }
    }

    // Commit transaction
    if (!db.commit()) {
        qDebug() << "Failed to commit transaction:" << db.lastError().text();
        db.rollback();
        return false;
    }

    return true;
}

bool DatabaseManager::clearPostProofCounts(const QString& week)
{
    if (!isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(db);
    if (week.isEmpty()) {
        // Clear all counts
        if (!query.exec("DELETE FROM post_proof_counts")) {
            qDebug() << "Failed to clear post-proof counts:" << query.lastError().text();
            return false;
        }
    } else {
        // Clear counts for specific week
        query.prepare("DELETE FROM post_proof_counts WHERE week = :week");
        query.bindValue(":week", week);
        if (!query.exec()) {
            qDebug() << "Failed to clear post-proof counts for week:" << query.lastError().text();
            return false;
        }
    }

    // Clear comparison data
    if (!query.exec("DELETE FROM count_comparison")) {
        qDebug() << "Failed to clear count comparison data:" << query.lastError().text();
        return false;
    }

    return true;
}

QList<QMap<QString, QVariant>> DatabaseManager::getPostProofCounts(const QString& week)
{
    QList<QMap<QString, QVariant>> result;

    if (!isInitialized()) {
        qDebug() << "Database not initialized";
        return result;
    }

    QSqlQuery query(db);
    if (week.isEmpty()) {
        query.prepare("SELECT job_number, week, project, pr_count, canc_count, us_count, postage FROM post_proof_counts");
    } else {
        query.prepare("SELECT job_number, week, project, pr_count, canc_count, us_count, postage FROM post_proof_counts WHERE week = :week");
        query.bindValue(":week", week);
    }

    if (!query.exec()) {
        qDebug() << "Failed to get post-proof counts:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        QMap<QString, QVariant> row;
        row["job_number"] = query.value("job_number");
        row["week"] = query.value("week");
        row["project"] = query.value("project");
        row["pr_count"] = query.value("pr_count");
        row["canc_count"] = query.value("canc_count");
        row["us_count"] = query.value("us_count");
        row["postage"] = query.value("postage");
        result.append(row);
    }

    return result;
}

QList<QMap<QString, QVariant>> DatabaseManager::getCountComparison()
{
    QList<QMap<QString, QVariant>> result;

    if (!isInitialized()) {
        qDebug() << "Database not initialized";
        return result;
    }

    QSqlQuery query("SELECT group_name, input_count, output_count, difference FROM count_comparison", db);

    if (!query.exec()) {
        qDebug() << "Failed to get count comparison:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        QMap<QString, QVariant> row;
        row["group_name"] = query.value("group_name");
        row["input_count"] = query.value("input_count");
        row["output_count"] = query.value("output_count");
        row["difference"] = query.value("difference");
        result.append(row);
    }

    return result;
}
