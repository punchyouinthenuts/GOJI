#include "tmweeklypcdbmanager.h"
#include "logger.h"
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QVariant>

// Initialize static member
TMWeeklyPCDBManager* TMWeeklyPCDBManager::m_instance = nullptr;

TMWeeklyPCDBManager* TMWeeklyPCDBManager::instance()
{
    if (!m_instance) {
        m_instance = new TMWeeklyPCDBManager();
    }
    return m_instance;
}

TMWeeklyPCDBManager::TMWeeklyPCDBManager()
{
    m_dbManager = DatabaseManager::instance();
}

bool TMWeeklyPCDBManager::initialize()
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Core database manager not initialized";
        return false;
    }
    return createTables();
}

bool TMWeeklyPCDBManager::createTables()
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for TMWeeklyPC table creation");
        return false;
    }

    // Create main jobs table (if not already exists)
    QString createJobsTable = R"(
        CREATE TABLE IF NOT EXISTS tm_weekly_pc_jobs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            job_number TEXT NOT NULL,
            year TEXT NOT NULL,
            month TEXT NOT NULL,
            week TEXT NOT NULL,
            proof_approval_checked BOOLEAN DEFAULT 0,
            html_display_state INTEGER DEFAULT 0,
            job_data_locked BOOLEAN DEFAULT 0,
            postage_data_locked BOOLEAN DEFAULT 0,
            postage TEXT,
            count TEXT,
            mail_class TEXT,
            permit TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(year, month, week)
        )
    )";

    QSqlQuery query(m_dbManager->getDatabase());

    // Add columns if they don't exist (for existing databases)
    query.exec("ALTER TABLE tm_weekly_pc_jobs ADD COLUMN job_data_locked BOOLEAN DEFAULT 0");
    query.exec("ALTER TABLE tm_weekly_pc_jobs ADD COLUMN postage_data_locked BOOLEAN DEFAULT 0");
    query.exec("ALTER TABLE tm_weekly_pc_jobs ADD COLUMN postage TEXT");
    query.exec("ALTER TABLE tm_weekly_pc_jobs ADD COLUMN count TEXT");
    query.exec("ALTER TABLE tm_weekly_pc_jobs ADD COLUMN mail_class TEXT");
    query.exec("ALTER TABLE tm_weekly_pc_jobs ADD COLUMN permit TEXT");

    if (!m_dbManager->createTable("tm_weekly_pc_jobs", createJobsTable.mid(createJobsTable.indexOf('(')))) {
        Logger::instance().error("Failed to create tm_weekly_pc_jobs table");
        return false;
    }

    // Create postage data table
    QString createPostageTable = R"(
        CREATE TABLE IF NOT EXISTS tm_weekly_pc_postage (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            year TEXT NOT NULL,
            month TEXT NOT NULL,
            week TEXT NOT NULL,
            postage TEXT,
            count TEXT,
            mail_class TEXT,
            permit TEXT,
            locked BOOLEAN DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(year, month, week)
        )
    )";

    if (!query.exec(createPostageTable)) {
        // Handle error if needed
    }

    if (!m_dbManager->createTable("tm_weekly_pc_postage",
                                  "("
                                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                  "year TEXT NOT NULL, "
                                  "month TEXT NOT NULL, "
                                  "week TEXT NOT NULL, "
                                  "postage TEXT, "
                                  "count TEXT, "
                                  "mail_class TEXT, "
                                  "permit TEXT, "
                                  "locked BOOLEAN DEFAULT 0, "
                                  "created_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
                                  "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
                                  "UNIQUE(year, month, week)"
                                  ")")) {
        Logger::instance().error("Failed to create tm_weekly_pc_postage table");
        return false;
    }

    // Migration: Move data from old table to new table
    QSqlQuery migrationQuery(m_dbManager->getDatabase());

    // Check if old table exists
    migrationQuery.prepare("SELECT name FROM sqlite_master WHERE type='table' AND name='tm_weekly_jobs'");
    if (migrationQuery.exec() && migrationQuery.next()) {
        // Old table exists, migrate data
        Logger::instance().info("Found old tm_weekly_jobs table, migrating data to tm_weekly_pc_jobs");

        QSqlQuery migrateDataQuery(m_dbManager->getDatabase());
        if (migrateDataQuery.exec("INSERT OR IGNORE INTO tm_weekly_pc_jobs (job_number, year, month, week, proof_approval_checked, html_display_state, created_at, updated_at) "
                                  "SELECT job_number, year, month, week, COALESCE(proof_approval_checked, 0), COALESCE(html_display_state, 0), COALESCE(created_at, CURRENT_TIMESTAMP), COALESCE(updated_at, CURRENT_TIMESTAMP) "
                                  "FROM tm_weekly_jobs")) {
            Logger::instance().info("Data migration completed successfully");

            // Check how many rows were migrated
            QSqlQuery countQuery(m_dbManager->getDatabase());
            if (countQuery.exec("SELECT COUNT(*) FROM tm_weekly_pc_jobs")) {
                if (countQuery.next()) {
                    int count = countQuery.value(0).toInt();
                    Logger::instance().info(QString("Migrated %1 jobs to tm_weekly_pc_jobs table").arg(count));
                }
            }

            // Optionally delete the old table after successful migration
            // Uncomment the next line if you want to remove the old table
            // migrateDataQuery.exec("DROP TABLE tm_weekly_jobs");
        } else {
            Logger::instance().error("Failed to migrate data: " + migrateDataQuery.lastError().text());
        }
    } else {
        Logger::instance().info("No old tm_weekly_jobs table found, no migration needed");
    }

    Logger::instance().info("TMWeeklyPC database tables created/verified successfully");
    return true;
}

bool TMWeeklyPCDBManager::saveJob(const QString& jobNumber, const QString& year,
                                  const QString& month, const QString& week)
{
    qDebug() << "TMWeeklyPCDBManager::saveJob called with:" << jobNumber << year << month << week;

    if (!m_dbManager) {
        qDebug() << "m_dbManager is null";
        return false;
    }

    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    // Validate inputs
    if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty() || week.isEmpty()) {
        qDebug() << "Invalid input data - empty values";
        return false;
    }

    qDebug() << "Creating SQL query...";
    QSqlQuery query(m_dbManager->getDatabase());

    // Check if database connection is valid
    if (!m_dbManager->getDatabase().isValid()) {
        qDebug() << "Database connection is not valid";
        return false;
    }

    if (!m_dbManager->getDatabase().isOpen()) {
        qDebug() << "Database connection is not open";
        return false;
    }

    query.prepare("INSERT OR REPLACE INTO tm_weekly_pc_jobs "
                  "(job_number, year, month, week, updated_at) "
                  "VALUES (:job_number, :year, :month, :week, :updated_at)");

    query.bindValue(":job_number", jobNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", week);
    query.bindValue(":updated_at", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    qDebug() << "Executing query...";
    bool result = query.exec();

    if (!result) {
        qDebug() << "Query execution failed";
        qDebug() << "Last error:" << query.lastError().text();
    } else {
        qDebug() << "Job saved successfully";
    }

    return result;
}

bool TMWeeklyPCDBManager::loadJob(const QString& year, const QString& month,
                                  const QString& week, QString& jobNumber)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT job_number FROM tm_weekly_pc_jobs "
                  "WHERE year = :year AND month = :month AND week = :week");

    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", week);

    if (!query.exec()) {
        return false;
    }

    if (!query.next()) {
        qDebug() << "No job found for" << year << month << week;
        return false;
    }

    jobNumber = query.value("job_number").toString();
    return true;
}

bool TMWeeklyPCDBManager::saveJobState(const QString& year, const QString& month, const QString& week,
                                       bool proofApprovalChecked, int htmlDisplayState,
                                       bool jobDataLocked, bool postageDataLocked,
                                       const QString& postage, const QString& count,
                                       const QString& mailClass, const QString& permit)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("UPDATE tm_weekly_pc_jobs SET "
                  "proof_approval_checked = :proof_approval_checked, "
                  "html_display_state = :html_display_state, "
                  "job_data_locked = :job_data_locked, "
                  "postage_data_locked = :postage_data_locked, "
                  "postage = :postage, "
                  "count = :count, "
                  "mail_class = :mail_class, "
                  "permit = :permit, "
                  "updated_at = :updated_at "
                  "WHERE year = :year AND month = :month AND week = :week");

    query.bindValue(":proof_approval_checked", proofApprovalChecked ? 1 : 0);
    query.bindValue(":html_display_state", htmlDisplayState);
    query.bindValue(":job_data_locked", jobDataLocked ? 1 : 0);
    query.bindValue(":postage_data_locked", postageDataLocked ? 1 : 0);
    query.bindValue(":postage", postage);
    query.bindValue(":count", count);
    query.bindValue(":mail_class", mailClass);
    query.bindValue(":permit", permit);
    query.bindValue(":updated_at", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", week);

    // Also save to separate postage table for compatibility
    savePostageData(year, month, week, postage, count, mailClass, permit, postageDataLocked);

    return query.exec();
}

bool TMWeeklyPCDBManager::loadJobState(const QString& year, const QString& month, const QString& week,
                                       bool& proofApprovalChecked, int& htmlDisplayState,
                                       bool& jobDataLocked, bool& postageDataLocked,
                                       QString& postage, QString& count,
                                       QString& mailClass, QString& permit)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT proof_approval_checked, html_display_state, "
                  "job_data_locked, postage_data_locked, postage, count, mail_class, permit "
                  "FROM tm_weekly_pc_jobs "
                  "WHERE year = :year AND month = :month AND week = :week");

    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", week);

    if (!query.exec()) {
        return false;
    }

    if (!query.next()) {
        // No job found in main table, try fallback from log table
        Logger::instance().info(QString("No job state found in main table for %1/%2/%3, trying fallback from log").arg(year, month, week));
        
        // Set base defaults first
        proofApprovalChecked = false;
        htmlDisplayState = 0; // DefaultState
        jobDataLocked = false;
        postageDataLocked = false;
        postage.clear();
        count.clear();
        mailClass.clear();
        permit.clear();
        
        // Try to load postage data from log table as fallback
        QString fallbackPostage, fallbackCount, fallbackMailClass, fallbackPermit;
        if (loadPostageDataFromLog(year, month, week, fallbackPostage, fallbackCount, fallbackMailClass, fallbackPermit)) {
            // Successfully loaded from log, populate the postage fields
            postage = fallbackPostage;
            count = fallbackCount;
            mailClass = fallbackMailClass;
            permit = fallbackPermit;
            Logger::instance().info(QString("Fallback: Loaded postage data from log for %1/%2/%3").arg(year, month, week));
            
            // Since we found data in log, assume job exists and set reasonable defaults
            jobDataLocked = true; // Job must have existed to be in log
            postageDataLocked = true; // Data was locked when saved to log
            htmlDisplayState = 1; // ProofState since job was locked
            
            return true;
        } else {
            Logger::instance().warning(QString("Fallback: No postage data found in log for %1/%2/%3").arg(year, month, week));
            return false;
        }
    }

    // Main table data found, load normally
    proofApprovalChecked = query.value("proof_approval_checked").toInt() == 1;
    htmlDisplayState = query.value("html_display_state").toInt();
    jobDataLocked = query.value("job_data_locked").toInt() == 1;
    postageDataLocked = query.value("postage_data_locked").toInt() == 1;
    postage = query.value("postage").toString();
    count = query.value("count").toString();
    mailClass = query.value("mail_class").toString();
    permit = query.value("permit").toString();

    return true;
}

bool TMWeeklyPCDBManager::savePostageData(const QString& year, const QString& month, const QString& week,
                                          const QString& postage, const QString& count, const QString& mailClass,
                                          const QString& permit, bool locked)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for TMWeeklyPC savePostageData");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(R"(
        INSERT OR REPLACE INTO tm_weekly_pc_postage
        (year, month, week, postage, count, mail_class, permit, locked, updated_at)
        VALUES (:year, :month, :week, :postage, :count, :mail_class, :permit, :locked, :updated_at)
    )");

    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", week);
    query.bindValue(":postage", postage);
    query.bindValue(":count", count);
    query.bindValue(":mail_class", mailClass);
    query.bindValue(":permit", permit);
    query.bindValue(":locked", locked ? 1 : 0);
    query.bindValue(":updated_at", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    bool success = query.exec();
    if (success) {
        Logger::instance().info(QString("TMWeeklyPC postage data saved for %1/%2/%3").arg(year, month, week));
    } else {
        qDebug() << "Query failed:" << query.lastError().text();
        Logger::instance().error(QString("Failed to save TMWeeklyPC postage data for %1/%2/%3").arg(year, month, week));
    }

    return success;
}

bool TMWeeklyPCDBManager::loadPostageData(const QString& year, const QString& month, const QString& week,
                                          QString& postage, QString& count, QString& mailClass,
                                          QString& permit, bool& locked)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for TMWeeklyPC loadPostageData");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT postage, count, mail_class, permit, locked FROM tm_weekly_pc_postage "
                  "WHERE year = :year AND month = :month AND week = :week");

    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", week);

    if (!query.exec()) {
        Logger::instance().error(QString("Failed to execute TMWeeklyPC loadPostageData query for %1/%2/%3").arg(year, month, week));
        return false;
    }

    if (!query.next()) {
        // No postage data found, set defaults
        postage.clear();
        count.clear();
        mailClass.clear();
        permit.clear();
        locked = false;
        Logger::instance().warning(QString("No TMWeeklyPC postage data found for %1/%2/%3, using defaults").arg(year, month, week));
        return false;
    }

    // Load values from database
    postage = query.value("postage").toString();
    count = query.value("count").toString();
    mailClass = query.value("mail_class").toString();
    permit = query.value("permit").toString();
    locked = query.value("locked").toBool();

    Logger::instance().info(QString("TMWeeklyPC postage data loaded for %1/%2/%3").arg(year, month, week));
    return true;
}

bool TMWeeklyPCDBManager::deleteJob(const QString& year, const QString& month, const QString& week)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("DELETE FROM tm_weekly_pc_jobs "
                  "WHERE year = :year AND month = :month AND week = :week");

    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", week);

    return query.exec();
}

bool TMWeeklyPCDBManager::jobExists(const QString& year, const QString& month, const QString& week)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT COUNT(*) FROM tm_weekly_pc_jobs "
                  "WHERE year = :year AND month = :month AND week = :week");

    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", week);

    if (!query.exec() || !query.next()) {
        return false;
    }

    return query.value(0).toInt() > 0;
}

QList<QMap<QString, QString>> TMWeeklyPCDBManager::getAllJobs()
{
    QList<QMap<QString, QString>> result;

    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return result;
    }

    QList<QMap<QString, QVariant>> queryResult = m_dbManager->executeSelectQuery(
        "SELECT year, month, week, job_number FROM tm_weekly_pc_jobs "
        "ORDER BY year DESC, month DESC, week DESC"
    );

    for (const QMap<QString, QVariant>& row : std::as_const(queryResult)) {
        QMap<QString, QString> job;
        job["year"] = row["year"].toString();
        job["month"] = row["month"].toString();
        job["week"] = row["week"].toString();
        job["job_number"] = row["job_number"].toString();
        result.append(job);
    }

    return result;
}

bool TMWeeklyPCDBManager::addLogEntry(const QString& jobNumber, const QString& description,
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

    // CRITICAL FIX: Check if an entry for this specific job + description combination already exists
    // This prevents overwriting different dates for the same job number
    query.prepare("SELECT id FROM tm_weekly_log WHERE job_number = :job_number AND description = :description");
    query.bindValue(":job_number", jobNumber);
    query.bindValue(":description", description);

    if (!query.exec()) {
        qDebug() << "Failed to check existing log entry:" << query.lastError().text();
        return false;
    }

    if (query.next()) {
        // Entry exists, update it
        int id = query.value(0).toInt();
        query.prepare("UPDATE tm_weekly_log SET description = :description, postage = :postage, "
                      "count = :count, per_piece = :per_piece, class = :class, shape = :shape, "
                      "permit = :permit, date = :date WHERE id = :id");
        query.bindValue(":description", description);
        query.bindValue(":postage", postage);
        query.bindValue(":count", count);
        query.bindValue(":per_piece", perPiece);
        query.bindValue(":class", mailClass);
        query.bindValue(":shape", shape);
        query.bindValue(":permit", permit);
        query.bindValue(":date", date);
        query.bindValue(":id", id);
    } else {
        // No entry exists, insert new one
        query.prepare("INSERT INTO tm_weekly_log "
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
    }

    return query.exec();
}

QList<QMap<QString, QVariant>> TMWeeklyPCDBManager::getLog()
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return QList<QMap<QString, QVariant>>();
    }

    return m_dbManager->executeSelectQuery(
        "SELECT * FROM tm_weekly_log ORDER BY id DESC"
    );
}

void TMWeeklyPCDBManager::debugDatabaseContents(const QString& year, const QString& month) const
{
    // Debug function to examine database contents
    qDebug() << "[DEBUG] Called debugDatabaseContents with year:" << year << "month:" << month;
    
    if (!m_dbManager->isInitialized()) {
        qDebug() << "[DEBUG] Database not initialized";
        return;
    }
    
    // Debug output for database contents
    Logger::instance().info(QString("DEBUG: Examining database contents for %1/%2").arg(year, month));
}

bool TMWeeklyPCDBManager::saveTerminalLog(const QString& year, const QString& month,
                                          const QString& week, const QString& message)
{
    return m_dbManager->saveTerminalLog(TAB_NAME, year, month, week, message);
}

QStringList TMWeeklyPCDBManager::getTerminalLogs(const QString& year, const QString& month,
                                                 const QString& week)
{
    return m_dbManager->getTerminalLogs(TAB_NAME, year, month, week);
}

// NEW FUNCTION: Load log entry by job number, month, and week
bool TMWeeklyPCDBManager::loadLogEntry(const QString& jobNumber, const QString& month, const QString& week,
                                       QString& postage, QString& count, QString& mailClass, QString& permit)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for TMWeeklyPC loadLogEntry");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT postage, count, class, permit FROM tm_weekly_log "
                  "WHERE job_number = :job_number AND description = :description");
    
    query.bindValue(":job_number", jobNumber);
    
    // Try multiple description formats for legacy compatibility
    QStringList formatPatterns;
    
    // 1. Current format: "TM WEEKLY 07.02"
    formatPatterns << QString("TM WEEKLY %1.%2").arg(month, week);
    
    // 2. Legacy format with full month name: "TM WEEKLY JULY.02"
    QMap<QString, QString> monthNames = {
        {"01", "JANUARY"}, {"02", "FEBRUARY"}, {"03", "MARCH"}, {"04", "APRIL"},
        {"05", "MAY"}, {"06", "JUNE"}, {"07", "JULY"}, {"08", "AUGUST"},
        {"09", "SEPTEMBER"}, {"10", "OCTOBER"}, {"11", "NOVEMBER"}, {"12", "DECEMBER"}
    };
    if (monthNames.contains(month)) {
        formatPatterns << QString("TM WEEKLY %1.%2").arg(monthNames[month], week);
    }
    
    // 3. Legacy format with abbreviated month: "TM WEEKLY JUL.02"
    QMap<QString, QString> monthAbbrevs = {
        {"01", "JAN"}, {"02", "FEB"}, {"03", "MAR"}, {"04", "APR"},
        {"05", "MAY"}, {"06", "JUN"}, {"07", "JUL"}, {"08", "AUG"},
        {"09", "SEP"}, {"10", "OCT"}, {"11", "NOV"}, {"12", "DEC"}
    };
    if (monthAbbrevs.contains(month)) {
        formatPatterns << QString("TM WEEKLY %1.%2").arg(monthAbbrevs[month], week);
    }
    
    // Try each format pattern until one succeeds
    for (const QString& descriptionPattern : formatPatterns) {
        query.bindValue(":description", descriptionPattern);
        
        Logger::instance().info(QString("TMWeeklyPC loadLogEntry: Trying job=%1, description='%2'")
                               .arg(jobNumber, descriptionPattern));
        
        if (!query.exec()) {
            Logger::instance().error(QString("Failed to execute TMWeeklyPC loadLogEntry query for job %1, %2/%3: %4")
                                     .arg(jobNumber, month, week, query.lastError().text()));
            continue; // Try next format
        }
        
        if (query.next()) {
            // Found a match! Load values from database
            postage = query.value("postage").toString();
            count = query.value("count").toString();
            mailClass = query.value("class").toString();
            permit = query.value("permit").toString();
            
            Logger::instance().info(QString("TMWeeklyPC log entry loaded for job %1, description '%2': postage=%3, count=%4, class=%5, permit=%6")
                                   .arg(jobNumber, descriptionPattern, postage, count, mailClass, permit));
            return true;
        }
        
        // Reset query for next attempt
        query.finish();
    }
    
    // No log entry found with any format, try debugging
    Logger::instance().warning(QString("No TMWeeklyPC log entry found for job %1 with any description format. Checking what's actually in database...")
                              .arg(jobNumber));
    
    // Debug: Show what descriptions exist for this job number
    QSqlQuery debugQuery(m_dbManager->getDatabase());
    debugQuery.prepare("SELECT description FROM tm_weekly_log WHERE job_number = :job_number");
    debugQuery.bindValue(":job_number", jobNumber);
    if (debugQuery.exec()) {
        QStringList foundDescriptions;
        while (debugQuery.next()) {
            foundDescriptions << debugQuery.value("description").toString();
        }
        Logger::instance().info(QString("TMWeeklyPC loadLogEntry: Job %1 has descriptions: [%2]")
                               .arg(jobNumber, foundDescriptions.join(", ")));
    }
    
    return false;
}

// NEW FUNCTION: Load postage data from log table (fallback method)
bool TMWeeklyPCDBManager::loadPostageDataFromLog(const QString& year, const QString& month, const QString& week,
                                                QString& postage, QString& count, QString& mailClass,
                                                QString& permit)
{
    Logger::instance().info(QString("TMWeeklyPC loadPostageDataFromLog: Attempting fallback for %1/%2/%3")
                           .arg(year, month, week));
    
    // First, get the job number for this year/month/week
    QString jobNumber;
    if (!loadJob(year, month, week, jobNumber)) {
        Logger::instance().warning(QString("Cannot load postage from log: no job found for %1/%2/%3")
                                  .arg(year, month, week));
        return false;
    }

    Logger::instance().info(QString("TMWeeklyPC loadPostageDataFromLog: Found job number %1 for %2/%3/%4")
                           .arg(jobNumber, year, month, week));

    // Now try to load from log entry using composite key
    QString rawPostage, rawCount, rawClass, rawPermit;
    if (!loadLogEntry(jobNumber, month, week, rawPostage, rawCount, rawClass, rawPermit)) {
        Logger::instance().warning(QString("TMWeeklyPC loadPostageDataFromLog: loadLogEntry failed for job %1, %2/%3")
                                  .arg(jobNumber, month, week));
        return false;
    }

    // Normalize and format the data for widget display
    
    // Add $ prefix to postage if missing
    postage = rawPostage;
    if (!postage.isEmpty() && !postage.startsWith("$")) {
        postage = "$" + postage;
    }
    
    // Count should be as-is from log
    count = rawCount;
    
    // Convert abbreviated class to full name
    if (rawClass == "STD") {
        mailClass = "STANDARD";
    } else if (rawClass == "FC") {
        mailClass = "FIRST CLASS";
    } else {
        mailClass = rawClass; // Use as-is if not abbreviated
    }
    
    // Permit should be as-is (METER, 1662, etc.)
    permit = rawPermit;

    Logger::instance().info(QString("TMWeeklyPC postage data loaded from log for %1/%2/%3: Postage=%4, Count=%5, Class=%6, Permit=%7")
                           .arg(year, month, week, postage, count, mailClass, permit));
    return true;
}
