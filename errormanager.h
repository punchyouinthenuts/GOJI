#ifndef ERRORMANAGER_H
#define ERRORMANAGER_H

#include "errorhandling.h"
#include <QObject>
#include <QMessageBox>
#include <QApplication>
#include <functional>
#include <memory>

/**
 * @brief Singleton class for centralized error handling
 *
 * This class provides a central point for handling and logging errors
 * throughout the application. It uses the exception types defined in
 * errorhandling.h and provides methods for reporting errors in a
 * consistent way.
 */
class ErrorManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the ErrorManager instance
     */
    static ErrorManager& instance();

    /**
     * @brief Handle an exception with appropriate UI feedback
     * @param e The exception to handle
     * @param parent The parent widget for dialog display
     * @param showDialog Whether to show a dialog or just log
     * @return True if handled, false if unhandled exception type
     */
    bool handleException(const std::exception& e, QWidget* parent = nullptr, bool showDialog = true);

    /**
     * @brief Handle a file operation error
     * @param message Error message
     * @param path File path that caused the error
     * @param parent The parent widget for dialog display
     * @param showDialog Whether to show a dialog or just log
     */
    void handleFileError(const QString& message, const QString& path = QString(),
                         QWidget* parent = nullptr, bool showDialog = true);

    /**
     * @brief Handle a database error
     * @param message Error message
     * @param query Query that caused the error (if applicable)
     * @param parent The parent widget for dialog display
     * @param showDialog Whether to show a dialog or just log
     */
    void handleDatabaseError(const QString& message, const QString& query = QString(),
                             QWidget* parent = nullptr, bool showDialog = true);

    /**
     * @brief Handle a network error
     * @param message Error message
     * @param errorCode Error code (if applicable)
     * @param parent The parent widget for dialog display
     * @param showDialog Whether to show a dialog or just log
     */
    void handleNetworkError(const QString& message, int errorCode = 0,
                            QWidget* parent = nullptr, bool showDialog = true);

    /**
     * @brief Handle a generic error
     * @param message Error message
     * @param title Dialog title
     * @param parent The parent widget for dialog display
     * @param showDialog Whether to show a dialog or just log
     */
    void handleError(const QString& message, const QString& title = "Error",
                     QWidget* parent = nullptr, bool showDialog = true);

    /**
     * @brief Execute code with exception handling
     * @param func Function to execute
     * @param parent Parent widget for error dialogs
     * @param showDialog Whether to show error dialogs
     * @return True if operation succeeded, false if it failed
     */
    template<typename Func>
    bool tryExec(Func func, QWidget* parent = nullptr, bool showDialog = true) {
        try {
            func();
            return true;
        }
        catch (const std::exception& e) {
            return handleException(e, parent, showDialog);
        }
        catch (...) {
            handleError("An unknown error occurred", "Unhandled Exception", parent, showDialog);
            return false;
        }
    }

    /**
     * @brief Set the global log function
     * @param logFunc Function that will be called for logging
     */
    void setLogFunction(std::function<void(const QString&)> logFunc);

signals:
    /**
     * @brief Signal emitted when an error occurs
     * @param message Error message
     * @param title Error category/title
     */
    void errorOccurred(const QString& message, const QString& title);

private:
    // Private constructor for singleton
    ErrorManager();
    ~ErrorManager();

    // Disable copy
    ErrorManager(const ErrorManager&) = delete;
    ErrorManager& operator=(const ErrorManager&) = delete;

    // Logging function
    std::function<void(const QString&)> m_logFunc;

    // Internal logging method
    void logMessage(const QString& message);
};

#endif // ERRORMANAGER_H
