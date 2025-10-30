#include "fhdbmanager.h"
#include "logger.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDate>
#include <QDebug>

// Initialize static member
FHDBManager* FHDBManager::m_instance = nullptr;

FHDBManager::FHDBManager(QObject *parent)
    : QObject(parent)
    , m_dbManager(DatabaseManager::instance())
    , m_trackerModel(nullptr)
{
    if (m_dbManager && m_dbManager->isInitialized()) {
        initializeTables();
    }
}

FHDBManager::~FHDBManager()
{
    if (m_trackerModel) {
        delete m_trackerModel;
    }
}

FHDBManager* FHDBManager::instance()
{
    if (!m_instance) {
        m_instance = new FHDBManager();
    }
    return m_instance;
}

bool FHDBManager::initializeTables()
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        Logger::instance().error("Database manager not initialized for FOUR HANDS");
        return false;
    }

    bool success = createTables();
    if (success) {
        // Create tracker model
        m_trackerModel = new QSqlTableModel(this, m_dbManager->getDatabase());
        m_trackerModel->setTable("fh_log");
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
        Logger::instance().info("FOUR HANDS tracker model initialized");
    }

    return success;
}

bool FHDBManager::createTables()
{
    QSqlQuery query(m_dbManager->getDatabase());

    // Create jobs table with UNIQUE constraint on year/month
    // ✅ Added: drop_number column for FOUR HANDS job persistence
    if (!query.exec("CREATE TABLE IF NOT EXISTS fh_jobs ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "job_number TEXT NOT NULL, "
                    "year TEXT NOT NULL, "
                    "month TEXT NOT NULL, "
                    "drop_number TEXT DEFAULT '', "
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
        qDebug() << "Error creating fh_jobs table:" << query.lastError().text();
        Logger::instance().error("Failed to create fh_jobs table: " + query.lastError().text());
        return false;
    }

    // ✅ Added: Ensure drop_number column exists for existing databases
    QSqlQuery checkColumn(m_dbManager->getDatabase());
    checkColumn.exec("PRAGMA table_info(fh_jobs)");
    bool hasDropNumber = false;
    while (checkColumn.next()) {
        if (checkColumn.value("name").toString() == "drop_number") {
            hasDropNumber = true;
            break;
        }
    }
    if (!hasDropNumber) {
        if (query.exec("ALTER TABLE fh_jobs ADD COLUMN drop_number TEXT DEFAULT ''")) {
            Logger::instance().info("Added drop_number column to existing fh_jobs table");
        } else {
            Logger::instance().warning("Failed to add drop_number column (may already exist): " + query.lastError().text());
        }
    }

    // Create log table
    if (!query.exec("CREATE TABLE IF NOT EXISTS fh_log ("
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
        qDebug() << "Error creating fh_log table:" << query.lastError().text();
        Logger::instance().error("Failed to create fh_log table: " + query.lastError().text());
        return false;
    }

    Logger::instance().info("FOUR HANDS database tables created successfully");
    return true;
}

bool FHDBManager::saveJob(const QString& jobNumber, const QString& year, const QString& month)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for FOUR HANDS saveJob");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());

    // First try to update existing record
    query.prepare("UPDATE fh_jobs SET "
                  "job_number = :job_number, "
                  "updated_at = :updated_at "
                  "WHERE year = :year AND month = :month");
    query.bindValue(":job_number", jobNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":updated_at", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    if (!m_dbManager->executeQuery(query)) {
        Logger::instance().error(QString("Failed to update FOUR HANDS job: %1 for %2/%3 - %4").arg(jobNumber, year, month, query.lastError().text()));
        return false;
    }

    // Check if update affected any rows
    if (query.numRowsAffected() == 0) {
        // No existing record, insert new one
        query.prepare("INSERT INTO fh_jobs "
                      "(job_number, year, month, created_at, updated_at) "
                      "VALUES (:job_number, :year, :month, :created_at, :updated_at)");
        query.bindValue(":job_number", jobNumber);
        query.bindValue(":year", year);
        query.bindValue(":month", month);
        QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        query.bindValue(":created_at", currentTime);
        query.bindValue(":updated_at", currentTime);

        if (!m_dbManager->executeQuery(query)) {
            Logger::instance().error(QString("Failed to insert FOUR HANDS job: %1 for %2/%3 - %4").arg(jobNumber, year, month, query.lastError().text()));
            return false;
        }
    }

    Logger::instance().info(QString("FOUR HANDS job saved: %1 for %2/%3").arg(jobNumber, year, month));
    return true;
}

bool FHDBManager::loadJob(const QString& year, const QString& month, QString& jobNumber)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for FOUR HANDS loadJob");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT job_number FROM fh_jobs WHERE year = :year AND month = :month ORDER BY updated_at DESC LIMIT 1");
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    if (m_dbManager->executeQuery(query) && query.next()) {
        jobNumber = query.value("job_number").toString();
        Logger::instance().info(QString("FOUR HANDS job loaded: %1 for %2/%3").arg(jobNumber, year, month));
        return true;
    } else {
        Logger::instance().warning(QString("No FOUR HANDS job found for %1/%2").arg(year, month));
        return false;
    }
}

bool FHDBManager::deleteJob(int year, int month)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for FOUR HANDS deleteJob");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("DELETE FROM fh_jobs "
                  "WHERE year = :year AND month = :month");
    query.bindValue(":year", QString::number(year));
    query.bindValue(":month", QString("%1").arg(month, 2, 10, QChar('0')));

    bool success = m_dbManager->executeQuery(query);
    if (success) {
        Logger::instance().info(QString("FOUR HANDS job deleted for %1/%2").arg(QString::number(year), QString("%1").arg(month, 2, 10, QChar('0'))));
    } else {
        Logger::instance().error(QString("Failed to delete FOUR HANDS job for %1/%2").arg(QString::number(year), QString("%1").arg(month, 2, 10, QChar('0'))));
    }
    return success;
}

QList<QMap<QString, QString>> FHDBManager::getAllJobs()
{
    QList<QMap<QString, QString>> jobs;
    if (!m_dbManager->isInitialized()) return jobs;
    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT job_number, year, month, drop_number "
                  "FROM fh_jobs "
                  "WHERE job_number != '' "
                  "ORDER BY year DESC, month DESC, updated_at DESC");
    if (m_dbManager->executeQuery(query)) {
        while (query.next()) {
            QMap<QString, QString> job;
            job["job_number"] = query.value("job_number").toString();
            job["year"] = query.value("year").toString();
            job["month"] = query.value("month").toString();
            job["drop_number"] = query.value("drop_number").toString();
            jobs.append(job);
        }
    } else {
        QSqlQuery fallback(m_dbManager->getDatabase());
        fallback.prepare("SELECT job_number, year, month "
                         "FROM fh_jobs "
                         "WHERE job_number != '' "
                         "ORDER BY year DESC, month DESC, updated_at DESC");
        if (m_dbManager->executeQuery(fallback)) {
            while (fallback.next()) {
                QMap<QString, QString> job;
                job["job_number"] = fallback.value("job_number").toString();
                job["year"] = fallback.value("year").toString();
                job["month"] = fallback.value("month").toString();
                job["drop_number"] = "";
                jobs.append(job);
            }
        }
    }
    return jobs;
}

QSqlTableModel* FHDBManager::getTrackerModel()
{
    return m_trackerModel;
}

bool FHDBManager::addLogEntry(const QString& jobNumber, const QString& description,
                              const QString& postage, const QString& count,
                              const QString& perPiece, const QString& mailClass,
                              const QString& shape, const QString& permit,
                              const QString& date)
{
    if (!m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for FOUR HANDS addLogEntry");
        return false;
    }

    // Extract year and month from description if possible
    QString year, month;
    if (description.contains("FH ") && description.length() > 6) {
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
            
            // For year, derive it from the current date
            year = QString::number(QDate::currentDate().year());
        }
    }

    QSqlQuery query(m_dbManager->getDatabase());
    
    // Check if an entry for this job+year+month combination already exists
    if (!year.isEmpty() && !month.isEmpty()) {
        query.prepare("SELECT id FROM fh_log WHERE job_number = :job_number AND description LIKE :description_pattern");
        query.bindValue(":job_number", jobNumber);
        QString monthAbbrev;
        QMap<QString, QString> reverseMonthMap = {
            {"01", "JAN"}, {"02", "FEB"}, {"03", "MAR"}, {"04", "APR"},
            {"05", "MAY"}, {"06", "JUN"}, {"07", "JUL"}, {"08", "AUG"},
            {"09", "SEP"}, {"10", "OCT"}, {"11", "NOV"}, {"12", "DEC"}
        };
        monthAbbrev = reverseMonthMap.value(month, month);
        query.bindValue(":description_pattern", QString("%%FH %1%%").arg(monthAbbrev));
    } else {
        // Fallback: use job_number + description + date
        Logger::instance().warning("Could not extract year/month from description: " + description + " - using job+description+date match");
        query.prepare("SELECT id FROM fh_log WHERE job_number = :job_number AND description = :description AND date = :date");
        query.bindValue(":job_number", jobNumber);
        query.bindValue(":description", description);
        query.bindValue(":date", date);
    }

    if (!query.exec()) {
        Logger::instance().error("Failed to check existing FOUR HANDS log entry: " + query.lastError().text());
        return false;
    }

    if (query.next()) {
        // Entry exists, update it
        int id = query.value(0).toInt();
        query.prepare("UPDATE fh_log SET "
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
            Logger::instance().info(QString("FOUR HANDS log entry updated for job %1, %2/%3: %4 pieces at %5")
                                       .arg(jobNumber, year, month, count, postage));
            if (m_trackerModel) {
                m_trackerModel->select(); // Refresh the model
            }
        } else {
            Logger::instance().error(QString("Failed to update FOUR HANDS log entry: Job %1 - %2").arg(jobNumber, query.lastError().text()));
        }

        // --- ensure job is visible in File > Open Job ---
        QSqlQuery stateQuery(m_dbManager->getDatabase());
        stateQuery.prepare("UPDATE fh_jobs "
                           "SET html_display_state = 0 "
                           "WHERE job_number = :job_number");
        stateQuery.bindValue(":job_number", jobNumber);
        stateQuery.exec();
        return success;
    } else {
        // No entry exists, insert new one
        query.prepare("INSERT INTO fh_log "
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
            Logger::instance().info(QString("FOUR HANDS log entry inserted for job %1, %2/%3: %4 pieces at %5")
                                       .arg(jobNumber, year, month, count, postage));

            // --- ensure job appears in Open Job list ---
            QSqlQuery stateQuery(m_dbManager->getDatabase());
            stateQuery.prepare("UPDATE fh_jobs "
                               "SET html_display_state = 0 "
                               "WHERE job_number = :job_number");
            stateQuery.bindValue(":job_number", jobNumber);
            stateQuery.exec();
            if (m_trackerModel) {
                m_trackerModel->select(); // Refresh the model
            }
        } else {
            Logger::instance().error(QString("Failed to insert FOUR HANDS log entry: Job %1 - %2").arg(jobNumber, query.lastError().text()));
        }
        return success;
    }
}

bool FHDBManager::deleteLogEntry(int id)
{
    if (!m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for FOUR HANDS deleteLogEntry");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("DELETE FROM fh_log WHERE id = :id");
    query.bindValue(":id", id);

    bool success = m_dbManager->executeQuery(query);
    if (success) {
        Logger::instance().info(QString("FOUR HANDS log entry deleted: ID %1").arg(id));
        if (m_trackerModel) {
            m_trackerModel->select(); // Refresh the model
        }
    } else {
        Logger::instance().error(QString("Failed to delete FOUR HANDS log entry: ID %1 - %2").arg(id).arg(query.lastError().text()));
    }

    return success;
}

bool FHDBManager::updateLogEntry(int id, const QString& jobNumber, const QString& description,
                                 const QString& postage, const QString& count,
                                 const QString& perPiece, const QString& mailClass,
                                 const QString& shape, const QString& permit,
                                 const QString& date)
{
    if (!m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for FOUR HANDS updateLogEntry");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("UPDATE fh_log SET "
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
        Logger::instance().info(QString("FOUR HANDS log entry updated: ID %1").arg(id));
        if (m_trackerModel) {
            m_trackerModel->select(); // Refresh the model
        }
    } else {
        Logger::instance().error(QString("Failed to update FOUR HANDS log entry: ID %1 - %2").arg(id).arg(query.lastError().text()));
    }

    return success;
}

bool FHDBManager::updateLogEntryForJob(const QString& jobNumber, const QString& description,
                                       const QString& postage, const QString& count,
                                       const QString& avgRate, const QString& mailClass,
                                       const QString& shape, const QString& permit,
                                       const QString& date)
{
    if (!m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for FOUR HANDS updateLogEntryForJob");
        return false;
    }

    // Update the existing log entry for this specific job
    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("UPDATE fh_log SET "
                  "description = :description, postage = :postage, count = :count, "
                  "per_piece = :per_piece, class = :class, shape = :shape, "
                  "permit = :permit, date = :date "
                  "WHERE job_number = :job_number");

    query.bindValue(":job_number", jobNumber);
    query.bindValue(":description", description);
    query.bindValue(":postage", postage);
    query.bindValue(":count", count);
    query.bindValue(":per_piece", avgRate);
    query.bindValue(":class", mailClass);
    query.bindValue(":shape", shape);
    query.bindValue(":permit", permit);
    query.bindValue(":date", date);

    bool success = m_dbManager->executeQuery(query);
    if (success && query.numRowsAffected() > 0) {
        Logger::instance().info(QString("FOUR HANDS log entry updated for job %1: %2 pieces at %3")
                                   .arg(jobNumber, count, postage));
        if (m_trackerModel) {
            m_trackerModel->select(); // Refresh the model
        }
        return true;
    }
    
    // No rows were affected (no existing entry found for this job)
    Logger::instance().info(QString("No existing FOUR HANDS log entry found for job %1, will need to insert new")
                               .arg(jobNumber));
    return false;
}

bool FHDBManager::updateLogJobNumber(const QString& oldJobNumber, const QString& newJobNumber)
{
    if (!m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for FOUR HANDS updateLogJobNumber");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("UPDATE fh_log SET job_number = :new_job_number WHERE job_number = :old_job_number");
    query.bindValue(":new_job_number", newJobNumber);
    query.bindValue(":old_job_number", oldJobNumber);

    const bool success = query.exec();
    if (success) {
        Logger::instance().info(QString("Updated FOUR HANDS log job number: %1 -> %2").arg(oldJobNumber, newJobNumber));
    } else {
        Logger::instance().error(QString("Failed FOUR HANDS job-number update: %1").arg(query.lastError().text()));
    }
    return success;
}

bool FHDBManager::saveJobState(const QString& year, const QString& month,
                               int htmlDisplayState, bool jobDataLocked, bool postageDataLocked,
                               const QString& lastExecutedScript)
{
    // Call the enhanced version with empty postage, count, and drop_number
    // ✅ Updated: Added empty drop_number parameter
    return saveJobState(year, month, htmlDisplayState, jobDataLocked, postageDataLocked,
                        "", "", "", lastExecutedScript);
}

bool FHDBManager::saveJobState(const QString& year, const QString& month,
                               int htmlDisplayState, bool jobDataLocked, bool postageDataLocked,
                               const QString& postage, const QString& count,
                               const QString& dropNumber, const QString& lastExecutedScript)  // ✅ Added: dropNumber parameter
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for FOUR HANDS saveJobState");
        return false;
    }

    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    QSqlQuery query(m_dbManager->getDatabase());
    // ✅ Updated: Added drop_number to UPDATE statement
    query.prepare("UPDATE fh_jobs SET "
                  "html_display_state = :html_display_state, "
                  "job_data_locked = :job_data_locked, "
                  "postage_data_locked = :postage_data_locked, "
                  "postage = :postage, "
                  "count = :count, "
                  "drop_number = :drop_number, "
                  "last_executed_script = :last_executed_script, "
                  "updated_at = :updated_at "
                  "WHERE year = :year AND month = :month");
    query.bindValue(":html_display_state", htmlDisplayState);
    query.bindValue(":job_data_locked", jobDataLocked ? 1 : 0);
    query.bindValue(":postage_data_locked", postageDataLocked ? 1 : 0);
    query.bindValue(":postage", postage);
    query.bindValue(":count", count);
    query.bindValue(":drop_number", dropNumber);
    query.bindValue(":last_executed_script", lastExecutedScript);
    query.bindValue(":updated_at", currentTime);
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    if (!m_dbManager->executeQuery(query)) {
        Logger::instance().error(QString("Failed to update FOUR HANDS job state for %1/%2: %3")
                                     .arg(year, month, query.lastError().text()));
        return false;
    }

    // Check if any rows were affected (updated)
    if (query.numRowsAffected() == 0) {
        // No existing record found, insert new one
        // ✅ Updated: Added drop_number to INSERT statement
        query.prepare("INSERT INTO fh_jobs "
                      "(year, month, job_number, html_display_state, job_data_locked, "
                      "postage_data_locked, postage, count, drop_number, last_executed_script, "
                      "created_at, updated_at) "
                      "VALUES (:year, :month, '', :html_display_state, :job_data_locked, "
                      ":postage_data_locked, :postage, :count, :drop_number, :last_executed_script, "
                      ":created_at, :updated_at)");

        query.bindValue(":year", year);
        query.bindValue(":month", month);
        query.bindValue(":html_display_state", htmlDisplayState);
        query.bindValue(":job_data_locked", jobDataLocked ? 1 : 0);
        query.bindValue(":postage_data_locked", postageDataLocked ? 1 : 0);
        query.bindValue(":postage", postage);
        query.bindValue(":count", count);
        query.bindValue(":drop_number", dropNumber);
        query.bindValue(":last_executed_script", lastExecutedScript);
        query.bindValue(":created_at", currentTime);
        query.bindValue(":updated_at", currentTime);

        if (!m_dbManager->executeQuery(query)) {
            Logger::instance().error(QString("Failed to insert FOUR HANDS job state for %1/%2: %3")
                                         .arg(year, month, query.lastError().text()));
            return false;
        }
    }

    Logger::instance().info(QString("FOUR HANDS job state saved for %1/%2: postage=%3, count=%4, locked=%5")
                                .arg(year, month, postage, count, postageDataLocked ? "true" : "false"));
    return true;
}

bool FHDBManager::loadJobState(const QString& year, const QString& month,
                               int& htmlDisplayState, bool& jobDataLocked, bool& postageDataLocked,
                               QString& lastExecutedScript)
{
    // Call the enhanced version and ignore postage, count, and drop_number
    // ✅ Updated: Added dropNumber parameter
    QString postage, count, dropNumber;
    return loadJobState(year, month, htmlDisplayState, jobDataLocked, postageDataLocked,
                        postage, count, dropNumber, lastExecutedScript);
}

bool FHDBManager::loadJobState(const QString& year, const QString& month,
                               int& htmlDisplayState, bool& jobDataLocked, bool& postageDataLocked,
                               QString& postage, QString& count, QString& dropNumber, QString& lastExecutedScript)  // ✅ Added: dropNumber parameter
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        Logger::instance().error("Database not initialized for FOUR HANDS loadJobState");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    // ✅ Updated: Added drop_number to SELECT statement
    query.prepare("SELECT html_display_state, job_data_locked, postage_data_locked, "
                  "postage, count, drop_number, last_executed_script FROM fh_jobs "
                  "WHERE year = :year AND month = :month");
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    if (!m_dbManager->executeQuery(query)) {
        Logger::instance().error(QString("Failed to execute FOUR HANDS loadJobState query for %1/%2: %3")
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
        dropNumber = "";  // ✅ Added: Initialize dropNumber default
        lastExecutedScript = "";
        Logger::instance().info(QString("No FOUR HANDS job state found for %1/%2, using defaults").arg(year, month));
        return false;
    }

    // Load values from database
    htmlDisplayState = query.value("html_display_state").toInt();
    jobDataLocked = query.value("job_data_locked").toInt() == 1;
    postageDataLocked = query.value("postage_data_locked").toInt() == 1;
    postage = query.value("postage").toString();
    count = query.value("count").toString();
    dropNumber = query.value("drop_number").toString();  // ✅ Added: Load dropNumber from database
    lastExecutedScript = query.value("last_executed_script").toString();

    Logger::instance().info(QString("FOUR HANDS job state loaded for %1/%2: postage=%3, count=%4, locked=%5")
                                .arg(year, month, postage, count, postageDataLocked ? "true" : "false"));
    return true;
}
