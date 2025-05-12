#include "logger.h"
#include "mainwindow.h"
#include <QDebug>
#include <QDir>
#include <QTextStream>
#include <QMutexLocker>

// Reference to the global logFile declared in main.cpp
extern QFile logFile;

// Static instance
Logger& Logger::instance()
{
    qDebug() << "Accessing Logger singleton instance";
    static Logger instance;
    return instance;
}

Logger::Logger()
    : QObject(nullptr),
    m_logToConsole(true),
    m_initialized(false)
{
    qDebug() << "Constructing Logger object";
}

Logger::~Logger()
{
    qDebug() << "Destructing Logger object";
    close();
}

bool Logger::initialize(const QString& logFilePath, bool logToConsole)
{
    qDebug() << "Initializing Logger with log file:" << logFilePath << "and console logging:" << logToConsole;
    QMutexLocker locker(&m_mutex);
    qDebug() << "Mutex locked for initialization";

    // If already initialized, close the existing file
    if (m_initialized && m_logFile.isOpen()) {
        qDebug() << "Closing existing log file";
        m_logFile.close();
    }

    // Set up the new log file
    m_logFile.setFileName(logFilePath);
    m_logToConsole = logToConsole;

    // Create the directory if it doesn't exist
    QFileInfo fileInfo(logFilePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        qDebug() << "Creating log directory:" << dir.path();
        if (!dir.mkpath(".")) {
            qDebug() << "Failed to create log directory:" << dir.path();
            return false;
        }
        qDebug() << "Log directory created successfully";
    }

    // Open the log file
    qDebug() << "Opening log file:" << logFilePath;
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        qDebug() << "Failed to open log file:" << logFilePath;
        return false;
    }
    qDebug() << "Log file opened successfully";

    m_initialized = true;

    // Log initialization
    qDebug() << "Logger initialized, logging initialization message";
    info("Logger initialized", "Logger::initialize");

    qDebug() << "Logger initialization complete";
    return true;
}

void Logger::debug(const QString& message, const QString& source)
{
    qDebug() << "Logging debug message:" << message << "from source:" << source;
    log(LogLevel::Debug, message, source);
}

void Logger::info(const QString& message, const QString& source)
{
    qDebug() << "Logging info message:" << message << "from source:" << source;
    log(LogLevel::Info, message, source);
}

void Logger::warning(const QString& message, const QString& source)
{
    qDebug() << "Logging warning message:" << message << "from source:" << source;
    log(LogLevel::Warning, message, source);
}

void Logger::error(const QString& message, const QString& source)
{
    qDebug() << "Logging error message:" << message << "from source:" << source;
    log(LogLevel::Error, message, source);
}

void Logger::fatal(const QString& message, const QString& source)
{
    qDebug() << "Logging fatal message:" << message << "from source:" << source;
    log(LogLevel::Fatal, message, source);
}

void Logger::log(LogLevel level, const QString& message, const QString& source)
{
    qDebug() << "Logging message with level:" << levelToString(level) << ", message:" << message << ", source:" << source;
    QMutexLocker locker(&m_mutex);
    qDebug() << "Mutex locked for logging";

    QString formattedMessage = formatLogMessage(level, message, source);
    qDebug() << "Formatted log message:" << formattedMessage;

    // Write to file if initialized
    if (m_initialized && m_logFile.isOpen()) {
        qDebug() << "Writing to log file";
        writeToFile(formattedMessage);
        qDebug() << "Log file write complete";
    } else {
        qDebug() << "Log file not initialized or not open, skipping file write";
    }

    // Write to console if enabled
    if (m_logToConsole) {
        qDebug() << "Writing to console";
        switch (level) {
        case LogLevel::Debug:
            qDebug().noquote() << formattedMessage;
            break;
        case LogLevel::Info:
            qInfo().noquote() << formattedMessage;
            qDebug().noquote() << formattedMessage; // Additional debug output
            break;
        case LogLevel::Warning:
            qWarning().noquote() << formattedMessage;
            qDebug().noquote() << formattedMessage; // Additional debug output
            break;
        case LogLevel::Error:
        case LogLevel::Fatal:
            qCritical().noquote() << formattedMessage;
            qDebug().noquote() << formattedMessage; // Additional debug output
            break;
        }
        qDebug() << "Console write complete";
    } else {
        qDebug() << "Console logging disabled";
    }

    // Call custom handler if set
    if (m_customHandler) {
        qDebug() << "Calling custom log handler";
        m_customHandler(level, formattedMessage);
        qDebug() << "Custom log handler called";
    } else {
        qDebug() << "No custom log handler set";
    }

    // Temporarily disable signal emission to test for crash
    qDebug() << "Skipping messageLogged signal emission to test for crash";
    // emit messageLogged(level, formattedMessage);
    // qDebug() << "messageLogged signal emitted";

    qDebug() << "Log operation complete";
}

void Logger::close()
{
    qDebug() << "Closing Logger";
    QMutexLocker locker(&m_mutex);
    qDebug() << "Mutex locked for closing";

    if (m_initialized && m_logFile.isOpen()) {
        qDebug() << "Closing log file";
        m_logFile.close();
        qDebug() << "Log file closed";
    } else {
        qDebug() << "Log file not initialized or already closed";
    }

    m_initialized = false;
    qDebug() << "Logger closed, initialized set to false";
}

bool Logger::isInitialized() const
{
    qDebug() << "Checking if Logger is initialized";
    bool result = m_initialized && m_logFile.isOpen();
    qDebug() << "Logger initialized status:" << result;
    return result;
}

void Logger::setCustomLogHandler(std::function<void(LogLevel, const QString&)> handler)
{
    qDebug() << "Setting custom log handler";
    QMutexLocker locker(&m_mutex);
    qDebug() << "Mutex locked for setting custom handler";
    m_customHandler = handler;
    qDebug() << "Custom log handler set";
}

QString Logger::formatLogMessage(LogLevel level, const QString& message, const QString& source) const
{
    qDebug() << "Formatting log message, level:" << levelToString(level) << ", message:" << message << ", source:" << source;
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

    qDebug() << "Formatted message:" << formattedMessage;
    return formattedMessage;
}

void Logger::writeToFile(const QString& message)
{
    qDebug() << "Writing to log file:" << message;
    QTextStream out(&m_logFile);
    out << message << "\n";
    out.flush();
    qDebug() << "Log file write complete";
}

QString Logger::levelToString(LogLevel level) const
{
    qDebug() << "Converting log level to string:" << static_cast<int>(level);
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
    qDebug() << "Global logMessage called with message:" << message;
    Logger::instance().info(message);
    qDebug() << "Global logMessage completed";
}
