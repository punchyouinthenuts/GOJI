#include "tmtarragondbmanager.h"
#include "logger.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDate>
#include <QRegularExpression>
#include <QDebug>

// Static instance
TMTarragonDBManager* TMTarragonDBManager::m_instance = nullptr;

TMTarragonDBManager::TMTarragonDBManager()
    : m_dbManager(nullptr)
{
    m_dbManager = DatabaseManager::instance();
}

TMTarragonDBManager* TMTarragonDBManager::instance()
{
    if (!m_instance) {
        m_instance = new TMTarragonDBManager();
    }
    return m_instance;
}

bool TMTarragonDBManager::initialize()
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        qDebug() << "Database manager not available or not initialized";
        return false;
    }

    return createTables();
}

bool TMTarragonDBManager::createTables()
{
    QSqlQuery query(m_dbManager->getDatabase());

    // Create jobs table
    QString createJobsTable = R"(
        CREATE TABLE IF NOT EXISTS tm_tarragon_jobs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            job_number TEXT NOT NULL,
            year TEXT NOT NULL,
            month TEXT NOT NULL,
            drop_number TEXT NOT NULL,
            html_display_state INTEGER DEFAULT 0,
            job_data_locked INTEGER DEFAULT 0,
            postage_data_locked INTEGER DEFAULT 0,
            last_executed_script TEXT DEFAULT '',
            created_at TEXT NOT NULL,
            updated_at TEXT NOT NULL,
            UNIQUE(year, month, drop_number)
        )
    )";

    if (!query.exec(createJobsTable)) {
        qDebug() << "Failed to create tm_tarragon_jobs table:" << query.lastError().text();
        return false;
    }

    // Add columns if they don't exist (for existing databases)
    query.exec("ALTER TABLE tm_tarragon_jobs ADD COLUMN html_display_state INTEGER DEFAULT 0");
    query.exec("ALTER TABLE tm_tarragon_jobs ADD COLUMN job_data_locked INTEGER DEFAULT 0");
    query.exec("ALTER TABLE tm_tarragon_jobs ADD COLUMN postage_data_locked INTEGER DEFAULT 0");
    query.exec("ALTER TABLE tm_tarragon_jobs ADD COLUMN last_executed_script TEXT DEFAULT ''");

    // Create postage table (standardized structure)
    QString createPostageTable = R"(
        CREATE TABLE IF NOT EXISTS tm_tarragon_postage (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            year TEXT NOT NULL,
            month TEXT NOT NULL,
            week TEXT,
            drop_number TEXT NOT NULL,
            postage TEXT,
            count TEXT,
            mail_class TEXT,
            permit TEXT,
            locked BOOLEAN DEFAULT 0,
            updated_at TEXT NOT NULL,
            UNIQUE(year, month, drop_number)
        )
    )";

    if (!query.exec(createPostageTable)) {
        qDebug() << "Failed to create tm_tarragon_postage table:" << query.lastError().text();
        return false;
    }

    // Create log table (standardized 8-column format)
    QString createLogTable = R"(
        CREATE TABLE IF NOT EXISTS tm_tarragon_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            job_number TEXT NOT NULL,
            description TEXT NOT NULL,
            postage TEXT NOT NULL,
            count TEXT NOT NULL,
            per_piece TEXT NOT NULL,
            mail_class TEXT NOT NULL,
            shape TEXT NOT NULL,
            permit TEXT NOT NULL,
            date TEXT NOT NULL,
            created_at TEXT NOT NULL
        )
    )";

    if (!query.exec(createLogTable)) {
        qDebug() << "Failed to create tm_tarragon_log table:" << query.lastError().text();
        return false;
    }

    Logger::instance().info("TM Tarragon database tables created successfully");
    return true;
}

bool TMTarragonDBManager::saveJob(const QString& jobNumber, const QString& year,
                                  const QString& month, const QString& dropNumber)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(R"(
        INSERT OR REPLACE INTO tm_tarragon_jobs
        (job_number, year, month, drop_number, created_at, updated_at)
        VALUES (:job_number, :year, :month, :drop_number, :created_at, :updated_at)
    )");

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    query.bindValue(":job_number", jobNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":drop_number", dropNumber);
    query.bindValue(":created_at", timestamp);
    query.bindValue(":updated_at", timestamp);

    bool success = query.exec();
    if (!success) {
        qDebug() << "Failed to save job:" << query.lastError().text();
        Logger::instance().error("Failed to save TM Tarragon job: " + query.lastError().text());
    }

    return success;
}

bool TMTarragonDBManager::loadJob(const QString& year, const QString& month,
                                  const QString& dropNumber, QString& jobNumber)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT job_number FROM tm_tarragon_jobs WHERE year = :year AND month = :month AND drop_number = :drop_number");
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":drop_number", dropNumber);

    if (!query.exec()) {
        qDebug() << "Failed to load job:" << query.lastError().text();
        return false;
    }

    if (query.next()) {
        jobNumber = query.value("job_number").toString();
        return true;
    }

    return false;
}

bool TMTarragonDBManager::deleteJob(const QString& year, const QString& month, const QString& dropNumber)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("DELETE FROM tm_tarragon_jobs WHERE year = :year AND month = :month AND drop_number = :drop_number");
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":drop_number", dropNumber);

    return query.exec();
}

bool TMTarragonDBManager::jobExists(const QString& year, const QString& month, const QString& dropNumber)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT COUNT(*) FROM tm_tarragon_jobs WHERE year = :year AND month = :month AND drop_number = :drop_number");
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":drop_number", dropNumber);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

QList<QMap<QString, QString>> TMTarragonDBManager::getAllJobs()
{
    QList<QMap<QString, QString>> jobs;

    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return jobs;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    if (!query.exec("SELECT job_number, year, month, drop_number FROM tm_tarragon_jobs ORDER BY year DESC, month DESC, drop_number DESC")) {
        qDebug() << "Failed to get all jobs:" << query.lastError().text();
        return jobs;
    }

    while (query.next()) {
        QMap<QString, QString> job;
        job["job_number"] = query.value("job_number").toString();
        job["year"] = query.value("year").toString();
        job["month"] = query.value("month").toString();
        job["drop_number"] = query.value("drop_number").toString();
        jobs.append(job);
    }

    return jobs;
}

bool TMTarragonDBManager::saveJobState(const QString& year, const QString& month, const QString& dropNumber,
                                       int htmlDisplayState, bool jobDataLocked, bool postageDataLocked,
                                       const QString& postage, const QString& count, const QString& lastExecutedScript)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    // Update the main job record with state information
    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("UPDATE tm_tarragon_jobs SET "
                  "html_display_state = :html_display_state, "
                  "job_data_locked = :job_data_locked, "
                  "postage_data_locked = :postage_data_locked, "
                  "last_executed_script = :last_executed_script, "
                  "updated_at = :updated_at "
                  "WHERE year = :year AND month = :month AND drop_number = :drop_number");
    query.bindValue(":html_display_state", htmlDisplayState);
    query.bindValue(":job_data_locked", jobDataLocked ? 1 : 0);
    query.bindValue(":postage_data_locked", postageDataLocked ? 1 : 0);
    query.bindValue(":last_executed_script", lastExecutedScript);
    query.bindValue(":updated_at", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":drop_number", dropNumber);

    bool success = query.exec();

    // Also save postage data separately using standardized method
    if (success) {
        success = savePostageData(year, month, dropNumber, postage, count, postageDataLocked);
    }

    return success;
}

bool TMTarragonDBManager::loadJobState(const QString& year, const QString& month, const QString& dropNumber,
                                       int& htmlDisplayState, bool& jobDataLocked, bool& postageDataLocked,
                                       QString& postage, QString& count, QString& lastExecutedScript)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT html_display_state, job_data_locked, postage_data_locked, last_executed_script FROM tm_tarragon_jobs "
                  "WHERE year = :year AND month = :month AND drop_number = :drop_number");
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":drop_number", dropNumber);

    if (!query.exec()) {
        // Fallback to old behavior if query fails
        bool success = loadPostageData(year, month, dropNumber, postage, count, postageDataLocked);
        jobDataLocked = jobExists(year, month, dropNumber);
        htmlDisplayState = jobDataLocked ? 1 : 0;
        lastExecutedScript = "";
        return success;
    }

    if (!query.next()) {
        // No job state found, set defaults
        htmlDisplayState = 0;
        jobDataLocked = false;
        postageDataLocked = false;
        postage = "";
        count = "";
        lastExecutedScript = "";
        return false;
    }

    htmlDisplayState = query.value("html_display_state").toInt();
    jobDataLocked = query.value("job_data_locked").toInt() == 1;
    postageDataLocked = query.value("postage_data_locked").toInt() == 1;
    lastExecutedScript = query.value("last_executed_script").toString();

    // Load postage data separately
    loadPostageData(year, month, dropNumber, postage, count, postageDataLocked);

    return true;
}

bool TMTarragonDBManager::savePostageData(const QString& year, const QString& month, const QString& dropNumber,
                                          const QString& postage, const QString& count, bool locked)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(R"(
        INSERT OR REPLACE INTO tm_tarragon_postage
        (year, month, week, drop_number, postage, count, mail_class, permit, locked, updated_at)
        VALUES (:year, :month, :week, :drop_number, :postage, :count, :mail_class, :permit, :locked, :updated_at)
    )");

    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", ""); // Blank for TMTARRAGON
    query.bindValue(":drop_number", dropNumber);
    query.bindValue(":postage", postage);
    query.bindValue(":count", count);
    query.bindValue(":mail_class", ""); // Blank for TMTARRAGON
    query.bindValue(":permit", ""); // Blank for TMTARRAGON
    query.bindValue(":locked", locked ? 1 : 0);
    query.bindValue(":updated_at", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    return query.exec();
}

bool TMTarragonDBManager::loadPostageData(const QString& year, const QString& month, const QString& dropNumber,
                                          QString& postage, QString& count, bool& locked)
{
    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT postage, count, locked FROM tm_tarragon_postage WHERE year = :year AND month = :month AND drop_number = :drop_number");
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":drop_number", dropNumber);

    if (!query.exec()) {
        return false;
    }

    if (!query.next()) {
        // No postage data found, set defaults
        postage = "";
        count = "";
        locked = false;
        return false;
    }

    postage = query.value("postage").toString();
    count = query.value("count").toString();
    locked = query.value("locked").toInt() == 1;
    return true;
}

bool TMTarragonDBManager::addLogEntry(const QString& jobNumber, const QString& description,
const QString& postage, const QString& count,
const QString& perPiece, const QString& mailClass,
const QString& shape, const QString& permit,
const QString& date)
{
if (!m_dbManager->isInitialized()) {
qDebug() << "Database not initialized";
Logger::instance().error("Database not initialized for TMTARRAGON addLogEntry");
return false;
}

// CRITICAL FIX: For TMTARRAGON, we need to use job_number + year + month + drop_number as unique key
// Extract year, month, and drop number from description if possible
QString year, month, dropNumber;
if (description.contains("TM TARRAGON HOMES D")) {
    // Extract drop number from description like "TM TARRAGON HOMES D3"
    QRegularExpression regex("TM TARRAGON HOMES D(\\d+)");
    QRegularExpressionMatch match = regex.match(description);
    if (match.hasMatch()) {
        dropNumber = match.captured(1);
        // For year, we derive it from the current date
        year = QString::number(QDate::currentDate().year());
        // For month, we need to get it from the current UI context - this is a limitation
        // We'll use current month as fallback
        month = QString("%1").arg(QDate::currentDate().month(), 2, 10, QChar('0'));
    }
}

QSqlQuery query(m_dbManager->getDatabase());

// FIXED: Check if an entry for this job+year+month+drop combination already exists
if (!year.isEmpty() && !month.isEmpty() && !dropNumber.isEmpty()) {
    // Find existing entry based on job_number and derived identifiers
    query.prepare("SELECT id FROM tm_tarragon_log WHERE job_number = :job_number AND description LIKE :description_pattern");
    query.bindValue(":job_number", jobNumber);
    query.bindValue(":description_pattern", QString("%%TM TARRAGON HOMES D%1%%").arg(dropNumber));
} else {
    // Fallback: just use job_number and exact description match
    Logger::instance().warning("Could not extract year/month/drop from description: " + description + " - using job+description match");
    query.prepare("SELECT id FROM tm_tarragon_log WHERE job_number = :job_number AND description = :description");
    query.bindValue(":job_number", jobNumber);
    query.bindValue(":description", description);
}

    if (!query.exec()) {
    qDebug() << "Failed to check existing log entry:" << query.lastError().text();
    Logger::instance().error("Failed to check existing TM Tarragon log entry: " + query.lastError().text());
    return false;
}

if (query.next()) {
    // Entry exists, update it
    int id = query.value(0).toInt();
    query.prepare(R"(
        UPDATE tm_tarragon_log SET description = :description, postage = :postage, count = :count,
            per_piece = :per_piece, mail_class = :mail_class, shape = :shape, permit = :permit,
        date = :date, created_at = :created_at WHERE id = :id
    )");
query.bindValue(":description", description);
query.bindValue(":postage", postage);
    query.bindValue(":count", count);
        query.bindValue(":per_piece", perPiece);
    query.bindValue(":mail_class", mailClass);
        query.bindValue(":shape", shape);
        query.bindValue(":permit", permit);
        query.bindValue(":date", date);
        query.bindValue(":created_at", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
        query.bindValue(":id", id);
        
        bool success = query.exec();
        if (success) {
            Logger::instance().info(QString("TMTARRAGON log entry updated for job %1, %2/%3/D%4: %5 pieces at %6")
                                       .arg(jobNumber, year, month, dropNumber, count, postage));
        } else {
            Logger::instance().error("Failed to update TMTARRAGON log entry: " + query.lastError().text());
        }
        return success;
    } else {
        // No entry exists, insert new one
        query.prepare(R"(
            INSERT INTO tm_tarragon_log
            (job_number, description, postage, count, per_piece, mail_class, shape, permit, date, created_at)
            VALUES (:job_number, :description, :postage, :count, :per_piece, :mail_class, :shape, :permit, :date, :created_at)
        )");
        query.bindValue(":job_number", jobNumber);
        query.bindValue(":description", description);
        query.bindValue(":postage", postage);
        query.bindValue(":count", count);
        query.bindValue(":per_piece", perPiece);
        query.bindValue(":mail_class", mailClass);
        query.bindValue(":shape", shape);
        query.bindValue(":permit", permit);
        query.bindValue(":date", date);
        query.bindValue(":created_at", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
        
        bool success = query.exec();
        if (success) {
            Logger::instance().info(QString("TMTARRAGON log entry inserted for job %1, %2/%3/D%4: %5 pieces at %6")
                                       .arg(jobNumber, year, month, dropNumber, count, postage));
        } else {
            Logger::instance().error("Failed to insert TMTARRAGON log entry: " + query.lastError().text());
        }
        return success;
    }
}

QList<QMap<QString, QVariant>> TMTarragonDBManager::getLog()
{
    QList<QMap<QString, QVariant>> logs;

    if (!m_dbManager->isInitialized()) {
        qDebug() << "Database not initialized";
        return logs;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    if (!query.exec("SELECT * FROM tm_tarragon_log ORDER BY created_at DESC")) {
        qDebug() << "Failed to get log entries:" << query.lastError().text();
        return logs;
    }

    while (query.next()) {
        QMap<QString, QVariant> log;
        log["id"] = query.value("id");
        log["job_number"] = query.value("job_number");
        log["description"] = query.value("description");
        log["postage"] = query.value("postage");
        log["count"] = query.value("count");
        log["per_piece"] = query.value("per_piece");
        log["mail_class"] = query.value("mail_class");
        log["shape"] = query.value("shape");
        log["permit"] = query.value("permit");
        log["date"] = query.value("date");
        log["created_at"] = query.value("created_at");
        logs.append(log);
    }

    return logs;
}

bool TMTarragonDBManager::saveTerminalLog(const QString& year, const QString& month,
                                          const QString& dropNumber, const QString& message)
{
    // Terminal logs could be stored in a separate table if needed
    // For now, just log to the main logger
    Logger::instance().info(QString("TM Tarragon %1-%2-%3: %4").arg(year, month, dropNumber, message));
    return true;
}

QStringList TMTarragonDBManager::getTerminalLogs(const QString& year, const QString& month, const QString& dropNumber)
{
    Q_UNUSED(year)
    Q_UNUSED(month)
    Q_UNUSED(dropNumber)

    // Return empty list for now - could implement dedicated terminal log table if needed
    return QStringList();
}
