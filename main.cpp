#include "mainwindow.h"
#include "logging.h"
#include <QApplication>
#include <QIcon>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>

// Declare global log file
QFile logFile;

// Implement the shared logMessage function
void logMessage(const QString& message) {
    qDebug() << message;
    if (logFile.isOpen()) {
        QTextStream out(&logFile);
        out << message << "\n";
        out.flush();
    }
}

int main(int argc, char *argv[])
{
    try {
        logMessage("Starting application...");
        logMessage("Initializing QApplication...");
        QApplication a(argc, argv);
        logMessage("QApplication initialized.");

        logMessage("Setting stylesheet...");
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
        logMessage("Stylesheet set.");

        logMessage("Setting application name...");
        QCoreApplication::setApplicationName("Goji");
        logMessage("Application name set.");

        // Setup log file
        logMessage("Setting up log file...");
        QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
        QDir().mkpath(logDir);
        QString logFilePath = logDir + "/goji_" +
                              QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".log";
        logFile.setFileName(logFilePath);
        if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qDebug() << "Could not open log file for writing:" << logFilePath;
        } else {
            logMessage("Log file opened successfully: " + logFilePath);
        }
        logMessage("Application started");

        logMessage("Creating MainWindow...");
        MainWindow w;
        logMessage("MainWindow created.");

        logMessage("Setting window icon...");
        w.setWindowIcon(QIcon(":/resources/icons/ShinGoji.ico"));
        logMessage("Window icon set.");

        logMessage("Showing MainWindow...");
        w.show();
        logMessage("MainWindow shown.");

        logMessage("Entering event loop...");
        int result = a.exec();
        logMessage("Application exiting with code: " + QString::number(result));

        // Close log file before exiting
        if (logFile.isOpen()) {
            logMessage("Closing log file...");
            logFile.close();
            logMessage("Log file closed.");
        }
        return result;
    }
    catch (const std::exception& e) {
        qCritical() << "Critical error: " << e.what();
        if (logFile.isOpen()) {
            QTextStream stream(&logFile);
            stream << "FATAL ERROR: " << e.what() << "\n";
            logFile.close();
        }
        QMessageBox::critical(nullptr, "Fatal Error",
                              QString("A fatal error occurred: %1").arg(e.what()));
        return 1;
    }
    catch (...) {
        qCritical() << "Unknown critical error occurred";
        if (logFile.isOpen()) {
            QTextStream stream(&logFile);
            stream << "FATAL ERROR: Unknown exception\n";
            logFile.close();
        }
        QMessageBox::critical(nullptr, "Fatal Error",
                              "An unknown fatal error occurred.");
        return 1;
    }
}
