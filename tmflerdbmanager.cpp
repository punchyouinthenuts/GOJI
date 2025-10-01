#include "tmflerdbmanager.h"
#include "logger.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDate>
#include <QDebug>

// Initialize static member
TMFLERDBManager* TMFLERDBManager::m_instance = nullptr;

TMFLERDBManager::TMFLERDBManager(QObject *parent)
    : QObject(parent)
    , m_dbManager(DatabaseManager::instance())
    , m_trackerModel(nullptr)
{
    if (m_dbManager && m_dbManager->isInitialized()) {
        initializeTables();
    }
}

TMFLERDBManager::~TMFLERDBManager()
{
    if (m_trackerModel) {
        delete m_trackerModel;
    }
}

TMFLERDBManager* TMFLERDBManager::instance()
{
    if (!m_instance) {
        m_instance = new TMFLERDBManager();
    }
    return m_instance;
}

bool TMFLERDBManager::initializeTables()
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        Logger::instance().error("Database manager not initialized for TMFLER");
        return false;
    }

    bool success = createTables();
    if (success) {
        // Create tracker model
        m_trackerModel = new QSqlTableModel(this, m_dbManager->getDatabase());
        m_trackerModel->setTable("tm_fler_log");
        m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);

        // Set headers
        m_trackerModel->setHeaderData(1, Qt::Horizontal, "JOB");
        m_trackerModel->setHeaderData(2, Qt::Horizontal, "DESCRIPTION");
        m_trackerModel->setHeaderData(3, Qt::Horizontal, "POSTAGE");
        m_trackerModel->setHeaderData(4, Qt::Horizontal, "COUNT");
        m_trackerModel->setHeaderData(5, Qt::Horizontal, "AVG RATE");
        m_trackerModel->setHeaderData(6, Qt::Horizontal, "CLASS");
        m_trackerModel->setHeaderData(7, Qt::Horizontal, "SHAPE");
        m_trackerModel->setHeaderData(8, Qt::Horizontal, "PERMIT");
        m_trackerModel->setHeaderData(9, Qt::Horizontal, "DATE");

        m_trackerModel->select();
        Logger::instance().info("TMFLER tracker model initialized");
    }

    return success;
}

bool TMFLERDBManager::updateLogEntryForJob(const QString& jobNumber, const QString& description,
                                           const QString& postage, const QString& count,
                                           const QString& avgRate, const QString& mailClass,
                                           const QString& shape, const QString& permit,
                                           const QString& date)
{
    if (!m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for TMFLER updateLogEntryForJob");
        return false;
    }

    // CRITICAL FIX: Update the existing log entry for this specific job
    // This targets the correct row based on the currently loaded job
    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("UPDATE tm_fler_log SET "
                  "description = :description, postage = :postage, count = :count, "
                  "per_piece = :per_piece, class = :class, shape = :shape, "
                  "permit = :permit, date = :date "
                  "WHERE job_number = :job_number");

    query.bindValue(":job_number", jobNumber);
    query.bindValue(":description", description);
    query.bindValue(":postage", postage);
    query.bindValue(":count", count);
    query.bindValue(":per_piece", avgRate);  // This is actually avg rate
    query.bindValue(":class", mailClass);
    query.bindValue(":shape", shape);
    query.bindValue(":permit", permit);
    query.bindValue(":date", date);

    bool success = m_dbManager->executeQuery(query);
    if (success && query.numRowsAffected() > 0) {
        Logger::instance().info(QString("TMFLER log entry updated for job %1: %2 pieces at %3")
                                   .arg(jobNumber, count, postage));
        if (m_trackerModel) {
            m_trackerModel->select(); // Refresh the model
        }
        return true;
    }
    
    // No rows were affected (no existing entry found for this job)
    Logger::instance().info(QString("No existing TMFLER log entry found for job %1, will need to insert new")
                               .arg(jobNumber));
    return false;
}

bool TMFLERDBManager::createTables()
{
    QSqlQuery query(m_dbManager->getDatabase());

    // MIGRATION: Check if old table exists with incorrect UNIQUE constraint
    bool needsMigration = false;
    if (query.exec("SELECT sql FROM sqlite_master WHERE type='table' AND name='tm_fler_jobs'")) {
        if (query.next()) {
            QString schema = query.value(0).toString();
            if (schema.contains("UNIQUE(job_number, year, month)")) {
                needsMigration = true;
                Logger::instance().info("Detected tm_fler_jobs table with old schema - migration needed");
            }
        }
    }

    if (needsMigration) {
        // Step 1: Rename old table
        if (!query.exec("ALTER TABLE tm_fler_jobs RENAME TO tm_fler_jobs_old")) {
            Logger::instance().error("Failed to rename old tm_fler_jobs table: " + query.lastError().text());
            return false;
        }
        Logger::instance().info("Renamed old tm_fler_jobs table to tm_fler_jobs_old");

        // Step 2: Create new table with correct schema
        if (!query.exec("CREATE TABLE tm_fler_jobs ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                        "job_number TEXT NOT NULL, "
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
            Logger::instance().error("Failed to create new tm_fler_jobs table: " + query.lastError().text());
            return false;
        }
        Logger::instance().info("Created new tm_fler_jobs table with UNIQUE(year, month) constraint");

        // Step 3: Migrate data - keep only the most recent entry per year/month
        if (!query.exec("INSERT INTO tm_fler_jobs "
                        "(job_number, year, month, html_display_state, job_data_locked, "
                        "postage_data_locked, postage, count, last_executed_script, created_at, updated_at) "
                        "SELECT job_number, year, month, html_display_state, job_data_locked, "
                        "postage_data_locked, postage, count, last_executed_script, created_at, updated_at "
                        "FROM tm_fler_jobs_old "
                        "WHERE id IN (SELECT id FROM tm_fler_jobs_old t1 "
                        "WHERE updated_at = (SELECT MAX(updated_at) FROM tm_fler_jobs_old t2 "
                        "WHERE t1.year = t2.year AND t1.month = t2.month))")) {
            Logger::instance().error("Failed to migrate data to new tm_fler_jobs table: " + query.lastError().text());
            return false;
        }

        int migratedRows = query.numRowsAffected();
        Logger::instance().info(QString("Migrated %1 job records to new schema (most recent per period)").arg(migratedRows));

        // Step 4: Drop old table
        if (!query.exec("DROP TABLE tm_fler_jobs_old")) {
            Logger::instance().warning("Failed to drop old tm_fler_jobs_old table: " + query.lastError().text());
            // Non-fatal - continue
        } else {
            Logger::instance().info("Dropped old tm_fler_jobs_old table");
        }
    } else {
        // Create jobs table with correct UNIQUE constraint
        if (!query.exec("CREATE TABLE IF NOT EXISTS tm_fler_jobs ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                        "job_number TEXT NOT NULL, "
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
            qDebug() << "Error creating tm_fler_jobs table:" << query.lastError().text();
            Logger::instance().error("Failed to create tm_fler_jobs table: " + query.lastError().text());
            return false;
        }
    }

    // Create log table
    if (!query.exec("CREATE TABLE IF NOT EXISTS tm_fler_log ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "job_number TEXT NOT NULL, "
                    "description TEXT NOT NULL, "
                    "postage TEXT NOT NULL, "
                    "count TEXT NOT NULL, "
                    "per_piece TEXT NOT NULL, "
                    "class TEXT NOT NULL, "
                    "shape TEXT NOT NULL, "
                    "permit TEXT NOT NULL, "
                    "date TEXT NOT NULL, "
                    "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                    ")")) {
        qDebug() << "Error creating tm_fler_log table:" << query.lastError().text();
        Logger::instance().error("Failed to create tm_fler_log table: " + query.lastError().text());
        return false;
    }

    Logger::instance().info("TMFLER database tables created successfully");
    return true;
}

bool TMFLERDBManager::saveJob(const QString& jobNumber, const QString& year, const QString& month)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for TMFLER saveJob");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());

    // First try to update existing record (matches TM Term pattern)
    query.prepare("UPDATE tm_fler_jobs SET "
                  "job_number = :job_number, "
                  "updated_at = :updated_at "
                  "WHERE year = :year AND month = :month");
    query.bindValue(":job_number", jobNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":updated_at", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    if (!m_dbManager->executeQuery(query)) {
        Logger::instance().error(QString("Failed to update TMFLER job: %1 for %2/%3 - %4").arg(jobNumber, year, month, query.lastError().text()));
        return false;
    }

    // Check if update affected any rows
    if (query.numRowsAffected() == 0) {
        // No existing record, insert new one
        query.prepare("INSERT INTO tm_fler_jobs "
                      "(job_number, year, month, created_at, updated_at) "
                      "VALUES (:job_number, :year, :month, :created_at, :updated_at)");
        query.bindValue(":job_number", jobNumber);
        query.bindValue(":year", year);
        query.bindValue(":month", month);
        QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        query.bindValue(":created_at", currentTime);
        query.bindValue(":updated_at", currentTime);

        if (!m_dbManager->executeQuery(query)) {
            Logger::instance().error(QString("Failed to insert TMFLER job: %1 for %2/%3 - %4").arg(jobNumber, year, month, query.lastError().text()));
            return false;
        }
    }

    Logger::instance().info(QString("TMFLER job saved: %1 for %2/%3").arg(jobNumber, year, month));
    return true;
}

bool TMFLERDBManager::loadJob(const QString& year, const QString& month, QString& jobNumber)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for TMFLER loadJob");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT job_number FROM tm_fler_jobs WHERE year = :year AND month = :month ORDER BY updated_at DESC LIMIT 1");
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    if (m_dbManager->executeQuery(query) && query.next()) {
        jobNumber = query.value("job_number").toString();
        Logger::instance().info(QString("TMFLER job loaded: %1 for %2/%3").arg(jobNumber, year, month));
        return true;
    } else {
        Logger::instance().warning(QString("No TMFLER job found for %1/%2").arg(year, month));
        return false;
    }
}

QList<QMap<QString, QString>> TMFLERDBManager::getAllJobs()
{
    QList<QMap<QString, QString>> jobs;

    if (!m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for TMFLER getAllJobs");
        return jobs;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT job_number, year, month FROM tm_fler_jobs ORDER BY year DESC, month DESC, updated_at DESC");

    if (m_dbManager->executeQuery(query)) {
        while (query.next()) {
            QMap<QString, QString> job;
            job["job_number"] = query.value("job_number").toString();
            job["year"] = query.value("year").toString();
            job["month"] = query.value("month").toString();
            jobs.append(job);
        }
    } else {
        Logger::instance().error("Failed to retrieve TMFLER jobs: " + query.lastError().text());
    }

    Logger::instance().info(QString("Retrieved %1 TMFLER jobs from database").arg(jobs.size()));
    return jobs;
}

QSqlTableModel* TMFLERDBManager::getTrackerModel()
{
    return m_trackerModel;
}

bool TMFLERDBManager::addLogEntry(const QString& jobNumber, const QString& description,
                                  const QString& postage, const QString& count,
                                  const QString& perPiece, const QString& mailClass,
                                  const QString& shape, const QString& permit,
                                  const QString& date)
{
    if (!m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for TMFLER addLogEntry");
        return false;
    }

    // CRITICAL FIX: For TMFLER, we need to use job_number + year + month as unique key
    // Extract year and month from description if possible
    QString year, month;
    if (description.contains("TM ") && description.contains(" FL ER")) {
        QStringList parts = description.split(" ", Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            QString monthAbbrev = parts[1]; // Month abbreviation like "JUL"
            
            // Convert month abbreviation to number for consistent lookup
            QMap<QString, QString> monthMap = {
                {"JAN", "01"}, {"FEB", "02"}, {"MAR", "03"}, {"APR", "04"},
                {"MAY", "05"}, {"JUN", "06"}, {"JUL", "07"}, {"AUG", "08"},
                {"SEP", "09"}, {"OCT", "10"}, {"NOV", "11"}, {"DEC", "12"}
            };
            month = monthMap.value(monthAbbrev, "");
            
            // For year, we derive it from the current date
            year = QString::number(QDate::currentDate().year());
        }
    }

    QSqlQuery query(m_dbManager->getDatabase());
    
    // FIXED: Check if an entry for this job+year+month combination already exists
    if (!year.isEmpty() && !month.isEmpty()) {
        // Find existing entry based on job_number and derived year/month
        query.prepare("SELECT id FROM tm_fler_log WHERE job_number = :job_number AND description LIKE :description_pattern");
        query.bindValue(":job_number", jobNumber);
        QString monthAbbrev;
        QMap<QString, QString> reverseMonthMap = {
            {"01", "JAN"}, {"02", "FEB"}, {"03", "MAR"}, {"04", "APR"},
            {"05", "MAY"}, {"06", "JUN"}, {"07", "JUL"}, {"08", "AUG"},
            {"09", "SEP"}, {"10", "OCT"}, {"11", "NOV"}, {"12", "DEC"}
        };
        monthAbbrev = reverseMonthMap.value(month, month);
        query.bindValue(":description_pattern", QString("%%TM %1 FL ER%%").arg(monthAbbrev));
    } else {
        // Fallback: use the original logic with job_number + description + date
        Logger::instance().warning("Could not extract year/month from description: " + description + " - using job+description+date match");
        query.prepare("SELECT id FROM tm_fler_log WHERE job_number = :job_number AND description = :description AND date = :date");
        query.bindValue(":job_number", jobNumber);
        query.bindValue(":description", description);
        query.bindValue(":date", date);
    }

    if (!query.exec()) {
        Logger::instance().error("Failed to check existing TMFLER log entry: " + query.lastError().text());
        return false;
    }

    if (query.next()) {
        // Entry exists, update it
        int id = query.value(0).toInt();
        query.prepare("UPDATE tm_fler_log SET "
                      "description = :description, postage = :postage, "
                      "count = :count, per_piece = :per_piece, class = :class, "
                      "shape = :shape, permit = :permit, date = :date "
                      "WHERE id = :id");

        query.bindValue(":id", id);
        query.bindValue(":description", description);
        query.bindValue(":postage", postage);
        query.bindValue(":count", count);
        query.bindValue(":per_piece", perPiece);
        query.bindValue(":class", mailClass);
        query.bindValue(":shape", shape);
        query.bindValue(":permit", permit);
        query.bindValue(":date", date);

        bool success = m_dbManager->executeQuery(query);
        if (success) {
            Logger::instance().info(QString("TMFLER log entry updated for job %1, %2/%3: %4 pieces at %5")
                                       .arg(jobNumber, year, month, count, postage));
            if (m_trackerModel) {
                m_trackerModel->select(); // Refresh the model
            }
        } else {
            Logger::instance().error(QString("Failed to update TMFLER log entry: Job %1 - %2").arg(jobNumber, query.lastError().text()));
        }
        return success;
    } else {
        // No entry exists, insert new one
        query.prepare("INSERT INTO tm_fler_log "
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

        bool success = m_dbManager->executeQuery(query);
        if (success) {
            Logger::instance().info(QString("TMFLER log entry inserted for job %1, %2/%3: %4 pieces at %5")
                                       .arg(jobNumber, year, month, count, postage));
            if (m_trackerModel) {
                m_trackerModel->select(); // Refresh the model
            }
        } else {
            Logger::instance().error(QString("Failed to insert TMFLER log entry: Job %1 - %2").arg(jobNumber, query.lastError().text()));
        }
        return success;
    }
}

bool TMFLERDBManager::deleteLogEntry(int id)
{
    if (!m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for TMFLER deleteLogEntry");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("DELETE FROM tm_fler_log WHERE id = :id");
    query.bindValue(":id", id);

    bool success = m_dbManager->executeQuery(query);
    if (success) {
        Logger::instance().info(QString("TMFLER log entry deleted: ID %1").arg(id));
        if (m_trackerModel) {
            m_trackerModel->select(); // Refresh the model
        }
    } else {
        Logger::instance().error(QString("Failed to delete TMFLER log entry: ID %1 - %2").arg(id).arg(query.lastError().text()));
    }

    return success;
}

bool TMFLERDBManager::updateLogEntry(int id, const QString& jobNumber, const QString& description,
                                     const QString& postage, const QString& count,
                                     const QString& perPiece, const QString& mailClass,
                                     const QString& shape, const QString& permit,
                                     const QString& date)
{
    if (!m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for TMFLER updateLogEntry");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("UPDATE tm_fler_log SET "
                  "job_number = :job_number, description = :description, postage = :postage, "
                  "count = :count, per_piece = :per_piece, class = :class, "
                  "shape = :shape, permit = :permit, date = :date "
                  "WHERE id = :id");

    query.bindValue(":id", id);
    query.bindValue(":job_number", jobNumber);
    query.bindValue(":description", description);
    query.bindValue(":postage", postage);
    query.bindValue(":count", count);
    query.bindValue(":per_piece", perPiece);
    query.bindValue(":class", mailClass);
    query.bindValue(":shape", shape);
    query.bindValue(":permit", permit);
    query.bindValue(":date", date);

    bool success = m_dbManager->executeQuery(query);
    if (success) {
        Logger::instance().info(QString("TMFLER log entry updated: ID %1").arg(id));
        if (m_trackerModel) {
            m_trackerModel->select(); // Refresh the model
        }
    } else {
        Logger::instance().error(QString("Failed to update TMFLER log entry: ID %1 - %2").arg(id).arg(query.lastError().text()));
    }

    return success;
}

bool TMFLERDBManager::saveJobState(const QString& year, const QString& month,
                                   int htmlDisplayState, bool jobDataLocked, bool postageDataLocked,
                                   const QString& lastExecutedScript)
{
    // Call the enhanced version with empty postage and count
    return saveJobState(year, month, htmlDisplayState, jobDataLocked, postageDataLocked,
                        "", "", lastExecutedScript);
}

// ADDED: Enhanced saveJobState with postage data (like TMTERM)
bool TMFLERDBManager::saveJobState(const QString& year, const QString& month,
                                   int htmlDisplayState, bool jobDataLocked, bool postageDataLocked,
                                   const QString& postage, const QString& count,
                                   const QString& lastExecutedScript)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for TMFLER saveJobState");
        return false;
    }

    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("UPDATE tm_fler_jobs SET "
                  "html_display_state = :html_display_state, "
                  "job_data_locked = :job_data_locked, "
                  "postage_data_locked = :postage_data_locked, "
                  "postage = :postage, "                    // ADDED: Save postage data
                  "count = :count, "                        // ADDED: Save count data
                  "last_executed_script = :last_executed_script, "
                  "updated_at = :updated_at "
                  "WHERE year = :year AND month = :month");
    query.bindValue(":html_display_state", htmlDisplayState);
    query.bindValue(":job_data_locked", jobDataLocked ? 1 : 0);
    query.bindValue(":postage_data_locked", postageDataLocked ? 1 : 0);
    query.bindValue(":postage", postage);                    // ADDED: Bind postage data
    query.bindValue(":count", count);                        // ADDED: Bind count data
    query.bindValue(":last_executed_script", lastExecutedScript);
    query.bindValue(":updated_at", currentTime);
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    if (!m_dbManager->executeQuery(query)) {
        Logger::instance().error(QString("Failed to update TMFLER job state for %1/%2: %3")
                                     .arg(year, month, query.lastError().text()));
        return false;
    }

    // Check if any rows were affected (updated)
    if (query.numRowsAffected() == 0) {
        // No existing record found, insert new one
        query.prepare("INSERT INTO tm_fler_jobs "
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
            Logger::instance().error(QString("Failed to insert TMFLER job state for %1/%2: %3")
                                         .arg(year, month, query.lastError().text()));
            return false;
        }
    }

    Logger::instance().info(QString("TMFLER job state saved for %1/%2: postage=%3, count=%4, locked=%5")
                                .arg(year, month, postage, count, postageDataLocked ? "true" : "false"));
    return true;
}

bool TMFLERDBManager::loadJobState(const QString& year, const QString& month,
                                   int& htmlDisplayState, bool& jobDataLocked, bool& postageDataLocked,
                                   QString& lastExecutedScript)
{
    // Call the enhanced version and ignore postage and count
    QString postage, count;
    return loadJobState(year, month, htmlDisplayState, jobDataLocked, postageDataLocked,
                        postage, count, lastExecutedScript);
}

// ADDED: Enhanced loadJobState with postage data (like TMTERM)
bool TMFLERDBManager::loadJobState(const QString& year, const QString& month,
                                   int& htmlDisplayState, bool& jobDataLocked, bool& postageDataLocked,
                                   QString& postage, QString& count, QString& lastExecutedScript)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for TMFLER loadJobState");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT html_display_state, job_data_locked, postage_data_locked, "
                  "postage, count, last_executed_script FROM tm_fler_jobs "
                  "WHERE year = :year AND month = :month");
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    if (!m_dbManager->executeQuery(query)) {
        Logger::instance().error(QString("Failed to execute TMFLER loadJobState query for %1/%2: %3")
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
        Logger::instance().info(QString("No TMFLER job state found for %1/%2, using defaults").arg(year, month));
        return false;
    }

    // Load values from database
    htmlDisplayState = query.value("html_display_state").toInt();
    jobDataLocked = query.value("job_data_locked").toInt() == 1;
    postageDataLocked = query.value("postage_data_locked").toInt() == 1;
    postage = query.value("postage").toString();
    count = query.value("count").toString();
    lastExecutedScript = query.value("last_executed_script").toString();

    Logger::instance().info(QString("TMFLER job state loaded for %1/%2: postage=%3, count=%4, locked=%5")
                                .arg(year, month, postage, count, postageDataLocked ? "true" : "false"));
    return true;
}

bool TMFLERDBManager::updateLogJobNumber(const QString& oldJobNumber, const QString& newJobNumber)
{
    if (!m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for FLER updateLogJobNumber");
        return false;
    }
    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("UPDATE tm_fler_log SET job_number = :new_job_number WHERE job_number = :old_job_number");
    query.bindValue(":new_job_number", newJobNumber);
    query.bindValue(":old_job_number", oldJobNumber);
    const bool success = query.exec();
    if (success) {
        Logger::instance().info(QString("Updated FLER log job number: %1 -> %2").arg(oldJobNumber, newJobNumber));
    } else {
        Logger::instance().error(QString("Failed FLER job-number update: %1").arg(query.lastError().text()));
    }
    return success;
}
