#include "tmcadbmanager.h"
#include "logger.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>

TMCADBManager* TMCADBManager::m_instance = nullptr;

TMCADBManager* TMCADBManager::instance()
{
    if (!m_instance) {
        m_instance = new TMCADBManager();
    }
    return m_instance;
}

TMCADBManager::TMCADBManager(QObject *parent)
    : QObject(parent)
    , m_dbManager(DatabaseManager::instance())
    , m_trackerModel(nullptr)
{
}

bool TMCADBManager::initializeTables()
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        Logger::instance().error("Database manager not initialized for TMCA");
        return false;
    }

    bool success = createTables();
    if (!success) {
        return false;
    }

    if (m_trackerModel) {
        delete m_trackerModel;
        m_trackerModel = nullptr;
    }

    m_trackerModel = new QSqlTableModel(this, m_dbManager->getDatabase());
    m_trackerModel->setTable("tm_ca_log");
    m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);

    // Match tracker headers used across TM modules (TMFLER/TMTERM)
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
    Logger::instance().info("TMCA tracker model initialized");
    return true;
}

bool TMCADBManager::createTables()
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for TMCA createTables");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());

    // Jobs table: one job per job_number + year + month
    // New schema uses UNIQUE(job_number, year, month); old schema had UNIQUE(year, month).
    // SQLite cannot drop constraints, so we recreate if the old constraint pattern is present.
    if (!query.exec("CREATE TABLE IF NOT EXISTS tm_ca_jobs ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "job_number TEXT NOT NULL DEFAULT '', "
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
                    "UNIQUE(job_number, year, month)"
                    ")")) {
        Logger::instance().error("Failed to create tm_ca_jobs table: " + query.lastError().text());
        return false;
    }

    // Add missing columns if they don't exist (non-fatal for existing DBs)
    query.exec("ALTER TABLE tm_ca_jobs ADD COLUMN html_display_state INTEGER DEFAULT 0");
    query.exec("ALTER TABLE tm_ca_jobs ADD COLUMN job_data_locked INTEGER DEFAULT 0");
    query.exec("ALTER TABLE tm_ca_jobs ADD COLUMN postage_data_locked INTEGER DEFAULT 0");
    query.exec("ALTER TABLE tm_ca_jobs ADD COLUMN postage TEXT DEFAULT ''");
    query.exec("ALTER TABLE tm_ca_jobs ADD COLUMN count TEXT DEFAULT ''");
    query.exec("ALTER TABLE tm_ca_jobs ADD COLUMN last_executed_script TEXT DEFAULT ''");

    // Schema migration: if tm_ca_jobs exists with old UNIQUE(year, month), rebuild it with
    // UNIQUE(job_number, year, month) using the safe rename-copy-drop pattern.
    // Rows with an empty job_number cannot be keyed under the new constraint and are moved
    // to tm_ca_jobs_legacy instead of being silently dropped or given invented keys.
    {
        QSqlQuery chk(m_dbManager->getDatabase());
        chk.exec("SELECT sql FROM sqlite_master WHERE type='table' AND name='tm_ca_jobs'");
        if (chk.next()) {
            const QString ddl = chk.value(0).toString();
            const bool hasOldUnique  = ddl.contains("UNIQUE(year, month)",            Qt::CaseInsensitive);
            const bool hasNewUnique  = ddl.contains("UNIQUE(job_number, year, month)", Qt::CaseInsensitive);

            if (hasOldUnique && !hasNewUnique) {
                Logger::instance().info("TMCA: migrating tm_ca_jobs to UNIQUE(job_number, year, month)");

                QSqlDatabase db = m_dbManager->getDatabase();
                if (!db.transaction()) {
                    Logger::instance().error("TMCA: schema migration — could not begin transaction: " +
                                             db.lastError().text());
                } else {
                    QSqlQuery mig(db);

                    // Step 0: Drop any leftover temp table from a previous interrupted migration
                    mig.exec("DROP TABLE IF EXISTS tm_ca_jobs_new");

                    // Step 1: Create new table with correct constraint
                    bool ok = mig.exec(
                        "CREATE TABLE tm_ca_jobs_new ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                        "job_number TEXT NOT NULL DEFAULT '', "
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
                        "UNIQUE(job_number, year, month))");

                    // Step 2: Preserve legacy rows (empty job_number) in a separate table.
                    // original_id stores the old PK; the legacy table has its own AUTOINCREMENT PK
                    // so repeated migrations (e.g. from multiple interrupted runs) never collide.
                    if (ok) ok = mig.exec(
                        "CREATE TABLE IF NOT EXISTS tm_ca_jobs_legacy ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                        "original_id INTEGER, "
                        "job_number TEXT, year TEXT, month TEXT, "
                        "html_display_state INTEGER, job_data_locked INTEGER, "
                        "postage_data_locked INTEGER, postage TEXT, count TEXT, "
                        "last_executed_script TEXT, "
                        "created_at TIMESTAMP, updated_at TIMESTAMP, "
                        "migrated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)");

                    if (ok) ok = mig.exec(
                        "INSERT INTO tm_ca_jobs_legacy "
                        "(original_id, job_number, year, month, html_display_state, "
                        "job_data_locked, postage_data_locked, postage, count, "
                        "last_executed_script, created_at, updated_at) "
                        "SELECT id, job_number, year, month, html_display_state, "
                        "job_data_locked, postage_data_locked, postage, count, "
                        "last_executed_script, created_at, updated_at "
                        "FROM tm_ca_jobs WHERE TRIM(job_number) = ''");

                    // Step 3: Copy rows that have a real job_number into new table
                    if (ok) ok = mig.exec(
                        "INSERT OR IGNORE INTO tm_ca_jobs_new "
                        "SELECT id, job_number, year, month, html_display_state, "
                        "job_data_locked, postage_data_locked, postage, count, "
                        "last_executed_script, created_at, updated_at "
                        "FROM tm_ca_jobs WHERE TRIM(job_number) != ''");

                    // Step 4: Swap tables
                    if (ok) ok = mig.exec("DROP TABLE tm_ca_jobs");
                    if (ok) ok = mig.exec("ALTER TABLE tm_ca_jobs_new RENAME TO tm_ca_jobs");

                    if (ok) {
                        db.commit();
                        Logger::instance().info("TMCA: schema migration completed successfully");
                    } else {
                        db.rollback();
                        Logger::instance().error("TMCA: schema migration failed — rolled back: " +
                                                 mig.lastError().text());
                    }
                }
            }
        }
    }

    // Tracker log table
    if (!query.exec("CREATE TABLE IF NOT EXISTS tm_ca_log ("
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
                    "year TEXT NOT NULL DEFAULT '', "
                    "month TEXT NOT NULL DEFAULT '', "
                    "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                    ")")) {
        Logger::instance().error("Failed to create tm_ca_log table: " + query.lastError().text());
        return false;
    }

    // Add year/month to existing tm_ca_log tables that predate this requirement (non-fatal)
    query.exec("ALTER TABLE tm_ca_log ADD COLUMN year TEXT NOT NULL DEFAULT ''");
    query.exec("ALTER TABLE tm_ca_log ADD COLUMN month TEXT NOT NULL DEFAULT ''");

    Logger::instance().info("TMCA database tables created successfully");
    return true;
}

bool TMCADBManager::saveJob(const QString& jobNumber, const QString& year, const QString& month)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for TMCA saveJob");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    // UPSERT keyed on all three columns (job_number, year, month)
    query.prepare("INSERT INTO tm_ca_jobs (job_number, year, month, created_at, updated_at) "
                  "VALUES (:job_number, :year, :month, :now, :now2) "
                  "ON CONFLICT(job_number, year, month) DO UPDATE SET updated_at = :now3");
    query.bindValue(":job_number", jobNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":now",  now);
    query.bindValue(":now2", now);
    query.bindValue(":now3", now);

    if (!m_dbManager->executeQuery(query)) {
        Logger::instance().error(QString("Failed to upsert TMCA job %1/%2/%3: %4")
                                 .arg(jobNumber, year, month, query.lastError().text()));
        return false;
    }

    Logger::instance().info(QString("TMCA job saved: %1 for %2/%3").arg(jobNumber, year, month));
    return true;
}

bool TMCADBManager::loadJob(const QString& jobNumber, const QString& year, const QString& month)
{
    QString foundJob;
    if (!loadJob(year, month, foundJob)) {
        return false;
    }
    return foundJob == jobNumber;
}

bool TMCADBManager::loadJob(const QString& year, const QString& month, QString& jobNumber)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for TMCA loadJob");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT job_number FROM tm_ca_jobs WHERE year = :year AND month = :month");
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    if (!m_dbManager->executeQuery(query)) {
        Logger::instance().error(QString("Failed to execute TMCA loadJob query for %1/%2: %3")
                                 .arg(year, month, query.lastError().text()));
        return false;
    }

    if (!query.next()) {
        Logger::instance().warning(QString("No TMCA job found for %1/%2").arg(year, month));
        return false;
    }

    jobNumber = query.value("job_number").toString();
    return true;
}

bool TMCADBManager::deleteJob(int year, int month)
{
    QString monthStr = QString("%1").arg(month, 2, 10, QChar('0'));
    return deleteJob(QString::number(year), monthStr);
}

bool TMCADBManager::deleteJob(const QString& year, const QString& month)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for TMCA deleteJob");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("DELETE FROM tm_ca_jobs WHERE year = :year AND month = :month");
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    bool success = m_dbManager->executeQuery(query);
    if (!success) {
        Logger::instance().error(QString("Failed to delete TMCA job for %1/%2: %3")
                                 .arg(year, month, query.lastError().text()));
    }
    return success;
}

bool TMCADBManager::jobExists(const QString& year, const QString& month)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT COUNT(*) FROM tm_ca_jobs WHERE year = :year AND month = :month");
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    if (!m_dbManager->executeQuery(query)) {
        return false;
    }

    if (!query.next()) {
        return false;
    }

    return query.value(0).toInt() > 0;
}

QList<QMap<QString, QString>> TMCADBManager::getAllJobs()
{
    QList<QMap<QString, QString>> jobs;

    if (!m_dbManager || !m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for TMCA getAllJobs");
        return jobs;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT job_number, year, month FROM tm_ca_jobs ORDER BY year DESC, month DESC");

    if (!m_dbManager->executeQuery(query)) {
        Logger::instance().error("Failed to execute TMCA getAllJobs query");
        return jobs;
    }

    while (query.next()) {
        QMap<QString, QString> job;
        job["job_number"] = query.value("job_number").toString();
        job["year"] = query.value("year").toString();
        job["month"] = query.value("month").toString();
        jobs.append(job);
    }

    return jobs;
}

bool TMCADBManager::saveJobState(const QString& jobNumber, const QString& year, const QString& month,
                                 int htmlDisplayState, bool jobDataLocked, bool postageDataLocked,
                                 const QString& postage, const QString& count,
                                 const QString& lastExecutedScript)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for TMCA saveJobState");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    // Update existing row keyed by all three columns
    query.prepare("UPDATE tm_ca_jobs SET "
                  "html_display_state = :html_display_state, "
                  "job_data_locked = :job_data_locked, "
                  "postage_data_locked = :postage_data_locked, "
                  "postage = :postage, "
                  "count = :count, "
                  "last_executed_script = :last_executed_script, "
                  "updated_at = :updated_at "
                  "WHERE job_number = :job_number AND year = :year AND month = :month");
    query.bindValue(":html_display_state", htmlDisplayState);
    query.bindValue(":job_data_locked", jobDataLocked ? 1 : 0);
    query.bindValue(":postage_data_locked", postageDataLocked ? 1 : 0);
    query.bindValue(":postage", postage);
    query.bindValue(":count", count);
    query.bindValue(":last_executed_script", lastExecutedScript);
    query.bindValue(":updated_at", now);
    query.bindValue(":job_number", jobNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    if (!m_dbManager->executeQuery(query)) {
        Logger::instance().error(QString("Failed to update TMCA job state for %1/%2/%3: %4")
                                 .arg(jobNumber, year, month, query.lastError().text()));
        return false;
    }

    if (query.numRowsAffected() == 0) {
        // No existing row — insert with all three keys
        query.prepare("INSERT INTO tm_ca_jobs "
                      "(job_number, year, month, html_display_state, job_data_locked, "
                      "postage_data_locked, postage, count, last_executed_script, created_at, updated_at) "
                      "VALUES (:job_number, :year, :month, :html_display_state, :job_data_locked, "
                      ":postage_data_locked, :postage, :count, :last_executed_script, :created_at, :updated_at)");
        query.bindValue(":job_number", jobNumber);
        query.bindValue(":year", year);
        query.bindValue(":month", month);
        query.bindValue(":html_display_state", htmlDisplayState);
        query.bindValue(":job_data_locked", jobDataLocked ? 1 : 0);
        query.bindValue(":postage_data_locked", postageDataLocked ? 1 : 0);
        query.bindValue(":postage", postage);
        query.bindValue(":count", count);
        query.bindValue(":last_executed_script", lastExecutedScript);
        query.bindValue(":created_at", now);
        query.bindValue(":updated_at", now);

        if (!m_dbManager->executeQuery(query)) {
            Logger::instance().error(QString("Failed to insert TMCA job state for %1/%2/%3: %4")
                                     .arg(jobNumber, year, month, query.lastError().text()));
            return false;
        }
    }

    return true;
}

bool TMCADBManager::loadJobState(const QString& jobNumber, const QString& year, const QString& month,
                                 int& htmlDisplayState, bool& jobDataLocked, bool& postageDataLocked,
                                 QString& postage, QString& count, QString& lastExecutedScript)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for TMCA loadJobState");
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT html_display_state, job_data_locked, postage_data_locked, "
                  "postage, count, last_executed_script FROM tm_ca_jobs "
                  "WHERE job_number = :job_number AND year = :year AND month = :month");
    query.bindValue(":job_number", jobNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);

    if (!m_dbManager->executeQuery(query)) {
        Logger::instance().error(QString("Failed to execute TMCA loadJobState query for %1/%2/%3: %4")
                                 .arg(jobNumber, year, month, query.lastError().text()));
        return false;
    }

    if (!query.next()) {
        htmlDisplayState = 0;
        jobDataLocked = false;
        postageDataLocked = false;
        postage.clear();
        count.clear();
        lastExecutedScript.clear();
        return false;
    }

    htmlDisplayState = query.value("html_display_state").toInt();
    jobDataLocked = query.value("job_data_locked").toInt() == 1;
    postageDataLocked = query.value("postage_data_locked").toInt() == 1;
    postage = query.value("postage").toString();
    count = query.value("count").toString();
    lastExecutedScript = query.value("last_executed_script").toString();
    return true;
}

bool TMCADBManager::addLogEntry(const QString& jobNumber, const QString& description,
                                const QString& postage, const QString& count,
                                const QString& avgRate, const QString& mailClass,
                                const QString& shape, const QString& permit,
                                const QString& date,
                                const QString& year, const QString& month)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for TMCA addLogEntry");
        return false;
    }

    // Update-most-recent pattern used across TM modules: try to update matching job+description first
    QSqlQuery find(m_dbManager->getDatabase());
    find.prepare("SELECT id FROM tm_ca_log WHERE job_number = :job_number AND description = :description "
                 "ORDER BY id DESC LIMIT 1");
    find.bindValue(":job_number", jobNumber);
    find.bindValue(":description", description);

    if (!m_dbManager->executeQuery(find)) {
        Logger::instance().error("Failed to query existing TMCA log entry: " + find.lastError().text());
        return false;
    }

    if (find.next()) {
        int id = find.value(0).toInt();
        QSqlQuery update(m_dbManager->getDatabase());
        update.prepare("UPDATE tm_ca_log SET "
                       "description = :description, postage = :postage, count = :count, "
                       "per_piece = :per_piece, class = :class, shape = :shape, "
                       "permit = :permit, date = :date, "
                       "year = :year, month = :month, "
                       "created_at = :created_at "
                       "WHERE id = :id");
        update.bindValue(":description", description);
        update.bindValue(":postage", postage);
        update.bindValue(":count", count);
        update.bindValue(":per_piece", avgRate);
        update.bindValue(":class", mailClass);
        update.bindValue(":shape", shape);
        update.bindValue(":permit", permit);
        update.bindValue(":date", date);
        update.bindValue(":year", year);
        update.bindValue(":month", month);
        update.bindValue(":created_at", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
        update.bindValue(":id", id);

        bool ok = m_dbManager->executeQuery(update);
        if (ok && m_trackerModel) {
            m_trackerModel->select();
        }
        return ok;
    }

    QSqlQuery insert(m_dbManager->getDatabase());
    insert.prepare("INSERT INTO tm_ca_log "
                   "(job_number, description, postage, count, per_piece, class, shape, permit, "
                   "date, year, month, created_at) "
                   "VALUES (:job_number, :description, :postage, :count, :per_piece, :class, :shape, :permit, "
                   ":date, :year, :month, :created_at)");
    insert.bindValue(":job_number", jobNumber);
    insert.bindValue(":description", description);
    insert.bindValue(":postage", postage);
    insert.bindValue(":count", count);
    insert.bindValue(":per_piece", avgRate);
    insert.bindValue(":class", mailClass);
    insert.bindValue(":shape", shape);
    insert.bindValue(":permit", permit);
    insert.bindValue(":date", date);
    insert.bindValue(":year", year);
    insert.bindValue(":month", month);
    insert.bindValue(":created_at", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    bool ok = m_dbManager->executeQuery(insert);
    if (ok && m_trackerModel) {
        m_trackerModel->select();
    }
    return ok;
}

bool TMCADBManager::insertLogRow(const QString& jobNumber, const QString& description,
                                 const QString& postage, const QString& count,
                                 const QString& avgRate, const QString& mailClass,
                                 const QString& shape, const QString& permit,
                                 const QString& date,
                                 const QString& year, const QString& month)
{
    return addLogEntry(jobNumber, description, postage, count,
                       avgRate, mailClass, shape, permit, date,
                       year, month);
}

QList<QMap<QString, QVariant>> TMCADBManager::getLog()
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for TMCA getLog");
        return QList<QMap<QString, QVariant>>();
    }

    return m_dbManager->executeSelectQuery("SELECT * FROM tm_ca_log ORDER BY id DESC");
}

bool TMCADBManager::saveTerminalLog(const QString& year, const QString& month, const QString& message)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        Logger::instance().error("Database not initialized for TMCA saveTerminalLog");
        return false;
    }

    // TMCA has no week concept
    return m_dbManager->saveTerminalLog(TAB_NAME, year, month, "", message);
}

QStringList TMCADBManager::getTerminalLogs(const QString& year, const QString& month)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        return QStringList();
    }

    // TMCA has no week concept
    return m_dbManager->getTerminalLogs(TAB_NAME, year, month, "");
}

QSqlTableModel* TMCADBManager::getTrackerModel()
{
    return m_trackerModel;
}
