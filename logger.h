#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QDateTime>
#include <QMutex>
#include <functional>
#include <memory>

/**
 * @brief Log level enumeration
 */
enum class LogLevel {
    Debug,   ///< Debug information
    Info,    ///< General information
    Warning, ///< Warnings that don't prevent operation
    Error,   ///< Errors that may prevent operation
    Fatal    ///< Critical errors that cause application failure
};

/**
 * @brief Singleton class for centralized logging
 *
 * This class provides a centralized logging facility that can:
 * - Write logs to a file
 * - Output logs to the console
 * - Emit signals for log messages that can be displayed in the UI
 * - Format log messages with timestamps and log levels
 */
class Logger : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the Logger instance
     */
    static Logger& instance();

    /**
     * @brief Initialize the logger with a log file
     * @param logFilePath Path to the log file
     * @param logToConsole Whether to also log to console
     * @return True if initialization succeeded
     */
    bool initialize(const QString& logFilePath, bool logToConsole = true);

    /**
     * @brief Log a debug message
     * @param message The message to log
     * @param source Optional source information (class/function)
     */
    void debug(const QString& message, const QString& source = QString());

    /**
     * @brief Log an informational message
     * @param message The message to log
     * @param source Optional source information (class/function)
     */
    void info(const QString& message, const QString& source = QString());

    /**
     * @brief Log a warning message
     * @param message The message to log
     * @param source Optional source information (class/function)
     */
    void warning(const QString& message, const QString& source = QString());

    /**
     * @brief Log an error message
     * @param message The message to log
     * @param source Optional source information (class/function)
     */
    void error(const QString& message, const QString& source = QString());

    /**
     * @brief Log a fatal error message
     * @param message The message to log
     * @param source Optional source information (class/function)
     */
    void fatal(const QString& message, const QString& source = QString());

    /**
     * @brief Log a message with a specified log level
     * @param level The log level
     * @param message The message to log
     * @param source Optional source information (class/function)
     */
    void log(LogLevel level, const QString& message, const QString& source = QString());

    /**
     * @brief Close the log file
     */
    void close();

    /**
     * @brief Check if the logger is initialized
     * @return True if initialized
     */
    bool isInitialized() const;

    /**
     * @brief Set custom log handler function
     * @param handler Function that receives formatted log messages
     */
    void setCustomLogHandler(std::function<void(LogLevel, const QString&)> handler);

signals:
    /**
     * @brief Signal emitted when a log message is generated
     * @param level The log level
     * @param message The formatted log message
     */
    void messageLogged(LogLevel level, const QString& message);

private:
    // Private constructor for singleton
    Logger();
    ~Logger();

    // Disable copy
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Log file handling
    QFile m_logFile;
    bool m_logToConsole;
    bool m_initialized;

    // Custom log handler
    std::function<void(LogLevel, const QString&)> m_customHandler;

    // Thread safety
    QMutex m_mutex;

    // Helper methods
    QString formatLogMessage(LogLevel level, const QString& message, const QString& source = QString()) const;
    void writeToFile(const QString& message);
    QString levelToString(LogLevel level) const;
};

// Convenience logging macros
#define LOG_DEBUG(msg) Logger::instance().debug(msg, __FUNCTION__)
#define LOG_INFO(msg) Logger::instance().info(msg, __FUNCTION__)
#define LOG_WARNING(msg) Logger::instance().warning(msg, __FUNCTION__)
#define LOG_ERROR(msg) Logger::instance().error(msg, __FUNCTION__)
#define LOG_FATAL(msg) Logger::instance().fatal(msg, __FUNCTION__)

// Global function for backward compatibility
void logMessage(const QString& message);

#endif // LOGGER_H
