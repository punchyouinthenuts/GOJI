#include "tmmadbmanager.h"

#include "databasemanager.h"
#include "logger.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace {
QString nonNullText(const QString& value)
{
    return value.isNull() ? QString::fromLatin1("") : value;
}

QString primarySqlError(const QSqlError& error)
{
    if (!error.databaseText().isEmpty()) {
        return error.databaseText();
    }
    if (!error.driverText().isEmpty()) {
        return error.driverText();
    }
    return error.text();
}
} // namespace

TMMADBManager* TMMADBManager::s_instance = nullptr;

TMMADBManager::TMMADBManager(QObject* parent)
    : QObject(parent)
    , m_dbManager(DatabaseManager::instance())
{
}

TMMADBManager* TMMADBManager::instance()
{
    if (!s_instance) {
        s_instance = new TMMADBManager();
    }
    return s_instance;
}

void TMMADBManager::setLastError(const QString& error) const
{
    m_lastError = error;
    if (!error.isEmpty()) {
        Logger::instance().error(error);
    }
}

QString TMMADBManager::lastError() const
{
    return m_lastError;
}

bool TMMADBManager::execPrepared(QSqlQuery& query, const QString& operation) const
{
    if (!query.exec()) {
        setLastError(QStringLiteral("TMMA database %1 failed: %2")
                         .arg(operation, query.lastError().text()));
        return false;
    }
    setLastError(QString());
    return true;
}

bool TMMADBManager::initializeTables()
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        setLastError(QStringLiteral("TMMA database initialization failed: GOJI database is unavailable."));
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS tm_ma_jobs ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "job_number TEXT NOT NULL, "
            "year TEXT NOT NULL, "
            "month TEXT NOT NULL, "
            "html_display_state INTEGER NOT NULL DEFAULT 0, "
            "job_data_locked INTEGER NOT NULL DEFAULT 0, "
            "postage_data_locked INTEGER NOT NULL DEFAULT 0, "
            "postage TEXT NOT NULL DEFAULT '', "
            "count TEXT NOT NULL DEFAULT '', "
            "mail_class TEXT NOT NULL DEFAULT '', "
            "permit TEXT NOT NULL DEFAULT '', "
            "last_executed_script TEXT NOT NULL DEFAULT '', "
            "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, "
            "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, "
            "UNIQUE(job_number, year, month))"))) {
        setLastError(QStringLiteral("TMMA jobs table creation failed: %1").arg(query.lastError().text()));
        return false;
    }

    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS tm_ma_log ("
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
            "year TEXT NOT NULL, "
            "month TEXT NOT NULL, "
            "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, "
            "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, "
            "UNIQUE(job_number, year, month))"))) {
        setLastError(QStringLiteral("TMMA log table creation failed: %1").arg(query.lastError().text()));
        return false;
    }

    setLastError(QString());
    Logger::instance().info(QStringLiteral("TMMA database tables initialized"));
    return true;
}

bool TMMADBManager::saveJob(const QString& jobNumber,
                            const QString& year,
                            const QString& month)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        setLastError(QStringLiteral("TMMA job save failed: GOJI database is unavailable."));
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(QStringLiteral(
        "UPDATE tm_ma_jobs SET updated_at = :updated_at "
        "WHERE job_number = :job_number AND year = :year AND month = :month"));
    query.bindValue(QStringLiteral(":job_number"), jobNumber);
    query.bindValue(QStringLiteral(":year"), year);
    query.bindValue(QStringLiteral(":month"), month);
    query.bindValue(QStringLiteral(":updated_at"), QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!execPrepared(query, QStringLiteral("job update"))) {
        return false;
    }
    if (query.numRowsAffected() > 0) {
        return true;
    }

    query.prepare(QStringLiteral(
        "INSERT INTO tm_ma_jobs (job_number, year, month, created_at, updated_at) "
        "VALUES (:job_number, :year, :month, :created_at, :updated_at)"));
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    query.bindValue(QStringLiteral(":job_number"), jobNumber);
    query.bindValue(QStringLiteral(":year"), year);
    query.bindValue(QStringLiteral(":month"), month);
    query.bindValue(QStringLiteral(":created_at"), now);
    query.bindValue(QStringLiteral(":updated_at"), now);
    return execPrepared(query, QStringLiteral("job insert"));
}

bool TMMADBManager::jobExists(const QString& jobNumber,
                              const QString& year,
                              const QString& month) const
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        setLastError(QStringLiteral("TMMA job lookup failed: GOJI database is unavailable."));
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(QStringLiteral(
        "SELECT 1 FROM tm_ma_jobs "
        "WHERE job_number = :job_number AND year = :year AND month = :month LIMIT 1"));
    query.bindValue(QStringLiteral(":job_number"), jobNumber);
    query.bindValue(QStringLiteral(":year"), year);
    query.bindValue(QStringLiteral(":month"), month);
    if (!execPrepared(query, QStringLiteral("job lookup"))) {
        return false;
    }
    if (!query.next()) {
        setLastError(QStringLiteral("TMMA job was not found for %1 / %2 / %3.")
                         .arg(jobNumber, year, month));
        return false;
    }
    setLastError(QString());
    return true;
}

bool TMMADBManager::identityCount(const QString& table,
                                  const QString& jobNumber,
                                  const QString& year,
                                  const QString& month,
                                  int& count,
                                  QSqlDatabase& database) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM %1 WHERE job_number = :job_number AND year = :year AND month = :month")
                      .arg(table));
    query.bindValue(QStringLiteral(":job_number"), jobNumber);
    query.bindValue(QStringLiteral(":year"), year);
    query.bindValue(QStringLiteral(":month"), month);
    if (!query.exec() || !query.next()) {
        setLastError(QStringLiteral("TMMA database identity lookup failed for %1: %2")
                         .arg(table, query.lastError().text()));
        return false;
    }
    count = query.value(0).toInt();
    return true;
}

bool TMMADBManager::jobIdentityExists(const QString& jobNumber,
                                      const QString& year,
                                      const QString& month,
                                      bool& exists) const
{
    exists = false;
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        setLastError(QStringLiteral("TMMA job identity lookup failed: GOJI database is unavailable."));
        return false;
    }
    int count = 0;
    QSqlDatabase& database = m_dbManager->getDatabase();
    if (!identityCount(QStringLiteral("tm_ma_jobs"), jobNumber, year, month, count, database)) {
        return false;
    }
    exists = count > 0;
    setLastError(QString());
    return true;
}

bool TMMADBManager::logIdentityExists(const QString& jobNumber,
                                      const QString& year,
                                      const QString& month,
                                      bool& exists) const
{
    exists = false;
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        setLastError(QStringLiteral("TMMA log identity lookup failed: GOJI database is unavailable."));
        return false;
    }
    int count = 0;
    QSqlDatabase& database = m_dbManager->getDatabase();
    if (!identityCount(QStringLiteral("tm_ma_log"), jobNumber, year, month, count, database)) {
        return false;
    }
    exists = count > 0;
    setLastError(QString());
    return true;
}

bool TMMADBManager::createJobWithState(const QString& jobNumber,
                                       const QString& year,
                                       const QString& month,
                                       const TMMAJobState& state)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        setLastError(QStringLiteral("TMMA job creation failed: GOJI database is unavailable."));
        return false;
    }

    QSqlDatabase& database = m_dbManager->getDatabase();
    if (!database.transaction()) {
        setLastError(QStringLiteral("TMMA job creation transaction failed: %1")
                         .arg(primarySqlError(database.lastError())));
        return false;
    }

    int jobCount = 0;
    int logCount = 0;
    if (!identityCount(QStringLiteral("tm_ma_jobs"), jobNumber, year, month, jobCount, database)
        || !identityCount(QStringLiteral("tm_ma_log"), jobNumber, year, month, logCount, database)) {
        database.rollback();
        return false;
    }
    if (jobCount != 0) {
        database.rollback();
        setLastError(QStringLiteral("TMMA job %1 / %2 / %3 already exists. Open it using File > Open Job. No data was changed.")
                         .arg(jobNumber, year, month));
        return false;
    }
    if (logCount != 0) {
        database.rollback();
        setLastError(QStringLiteral("TMMA tracker identity %1 / %2 / %3 already exists without an available job. No data was changed.")
                         .arg(jobNumber, year, month));
        return false;
    }

    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO tm_ma_jobs (job_number, year, month, html_display_state, "
        "job_data_locked, postage_data_locked, postage, count, mail_class, permit, "
        "last_executed_script, created_at, updated_at) VALUES "
        "(:job_number, :year, :month, :html_display_state, :job_data_locked, "
        ":postage_data_locked, :postage, :count, :mail_class, :permit, "
        ":last_executed_script, :created_at, :updated_at)"));
    query.bindValue(QStringLiteral(":job_number"), jobNumber);
    query.bindValue(QStringLiteral(":year"), year);
    query.bindValue(QStringLiteral(":month"), month);
    query.bindValue(QStringLiteral(":html_display_state"), state.htmlDisplayState);
    query.bindValue(QStringLiteral(":job_data_locked"), state.jobDataLocked ? 1 : 0);
    query.bindValue(QStringLiteral(":postage_data_locked"), state.postageDataLocked ? 1 : 0);
    query.bindValue(QStringLiteral(":postage"), nonNullText(state.postage));
    query.bindValue(QStringLiteral(":count"), nonNullText(state.count));
    query.bindValue(QStringLiteral(":mail_class"), nonNullText(state.mailClass));
    query.bindValue(QStringLiteral(":permit"), nonNullText(state.permit));
    query.bindValue(QStringLiteral(":last_executed_script"), nonNullText(state.lastExecutedScript));
    query.bindValue(QStringLiteral(":created_at"), now);
    query.bindValue(QStringLiteral(":updated_at"), now);
    if (!query.exec() || query.numRowsAffected() != 1) {
        const QString error = primarySqlError(query.lastError());
        database.rollback();
        setLastError(QStringLiteral("TMMA job creation failed: %1").arg(error));
        return false;
    }
    if (!database.commit()) {
        const QString error = primarySqlError(database.lastError());
        database.rollback();
        setLastError(QStringLiteral("TMMA job creation commit failed: %1").arg(error));
        return false;
    }
    setLastError(QString());
    return true;
}

bool TMMADBManager::migrateJobIdentity(const QString& oldJobNumber,
                                       const QString& oldYear,
                                       const QString& oldMonth,
                                       const QString& newJobNumber,
                                       const QString& newYear,
                                       const QString& newMonth,
                                       const TMMAJobState& state)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        setLastError(QStringLiteral("TMMA identity migration failed: GOJI database is unavailable."));
        return false;
    }
    if (oldJobNumber == newJobNumber && oldYear == newYear && oldMonth == newMonth) {
        setLastError(QStringLiteral("TMMA identity migration was requested with identical old and new identities."));
        return false;
    }

    QSqlDatabase& database = m_dbManager->getDatabase();
    if (!database.transaction()) {
        setLastError(QStringLiteral("TMMA identity migration transaction failed: %1")
                         .arg(primarySqlError(database.lastError())));
        return false;
    }

    int oldJobCount = 0;
    int newJobCount = 0;
    int oldLogCount = 0;
    int newLogCount = 0;
    if (!identityCount(QStringLiteral("tm_ma_jobs"), oldJobNumber, oldYear, oldMonth, oldJobCount, database)
        || !identityCount(QStringLiteral("tm_ma_jobs"), newJobNumber, newYear, newMonth, newJobCount, database)
        || !identityCount(QStringLiteral("tm_ma_log"), oldJobNumber, oldYear, oldMonth, oldLogCount, database)
        || !identityCount(QStringLiteral("tm_ma_log"), newJobNumber, newYear, newMonth, newLogCount, database)) {
        database.rollback();
        return false;
    }
    if (oldJobCount != 1) {
        database.rollback();
        setLastError(QStringLiteral("TMMA identity migration expected exactly one old job row for %1 / %2 / %3, but found %4. No data was changed.")
                         .arg(oldJobNumber, oldYear, oldMonth, QString::number(oldJobCount)));
        return false;
    }
    if (newJobCount != 0) {
        database.rollback();
        setLastError(QStringLiteral("TMMA job %1 / %2 / %3 already exists. No data was changed.")
                         .arg(newJobNumber, newYear, newMonth));
        return false;
    }
    if (oldLogCount > 1 || newLogCount != 0) {
        database.rollback();
        setLastError(newLogCount != 0
                         ? QStringLiteral("TMMA tracker identity %1 / %2 / %3 already exists. No data was changed.")
                               .arg(newJobNumber, newYear, newMonth)
                         : QStringLiteral("TMMA identity migration found more than one old tracker row. No data was changed."));
        return false;
    }

    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE tm_ma_jobs SET job_number = :new_job_number, year = :new_year, month = :new_month, "
        "html_display_state = :html_display_state, job_data_locked = :job_data_locked, "
        "postage_data_locked = :postage_data_locked, postage = :postage, count = :count, "
        "mail_class = :mail_class, permit = :permit, last_executed_script = :last_executed_script, "
        "updated_at = :updated_at WHERE job_number = :old_job_number AND year = :old_year AND month = :old_month"));
    query.bindValue(QStringLiteral(":new_job_number"), newJobNumber);
    query.bindValue(QStringLiteral(":new_year"), newYear);
    query.bindValue(QStringLiteral(":new_month"), newMonth);
    query.bindValue(QStringLiteral(":html_display_state"), state.htmlDisplayState);
    query.bindValue(QStringLiteral(":job_data_locked"), state.jobDataLocked ? 1 : 0);
    query.bindValue(QStringLiteral(":postage_data_locked"), state.postageDataLocked ? 1 : 0);
    query.bindValue(QStringLiteral(":postage"), nonNullText(state.postage));
    query.bindValue(QStringLiteral(":count"), nonNullText(state.count));
    query.bindValue(QStringLiteral(":mail_class"), nonNullText(state.mailClass));
    query.bindValue(QStringLiteral(":permit"), nonNullText(state.permit));
    query.bindValue(QStringLiteral(":last_executed_script"), nonNullText(state.lastExecutedScript));
    query.bindValue(QStringLiteral(":updated_at"), now);
    query.bindValue(QStringLiteral(":old_job_number"), oldJobNumber);
    query.bindValue(QStringLiteral(":old_year"), oldYear);
    query.bindValue(QStringLiteral(":old_month"), oldMonth);
    if (!query.exec() || query.numRowsAffected() != 1) {
        const QString error = primarySqlError(query.lastError());
        database.rollback();
        setLastError(QStringLiteral("TMMA job identity update failed: %1").arg(error));
        return false;
    }

    query.prepare(QStringLiteral(
        "UPDATE tm_ma_log SET job_number = :new_job_number, year = :new_year, month = :new_month, "
        "updated_at = :updated_at WHERE job_number = :old_job_number AND year = :old_year AND month = :old_month"));
    query.bindValue(QStringLiteral(":new_job_number"), newJobNumber);
    query.bindValue(QStringLiteral(":new_year"), newYear);
    query.bindValue(QStringLiteral(":new_month"), newMonth);
    query.bindValue(QStringLiteral(":updated_at"), now);
    query.bindValue(QStringLiteral(":old_job_number"), oldJobNumber);
    query.bindValue(QStringLiteral(":old_year"), oldYear);
    query.bindValue(QStringLiteral(":old_month"), oldMonth);
    if (!query.exec() || query.numRowsAffected() != oldLogCount) {
        const QString error = primarySqlError(query.lastError());
        database.rollback();
        setLastError(QStringLiteral("TMMA tracker identity update failed: %1").arg(error));
        return false;
    }

    int verifiedOldJobs = 0;
    int verifiedNewJobs = 0;
    int verifiedOldLogs = 0;
    int verifiedNewLogs = 0;
    if (!identityCount(QStringLiteral("tm_ma_jobs"), oldJobNumber, oldYear, oldMonth, verifiedOldJobs, database)
        || !identityCount(QStringLiteral("tm_ma_jobs"), newJobNumber, newYear, newMonth, verifiedNewJobs, database)
        || !identityCount(QStringLiteral("tm_ma_log"), oldJobNumber, oldYear, oldMonth, verifiedOldLogs, database)
        || !identityCount(QStringLiteral("tm_ma_log"), newJobNumber, newYear, newMonth, verifiedNewLogs, database)
        || verifiedOldJobs != 0 || verifiedNewJobs != 1
        || verifiedOldLogs != 0 || verifiedNewLogs != oldLogCount) {
        database.rollback();
        if (m_lastError.isEmpty()) {
            setLastError(QStringLiteral("TMMA identity migration verification failed. No data was changed."));
        }
        return false;
    }

    if (!database.commit()) {
        const QString error = primarySqlError(database.lastError());
        database.rollback();
        setLastError(QStringLiteral("TMMA identity migration commit failed: %1").arg(error));
        return false;
    }
    setLastError(QString());
    return true;
}

bool TMMADBManager::saveJobState(const QString& jobNumber,
                                 const QString& year,
                                 const QString& month,
                                 const TMMAJobState& state)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        setLastError(QStringLiteral("TMMA job-state save failed: GOJI database is unavailable."));
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(QStringLiteral(
        "UPDATE tm_ma_jobs SET html_display_state = :html_display_state, "
        "job_data_locked = :job_data_locked, postage_data_locked = :postage_data_locked, "
        "postage = :postage, count = :count, mail_class = :mail_class, permit = :permit, "
        "last_executed_script = :last_executed_script, updated_at = :updated_at "
        "WHERE job_number = :job_number AND year = :year AND month = :month"));
    query.bindValue(QStringLiteral(":html_display_state"), state.htmlDisplayState);
    query.bindValue(QStringLiteral(":job_data_locked"), state.jobDataLocked ? 1 : 0);
    query.bindValue(QStringLiteral(":postage_data_locked"), state.postageDataLocked ? 1 : 0);
    query.bindValue(QStringLiteral(":postage"), nonNullText(state.postage));
    query.bindValue(QStringLiteral(":count"), nonNullText(state.count));
    query.bindValue(QStringLiteral(":mail_class"), nonNullText(state.mailClass));
    query.bindValue(QStringLiteral(":permit"), nonNullText(state.permit));
    query.bindValue(QStringLiteral(":last_executed_script"), nonNullText(state.lastExecutedScript));
    query.bindValue(QStringLiteral(":updated_at"), QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":job_number"), jobNumber);
    query.bindValue(QStringLiteral(":year"), year);
    query.bindValue(QStringLiteral(":month"), month);
    if (!query.exec()) {
        setLastError(QStringLiteral("TMMA database job-state update failed: %1")
                         .arg(primarySqlError(query.lastError())));
        return false;
    }
    setLastError(QString());
    if (query.numRowsAffected() != 1) {
        setLastError(QStringLiteral("TMMA job-state update found no exact active identity for %1 / %2 / %3.")
                         .arg(jobNumber, year, month));
        return false;
    }
    return true;
}

bool TMMADBManager::loadJobState(const QString& jobNumber,
                                 const QString& year,
                                 const QString& month,
                                 TMMAJobState& state) const
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        setLastError(QStringLiteral("TMMA job load failed: GOJI database is unavailable."));
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(QStringLiteral(
        "SELECT html_display_state, job_data_locked, postage_data_locked, postage, count, "
        "mail_class, permit, last_executed_script FROM tm_ma_jobs "
        "WHERE job_number = :job_number AND year = :year AND month = :month"));
    query.bindValue(QStringLiteral(":job_number"), jobNumber);
    query.bindValue(QStringLiteral(":year"), year);
    query.bindValue(QStringLiteral(":month"), month);
    if (!execPrepared(query, QStringLiteral("job-state load")) || !query.next()) {
        if (m_lastError.isEmpty()) {
            setLastError(QStringLiteral("TMMA job was not found for %1 / %2 / %3.")
                             .arg(jobNumber, year, month));
        }
        return false;
    }

    state.htmlDisplayState = query.value(QStringLiteral("html_display_state")).toInt();
    state.jobDataLocked = query.value(QStringLiteral("job_data_locked")).toBool();
    state.postageDataLocked = query.value(QStringLiteral("postage_data_locked")).toBool();
    state.postage = query.value(QStringLiteral("postage")).toString();
    state.count = query.value(QStringLiteral("count")).toString();
    state.mailClass = query.value(QStringLiteral("mail_class")).toString();
    state.permit = query.value(QStringLiteral("permit")).toString();
    state.lastExecutedScript = query.value(QStringLiteral("last_executed_script")).toString();
    setLastError(QString());
    return true;
}

QList<QMap<QString, QString>> TMMADBManager::getAllJobs() const
{
    QList<QMap<QString, QString>> jobs;
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        setLastError(QStringLiteral("TMMA job listing failed: GOJI database is unavailable."));
        return jobs;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(QStringLiteral(
        "SELECT job_number, year, month FROM tm_ma_jobs "
        "ORDER BY CAST(year AS INTEGER) DESC, CAST(month AS INTEGER) DESC, "
        "CAST(job_number AS INTEGER) DESC, job_number DESC"));
    if (!execPrepared(query, QStringLiteral("job listing"))) {
        return jobs;
    }
    while (query.next()) {
        QMap<QString, QString> row;
        row.insert(QStringLiteral("job_number"), query.value(QStringLiteral("job_number")).toString());
        row.insert(QStringLiteral("year"), query.value(QStringLiteral("year")).toString());
        row.insert(QStringLiteral("month"), query.value(QStringLiteral("month")).toString());
        jobs.append(row);
    }
    return jobs;
}

bool TMMADBManager::deleteJob(const QString& jobNumber,
                              const QString& year,
                              const QString& month)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        setLastError(QStringLiteral("TMMA job delete failed: GOJI database is unavailable."));
        return false;
    }

    QSqlDatabase& db = m_dbManager->getDatabase();
    if (!db.transaction()) {
        setLastError(QStringLiteral("TMMA job delete transaction failed: %1").arg(db.lastError().text()));
        return false;
    }

    QSqlQuery query(db);
    const auto deleteFrom = [&](const QString& table) {
        query.prepare(QStringLiteral("DELETE FROM %1 WHERE job_number = :job_number AND year = :year AND month = :month")
                          .arg(table));
        query.bindValue(QStringLiteral(":job_number"), jobNumber);
        query.bindValue(QStringLiteral(":year"), year);
        query.bindValue(QStringLiteral(":month"), month);
        return execPrepared(query, table + QStringLiteral(" delete"));
    };

    if (!deleteFrom(QStringLiteral("tm_ma_log")) || !deleteFrom(QStringLiteral("tm_ma_jobs"))) {
        db.rollback();
        return false;
    }
    if (!db.commit()) {
        setLastError(QStringLiteral("TMMA job delete commit failed: %1").arg(db.lastError().text()));
        return false;
    }
    return true;
}

bool TMMADBManager::upsertLogEntry(const QString& jobNumber,
                                   const QString& year,
                                   const QString& month,
                                   const QString& description,
                                   const QString& postage,
                                   const QString& count,
                                   const QString& perPiece,
                                   const QString& mailClass,
                                   const QString& shape,
                                   const QString& permit,
                                   const QString& date)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        setLastError(QStringLiteral("TMMA tracker save failed: GOJI database is unavailable."));
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(QStringLiteral(
        "UPDATE tm_ma_log SET description = :description, postage = :postage, count = :count, "
        "per_piece = :per_piece, class = :class, shape = :shape, permit = :permit, "
        "date = :date, updated_at = :updated_at "
        "WHERE job_number = :job_number AND year = :year AND month = :month"));
    query.bindValue(QStringLiteral(":description"), description);
    query.bindValue(QStringLiteral(":postage"), postage);
    query.bindValue(QStringLiteral(":count"), count);
    query.bindValue(QStringLiteral(":per_piece"), perPiece);
    query.bindValue(QStringLiteral(":class"), mailClass);
    query.bindValue(QStringLiteral(":shape"), shape);
    query.bindValue(QStringLiteral(":permit"), permit);
    query.bindValue(QStringLiteral(":date"), date);
    query.bindValue(QStringLiteral(":updated_at"), QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":job_number"), jobNumber);
    query.bindValue(QStringLiteral(":year"), year);
    query.bindValue(QStringLiteral(":month"), month);
    if (!execPrepared(query, QStringLiteral("tracker update"))) {
        return false;
    }
    if (query.numRowsAffected() > 0) {
        return true;
    }

    query.prepare(QStringLiteral(
        "INSERT INTO tm_ma_log (job_number, description, postage, count, per_piece, class, "
        "shape, permit, date, year, month, created_at, updated_at) VALUES "
        "(:job_number, :description, :postage, :count, :per_piece, :class, :shape, :permit, "
        ":date, :year, :month, :created_at, :updated_at)"));
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    query.bindValue(QStringLiteral(":job_number"), jobNumber);
    query.bindValue(QStringLiteral(":description"), description);
    query.bindValue(QStringLiteral(":postage"), postage);
    query.bindValue(QStringLiteral(":count"), count);
    query.bindValue(QStringLiteral(":per_piece"), perPiece);
    query.bindValue(QStringLiteral(":class"), mailClass);
    query.bindValue(QStringLiteral(":shape"), shape);
    query.bindValue(QStringLiteral(":permit"), permit);
    query.bindValue(QStringLiteral(":date"), date);
    query.bindValue(QStringLiteral(":year"), year);
    query.bindValue(QStringLiteral(":month"), month);
    query.bindValue(QStringLiteral(":created_at"), now);
    query.bindValue(QStringLiteral(":updated_at"), now);
    return execPrepared(query, QStringLiteral("tracker insert"));
}
