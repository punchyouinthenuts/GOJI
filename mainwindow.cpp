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
#include "tmweeklypccontroller.h"
#include "tmweeklypidocontroller.h"
#include "tmtermcontroller.h"
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
    m_inactivityTimer(nullptr),
    m_saveJobShortcut(nullptr),
    m_closeJobShortcut(nullptr),
    m_exitShortcut(nullptr),
    m_tabCycleShortcut(nullptr)
{
    qDebug() << "Entering MainWindow constructor";
    Logger::instance().info("Entering MainWindow constructor...");

    try {
        // Setup UI first - this is critical for safe initialization
        ui->setupUi(this);
        ui->tabWidget->setCurrentIndex(0);  // Force TM WEEKLY PC tab
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
        QString dbDirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Goji/SQL";
        qDebug() << "Using consistent database path:" << dbDirPath;
        Logger::instance().info("Using database path: " + dbDirPath);
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

        // Create TM WEEKLY PACK/IDO controller
        m_tmWeeklyPIDOController = new TMWeeklyPIDOController(this);

        // Create TM TERM controller
        m_tmTermController = new TMTermController(this);

        qDebug() << "Managers and controllers created";

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
        setupKeyboardShortcuts();
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
    delete m_tmWeeklyPIDOController;
    delete m_tmTermController;
    delete openJobMenu;
    delete m_printWatcher;
    delete m_inactivityTimer;

    // Clean up shortcuts
    delete m_saveJobShortcut;
    delete m_closeJobShortcut;
    delete m_exitShortcut;
    delete m_tabCycleShortcut;

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
        ui->openProofFileTMWPC,
        ui->runWeeklyMergedTMWPC,
        ui->openPrintFileTMWPC,
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
        ui->trackerTMWPC,
        ui->textBrowserTMWPC,        // Added textBrowser
        ui->pacbTMWPC                // Added proof approval checkbox
        );

    // This connects the textBrowser to the controller and loads default.html immediately
    m_tmWeeklyPCController->setTextBrowser(ui->textBrowserTMWPC);

    // Initialize TM WEEKLY PACK/IDO controller with UI elements
    m_tmWeeklyPIDOController->initializeUI(
        ui->processIndv01TMWPIDO,
        ui->processIndv02TMWPIDO,
        ui->dpzipTMWPIDO,
        ui->dpzipbackupTMWPIDO,
        ui->bulkMailerTMWPIDO,
        ui->fileListTMWPIDO,
        ui->terminalWindowTMWPIDO,
        ui->textBrowserTMWPIDO
        );

    // Connect the textBrowser to the PIDO controller
    m_tmWeeklyPIDOController->setTextBrowser(ui->textBrowserTMWPIDO);

    // Initialize TM TERM controller with UI elements
    m_tmTermController->initializeUI(
        ui->openBulkMailerTMTERM,
        ui->runInitialTMTERM,
        ui->finalStepTMTERM,
        ui->lockButtonTMTERM,
        ui->editButtonTMTERM,
        ui->postageLockTMTERM,
        ui->yearDDboxTMTERM,
        ui->monthDDboxTMTERM,
        ui->jobNumberBoxTMTERM,
        ui->postageBoxTMTERM,
        ui->countBoxTMTERM,
        ui->terminalWindowTMTERM,
        ui->trackerTMTERM,
        ui->textBrowserTMTERM
        );

    // Connect the textBrowser to the TERM controller
    m_tmTermController->setTextBrowser(ui->textBrowserTMTERM);

    Logger::instance().info("UI elements setup complete.");
}

void MainWindow::setupKeyboardShortcuts()
{
    Logger::instance().info("Setting up keyboard shortcuts...");

    // Create shortcuts
    m_saveJobShortcut = new QShortcut(QKeySequence::Save, this);  // Ctrl+S
    m_closeJobShortcut = new QShortcut(QKeySequence("Ctrl+D"), this);  // Ctrl+D
    m_exitShortcut = new QShortcut(QKeySequence("Ctrl+Q"), this);  // Explicitly Ctrl+Q
    m_tabCycleShortcut = new QShortcut(QKeySequence("Ctrl+Tab"), this);  // Ctrl+Tab

    // Connect shortcuts to their respective actions
    connect(m_saveJobShortcut, &QShortcut::activated, this, [this]() {
        ui->actionSave_Job->trigger();
    });
    connect(m_closeJobShortcut, &QShortcut::activated, this, [this]() {
        ui->actionClose_Job->trigger();
    });
    connect(m_exitShortcut, &QShortcut::activated, this, &MainWindow::onActionExitTriggered);
    connect(m_tabCycleShortcut, &QShortcut::activated, this, &MainWindow::cycleToNextTab);

    // Set shortcuts on the menu actions so they display in the menu
    ui->actionSave_Job->setShortcut(QKeySequence::Save);
    ui->actionClose_Job->setShortcut(QKeySequence("Ctrl+D"));
    ui->actionExit->setShortcut(QKeySequence("Ctrl+Q"));  // Explicitly Ctrl+Q

    Logger::instance().info("Keyboard shortcuts setup complete.");
}

void MainWindow::cycleToNextTab()
{
    if (!ui->tabWidget) {
        return;
    }

    int currentIndex = ui->tabWidget->currentIndex();
    int tabCount = ui->tabWidget->count();

    if (tabCount <= 1) {
        return;  // No point cycling with only one tab
    }

    // Move to next tab, wrapping around to first tab if at the end
    int nextIndex = (currentIndex + 1) % tabCount;
    ui->tabWidget->setCurrentIndex(nextIndex);

    logToTerminal(QString("Switched to tab: %1").arg(ui->tabWidget->tabText(nextIndex)));
}

void MainWindow::setupMenus()
{
    Logger::instance().info("Setting up menus...");

    // Apply menu styling for shortcut text
    QString menuStyleSheet =
        "QMenu {"
        "    background-color: #f0f0f0;"
        "    border: 1px solid #999999;"
        "    selection-background-color: #0078d4;"
        "    selection-color: white;"
        "}"
        "QMenu::item {"
        "    padding: 4px 30px 4px 20px;"  // Extra right padding for shortcuts
        "    background-color: transparent;"
        "    color: black;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #0078d4;"
        "    color: white;"
        "}"
        "QMenu::item:disabled {"
        "    color: #666666;"
        "}"
        "QMenu::shortcut {"
        "    color: #666666;"
        "    font-size: 11px;"
        "}";

    // Apply to all menus
    ui->menuFile->setStyleSheet(menuStyleSheet);
    ui->menuInput->setStyleSheet(menuStyleSheet);
    ui->menuTools->setStyleSheet(menuStyleSheet);

    // Setup File menu
    openJobMenu = new QMenu(tr("Open File"));
    openJobMenu->setStyleSheet(menuStyleSheet);
    ui->menuFile->insertMenu(ui->actionSave_Job, openJobMenu);

    // Setup Settings menu
    QMenu* settingsMenu = ui->menubar->addMenu(tr("Settings"));
    settingsMenu->setStyleSheet(menuStyleSheet);
    QAction* updateSettingsAction = new QAction(tr("Update Settings"));
    connect(updateSettingsAction, &QAction::triggered, this, &MainWindow::onUpdateSettingsTriggered);
    settingsMenu->addAction(updateSettingsAction);

    // Setup Script Management menu
    QMenu* manageScriptsMenu = ui->menuInput->findChild<QMenu*>("menuManage_Scripts");
    if (manageScriptsMenu) {
        manageScriptsMenu->setStyleSheet(menuStyleSheet);
        manageScriptsMenu->clear();
        QMap<QString, QVariant> scriptDirs;

        scriptDirs["Trachmar"] = QVariant::fromValue(QMap<QString, QString>{
            {"Weekly PC", "C:/Goji/Scripts/TRACHMAR/WEEKLY PC"},
            {"Weekly Packets/IDO", "C:/Goji/Scripts/TRACHMAR/WEEKLY PACKET & IDO"},
            {"Term", "C:/Goji/Scripts/TRACHMAR/TERM"}
        });

        for (auto it = scriptDirs.constBegin(); it != scriptDirs.constEnd(); ++it) {
            QMenu* parentMenu = manageScriptsMenu->addMenu(it.key());
            parentMenu->setStyleSheet(menuStyleSheet);
            const auto& subdirs = it.value().value<QMap<QString, QString>>();
            for (auto subIt = subdirs.constBegin(); subIt != subdirs.constEnd(); ++subIt) {
                QMenu* subMenu = parentMenu->addMenu(subIt.key());
                subMenu->setStyleSheet(menuStyleSheet);
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

    // Create file system watcher for print directory (but don't set it up yet)
    m_printWatcher = new QFileSystemWatcher(this);
    connect(m_printWatcher, &QFileSystemWatcher::directoryChanged, this, &MainWindow::onPrintDirChanged);

    // Set up the print watcher for the current tab
    setupPrintWatcher();

    // Inactivity timer for auto-save
    m_inactivityTimer = new QTimer(this);
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
    Logger::instance().info(QString("Tab changed to index: %1 (%2)").arg(index).arg(tabName));

    // Update print watcher for the new tab
    setupPrintWatcher();

    // Rebuild Open Job menu based on active tab
    if (openJobMenu) {
        openJobMenu->clear();

        if (tabName == "TM WEEKLY PC") {
            // Create year/month/week structure for TMWPC
            openJobMenu->addAction("Load TMWPC Job...");
        }
        else if (tabName == "TM TERM") {
            // Create year/month structure for TMTERM (no weeks)
            openJobMenu->addAction("Load TMTERM Job...");
        }
        // TMWEEKLYPIDO has no Open Job menu item per requirements
    }
}

void MainWindow::setupPrintWatcher()
{
    if (!m_printWatcher) {
        return;
    }

    // Clear existing paths
    QStringList currentPaths = m_printWatcher->directories();
    if (!currentPaths.isEmpty()) {
        m_printWatcher->removePaths(currentPaths);
    }

    // Get current tab
    int currentIndex = ui->tabWidget->currentIndex();
    QString tabName = ui->tabWidget->tabText(currentIndex);
    QString printPath;

    // Determine the appropriate print path based on current tab
    if (tabName == "TM WEEKLY PC" && m_tmWeeklyPCController) {
        // Use TM WEEKLY PC print path
        printPath = "C:/Goji/TRACHMAR/WEEKLY PC/JOB/PRINT";
        Logger::instance().info("Setting up print watcher for TM WEEKLY PC");
    }
    else if (tabName == "TM WEEKLY PACK/IDO" && m_tmWeeklyPIDOController) {
        // Use TM WEEKLY PACK/IDO output path (they use output for generated files)
        printPath = "C:/Goji/TRACHMAR/WEEKLY PACK&IDO/JOB/OUTPUT";
        Logger::instance().info("Setting up print watcher for TM WEEKLY PACK/IDO");
    }
    else if (tabName == "TM TERM" && m_tmTermController) {
        // Use TM TERM archive path (they use archive for generated files)
        printPath = "C:/Goji/TRACHMAR/TERM/ARCHIVE";
        Logger::instance().info("Setting up print watcher for TM TERM");
    }
    else {
        // Default fallback - use a generic path
        printPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Goji_Output";
        Logger::instance().warning("Unknown tab or controller not initialized, using fallback path");
    }

    // Check if directory exists and add to watcher
    if (QDir(printPath).exists()) {
        m_printWatcher->addPath(printPath);
        logToTerminal(tr("Watching print directory: %1").arg(printPath));
        Logger::instance().info(QString("Print watcher set to: %1").arg(printPath));
    } else {
        logToTerminal(tr("Print directory not found: %1").arg(printPath));
        Logger::instance().warning(QString("Print directory does not exist: %1").arg(printPath));

        // Try to create the directory
        if (QDir().mkpath(printPath)) {
            m_printWatcher->addPath(printPath);
            logToTerminal(tr("Created and now watching print directory: %1").arg(printPath));
            Logger::instance().info(QString("Created and watching print directory: %1").arg(printPath));
        } else {
            Logger::instance().error(QString("Failed to create print directory: %1").arg(printPath));
        }
    }
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
    // Apply consistent styling to the menu
    QString menuStyleSheet =
        "QMenu {"
        "    background-color: #f0f0f0;"
        "    border: 1px solid #999999;"
        "    selection-background-color: #0078d4;"
        "    selection-color: white;"
        "}"
        "QMenu::item {"
        "    padding: 4px 30px 4px 20px;"
        "    background-color: transparent;"
        "    color: black;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #0078d4;"
        "    color: white;"
        "}"
        "QMenu::item:disabled {"
        "    color: #666666;"
        "}";

    menu->setStyleSheet(menuStyleSheet);

    QDir dir(dirPath);
    if (!dir.exists()) {
        QAction* notFoundAction = new QAction(tr("Directory not found"), this);
        notFoundAction->setEnabled(false);
        menu->addAction(notFoundAction);
        return;
    }

    // Get lists of files by type (removed R files)
    QStringList batFiles = dir.entryList(QStringList() << "*.bat", QDir::Files, QDir::Name);
    QStringList pyFiles = dir.entryList(QStringList() << "*.py", QDir::Files, QDir::Name);
    QStringList psFiles = dir.entryList(QStringList() << "*.ps1", QDir::Files, QDir::Name);

    if (batFiles.isEmpty() && pyFiles.isEmpty() && psFiles.isEmpty()) {
        QAction* noScriptsAction = new QAction(tr("No scripts found"), this);
        noScriptsAction->setEnabled(false);
        menu->addAction(noScriptsAction);
        return;
    }

    // Add batch files
    if (!batFiles.isEmpty()) {
        QMenu* batMenu = menu->addMenu("Batch Scripts");
        batMenu->setStyleSheet(menuStyleSheet);
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
        pyMenu->setStyleSheet(menuStyleSheet);
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
        psMenu->setStyleSheet(menuStyleSheet);
        for (const QString& file : psFiles) {
            QAction* fileAction = new QAction(file, this);
            connect(fileAction, &QAction::triggered, this, [=]() {
                openScriptFile(dirPath + "/" + file);
            });
            psMenu->addAction(fileAction);
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
    else {
        // For unknown file types, try to open with default application
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    }

    logToTerminal(tr("Opening script: %1").arg(filePath));
}

void MainWindow::logToTerminal(const QString& message)
{
    // Log to ALL terminal windows for application-wide messages
    if (ui->terminalWindowTMWPC) {
        ui->terminalWindowTMWPC->append(message);
        ui->terminalWindowTMWPC->ensureCursorVisible();
    }

    if (ui->terminalWindowTMWPIDO) {
        ui->terminalWindowTMWPIDO->append(message);
        ui->terminalWindowTMWPIDO->ensureCursorVisible();
    }

    if (ui->terminalWindowTMTERM) {
        ui->terminalWindowTMTERM->append(message);
        ui->terminalWindowTMTERM->ensureCursorVisible();
    }

    // Log to system logger
    Logger::instance().info(message);
}

