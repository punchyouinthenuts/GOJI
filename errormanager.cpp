#include "errormanager.h"
#include <QDebug>

// Initialize static instance
ErrorManager& ErrorManager::instance()
{
    static ErrorManager instance;
    return instance;
}

ErrorManager::ErrorManager()
    : QObject(nullptr)
{
    // Default log function
    m_logFunc = [](const QString& message) {
        qDebug() << message;
    };
}

ErrorManager::~ErrorManager()
{
}

void ErrorManager::setLogFunction(std::function<void(const QString&)> logFunc)
{
    if (logFunc) {
        m_logFunc = logFunc;
    }
}

void ErrorManager::logMessage(const QString& message)
{
    if (m_logFunc) {
        m_logFunc(message);
    }
}

bool ErrorManager::handleException(const std::exception& e, QWidget* parent, bool showDialog)
{
    QString message;
    QString title = tr("Error");

    // Determine the exception type and format an appropriate message
    if (const auto* fileEx = dynamic_cast<const FileOperationException*>(&e)) {
        message = tr("File operation error: %1").arg(e.what());
        if (!fileEx->path().isEmpty()) {
            message += tr("\nPath: %1").arg(fileEx->path());
        }
        title = tr("File Error");
    }
    else if (const auto* dbEx = dynamic_cast<const DatabaseException*>(&e)) {
        message = tr("Database error: %1").arg(e.what());
        if (!dbEx->query().isEmpty()) {
            message += tr("\nQuery: %1").arg(dbEx->query());
        }
        title = tr("Database Error");
    }
    else if (const auto* netEx = dynamic_cast<const NetworkException*>(&e)) {
        message = tr("Network error: %1").arg(e.what());
        if (netEx->errorCode() != 0) {
            message += tr("\nError code: %1").arg(netEx->errorCode());
        }
        title = tr("Network Error");
    }
    else if (const auto* valEx = dynamic_cast<const ValidationException*>(&e)) {
        message = tr("Validation error: %1").arg(e.what());
        if (!valEx->field().isEmpty()) {
            message += tr("\nField: %1").arg(valEx->field());
        }
        title = tr("Validation Error");
    }
    else {
        message = tr("An error occurred: %1").arg(e.what());
    }

    // Log the error
    logMessage("ERROR: " + message);

    // Show dialog if requested
    if (showDialog && parent) {
        QMessageBox::critical(parent, title, message);
    }

    // Emit signal
    emit errorOccurred(message, title);

    return true;
}

void ErrorManager::handleFileError(const QString& message, const QString& path,
                                   QWidget* parent, bool showDialog)
{
    QString fullMessage = message;
    if (!path.isEmpty()) {
        fullMessage += tr("\nPath: %1").arg(path);
    }

    logMessage("FILE ERROR: " + fullMessage);

    if (showDialog && parent) {
        QMessageBox::critical(parent, tr("File Error"), fullMessage);
    }

    emit errorOccurred(fullMessage, tr("File Error"));
}

void ErrorManager::handleDatabaseError(const QString& message, const QString& query,
                                       QWidget* parent, bool showDialog)
{
    QString fullMessage = message;
    if (!query.isEmpty()) {
        fullMessage += tr("\nQuery: %1").arg(query);
    }

    logMessage("DATABASE ERROR: " + fullMessage);

    if (showDialog && parent) {
        QMessageBox::critical(parent, tr("Database Error"), fullMessage);
    }

    emit errorOccurred(fullMessage, tr("Database Error"));
}

void ErrorManager::handleNetworkError(const QString& message, int errorCode,
                                      QWidget* parent, bool showDialog)
{
    QString fullMessage = message;
    if (errorCode != 0) {
        fullMessage += tr("\nError code: %1").arg(errorCode);
    }

    logMessage("NETWORK ERROR: " + fullMessage);

    if (showDialog && parent) {
        QMessageBox::critical(parent, tr("Network Error"), fullMessage);
    }

    emit errorOccurred(fullMessage, tr("Network Error"));
}

void ErrorManager::handleError(const QString& message, const QString& title,
                               QWidget* parent, bool showDialog)
{
    logMessage("ERROR [" + title + "]: " + message);

    if (showDialog && parent) {
        QMessageBox::critical(parent, title, message);
    }

    emit errorOccurred(message, title);
}
