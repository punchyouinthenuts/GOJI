#include "tmfarmdbmanager.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

// ---------- Singleton ----------
TMFarmDBManager* TMFarmDBManager::s_instance = nullptr;

TMFarmDBManager* TMFarmDBManager::instance(QObject* parent) {
    if (!s_instance) {
        s_instance = new TMFarmDBManager(parent);
    }
    return s_instance;
}

// ---------- Ctor / Dtor ----------
TMFarmDBManager::TMFarmDBManager(QObject* parent)
    : QObject(parent),
      m_initialized(false) {
    // Reuse the shared DatabaseManager connection if your app centralizes DB access there.
    // Fallback: ensure a default SQLite connection exists.
    if (QSqlDatabase::contains("goji")) {
        m_db = QSqlDatabase::database("goji");
    } else {
        m_db = QSqlDatabase::addDatabase("QSQLITE", "goji");
        // NOTE: Path here is conservative; adjust if your DatabaseManager sets it globally.
        const QString dbDir = QDir::homePath() + "/GojiData";
        QDir().mkpath(dbDir);
        m_db.setDatabaseName(dbDir + "/goji.sqlite");
    }

    if (!m_db.isOpen() && !m_db.open()) {
        qWarning() << "[TMFARM] Failed to open database:" << m_db.lastError().text();
        m_initialized = false;
        return;
    }

    if (!ensureTables()) {
        qWarning() << "[TMFARM] ensureTables() failed:" << m_db.lastError().text();
        m_initialized = false;
        return;
    }

    m_initialized = true;
}

TMFarmDBManager::~TMFarmDBManager() = default;

// ---------- Basic state ----------
bool TMFarmDBManager::isInitialized() const {
    return m_initialized;
}

QSqlDatabase TMFarmDBManager::getDatabase() const {
    return m_db;
}

// ---------- Schema helpers ----------
static bool execQuery(QSqlQuery& q, const QString& sql) {
    if (!q.exec(sql)) {
        qWarning() << "[TMFARM][SQL] Error:" << q.lastError().text() << " while running:" << sql;
        return false;
    }
    return true;
}

bool TMFarmDBManager::ensureTables() {
    if (!m_db.isValid()) return false;
    QSqlQuery q(m_db);

    // Minimal, safe tables that mirror patterns used by other tabs (TMTERM/TMBA/TMHB).
    // If your app already creates these centrally, these CREATEs are harmless due to IF NOT EXISTS.

    // 1) Jobs table for Farmworkers
    if (!execQuery(q, QStringLiteral(
        "CREATE TABLE IF NOT EXISTS tmfarm_jobs ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  year TEXT,"
        "  month TEXT,"
        "  job_number TEXT,"
        "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")"))) return false;

    // 2) Job state table (lock state, html_state)
    if (!execQuery(q, QStringLiteral(
        "CREATE TABLE IF NOT EXISTS tmfarm_job_state ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  year TEXT,"
        "  month TEXT,"
        "  job_number TEXT,"
        "  job_locked INTEGER DEFAULT 1,"
        "  html_state TEXT DEFAULT 'Default',"
        "  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")"))) return false;

    // 3) Tracker/log table (displayed in tracker view)
    if (!execQuery(q, QStringLiteral(
        "CREATE TABLE IF NOT EXISTS tmfarm_log ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  year TEXT,"
        "  month TEXT,"
        "  job_number TEXT,"
        "  description TEXT,"
        "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")"))) return false;

    // 4) Optional terminal log (if the controller uses it)
    if (!execQuery(q, QStringLiteral(
        "CREATE TABLE IF NOT EXISTS tmfarm_terminal_log ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  message TEXT,"
        "  type INTEGER,"
        "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")"))) return false;

    return true;
}

// ---------- Example API used by controller (safe stubs) ----------
// NOTE: These can be expanded later. They are provided to prevent undefined-symbol build errors
// if the header declares them and controller links to them.

bool TMFarmDBManager::upsertJob(const QString& year, const QString& month, const QString& jobNumber) {
    if (!m_db.isOpen()) return false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO tmfarm_jobs (year, month, job_number) VALUES (:y, :m, :j)"));
    q.bindValue(":y", year);
    q.bindValue(":m", month);
    q.bindValue(":j", jobNumber);
    if (!q.exec()) {
        // Ignore UNIQUE conflicts if schema later adds a unique index; treat as success.
        qWarning() << "[TMFARM] upsertJob failed:" << q.lastError().text();
    }
    return true;
}

bool TMFarmDBManager::saveJobState(const QString& year, const QString& month, const QString& jobNumber,
                                   bool jobLocked, const QString& htmlState) {
    if (!m_db.isOpen()) return false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO tmfarm_job_state (year, month, job_number, job_locked, html_state, updated_at) "
        "VALUES (:y, :m, :j, :locked, :html, CURRENT_TIMESTAMP)"));
    q.bindValue(":y", year);
    q.bindValue(":m", month);
    q.bindValue(":j", jobNumber);
    q.bindValue(":locked", jobLocked ? 1 : 0);
    q.bindValue(":html", htmlState);
    if (!q.exec()) {
        qWarning() << "[TMFARM] saveJobState failed:" << q.lastError().text();
        return false;
    }
    return true;
}

QVariantMap TMFarmDBManager::loadJobState(const QString& year, const QString& month, const QString& jobNumber) {
    QVariantMap out;
    if (!m_db.isOpen()) return out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT job_locked, html_state FROM tmfarm_job_state "
        "WHERE year=:y AND month=:m AND job_number=:j "
        "ORDER BY id DESC LIMIT 1"));
    q.bindValue(":y", year);
    q.bindValue(":m", month);
    q.bindValue(":j", jobNumber);
    if (q.exec() && q.next()) {
        out["job_locked"] = q.value(0).toInt() != 0;
        out["html_state"] = q.value(1).toString();
    }
    return out;
}

bool TMFarmDBManager::addLogEntry(const QString& year, const QString& month, const QString& jobNumber,
                                  const QString& description) {
    if (!m_db.isOpen()) return false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO tmfarm_log (year, month, job_number, description) "
        "VALUES (:y, :m, :j, :d)"));
    q.bindValue(":y", year);
    q.bindValue(":m", month);
    q.bindValue(":j", jobNumber);
    q.bindValue(":d", description);
    if (!q.exec()) {
        qWarning() << "[TMFARM] addLogEntry failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool TMFarmDBManager::addTerminalLog(const QString& message, int type) {
    if (!m_db.isOpen()) return false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO tmfarm_terminal_log (message, type) VALUES (:m, :t)"));
    q.bindValue(":m", message);
    q.bindValue(":t", type);
    if (!q.exec()) {
        qWarning() << "[TMFARM] addTerminalLog failed:" << q.lastError().text();
        return false;
    }
    return true;
}
