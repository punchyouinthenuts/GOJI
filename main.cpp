#include "mainwindow.h"
#include "logger.h"
#include "configmanager.h"
#include "errormanager.h"
#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QStyleFactory>
#include <QDebug>

#ifndef VERSION
#define VERSION "0.9.968"
#endif

int main(int argc, char *argv[])
{
    qDebug() << "Starting GOJI application, Qt version:" << QT_VERSION_STR;
    qDebug() << "Current directory:" << QDir::currentPath();

    try {
        qDebug() << "Initializing logging...";
        // Initialize logging first for early diagnostics
        QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
        qDebug() << "Creating log directory:" << logDir;
        if (!QDir().mkpath(logDir)) {
            qDebug() << "Failed to create log directory:" << logDir;
            QMessageBox::critical(nullptr, "Startup Error", "Failed to create log directory: " + logDir);
            return 1;
        }
        QString logFilePath = logDir + "/goji_" +
                              QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".log";
        qDebug() << "Initializing Logger with file:" << logFilePath;
        if (!Logger::instance().initialize(logFilePath, true)) {
            qDebug() << "Logger initialization failed";
            QMessageBox::critical(nullptr, "Startup Error", "Failed to initialize logger with file: " + logFilePath);
            return 1;
        }
        LOG_INFO("Logger initialized");
        qDebug() << "Logger initialized successfully";

        qDebug() << "Initializing ConfigManager...";
        ConfigManager::instance().initialize("GojiApp", "Goji");
        LOG_INFO("ConfigManager initialized");
        qDebug() << "ConfigManager initialized successfully";

        qDebug() << "Setting ErrorManager log function...";
        ErrorManager::instance().setLogFunction([](const QString& msg) {
            Logger::instance().info(msg);
        });
        LOG_INFO("ErrorManager log function set");
        qDebug() << "ErrorManager log function set successfully";

        LOG_INFO("Starting application...");
        qDebug() << "Application startup logged";

        // Initialize QApplication early to ensure proper resource handling
        qDebug() << "Creating QApplication...";
        QApplication a(argc, argv);
        qDebug() << "QApplication created successfully";
        LOG_INFO("QApplication initialized");

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        qDebug() << "Setting HighDpiScaleFactorRoundingPolicy...";
        QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
        qDebug() << "HighDpiScaleFactorRoundingPolicy set";
#endif

        // Set application information
        qDebug() << "Setting application information...";
        QCoreApplication::setApplicationName("Goji");
        QCoreApplication::setOrganizationName("GojiApp");
        QCoreApplication::setApplicationVersion(VERSION);
        LOG_INFO("Application name and organization set");
        qDebug() << "Application information set successfully";

        // Set application-wide stylesheet
        qDebug() << "Setting stylesheet...";
        a.setStyleSheet(R"(
        QPushButton:disabled, QToolButton:disabled {
            background-color: #d3d3d3; /* Light grey background */
            color: #a9a9a9; /* Dark grey text */
            border: 1px solid #a9a9a9; /* Dark grey border */
        }
        QComboBox:disabled {
            background-color: #d3d3d3; /* Light grey background */
            color: #696969; /* Darker grey text */
            border: 1px solid #a9a9a9; /* Dark grey border */
        }
        )");
        LOG_INFO("Stylesheet set");
        qDebug() << "Stylesheet set successfully";

        // Create and show main window
        qDebug() << "Creating MainWindow...";
        MainWindow w;
        qDebug() << "MainWindow created successfully";
        LOG_INFO("MainWindow created");

        qDebug() << "Setting window icon...";
        w.setWindowIcon(QIcon(":/resources/icons/ShinGoji.ico"));
        LOG_INFO("Window icon set");
        qDebug() << "Window icon set successfully";

        qDebug() << "Showing MainWindow...";
        w.show();
        LOG_INFO("MainWindow shown");
        qDebug() << "MainWindow shown successfully";

        // Enter application event loop
        qDebug() << "Entering event loop...";
        int result = a.exec();
        LOG_INFO("Application exiting with code: " + QString::number(result));
        qDebug() << "Application exited with code:" << result;

        // Clean up resources
        qDebug() << "Cleaning up Logger...";
        Logger::instance().close();
        LOG_INFO("Logger closed");
        qDebug() << "Logger closed successfully";

        return result;
    }
    catch (const std::exception& e) {
        qDebug() << "Caught std::exception:" << e.what();
        qCritical() << "Critical error: " << e.what();
        if (Logger::instance().isInitialized()) {
            LOG_FATAL("FATAL ERROR: " + QString(e.what()));
        }

        QMessageBox::critical(nullptr, "Fatal Error",
                              QString("A fatal error occurred: %1").arg(e.what()));
        return 1;
    }
    catch (...) {
        qDebug() << "Caught unknown exception";
        qCritical() << "Unknown critical error occurred";
        if (Logger::instance().isInitialized()) {
            LOG_FATAL("FATAL ERROR: Unknown exception");
        }

        QMessageBox::critical(nullptr, "Fatal Error",
                              "An unknown fatal error occurred.");
        return 1;
    }
}
