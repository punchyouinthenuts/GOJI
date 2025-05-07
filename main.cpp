#include "mainwindow.h"
#include "logger.h"
#include "configmanager.h"
#include <QApplication>
#include <QIcon>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QStyleFactory>

#ifndef VERSION
#define VERSION "0.9.968"
#endif

int main(int argc, char *argv[])
{
    try {
        // Initialize logging first for early diagnostics
        QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
        QDir().mkpath(logDir);
        QString logFilePath = logDir + "/goji_" +
                              QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".log";
        Logger::instance().initialize(logFilePath, true);
        LOG_INFO("Starting application...");

        // Initialize QApplication early to ensure proper resource handling
        LOG_INFO("Initializing QApplication...");
        QApplication a(argc, argv);

        // Set application information
        LOG_INFO("Setting application name and organization...");
        QCoreApplication::setApplicationName("Goji");
        QCoreApplication::setOrganizationName("GojiApp");
        QCoreApplication::setApplicationVersion(VERSION);

        // Initialize configuration manager
        LOG_INFO("Initializing configuration...");
        ConfigManager::instance().initialize("GojiApp", "Goji");

        // Set application-wide stylesheet
        LOG_INFO("Setting stylesheet...");
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

        // Create and show main window
        LOG_INFO("Creating MainWindow...");
        MainWindow w;

        LOG_INFO("Setting window icon...");
        w.setWindowIcon(QIcon(":/resources/icons/ShinGoji.ico"));

        LOG_INFO("Showing MainWindow...");
        w.show();

        // Enter application event loop
        LOG_INFO("Entering event loop...");
        int result = a.exec();
        LOG_INFO("Application exiting with code: " + QString::number(result));

        // Clean up resources
        Logger::instance().close();

        return result;
    }
    catch (const std::exception& e) {
        qCritical() << "Critical error: " << e.what();
        if (Logger::instance().isInitialized()) {
            LOG_FATAL("FATAL ERROR: " + QString(e.what()));
        }

        QMessageBox::critical(nullptr, "Fatal Error",
                              QString("A fatal error occurred: %1").arg(e.what()));
        return 1;
    }
    catch (...) {
        qCritical() << "Unknown critical error occurred";
        if (Logger::instance().isInitialized()) {
            LOG_FATAL("FATAL ERROR: Unknown exception");
        }

        QMessageBox::critical(nullptr, "Fatal Error",
                              "An unknown fatal error occurred.");
        return 1;
    }
}
