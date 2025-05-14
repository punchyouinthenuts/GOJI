// Standard library includes
#include <algorithm>
#include <cfloat>   // For DBL_MAX, FLT_MAX, etc.
#include <climits>  // For INT_MAX, INT_MIN, etc.
#include <stdexcept> // For std::exception, std::runtime_error

// Include the mainwindow.h first
#include "mainwindow.h"

// Qt base includes - ensure these are included in proper order
#include <QtCore>
#include <QtGui>
#include <QtWidgets>

// Specific Qt includes
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QLocale>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QWidget>
#include <QtConcurrent>
#include <QDebug>

// Custom includes
#include "configmanager.h"
#include "errormanager.h"
#include "filelocationsdialog.h"
#include "fileutils.h"
#include "logger.h"
#include "ui_GOJI.h"
#include "updatedialog.h"
#include "updatesettingsdialog.h"
#include "validator.h"
#include "tmweeklypccontroller.h"  // Add this for new controller
#include "databasemanager.h"

// Use version defined in GOJI.pro
#ifdef APP_VERSION
const QString VERSION = QString(APP_VERSION);
#else
const QString VERSION = "1.0.0";
#endif

// Reference the global logFile from main.cpp
extern QFile logFile;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_settings(nullptr),
    openJobMenu(nullptr),
    m_printWatcher(nullptr),
    m_inactivityTimer(nullptr)
{
    qDebug() << "Entering MainWindow constructor";
    Logger::instance().info("Entering MainWindow constructor...");

    try {
        // Setup UI first - this is critical for safe initialization
        ui->setupUi(this);
        setWindowTitle(tr("Goji v%1").arg(VERSION));
        qDebug() << "UI setup complete";
        Logger::instance().info("UI setup complete.");

        // Initialize QSettings
        qDebug() << "Initializing QSettings";
        Logger::instance().info("Initializing QSettings...");

        try {
            // Use ConfigManager but keep m_settings pointer for compatibility
            ConfigManager& config = ConfigManager::instance();
            m_settings = config.getSettings();

            if (!m_settings) {
                qDebug() << "ConfigManager returned null settings, creating manually";
                m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope,
                                           "GojiApp", "Goji");
            }

            qDebug() << "QSettings instance obtained";

            // Set default values if needed
            if (!m_settings->contains("UpdateServerUrl")) {
                m_settings->setValue("UpdateServerUrl", "https://goji-updates.s3.amazonaws.com");
            }
            if (!m_settings->contains("UpdateInfoFile")) {
                m_settings->setValue("UpdateInfoFile", "latest.json");
            }
            if (!m_settings->contains("AwsCredentialsPath")) {
                m_settings->setValue("AwsCredentialsPath",
                                     QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/aws_credentials.json");
            }
            qDebug() << "QSettings defaults set";
            Logger::instance().info("QSettings initialized.");
        }
        catch (const std::exception& e) {
            qDebug() << "Exception initializing settings:" << e.what();
            // Create QSettings directly as fallback
            m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope,
                                       "GojiApp", "Goji");
            Logger::instance().info("Created QSettings directly due to error.");
        }

        // Initialize database manager
        qDebug() << "Setting up database directory";
        Logger::instance().info("Setting up database directory...");
        QString defaultDbDirPath;
#ifdef QT_DEBUG
        defaultDbDirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Goji/SQL/debug";
#else
        defaultDbDirPath = "C:/Goji/database";
#endif
        QString dbDirPath = m_settings->value("DatabasePath", defaultDbDirPath).toString();
        QDir dbDir(dbDirPath);
        if (!dbDir.exists()) {
            qDebug() << "Creating database directory:" << dbDirPath;
            Logger::instance().info("Creating database directory: " + dbDirPath);
            if (!dbDir.mkpath(".")) {
                qDebug() << "Failed to create database directory:" << dbDirPath;
                Logger::instance().info("Failed to create database directory: " + dbDirPath);
                throw std::runtime_error("Failed to create database directory");
            }
        }
        QString dbPath = dbDirPath + "/jobs.db";
        qDebug() << "Database directory setup complete:" << dbPath;
        Logger::instance().info("Database directory setup complete: " + dbPath);

        qDebug() << "Initializing DatabaseManager";
        Logger::instance().info("Initializing DatabaseManager...");

        // Get the DatabaseManager singleton instance and initialize it
        m_dbManager = DatabaseManager::instance();
        if (!m_dbManager->initialize(dbPath)) {
            qDebug() << "Failed to initialize database";
            Logger::instance().info("Failed to initialize database.");
            throw std::runtime_error("Failed to initialize database");
        }
        qDebug() << "DatabaseManager initialized";
        Logger::instance().info("DatabaseManager initialized.");

        qDebug() << "Creating managers and controllers";
        Logger::instance().info("Creating managers and controllers...");
        m_fileManager = new FileSystemManager(m_settings);
        m_scriptRunner = new ScriptRunner(this);
        m_updateManager = new UpdateManager(m_settings, this);

        // Create TM WEEKLY PC controller
        m_tmWeeklyPCController = new TMWeeklyPCController(this);

        qDebug() << "Managers and controllers created";
        Logger::instance().info("Managers and controllers created.");

        // Connect UpdateManager signals...
        qDebug() << "Connecting UpdateManager signals";
        Logger::instance().info("Connecting UpdateManager signals...");
        connect(m_updateManager, &UpdateManager::logMessage, this, &MainWindow::logToTerminal);
        connect(m_updateManager, &UpdateManager::updateDownloadProgress, this,
                [this](qint64 bytesReceived, qint64 bytesTotal) {
                    double percentage = (bytesTotal > 0) ? (bytesReceived * 100.0 / bytesTotal) : 0;
                    logToTerminal(tr("Downloading update: %1%").arg(percentage, 0, 'f', 1));
                });
        connect(m_updateManager, &UpdateManager::updateDownloadFinished, this,
                [this](bool success) {
                    logToTerminal(success ? "Update downloaded successfully." : "Update download failed.");
                });
        connect(m_updateManager, &UpdateManager::updateInstallFinished, this,
                [this](bool success) {
                    logToTerminal(success ? "Update installation initiated. Application will restart." : "Update installation failed.");
                    Logger::instance().info(success ? "Update installation initiated." : "Update installation failed.");
                });
        connect(m_updateManager, &UpdateManager::errorOccurred, this,
                [this](const QString& error) {
                    logToTerminal(tr("Update error: %1").arg(error));
                });
        qDebug() << "UpdateManager signals connected";
        Logger::instance().info("UpdateManager signals connected.");

        qDebug() << "Checking for updates";
        Logger::instance().info("Checking for updates...");
        bool checkUpdatesOnStartup = m_settings->value("Updates/CheckOnStartup", true).toBool();
        if (checkUpdatesOnStartup) {
            QDateTime lastCheck = m_settings->value("Updates/LastCheckTime").toDateTime();
            QDateTime currentTime = QDateTime::currentDateTime();
            int checkInterval = m_settings->value("Updates/CheckIntervalDays", 1).toInt();
            if (!lastCheck.isValid() || lastCheck.daysTo(currentTime) >= checkInterval) {
                qDebug() << "Scheduling update check";
                QTimer::singleShot(5000, this, [this]() {
                    logToTerminal(tr("Checking updates from %1/%2").arg(
                        m_settings->value("UpdateServerUrl").toString(),
                        m_settings->value("UpdateInfoFile").toString()));
                    m_updateManager->checkForUpdates(true);
                    connect(m_updateManager, &UpdateManager::updateCheckFinished, this,
                            [this](bool available) {
                                if (available) {
                                    logToTerminal("Update available. Showing update dialog.");
                                    UpdateDialog* updateDialog = new UpdateDialog(m_updateManager, this);
                                    updateDialog->setAttribute(Qt::WA_DeleteOnClose);
                                    updateDialog->show();
                                } else {
                                    logToTerminal("No updates available.");
                                }
                                m_settings->setValue("Updates/LastCheckTime", QDateTime::currentDateTime());
                            }, Qt::SingleShotConnection);
                });
            }
        }
        qDebug() << "Update check setup complete";
        Logger::instance().info("Update check setup complete.");

        qDebug() << "Setting up UI elements";
        Logger::instance().info("Setting up UI elements...");
        setupUi();
        setupSignalSlots();
        setupMenus();
        initWatchersAndTimers();
        qDebug() << "UI elements setup complete";
        Logger::instance().info("UI elements setup complete.");

        qDebug() << "Logging startup";
        Logger::instance().info("Logging startup...");
        logToTerminal(tr("Goji started: %1").arg(QDateTime::currentDateTime().toString()));
        qDebug() << "MainWindow constructor finished";
        Logger::instance().info("MainWindow constructor finished.");
    }
    catch (const std::exception& e) {
        qDebug() << "Critical error in MainWindow constructor:" << e.what();
        Logger::instance().info(QString("Critical error in MainWindow constructor: %1").arg(e.what()));
        QMessageBox::critical(this, "Startup Error",
                              QString("A critical error occurred during application startup: %1").arg(e.what()));
        throw; // Re-throw to be handled by main()
    }
    catch (...) {
        qDebug() << "Unknown critical error in MainWindow constructor";
        Logger::instance().info("Unknown critical error in MainWindow constructor");
        QMessageBox::critical(this, "Startup Error",
                              "An unknown critical error occurred during application startup");
        throw; // Re-throw to be handled by main()
    }
}

MainWindow::~MainWindow()
{
    qDebug() << "MainWindow destruction starting...";

    delete ui;
    // Don't delete m_dbManager as it's a singleton
    delete m_fileManager;
    delete m_scriptRunner;
    delete m_updateManager;
    delete m_tmWeeklyPCController;
    delete openJobMenu;
    delete m_printWatcher;
    delete m_inactivityTimer;

    qDebug() << "MainWindow destruction complete";
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    Logger::instance().info("Handling close event...");
    // Implement any cleanup needed before closing
    event->accept();
}

void MainWindow::setupUi()
{
    Logger::instance().info("Setting up UI elements...");

    // Initialize TM WEEKLY PC controller with UI elements
    m_tmWeeklyPCController->initializeUI(
        ui->runInitialTMWPC,
        ui->openBulkMailerTMWPC,
        ui->runProofDataTMWPC,
        ui->openProofFilesTMWPC,
        ui->runWeeklyMergedTMWPC,
        ui->openPrintFilesTMWPC,
        ui->runPostPrintTMWPC,
        ui->lockButtonTMWPC,
        ui->editButtonTMWPC,
        ui->postageLockTMWPC,
        ui->proofDDboxTMWPC,
        ui->printDDboxTMWPC,
        ui->yearDDboxTMWPC,
        ui->monthDDboxTMWPC,
        ui->weekDDboxTMWPC,
        ui->classDDboxTMWPC,
        ui->permitDDboxTMWPC,
        ui->jobNumberBoxTMWPC,
        ui->postageBoxTMWPC,
        ui->countBoxTMWPC,
        ui->terminalWindowTMWPC,
        ui->trackerTMWPC
        );

    Logger::instance().info("UI elements setup complete.");
}

void MainWindow::setupMenus()
{
    Logger::instance().info("Setting up menus...");

    // Setup File menu
    openJobMenu = new QMenu(tr("Open File"));
    ui->menuFile->insertMenu(ui->actionSave_Job, openJobMenu);

    // Setup Settings menu
    QMenu* settingsMenu = ui->menubar->addMenu(tr("Settings"));
    QAction* updateSettingsAction = new QAction(tr("Update Settings"));
    connect(updateSettingsAction, &QAction::triggered, this, &MainWindow::onUpdateSettingsTriggered);
    settingsMenu->addAction(updateSettingsAction);

    // Setup Script Management menu
    QMenu* manageScriptsMenu = ui->menuInput->findChild<QMenu*>("menuManage_Scripts");
    if (manageScriptsMenu) {
        manageScriptsMenu->clear();
        QMap<QString, QVariant> scriptDirs;

        scriptDirs["Trachmar"] = QVariant::fromValue(QMap<QString, QString>{
            {"Weekly PC", "C:/Goji/Scripts/TRACHMAR/WEEKLY PC"},
            {"Weekly Packets/IDO", "C:/Goji/Scripts/TRACHMAR/WEEKLY PACKET & IDO"},
            {"Term", "C:/Goji/Scripts/TRACHMAR/TERM"}
        });

        for (auto it = scriptDirs.constBegin(); it != scriptDirs.constEnd(); ++it) {
            QMenu* parentMenu = manageScriptsMenu->addMenu(it.key());
            const auto& subdirs = it.value().value<QMap<QString, QString>>();
            for (auto subIt = subdirs.constBegin(); subIt != subdirs.constEnd(); ++subIt) {
                QMenu* subMenu = parentMenu->addMenu(subIt.key());
                populateScriptMenu(subMenu, subIt.value());
            }
        }
    }

    // Connect tab change handler
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    Logger::instance().info("Menus setup complete.");
}

void MainWindow::setupSignalSlots()
{
    Logger::instance().info("Setting up signal slots...");

    // Menu connections
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::onActionExitTriggered);
    connect(ui->actionCheck_for_updates, &QAction::triggered, this, &MainWindow::onCheckForUpdatesTriggered);

    Logger::instance().info("Signal slots setup complete.");
}

void MainWindow::initWatchersAndTimers()
{
    Logger::instance().info("Initializing watchers and timers...");

    // File system watcher for print directory
    m_printWatcher = new QFileSystemWatcher();
    QString printPath = m_settings->value("PrintPath", QCoreApplication::applicationDirPath() + "/Output").toString();
    if (QDir(printPath).exists()) {
        m_printWatcher->addPath(printPath);
        logToTerminal(tr("Watching print directory: %1").arg(printPath));
    } else {
        logToTerminal(tr("Print directory not found: %1").arg(printPath));
    }
    connect(m_printWatcher, &QFileSystemWatcher::directoryChanged, this, &MainWindow::onPrintDirChanged);

    // Inactivity timer for auto-save
    m_inactivityTimer = new QTimer();
    m_inactivityTimer->setInterval(300000); // 5 minutes
    m_inactivityTimer->setSingleShot(false);
    connect(m_inactivityTimer, &QTimer::timeout, this, &MainWindow::onInactivityTimeout);
    m_inactivityTimer->start();
    logToTerminal(tr("Inactivity timer started (5 minutes)."));

    Logger::instance().info("Watchers and timers initialized.");
}

void MainWindow::onTabChanged(int index)
{
    QString tabName = ui->tabWidget->tabText(index);
    logToTerminal("Switched to tab: " + tabName);

    // Enable/disable menu items based on active tab
    ui->menuFile->actions();
}

void MainWindow::onPrintDirChanged(const QString &path)
{
    logToTerminal(tr("Print directory changed: %1").arg(path));
}

void MainWindow::onInactivityTimeout()
{
    // Auto-save functionality
    logToTerminal("Auto-save triggered due to inactivity.");
}

void MainWindow::onActionExitTriggered()
{
    Logger::instance().info("Exit action triggered.");
    close();
}

void MainWindow::onCheckForUpdatesTriggered()
{
    Logger::instance().info("Check for updates triggered.");
    logToTerminal(tr("Checking for updates..."));

    ui->actionCheck_for_updates->setEnabled(false);

    m_updateManager->checkForUpdates(false);

    connect(m_updateManager, &UpdateManager::updateCheckFinished, this,
            [this](bool available) {
                if (available) {
                    UpdateDialog* updateDialog = new UpdateDialog(m_updateManager, this);
                    updateDialog->setAttribute(Qt::WA_DeleteOnClose);
                    updateDialog->show();
                } else {
                    QMessageBox::information(this, tr("No Updates"), tr("No updates are available."));
                }
                ui->actionCheck_for_updates->setEnabled(true);
                logToTerminal(tr("Update check completed."));
            }, Qt::SingleShotConnection);

    connect(m_updateManager, &UpdateManager::errorOccurred, this,
            [this](const QString& error) {
                logToTerminal(tr("Update check failed: %1").arg(error));
                QMessageBox::warning(this, tr("Update Error"), tr("Failed to check for updates: %1").arg(error));
                ui->actionCheck_for_updates->setEnabled(true);
                logToTerminal(tr("Update check completed with error."));
            }, Qt::SingleShotConnection);
}

void MainWindow::onUpdateSettingsTriggered()
{
    Logger::instance().info("Update settings triggered.");
    UpdateSettingsDialog dialog(m_settings, this);
    dialog.exec();
    logToTerminal(tr("Update settings updated."));
}

void MainWindow::populateScriptMenu(QMenu* menu, const QString& dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        QAction* notFoundAction = new QAction(tr("Directory not found"), this);
        notFoundAction->setEnabled(false);
        menu->addAction(notFoundAction);
        return;
    }

    // Get lists of files by type
    QStringList batFiles = dir.entryList(QStringList() << "*.bat", QDir::Files, QDir::Name);
    QStringList pyFiles = dir.entryList(QStringList() << "*.py", QDir::Files, QDir::Name);
    QStringList psFiles = dir.entryList(QStringList() << "*.ps1", QDir::Files, QDir::Name);
    QStringList rFiles = dir.entryList(QStringList() << "*.r" << "*.R", QDir::Files, QDir::Name);

    if (batFiles.isEmpty() && pyFiles.isEmpty() && psFiles.isEmpty() && rFiles.isEmpty()) {
        QAction* noScriptsAction = new QAction(tr("No scripts found"), this);
        noScriptsAction->setEnabled(false);
        menu->addAction(noScriptsAction);
        return;
    }

    // Add batch files
    if (!batFiles.isEmpty()) {
        QMenu* batMenu = menu->addMenu("Batch Scripts");
        for (const QString& file : batFiles) {
            QAction* fileAction = new QAction(file, this);
            connect(fileAction, &QAction::triggered, this, [=]() {
                openScriptFile(dirPath + "/" + file);
            });
            batMenu->addAction(fileAction);
        }
    }

    // Add Python files
    if (!pyFiles.isEmpty()) {
        QMenu* pyMenu = menu->addMenu("Python Scripts");
        for (const QString& file : pyFiles) {
            QAction* fileAction = new QAction(file, this);
            connect(fileAction, &QAction::triggered, this, [=]() {
                openScriptFile(dirPath + "/" + file);
            });
            pyMenu->addAction(fileAction);
        }
    }

    // Add PowerShell files
    if (!psFiles.isEmpty()) {
        QMenu* psMenu = menu->addMenu("PowerShell Scripts");
        for (const QString& file : psFiles) {
            QAction* fileAction = new QAction(file, this);
            connect(fileAction, &QAction::triggered, this, [=]() {
                openScriptFile(dirPath + "/" + file);
            });
            psMenu->addAction(fileAction);
        }
    }

    // Add R files
    if (!rFiles.isEmpty()) {
        QMenu* rMenu = menu->addMenu("R Scripts");
        for (const QString& file : rFiles) {
            QAction* fileAction = new QAction(file, this);
            connect(fileAction, &QAction::triggered, this, [=]() {
                openScriptFile(dirPath + "/" + file);
            });
            rMenu->addAction(fileAction);
        }
    }
}

void MainWindow::openScriptFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        QMessageBox::warning(this, tr("File Not Found"),
                             tr("The script file does not exist: %1").arg(filePath));
        return;
    }

    QString ext = fileInfo.suffix().toLower();

    if (ext == "bat" || ext == "cmd") {
        m_scriptRunner->runScript(filePath, QStringList());
    }
    else if (ext == "py") {
        m_scriptRunner->runScript("python", QStringList() << filePath);
    }
    else if (ext == "ps1") {
        QStringList args;
        args << "-ExecutionPolicy" << "Bypass"
             << "-File" << filePath;
        m_scriptRunner->runScript("powershell", args);
    }
    else if (ext == "r" || ext == "R") {
        QString rscriptPath = "C:/Program Files/R/R-4.4.2/bin/Rscript.exe";
        m_scriptRunner->runScript(rscriptPath, QStringList() << filePath);
    }
    else {
        // For unknown file types, try to open with default application
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    }

    logToTerminal(tr("Opening script: %1").arg(filePath));
}

void MainWindow::logToTerminal(const QString& message)
{
    // Log to active terminal window based on current tab
    int currentTab = ui->tabWidget->currentIndex();
    QString tabName = ui->tabWidget->tabText(currentTab);

    if (tabName == "TM WEEKLY PC" && ui->terminalWindowTMWPC) {
        ui->terminalWindowTMWPC->append(message);
        ui->terminalWindowTMWPC->ensureCursorVisible();
    }

    // Log to system logger
    Logger::instance().info(message);
}
