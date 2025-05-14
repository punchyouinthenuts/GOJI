#include "logger.h"
#include "mainwindow.h"
#include <QDebug>
#include <QDir>
#include <QTextStream>
#include <QMutexLocker>

// Static instance
Logger& Logger::instance()
{
    static Logger instance;
    return instance;
}

Logger::Logger()
    : QObject(nullptr),
    m_logToConsole(true),
    m_initialized(false)
{
    // No qDebug logging in constructor
}

Logger::~Logger()
{
    close();
}

bool Logger::initialize(const QString& logFilePath, bool logToConsole)
{
    QMutexLocker locker(&m_mutex);

    // If already initialized, close the existing file
    if (m_initialized && m_logFile.isOpen()) {
        m_logFile.close();
    }

    // Set up the new log file
    m_logFile.setFileName(logFilePath);
    m_logToConsole = logToConsole;

    // Create the directory if it doesn't exist
    QFileInfo fileInfo(logFilePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            return false;
        }
    }

    // Open the log file
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        return false;
    }

    m_initialized = true;

    // Log initialization without using too much debugging
    info("Logger initialized", "Logger::initialize");

    return true;
}

void Logger::debug(const QString& message, const QString& source)
{
    log(LogLevel::Debug, message, source);
}

void Logger::info(const QString& message, const QString& source)
{
    log(LogLevel::Info, message, source);
}

void Logger::warning(const QString& message, const QString& source)
{
    log(LogLevel::Warning, message, source);
}

void Logger::error(const QString& message, const QString& source)
{
    log(LogLevel::Error, message, source);
}

void Logger::fatal(const QString& message, const QString& source)
{
    log(LogLevel::Fatal, message, source);
}

void Logger::log(LogLevel level, const QString& message, const QString& source)
{
    QMutexLocker locker(&m_mutex);

    QString formattedMessage = formatLogMessage(level, message, source);

    // Write to file if initialized
    if (m_initialized && m_logFile.isOpen()) {
        writeToFile(formattedMessage);
    }

    // Write to console if enabled
    if (m_logToConsole) {
        switch (level) {
        case LogLevel::Debug:
            qDebug().noquote() << formattedMessage;
            break;
        case LogLevel::Info:
            qInfo().noquote() << formattedMessage;
            break;
        case LogLevel::Warning:
            qWarning().noquote() << formattedMessage;
            break;
        case LogLevel::Error:
        case LogLevel::Fatal:
            qCritical().noquote() << formattedMessage;
            break;
        }
    }

    // Call custom handler if set
    if (m_customHandler) {
        m_customHandler(level, formattedMessage);
    }

    // Re-enable signal emission - this was previously commented out to test for crash
    // Using a try-catch block in case signal emission is the problem
    try {
        emit messageLogged(level, formattedMessage);
    } catch (...) {
        // If signal emission causes a crash, silently catch and continue
        qCritical() << "Error emitting messageLogged signal";
    }
}

void Logger::close()
{
    QMutexLocker locker(&m_mutex);

    if (m_initialized && m_logFile.isOpen()) {
        m_logFile.close();
    }

    m_initialized = false;
}

bool Logger::isInitialized() const
{
    return m_initialized && m_logFile.isOpen();
}

void Logger::setCustomLogHandler(std::function<void(LogLevel, const QString&)> handler)
{
    QMutexLocker locker(&m_mutex);
    m_customHandler = handler;
}

QString Logger::formatLogMessage(LogLevel level, const QString& message, const QString& source) const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr = levelToString(level);

    QString formattedMessage;
    if (source.isEmpty()) {
        formattedMessage = QString("[%1] [%2] %3")
        .arg(timestamp, levelStr, message);
    } else {
        formattedMessage = QString("[%1] [%2] [%3] %4")
        .arg(timestamp, levelStr, source, message);
    }

    return formattedMessage;
}

void Logger::writeToFile(const QString& message)
{
    QTextStream out(&m_logFile);
    out << message << "\n";
    out.flush();
}

QString Logger::levelToString(LogLevel level) const
{
    switch (level) {
    case LogLevel::Debug:   return "DEBUG";
    case LogLevel::Info:    return "INFO";
    case LogLevel::Warning: return "WARNING";
    case LogLevel::Error:   return "ERROR";
    case LogLevel::Fatal:   return "FATAL";
    default:                return "UNKNOWN";
    }
}

// Global function for backward compatibility with existing code
void logMessage(const QString& message)
{
    Logger::instance().info(message);
}
