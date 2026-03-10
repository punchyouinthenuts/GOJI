#include "ailidbmanager.h"
#include "databasemanager.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

AILIDBManager* AILIDBManager::m_instance = nullptr;

AILIDBManager* AILIDBManager::instance()
{
    if (!m_instance) {
        m_instance = new AILIDBManager(nullptr);
    }
    return m_instance;
}

AILIDBManager::AILIDBManager(QObject* parent)
    : QObject(parent)
    , m_dbManager(DatabaseManager::instance())
{
}

bool AILIDBManager::initializeTables()
{
    return createTables();
}

bool AILIDBManager::createTables()
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        qWarning() << "AILI DB: DatabaseManager not initialized";
        return false;
    }

    QSqlDatabase db = m_dbManager->getDatabase();
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "AILI DB: database connection not open";
        return false;
    }

    QSqlQuery query(db);

    const QString createJobsTable = R"(
        CREATE TABLE IF NOT EXISTS aili_jobs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            job_number TEXT NOT NULL,
            issue_number TEXT NOT NULL,
            year TEXT NOT NULL,
            month TEXT NOT NULL,
            version TEXT NOT NULL,
            page_count TEXT,
            postage TEXT,
            count TEXT,
            html_display_state INTEGER DEFAULT 0,
            job_data_locked INTEGER DEFAULT 0,
            postage_data_locked INTEGER DEFAULT 0,
            last_executed_script TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(job_number, issue_number, year, month, version)
        );
    )";

    if (!query.exec(createJobsTable)) {
        qWarning() << "AILI DB: failed creating aili_jobs table:" << query.lastError().text();
        return false;
    }

    const QString createLogsTable = R"(
        CREATE TABLE IF NOT EXISTS aili_terminal_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            job_number TEXT NOT NULL,
            issue_number TEXT NOT NULL,
            year TEXT NOT NULL,
            month TEXT NOT NULL,
            version TEXT NOT NULL,
            message TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    if (!query.exec(createLogsTable)) {
        qWarning() << "AILI DB: failed creating aili_terminal_logs table:" << query.lastError().text();
        return false;
    }

    return true;
}

bool AILIDBManager::saveJob(const QString& jobNumber,
                            const QString& issueNumber,
                            const QString& year,
                            const QString& month,
                            const QString& version,
                            const QString& pageCount)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(R"(
        INSERT INTO aili_jobs (
            job_number, issue_number, year, month, version,
            page_count, updated_at
        ) VALUES (
            :job_number, :issue_number, :year, :month, :version,
            :page_count, CURRENT_TIMESTAMP
        )
        ON CONFLICT(job_number, issue_number, year, month, version)
        DO UPDATE SET
            page_count = excluded.page_count,
            updated_at = CURRENT_TIMESTAMP
    )");

    query.bindValue(":job_number", jobNumber);
    query.bindValue(":issue_number", issueNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":version", version);
    query.bindValue(":page_count", pageCount);

    if (!query.exec()) {
        qWarning() << "AILI DB: saveJob failed:" << query.lastError().text();
        return false;
    }

    return true;
}

bool AILIDBManager::loadJob(const QString& jobNumber,
                            const QString& issueNumber,
                            const QString& year,
                            const QString& month,
                            const QString& version,
                            QString& pageCountOut,
                            QString& postageOut,
                            QString& countOut)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(R"(
        SELECT page_count, postage, count
        FROM aili_jobs
        WHERE job_number = :job_number
          AND issue_number = :issue_number
          AND year = :year
          AND month = :month
          AND version = :version
        LIMIT 1
    )");

    query.bindValue(":job_number", jobNumber);
    query.bindValue(":issue_number", issueNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":version", version);

    if (!query.exec()) {
        qWarning() << "AILI DB: loadJob failed:" << query.lastError().text();
        return false;
    }

    if (!query.next()) {
        return false;
    }

    pageCountOut = query.value(0).toString();
    postageOut = query.value(1).toString();
    countOut = query.value(2).toString();
    return true;
}

bool AILIDBManager::deleteJob(const QString& jobNumber,
                              const QString& issueNumber,
                              const QString& year,
                              const QString& month,
                              const QString& version)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        return false;
    }

    QSqlDatabase db = m_dbManager->getDatabase();

    QSqlQuery deleteLogsQuery(db);
    deleteLogsQuery.prepare(R"(
        DELETE FROM aili_terminal_logs
        WHERE job_number = :job_number
          AND issue_number = :issue_number
          AND year = :year
          AND month = :month
          AND version = :version
    )");
    deleteLogsQuery.bindValue(":job_number", jobNumber);
    deleteLogsQuery.bindValue(":issue_number", issueNumber);
    deleteLogsQuery.bindValue(":year", year);
    deleteLogsQuery.bindValue(":month", month);
    deleteLogsQuery.bindValue(":version", version);
    if (!deleteLogsQuery.exec()) {
        qWarning() << "AILI DB: deleteJob logs cleanup failed:" << deleteLogsQuery.lastError().text();
        return false;
    }

    QSqlQuery deleteJobQuery(db);
    deleteJobQuery.prepare(R"(
        DELETE FROM aili_jobs
        WHERE job_number = :job_number
          AND issue_number = :issue_number
          AND year = :year
          AND month = :month
          AND version = :version
    )");

    deleteJobQuery.bindValue(":job_number", jobNumber);
    deleteJobQuery.bindValue(":issue_number", issueNumber);
    deleteJobQuery.bindValue(":year", year);
    deleteJobQuery.bindValue(":month", month);
    deleteJobQuery.bindValue(":version", version);

    if (!deleteJobQuery.exec()) {
        qWarning() << "AILI DB: deleteJob failed:" << deleteJobQuery.lastError().text();
        return false;
    }

    return deleteJobQuery.numRowsAffected() > 0;
}

bool AILIDBManager::jobExists(const QString& jobNumber,
                              const QString& issueNumber,
                              const QString& year,
                              const QString& month,
                              const QString& version) const
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(R"(
        SELECT 1
        FROM aili_jobs
        WHERE job_number = :job_number
          AND issue_number = :issue_number
          AND year = :year
          AND month = :month
          AND version = :version
        LIMIT 1
    )");

    query.bindValue(":job_number", jobNumber);
    query.bindValue(":issue_number", issueNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":version", version);

    if (!query.exec()) {
        qWarning() << "AILI DB: jobExists failed:" << query.lastError().text();
        return false;
    }

    return query.next();
}

QList<QMap<QString, QString>> AILIDBManager::getAllJobs() const
{
    QList<QMap<QString, QString>> rows;

    if (!m_dbManager || !m_dbManager->isInitialized()) {
        return rows;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    if (!query.exec(R"(
        SELECT
            job_number,
            issue_number,
            year,
            month,
            version,
            page_count,
            postage,
            count
        FROM aili_jobs
        ORDER BY updated_at DESC, id DESC
    )")) {
        qWarning() << "AILI DB: getAllJobs failed:" << query.lastError().text();
        return rows;
    }

    while (query.next()) {
        QMap<QString, QString> row;
        row["job_number"] = query.value(0).toString();
        row["issue_number"] = query.value(1).toString();
        row["year"] = query.value(2).toString();
        row["month"] = query.value(3).toString();
        row["version"] = query.value(4).toString();
        row["page_count"] = query.value(5).toString();
        row["postage"] = query.value(6).toString();
        row["count"] = query.value(7).toString();
        rows.append(row);
    }

    return rows;
}

bool AILIDBManager::saveJobState(const QString& jobNumber,
                                 const QString& issueNumber,
                                 const QString& year,
                                 const QString& month,
                                 const QString& version,
                                 const QString& pageCount,
                                 int htmlDisplayState,
                                 bool jobDataLocked,
                                 bool postageDataLocked,
                                 const QString& postage,
                                 const QString& count,
                                 const QString& lastExecutedScript)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(R"(
        INSERT INTO aili_jobs (
            job_number, issue_number, year, month, version,
            page_count, html_display_state, job_data_locked,
            postage_data_locked, postage, count, last_executed_script,
            updated_at
        ) VALUES (
            :job_number, :issue_number, :year, :month, :version,
            :page_count, :html_display_state, :job_data_locked,
            :postage_data_locked, :postage, :count, :last_executed_script,
            CURRENT_TIMESTAMP
        )
        ON CONFLICT(job_number, issue_number, year, month, version)
        DO UPDATE SET
            page_count = excluded.page_count,
            html_display_state = excluded.html_display_state,
            job_data_locked = excluded.job_data_locked,
            postage_data_locked = excluded.postage_data_locked,
            postage = excluded.postage,
            count = excluded.count,
            last_executed_script = excluded.last_executed_script,
            updated_at = CURRENT_TIMESTAMP
    )");

    query.bindValue(":job_number", jobNumber);
    query.bindValue(":issue_number", issueNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":version", version);
    query.bindValue(":page_count", pageCount);
    query.bindValue(":html_display_state", htmlDisplayState);
    query.bindValue(":job_data_locked", jobDataLocked ? 1 : 0);
    query.bindValue(":postage_data_locked", postageDataLocked ? 1 : 0);
    query.bindValue(":postage", postage);
    query.bindValue(":count", count);
    query.bindValue(":last_executed_script", lastExecutedScript);

    if (!query.exec()) {
        qWarning() << "AILI DB: saveJobState failed:" << query.lastError().text();
        return false;
    }

    return true;
}

bool AILIDBManager::loadJobState(const QString& jobNumber,
                                 const QString& issueNumber,
                                 const QString& year,
                                 const QString& month,
                                 const QString& version,
                                 QString& pageCountOut,
                                 int& htmlDisplayState,
                                 bool& jobDataLocked,
                                 bool& postageDataLocked,
                                 QString& postageOut,
                                 QString& countOut,
                                 QString& lastExecutedScriptOut)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(R"(
        SELECT
            page_count,
            html_display_state,
            job_data_locked,
            postage_data_locked,
            postage,
            count,
            last_executed_script
        FROM aili_jobs
        WHERE job_number = :job_number
          AND issue_number = :issue_number
          AND year = :year
          AND month = :month
          AND version = :version
        LIMIT 1
    )");

    query.bindValue(":job_number", jobNumber);
    query.bindValue(":issue_number", issueNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":version", version);

    if (!query.exec()) {
        qWarning() << "AILI DB: loadJobState failed:" << query.lastError().text();
        return false;
    }

    if (!query.next()) {
        return false;
    }

    pageCountOut = query.value(0).toString();
    htmlDisplayState = query.value(1).toInt();
    jobDataLocked = query.value(2).toInt() != 0;
    postageDataLocked = query.value(3).toInt() != 0;
    postageOut = query.value(4).toString();
    countOut = query.value(5).toString();
    lastExecutedScriptOut = query.value(6).toString();

    return true;
}

bool AILIDBManager::savePostageData(const QString& jobNumber,
                                    const QString& issueNumber,
                                    const QString& year,
                                    const QString& month,
                                    const QString& version,
                                    const QString& postage,
                                    const QString& count,
                                    bool locked)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(R"(
        UPDATE aili_jobs
        SET postage = :postage,
            count = :count,
            postage_data_locked = :postage_data_locked,
            updated_at = CURRENT_TIMESTAMP
        WHERE job_number = :job_number
          AND issue_number = :issue_number
          AND year = :year
          AND month = :month
          AND version = :version
    )");

    query.bindValue(":postage", postage);
    query.bindValue(":count", count);
    query.bindValue(":postage_data_locked", locked ? 1 : 0);
    query.bindValue(":job_number", jobNumber);
    query.bindValue(":issue_number", issueNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":version", version);

    if (!query.exec()) {
        qWarning() << "AILI DB: savePostageData failed:" << query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() > 0) {
        return true;
    }

    QSqlQuery upsertQuery(m_dbManager->getDatabase());
    upsertQuery.prepare(R"(
        INSERT INTO aili_jobs (
            job_number, issue_number, year, month, version,
            page_count, postage, count, postage_data_locked, updated_at
        ) VALUES (
            :job_number, :issue_number, :year, :month, :version,
            '', :postage, :count, :postage_data_locked, CURRENT_TIMESTAMP
        )
        ON CONFLICT(job_number, issue_number, year, month, version)
        DO UPDATE SET
            postage = excluded.postage,
            count = excluded.count,
            postage_data_locked = excluded.postage_data_locked,
            updated_at = CURRENT_TIMESTAMP
    )");

    upsertQuery.bindValue(":job_number", jobNumber);
    upsertQuery.bindValue(":issue_number", issueNumber);
    upsertQuery.bindValue(":year", year);
    upsertQuery.bindValue(":month", month);
    upsertQuery.bindValue(":version", version);
    upsertQuery.bindValue(":postage", postage);
    upsertQuery.bindValue(":count", count);
    upsertQuery.bindValue(":postage_data_locked", locked ? 1 : 0);

    if (!upsertQuery.exec()) {
        qWarning() << "AILI DB: savePostageData upsert failed:" << upsertQuery.lastError().text();
        return false;
    }

    return true;
}

bool AILIDBManager::loadPostageData(const QString& jobNumber,
                                    const QString& issueNumber,
                                    const QString& year,
                                    const QString& month,
                                    const QString& version,
                                    QString& postageOut,
                                    QString& countOut,
                                    bool& lockedOut)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(R"(
        SELECT postage, count, postage_data_locked
        FROM aili_jobs
        WHERE job_number = :job_number
          AND issue_number = :issue_number
          AND year = :year
          AND month = :month
          AND version = :version
        LIMIT 1
    )");

    query.bindValue(":job_number", jobNumber);
    query.bindValue(":issue_number", issueNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":version", version);

    if (!query.exec()) {
        qWarning() << "AILI DB: loadPostageData failed:" << query.lastError().text();
        return false;
    }

    if (!query.next()) {
        return false;
    }

    postageOut = query.value(0).toString();
    countOut = query.value(1).toString();
    lockedOut = query.value(2).toInt() != 0;
    return true;
}

bool AILIDBManager::saveTerminalLog(const QString& jobNumber,
                                    const QString& issueNumber,
                                    const QString& year,
                                    const QString& month,
                                    const QString& version,
                                    const QString& message)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(R"(
        INSERT INTO aili_terminal_logs (
            job_number, issue_number, year, month, version, message
        ) VALUES (
            :job_number, :issue_number, :year, :month, :version, :message
        )
    )");

    query.bindValue(":job_number", jobNumber);
    query.bindValue(":issue_number", issueNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":version", version);
    query.bindValue(":message", message);

    if (!query.exec()) {
        qWarning() << "AILI DB: saveTerminalLog failed:" << query.lastError().text();
        return false;
    }

    return true;
}

QStringList AILIDBManager::getTerminalLogs(const QString& jobNumber,
                                           const QString& issueNumber,
                                           const QString& year,
                                           const QString& month,
                                           const QString& version) const
{
    QStringList logs;

    if (!m_dbManager || !m_dbManager->isInitialized()) {
        return logs;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(R"(
        SELECT message
        FROM aili_terminal_logs
        WHERE job_number = :job_number
          AND issue_number = :issue_number
          AND year = :year
          AND month = :month
          AND version = :version
        ORDER BY id ASC
    )");

    query.bindValue(":job_number", jobNumber);
    query.bindValue(":issue_number", issueNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":version", version);

    if (!query.exec()) {
        qWarning() << "AILI DB: getTerminalLogs failed:" << query.lastError().text();
        return logs;
    }

    while (query.next()) {
        logs.append(query.value(0).toString());
    }

    return logs;
}