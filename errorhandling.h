#ifndef ERRORHANDLING_H
#define ERRORHANDLING_H

#include <QString>
#include <QDebug>
#include <QObject>
#include <QMessageBox>
#include <QApplication>
#include <QWidget>
#include <QFile>
#include <QDateTime>
#include <exception>
#include <stdexcept>

// Custom exception classes for specific error types
class FileOperationException : public std::runtime_error {
private:
    QString m_path;

public:
    FileOperationException(const QString& message, const QString& path = QString())
        : std::runtime_error(message.toStdString()), m_path(path) {}

    const QString& path() const { return m_path; }
};

class DatabaseException : public std::runtime_error {
private:
    QString m_query;

public:
    DatabaseException(const QString& message, const QString& query = QString())
        : std::runtime_error(message.toStdString()), m_query(query) {}

    const QString& query() const { return m_query; }
};

class NetworkException : public std::runtime_error {
private:
    int m_errorCode;

public:
    NetworkException(const QString& message, int errorCode = 0)
        : std::runtime_error(message.toStdString()), m_errorCode(errorCode) {}

    int errorCode() const { return m_errorCode; }
};

class ValidationException : public std::runtime_error {
private:
    QString m_field;

public:
    ValidationException(const QString& message, const QString& field = QString())
        : std::runtime_error(message.toStdString()), m_field(field) {}

    const QString& field() const { return m_field; }
};

// Standardized error logging macros
#define LOG_ERROR(msg) qCritical() << "ERROR:" << (msg)
#define LOG_WARNING(msg) qWarning() << "WARNING:" << (msg)
#define LOG_INFO(msg) qInfo() << "INFO:" << (msg)
#define LOG_DEBUG(msg) qDebug() << "DEBUG:" << (msg)

// Log and throw exceptions
#define THROW_FILE_ERROR(msg, path) \
do { \
        QString error = QString("%1 (%2:%3)").arg(msg).arg(__FILE__).arg(__LINE__); \
        LOG_ERROR(QString("%1 - Path: %2").arg(error, path)); \
        throw FileOperationException(error, path); \
} while (0)

#define THROW_DB_ERROR(msg, query) \
    do { \
        QString error = QString("%1 (%2:%3)").arg(msg).arg(__FILE__).arg(__LINE__); \
        LOG_ERROR(QString("%1 - Query: %2").arg(error, query)); \
        throw DatabaseException(error, query); \
} while (0)

#define THROW_NETWORK_ERROR(msg, code) \
    do { \
            QString error = QString("%1 (%2:%3)").arg(msg).arg(__FILE__).arg(__LINE__); \
            LOG_ERROR(QString("%1 - Code: %2").arg(error).arg(code)); \
            throw NetworkException(error, code); \
    } while (0)

#define THROW_VALIDATION_ERROR(msg, field) \
        do { \
            QString error = QString("%1 (%2:%3)").arg(msg).arg(__FILE__).arg(__LINE__); \
            LOG_ERROR(QString("%1 - Field: %2").arg(error, field)); \
            throw ValidationException(error, field); \
    } while (0)

    // Generic error handling utility class
    class ErrorHandler : public QObject {
        Q_OBJECT

    public:
        explicit ErrorHandler(QObject* parent = nullptr) : QObject(parent) {}

        /**
     * @brief Handle an exception and display appropriate UI feedback
     * @param e The exception to handle
     * @param parent The parent widget for any dialog
     * @param showDialog Whether to show an error dialog
     * @return True if handled, false otherwise
     */
        bool handleException(const std::exception& e, QWidget* parent = nullptr, bool showDialog = true) {
            QString message;
            QString title = tr("Error");

            if (const auto* fileEx = dynamic_cast<const FileOperationException*>(&e)) {
                message = tr("File operation error: %1").arg(e.what());
                if (!fileEx->path().isEmpty()) {
                    message += tr("\nPath: %1").arg(fileEx->path());
                }
                title = tr("File Error");
            } else if (const auto* dbEx = dynamic_cast<const DatabaseException*>(&e)) {
                message = tr("Database error: %1").arg(e.what());
                title = tr("Database Error");
            } else if (const auto* netEx = dynamic_cast<const NetworkException*>(&e)) {
                message = tr("Network error: %1").arg(e.what());
                if (netEx->errorCode() != 0) {
                    message += tr("\nError code: %1").arg(netEx->errorCode());
                }
                title = tr("Network Error");
            } else if (const auto* valEx = dynamic_cast<const ValidationException*>(&e)) {
                message = tr("Validation error: %1").arg(e.what());
                if (!valEx->field().isEmpty()) {
                    message += tr("\nField: %1").arg(valEx->field());
                }
                title = tr("Validation Error");
            } else {
                message = tr("An error occurred: %1").arg(e.what());
            }

            // Log the error
            LOG_ERROR(message);

            // Show dialog if requested
            if (showDialog && parent) {
                QMessageBox::critical(parent, title, message);
            }

            // Emit signal
            emit errorOccurred(message, title);

            return true;
        }

        /**
     * @brief Handle a generic error with a message
     * @param message The error message
     * @param parent The parent widget for any dialog
     * @param showDialog Whether to show an error dialog
     * @return True if handled, false otherwise
     */
        bool handleError(const QString& message, QWidget* parent = nullptr, bool showDialog = true) {
            // Log the error
            LOG_ERROR(message);

            // Show dialog if requested
            if (showDialog && parent) {
                QMessageBox::critical(parent, tr("Error"), message);
            }

            // Emit signal
            emit errorOccurred(message, tr("Error"));

            return true;
        }

        /**
     * @brief Execute a function with try/catch error handling
     * @param func The function to execute
     * @param parent The parent widget for any error dialog
     * @param showDialog Whether to show an error dialog
     * @return True if successful, false if an exception was thrown
     */
        template<typename Func>
        bool tryExec(Func func, QWidget* parent = nullptr, bool showDialog = true) {
            try {
                func();
                return true;
            } catch (const std::exception& e) {
                return handleException(e, parent, showDialog);
            } catch (...) {
                QString message = tr("An unknown error occurred");
                LOG_ERROR(message);

                if (showDialog && parent) {
                    QMessageBox::critical(parent, tr("Unknown Error"), message);
                }

                emit errorOccurred(message, tr("Unknown Error"));
                return false;
            }
        }

    signals:
        void errorOccurred(const QString& message, const QString& title);
    };

#endif // ERRORHANDLING_H
