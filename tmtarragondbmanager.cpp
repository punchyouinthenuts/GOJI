#include "tmtarragondbmanager.h"
#include "logger.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
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
            created_at TEXT NOT NULL,
            updated_at TEXT NOT NULL,
            UNIQUE(year, month, drop_number)
        )
    )";

    if (!query.exec(createJobsTable)) {
        qDebug() << "Failed to create tm_tarragon_jobs table:" << query.lastError().text();
        return false;
    }

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
                                       const QString& postage, const QString& count)
{
    Q_UNUSED(htmlDisplayState)
    Q_UNUSED(jobDataLocked)

    // Save postage data separately using standardized method
    return savePostageData(year, month, dropNumber, postage, count, postageDataLocked);
}

bool TMTarragonDBManager::loadJobState(const QString& year, const QString& month, const QString& dropNumber,
                                       int& htmlDisplayState, bool& jobDataLocked, bool& postageDataLocked,
                                       QString& postage, QString& count)
{
    // Load postage data using standardized method
    bool success = loadPostageData(year, month, dropNumber, postage, count, postageDataLocked);

    // Determine states based on data
    jobDataLocked = jobExists(year, month, dropNumber);
    htmlDisplayState = jobDataLocked ? 1 : 0; // InstructionsState : DefaultState

    return success;
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
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
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
    if (!success) {
        qDebug() << "Failed to add log entry:" << query.lastError().text();
        Logger::instance().error("Failed to add TM Tarragon log entry: " + query.lastError().text());
    }

    return success;
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
