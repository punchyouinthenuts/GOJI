#include <QApplication>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSqlDatabase>

#include "mainwindow.h"
#include "databasemanager.h"

// Global log file for application-wide logging
QFile logFile;

void setupLogFile()
{
    // Create logs directory if it doesn't exist
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Set up log file with date-based naming
    QString logFileName = QString("%1/goji_%2.log")
                              .arg(logDir)
                              .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));

    logFile.setFileName(logFileName);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        qDebug() << "Log file opened:" << logFileName;
        // Flush to ensure file handle is stable
        logFile.flush();
    } else {
        qDebug() << "Failed to open log file:" << logFileName;
        qDebug() << "Error:" << logFile.errorString();
        // Continue without file logging - use console only
    }
}

void messageHandler(QtMsgType type, const QMessageLogContext& /*context*/, const QString& msg)
{
    // Format the message based on type
    QString logMessage;
    switch (type) {
    case QtDebugMsg:
        logMessage = QString("[DEBUG] %1").arg(msg);
        break;
    case QtInfoMsg:
        logMessage = QString("[INFO] %1").arg(msg);
        break;
    case QtWarningMsg:
        logMessage = QString("[WARNING] %1").arg(msg);
        break;
    case QtCriticalMsg:
        logMessage = QString("[CRITICAL] %1").arg(msg);
        break;
    case QtFatalMsg:
        logMessage = QString("[FATAL] %1").arg(msg);
        break;
    }

    // Add timestamp
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    logMessage = QString("[%1] %2").arg(timestamp, logMessage);

    // Write to log file if open
    if (logFile.isOpen()) {
        QTextStream stream(&logFile);
        stream << logMessage << Qt::endl;
        stream.flush(); // Ensure immediate write
    }

    // Output to console as well
    fprintf(stderr, "%s\n", qPrintable(logMessage));
}

int main(int argc, char *argv[])
{
    // Set up logging before anything else
    setupLogFile();
    qInstallMessageHandler(messageHandler);

    qDebug() << "Starting GOJI application...";

    // Create the application
    QApplication app(argc, argv);
    app.setApplicationName("GOJI");
    app.setOrganizationName("Yourorganization");
    app.setOrganizationDomain("yourdomain.com");

    try {
        // Check available SQL drivers
        qDebug() << "Available SQL drivers:";
        QStringList drivers = QSqlDatabase::drivers();
        for (const QString& driver : drivers) {
            qDebug() << "  " << driver;
        }

        // Check specifically for SQLite
        if (!drivers.contains("QSQLITE")) {
            qCritical() << "SQLite driver is not available. Make sure Qt SQL drivers are properly installed.";
            QMessageBox::critical(nullptr, "Database Error",
                                  "SQLite driver is not available. The application cannot run without database support.");
            return 1;
        }

        // Initialize database system
        QString dbPath = QDir::toNativeSeparators("C:/Goji/database/goji.db");
        qDebug() << "Initializing database at:" << dbPath;

        // Ensure directory exists
        QFileInfo fileInfo(dbPath);
        QDir dir = fileInfo.dir();
        if (!dir.exists()) {
            qDebug() << "Creating database directory at:" << dir.path();
            if (!dir.mkpath(".")) {
                qCritical() << "Failed to create database directory at:" << dir.path();
                QMessageBox::critical(nullptr, "Database Error",
                                      "Failed to create database directory. Check permissions.");
                return 1;
            }
            qDebug() << "Successfully created database directory:" << dir.path();
        } else {
            qDebug() << "Database directory exists:" << dir.path();
        }

        // Test write permissions
        QFile testFile(dir.filePath("test_write.tmp"));
        if (!testFile.open(QIODevice::WriteOnly)) {
            qCritical() << "No write permission in database directory:" << dir.path();
            QMessageBox::critical(nullptr, "Database Error",
                                  "Cannot write to database directory. Check permissions.");
            return 1;
        }
        testFile.close();
        testFile.remove();
        qDebug() << "Write permissions verified for database directory";

        // Initialize database with single, reliable approach
        qDebug() << "Initializing database with standard approach";
        if (!DatabaseManager::instance()->initialize(dbPath)) {
            qCritical() << "Standard database initialization failed";
            throw std::runtime_error("Failed to initialize database with standard approach");
        }

        qDebug() << "Database initialization successful";

        // Create and show the main window
        MainWindow mainWindow;
        mainWindow.show();

        qDebug() << "Main window created and shown";
        qDebug() << "Entering application event loop";

        return app.exec();
    }
    catch (const std::exception& e) {
        qCritical() << "Fatal error:" << e.what();
        QMessageBox::critical(nullptr, "Fatal Error",
                              QString("A fatal error occurred while starting the application:\n\n%1").arg(e.what()));
        return 1;
    }
    catch (...) {
        qCritical() << "Unknown fatal error occurred";
        QMessageBox::critical(nullptr, "Fatal Error",
                              "An unknown fatal error occurred while starting the application.");
        return 1;
    }
}
