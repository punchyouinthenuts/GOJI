#include "tmflerdbmanager.h"
#include "logger.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
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

bool TMFLERDBManager::createTables()
{
    QSqlQuery query(m_dbManager->getDatabase());

    // Create jobs table
    if (!query.exec("CREATE TABLE IF NOT EXISTS tm_fler_jobs ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "job_number TEXT NOT NULL, "
                    "year TEXT NOT NULL, "
                    "month TEXT NOT NULL, "
                    "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                    "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                    "UNIQUE(job_number, year, month)"
                    ")")) {
        qDebug() << "Error creating tm_fler_jobs table:" << query.lastError().text();
        Logger::instance().error("Failed to create tm_fler_jobs table");
        return false;
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
        Logger::instance().error("Failed to create tm_fler_log table");
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
    query.prepare("INSERT OR REPLACE INTO tm_fler_jobs "
                  "(job_number, year, month, updated_at) "
                  "VALUES (:job_number, :year, :month, :updated_at)");
    query.bindValue(":job_number", jobNumber);
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":updated_at", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    bool success = m_dbManager->executeQuery(query);
    if (success) {
        Logger::instance().info(QString("TMFLER job saved: %1 for %2/%3").arg(jobNumber, year, month));
    } else {
        Logger::instance().error(QString("Failed to save TMFLER job: %1 for %2/%3").arg(jobNumber, year, month));
    }
    return success;
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

    QSqlQuery query(m_dbManager->getDatabase());
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
        Logger::instance().info(QString("TMFLER log entry added: Job %1").arg(jobNumber));
        if (m_trackerModel) {
            m_trackerModel->select(); // Refresh the model
        }
    } else {
        Logger::instance().error(QString("Failed to add TMFLER log entry: Job %1").arg(jobNumber));
    }

    return success;
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
        Logger::instance().error(QString("Failed to delete TMFLER log entry: ID %1").arg(id));
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
        Logger::instance().error(QString("Failed to update TMFLER log entry: ID %1").arg(id));
    }

    return success;
}
