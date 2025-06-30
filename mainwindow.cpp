// Standard library includes
#include <algorithm>
#include <cfloat>   // For DBL_MAX, FLT_MAX, etc.
#include <climits>  // For INT_MAX, INT_MIN, etc.
#include <stdexcept> // For std::exception, std::runtime_error

// Include the mainwindow.h first
#include "mainwindow.h"

// Use specific Qt includes instead of module includes
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
#include <QInputDialog>
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
#include <QtConcurrent/QtConcurrent>

// Custom includes
#include "configmanager.h"
#include "dropwindow.h"
#include "errormanager.h"
#include "filelocationsdialog.h"
#include "fileutils.h"
#include "logger.h"
#include "ui_GOJI.h"
#include "updatedialog.h"
#include "updatesettingsdialog.h"
#include "validator.h"
#include "tmweeklypccontroller.h"
#include "tmweeklypcdbmanager.h"
#include "tmtermdbmanager.h"
#include "tmweeklypidocontroller.h"
#include "tmtermcontroller.h"
#include "tmtarragoncontroller.h"
#include "tmtarragondbmanager.h"
#include "databasemanager.h"

// Use version defined in GOJI.pro - make it static to avoid non-POD global static warning
#ifdef APP_VERSION
static const QString VERSION = QString(APP_VERSION);
#else
static const QString VERSION = "1.0.0";
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

        // Replace QListView with DropWindow widget
        QWidget* parent = ui->dropWindowTMWPIDO->parentWidget();
        QRect geometry = ui->dropWindowTMWPIDO->geometry();
        QString objectName = ui->dropWindowTMWPIDO->objectName();

        // Delete the old QListView widget
        delete ui->dropWindowTMWPIDO;

        // Create new DropWindow widget
        ui->dropWindowTMWPIDO = new DropWindow(parent);
        ui->dropWindowTMWPIDO->setObjectName(objectName);
        ui->dropWindowTMWPIDO->setGeometry(geometry);

        // Initialize settings
        m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope,
                                   QCoreApplication::organizationName(),
                                   QCoreApplication::applicationName(), this);

        // Use the fixed database path to match main.cpp
        QString dbPath = "C:/Goji/database/goji.db";
        qDebug() << "Using database path:" << dbPath;
        Logger::instance().info("Using database path: " + dbPath);

        // Ensure database directory exists
        QFileInfo fileInfo(dbPath);
        QDir dbDir = fileInfo.dir();
        if (!dbDir.exists()) {
            qDebug() << "Creating database directory:" << dbDir.path();
            Logger::instance().info("Creating database directory: " + dbDir.path());
            if (!dbDir.mkpath(".")) {
                qDebug() << "Failed to create database directory:" << dbDir.path();
                Logger::instance().error("Failed to create database directory: " + dbDir.path());
                throw std::runtime_error("Failed to create database directory");
            }
        }
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

        // Create TM TARRAGON controller
        m_tmTarragonController = new TMTarragonController(this);

        // Initialize database managers
        Logger::instance().info("Initializing database managers...");

        // Initialize TM Weekly PC database manager
        if (!TMWeeklyPCDBManager::instance()->initialize()) {
            Logger::instance().error("Failed to initialize TM Weekly PC database manager");
            throw std::runtime_error("Failed to initialize TM Weekly PC database manager");
        }

        // Initialize TM Term database manager
        if (!TMTermDBManager::instance()->initialize()) {
            Logger::instance().error("Failed to initialize TM Term database manager");
            throw std::runtime_error("Failed to initialize TM Term database manager");
        }

        // Initialize TM Tarragon database manager
        if (!TMTarragonDBManager::instance()->initialize()) {
            Logger::instance().error("Failed to initialize TM Tarragon database manager");
            throw std::runtime_error("Failed to initialize TM Tarragon database manager");
        }

        Logger::instance().info("Database managers initialized successfully");

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

        qDebug() << "Core components initialized";
        Logger::instance().info("Core components initialized.");

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
    delete m_tmTarragonController;
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

    // Connect the textBrowser to the controller FIRST
    m_tmWeeklyPCController->setTextBrowser(ui->textBrowserTMWPC);

    // THEN initialize TM WEEKLY PC controller with UI elements
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

    // Connect auto-save timer signals for TM WEEKLY PC
    connect(m_tmWeeklyPCController, &TMWeeklyPCController::jobOpened, this, [this]() {
        if (m_inactivityTimer) {
            m_inactivityTimer->start();
            logToTerminal("Auto-save timer started (15 minutes)");
        }
    });
    connect(m_tmWeeklyPCController, &TMWeeklyPCController::jobClosed, this, [this]() {
        if (m_inactivityTimer) {
            m_inactivityTimer->stop();
            logToTerminal("Auto-save timer stopped");
        }
    });

    // Connect the textBrowser to the PIDO controller FIRST
    m_tmWeeklyPIDOController->setTextBrowser(ui->textBrowserTMWPIDO);

    // Initialize TM WEEKLY PACK/IDO controller with UI elements
    m_tmWeeklyPIDOController->initializeUI(
        ui->runInitialTMWPIDO,           // ADD THIS LINE
        ui->processIndv01TMWPIDO,        // CORRECT - matches UI
        ui->processIndv02TMWPIDO,        // CORRECT - matches UI
        ui->dpzipTMWPIDO,                // CORRECT - matches UI
        ui->dpzipbackupTMWPIDO,          // CORRECT - matches UI
        ui->bulkMailerTMWPIDO,           // CORRECT - matches UI
        ui->fileListTMWPIDO,             // CORRECT - matches UI
        ui->terminalWindowTMWPIDO,       // CORRECT - matches UI
        ui->textBrowserTMWPIDO,          // CORRECT - matches UI
        static_cast<DropWindow*>(ui->dropWindowTMWPIDO)            // ADD THIS LINE
        );

    // Connect the textBrowser to the TERM controller FIRST
    m_tmTermController->setTextBrowser(ui->textBrowserTMTERM);

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

    // Connect the textBrowser to the TARRAGON controller FIRST
    m_tmTarragonController->setTextBrowser(ui->textBrowserTMTH);

    // Initialize TM TARRAGON controller with UI elements
    m_tmTarragonController->initializeUI(
        ui->openBulkMailerTMTH,
        ui->runInitialTMTH,
        ui->finalStepTMTH,
        ui->lockButtonTMTH,
        ui->editButtonTMTH,
        ui->postageLockTMTH,
        ui->yearDDboxTMTH,
        ui->monthDDboxTMTH,
        ui->dropNumberddBoxTMTH,
        ui->jobNumberBoxTMTH,
        ui->postageBoxTMTH,
        ui->countBoxTMTH,
        ui->terminalWindowTMTH,
        ui->trackerTMTH,
        ui->textBrowserTMTH
        );

    // Connect auto-save timer signals for TM TARRAGON
    connect(m_tmTarragonController, &TMTarragonController::jobOpened, this, [this]() {
        if (m_inactivityTimer) {
            m_inactivityTimer->start();
            logToTerminal("Auto-save timer started (15 minutes)");
        }
    });
    connect(m_tmTarragonController, &TMTarragonController::jobClosed, this, [this]() {
        if (m_inactivityTimer) {
            m_inactivityTimer->stop();
            logToTerminal("Auto-save timer stopped");
        }
    });

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
        qDebug() << "Ctrl+S shortcut activated!";
        Logger::instance().info("Ctrl+S shortcut activated");
        onSaveJobTriggered(); // Call directly instead of triggering menu action
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
        printPath = "C:/Goji/TRACHMAR/WEEKLY IDO FULL/PROCESSED";
        Logger::instance().info("Setting up print watcher for TM WEEKLY PACK/IDO");
    }
    else if (tabName == "TM TERM" && m_tmTermController) {
        // Use TM TERM archive path (they use archive for generated files)
        printPath = "C:/Goji/TRACHMAR/TERM/ARCHIVE";
        Logger::instance().info("Setting up print watcher for TM TERM");
    }
    else if (tabName == "TM TARRAGON" && m_tmTarragonController) {
        // Use TM TARRAGON archive path
        printPath = "C:/Goji/TRACHMAR/TARRAGON HOMES/ARCHIVE";
        Logger::instance().info("Setting up print watcher for TM TARRAGON");
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

void MainWindow::onTabChanged(int index)
{
    QString tabName = ui->tabWidget->tabText(index);
    logToTerminal("Switched to tab: " + tabName);
    Logger::instance().info(QString("Tab changed to index: %1 (%2)").arg(index).arg(tabName));

    // Update print watcher for the new tab
    setupPrintWatcher();

    // No menu population needed - happens on hover
}

void MainWindow::onPrintDirChanged(const QString &path)
{
    logToTerminal(tr("Print directory changed: %1").arg(path));
}

void MainWindow::onInactivityTimeout()
{
    // Only auto-save if there's actually a job open
    int currentIndex = ui->tabWidget->currentIndex();
    QString tabName = ui->tabWidget->tabText(currentIndex);

    bool hasOpenJob = false;

    if (tabName == "TM WEEKLY PC" && m_tmWeeklyPCController) {
        // Check if job is locked (has open job data)
        QString jobNumber = ui->jobNumberBoxTMWPC->text();
        QString year = ui->yearDDboxTMWPC->currentText();
        if (!jobNumber.isEmpty() && !year.isEmpty()) {
            hasOpenJob = true;
        }
    }
    else if (tabName == "TM TERM" && m_tmTermController) {
        // Check if job is locked (has open job data)
        QString jobNumber = ui->jobNumberBoxTMTERM->text();
        QString year = ui->yearDDboxTMTERM->currentText();
        if (!jobNumber.isEmpty() && !year.isEmpty()) {
            hasOpenJob = true;
        }
    }

    else if (tabName == "TM TARRAGON" && m_tmTarragonController) {
        // Check if job is locked (has open job data)
        QString jobNumber = ui->jobNumberBoxTMTH->text();
        QString year = ui->yearDDboxTMTH->currentText();
        if (!jobNumber.isEmpty() && !year.isEmpty()) {
            hasOpenJob = true;
        }
    }

    if (hasOpenJob) {
        logToTerminal("Auto-save triggered due to inactivity.");
        onSaveJobTriggered();
    } else {
        logToTerminal("Auto-save skipped - no job is currently open.");
    }
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

    // Add batch files - use std::as_const to avoid detachment
    if (!batFiles.isEmpty()) {
        QMenu* batMenu = menu->addMenu("Batch Scripts");
        batMenu->setStyleSheet(menuStyleSheet);
        for (const QString& file : std::as_const(batFiles)) {
            QAction* fileAction = new QAction(file, this);
            connect(fileAction, &QAction::triggered, this, [=]() {
                openScriptFile(dirPath + "/" + file);
            });
            batMenu->addAction(fileAction);
        }
    }

    // Add Python files - use std::as_const to avoid detachment
    if (!pyFiles.isEmpty()) {
        QMenu* pyMenu = menu->addMenu("Python Scripts");
        pyMenu->setStyleSheet(menuStyleSheet);
        for (const QString& file : std::as_const(pyFiles)) {
            QAction* fileAction = new QAction(file, this);
            connect(fileAction, &QAction::triggered, this, [=]() {
                openScriptFile(dirPath + "/" + file);
            });
            pyMenu->addAction(fileAction);
        }
    }

    // Add PowerShell files - use std::as_const to avoid detachment
    if (!psFiles.isEmpty()) {
        QMenu* psMenu = menu->addMenu("PowerShell Scripts");
        psMenu->setStyleSheet(menuStyleSheet);
        for (const QString& file : std::as_const(psFiles)) {
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

void MainWindow::populateTMTarragonJobMenu()
{
    if (!openJobMenu) return;

    // Get all TMTARRAGON jobs from database
    TMTarragonDBManager* dbManager = TMTarragonDBManager::instance();
    if (!dbManager) {
        QAction* errorAction = openJobMenu->addAction("Database not available");
        errorAction->setEnabled(false);
        logToTerminal("Open Job: TMTARRAGON Database manager not available");
        return;
    }

    QList<QMap<QString, QString>> jobs = dbManager->getAllJobs();
    logToTerminal(QString("Open Job: Found %1 TMTARRAGON jobs in database").arg(jobs.size()));

    if (jobs.isEmpty()) {
        QAction* noJobsAction = openJobMenu->addAction("No saved jobs found");
        noJobsAction->setEnabled(false);
        logToTerminal("Open Job: No TMTARRAGON jobs found in database");
        return;
    }

    // Group jobs by year, then month
    QMap<QString, QMap<QString, QList<QMap<QString, QString>>>> groupedJobs;
    for (const auto& job : std::as_const(jobs)) {
        groupedJobs[job["year"]][job["month"]].append(job);
        logToTerminal(QString("Open Job: Adding TMTARRAGON job %1 for %2-%3-D%4").arg(job["job_number"], job["year"], job["month"], job["drop_number"]));
    }

    // Create nested menu structure: Year -> Month -> Drop (Job#)
    for (auto yearIt = groupedJobs.constBegin(); yearIt != groupedJobs.constEnd(); ++yearIt) {
        QMenu* yearMenu = openJobMenu->addMenu(yearIt.key());

        for (auto monthIt = yearIt.value().constBegin(); monthIt != yearIt.value().constEnd(); ++monthIt) {
            QMenu* monthMenu = yearMenu->addMenu(QString("Month %1").arg(monthIt.key()));

            for (const auto& job : monthIt.value()) {
                QString actionText = QString("Drop %1 (Job %2)").arg(job["drop_number"], job["job_number"]);

                QAction* jobAction = monthMenu->addAction(actionText);

                // Store job data in action for later use
                jobAction->setData(QStringList() << job["year"] << job["month"] << job["drop_number"]);

                // Connect to load job function
                connect(jobAction, &QAction::triggered, this, [this, job]() {
                    loadTMTarragonJob(job["year"], job["month"], job["drop_number"]);
                });
            }
        }
    }
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

    if (ui->terminalWindowTMTH) {
        ui->terminalWindowTMTH->append(message);
        ui->terminalWindowTMTH->ensureCursorVisible();
    }

    // Log to system logger
    Logger::instance().info(message);
}

void MainWindow::onUpdateMeteredRateTriggered()
{
    Logger::instance().info("Update metered rate triggered.");
    logToTerminal(tr("Update metered rate functionality not yet implemented."));
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
    openJobMenu = new QMenu(tr("Open Job"));
    openJobMenu->setStyleSheet(menuStyleSheet);
    ui->menuFile->insertMenu(ui->actionSave_Job, openJobMenu);

    // Connect the aboutToShow signal to populate the menu on hover
    connect(openJobMenu, &QMenu::aboutToShow, this, &MainWindow::populateOpenJobMenu);

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
        // Removed unused QMap<QString, QVariant> scriptDirs;

        // Only populate Trachmar scripts (RAC scripts removed)
        QMap<QString, QString> trachmarScripts = {
            {"Weekly PC", "C:/Goji/Scripts/TRACHMAR/WEEKLY PC"},
            {"Weekly Packets/IDO", "C:/Goji/Scripts/TRACHMAR/WEEKLY PACKET & IDO"},
            {"Term", "C:/Goji/Scripts/TRACHMAR/TERM"}
        };

        QMenu* trachmarMenu = manageScriptsMenu->findChild<QMenu*>("menuTrachmar");
        if (trachmarMenu) {
            trachmarMenu->setStyleSheet(menuStyleSheet);
            trachmarMenu->clear();

            for (auto it = trachmarScripts.constBegin(); it != trachmarScripts.constEnd(); ++it) {
                QMenu* subMenu = trachmarMenu->addMenu(it.key());
                subMenu->setStyleSheet(menuStyleSheet);
                populateScriptMenu(subMenu, it.value());
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
    connect(ui->actionUpdate_Metered_Rate, &QAction::triggered, this, &MainWindow::onUpdateMeteredRateTriggered);
    connect(ui->actionSave_Job, &QAction::triggered, this, &MainWindow::onSaveJobTriggered);
    connect(ui->actionClose_Job, &QAction::triggered, this, &MainWindow::onCloseJobTriggered);

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
    m_inactivityTimer->setInterval(900000); // 15 minutes
    m_inactivityTimer->setSingleShot(false);
    connect(m_inactivityTimer, &QTimer::timeout, this, &MainWindow::onInactivityTimeout);
    m_inactivityTimer->start();
    // Timer will be started when a job is opened
    logToTerminal(tr("Inactivity timer initialized (15 minutes, stopped)."));

    Logger::instance().info("Watchers and timers initialized.");
}

void MainWindow::populateTMWPCJobMenu()
{
    if (!openJobMenu) return;

    // Get all TMWPC jobs from database
    TMWeeklyPCDBManager* dbManager = TMWeeklyPCDBManager::instance();
    if (!dbManager) {
        QAction* errorAction = openJobMenu->addAction("Database not available");
        errorAction->setEnabled(false);
        logToTerminal("Open Job: Database manager not available");
        return;
    }

    QList<QMap<QString, QString>> jobs = dbManager->getAllJobs();
    logToTerminal(QString("Open Job: Found %1 TMWPC jobs in database").arg(jobs.size()));

    if (jobs.isEmpty()) {
        QAction* noJobsAction = openJobMenu->addAction("No saved jobs found");
        noJobsAction->setEnabled(false);
        logToTerminal("Open Job: No TMWPC jobs found in database");
        return;
    }

    // Group jobs by year, then month
    QMap<QString, QMap<QString, QList<QMap<QString, QString>>>> groupedJobs;
    for (const auto& job : std::as_const(jobs)) {
        groupedJobs[job["year"]][job["month"]].append(job);
        logToTerminal(QString("Open Job: Adding job %1 for %2-%3-%4").arg(job["job_number"], job["year"], job["month"], job["week"]));
    }

    // Create nested menu structure: Year -> Month -> Week (Job#)
    for (auto yearIt = groupedJobs.constBegin(); yearIt != groupedJobs.constEnd(); ++yearIt) {
        QMenu* yearMenu = openJobMenu->addMenu(yearIt.key());

        for (auto monthIt = yearIt.value().constBegin(); monthIt != yearIt.value().constEnd(); ++monthIt) {
            QMenu* monthMenu = yearMenu->addMenu(QString("Month %1").arg(monthIt.key()));

            for (const auto& job : monthIt.value()) {
                // Use multi-arg QString formatting
                QString actionText = QString("Week %1 (Job %2)").arg(job["week"], job["job_number"]);

                QAction* jobAction = monthMenu->addAction(actionText);

                // Store job data in action for later use
                jobAction->setData(QStringList() << job["year"] << job["month"] << job["week"]);

                // Connect to load job function
                connect(jobAction, &QAction::triggered, this, [this, job]() {
                    loadTMWPCJob(job["year"], job["month"], job["week"]);
                });
            }
        }
    }
}

void MainWindow::populateTMTermJobMenu()
{
    if (!openJobMenu) return;

    // Get all TMTERM jobs from database
    TMTermDBManager* dbManager = TMTermDBManager::instance();
    if (!dbManager) {
        QAction* errorAction = openJobMenu->addAction("Database not available");
        errorAction->setEnabled(false);
        logToTerminal("Open Job: TMTERM Database manager not available");
        return;
    }

    QList<QMap<QString, QString>> jobs = dbManager->getAllJobs();
    logToTerminal(QString("Open Job: Found %1 TMTERM jobs in database").arg(jobs.size()));

    if (jobs.isEmpty()) {
        QAction* noJobsAction = openJobMenu->addAction("No saved jobs found");
        noJobsAction->setEnabled(false);
        logToTerminal("Open Job: No TMTERM jobs found in database");
        return;
    }

    // Group jobs by year
    QMap<QString, QList<QMap<QString, QString>>> groupedJobs;
    for (const auto& job : std::as_const(jobs)) {
        groupedJobs[job["year"]].append(job);
        logToTerminal(QString("Open Job: Adding TMTERM job %1 for %2-%3").arg(job["job_number"], job["year"], job["month"]));
    }

    // Create nested menu structure: Year -> Month (Job#)
    for (auto yearIt = groupedJobs.constBegin(); yearIt != groupedJobs.constEnd(); ++yearIt) {
        QMenu* yearMenu = openJobMenu->addMenu(yearIt.key());

        for (const auto& job : yearIt.value()) {
            // Use multi-arg QString formatting
            QString actionText = QString("Month %1 (Job %2)").arg(job["month"], job["job_number"]);

            QAction* jobAction = yearMenu->addAction(actionText);

            // Store job data in action for later use
            jobAction->setData(QStringList() << job["year"] << job["month"]);

            // Connect to load job function
            connect(jobAction, &QAction::triggered, this, [this, job]() {
                loadTMTermJob(job["year"], job["month"]);
            });
        }
    }
}

void MainWindow::loadTMWPCJob(const QString& year, const QString& month, const QString& week)
{
    if (m_tmWeeklyPCController) {
        bool success = m_tmWeeklyPCController->loadJob(year, month, week);
        if (success) {
            logToTerminal(QString("Loaded TMWPC job for %1-%2-%3").arg(year, month, week));
        } else {
            logToTerminal(QString("Failed to load TMWPC job for %1-%2-%3").arg(year, month, week));
        }
    }
}

void MainWindow::loadTMTermJob(const QString& year, const QString& month)
{
    if (m_tmTermController) {
        bool success = m_tmTermController->loadJob(year, month);
        if (success) {
            logToTerminal(QString("Loaded TMTERM job for %1-%2").arg(year, month));
        } else {
            logToTerminal(QString("Failed to load TMTERM job for %1-%2").arg(year, month));
        }
    }
}

void MainWindow::loadTMTarragonJob(const QString& year, const QString& month, const QString& dropNumber)
{
    if (m_tmTarragonController) {
        bool success = m_tmTarragonController->loadJob(year, month, dropNumber);
        if (success) {
            logToTerminal(QString("Loaded TMTARRAGON job for %1-%2-D%3").arg(year, month, dropNumber));
        } else {
            logToTerminal(QString("Failed to load TMTARRAGON job for %1-%2-D%3").arg(year, month, dropNumber));
        }
    }
}

void MainWindow::populateOpenJobMenu()
{
    if (!openJobMenu) return;

    // Clear existing menu items
    openJobMenu->clear();

    // Get current tab to determine which controller to use
    int currentIndex = ui->tabWidget->currentIndex();
    QString tabName = ui->tabWidget->tabText(currentIndex);

    if (tabName == "TM WEEKLY PC") {
        populateTMWPCJobMenu();
    }
    else if (tabName == "TM TERM") {
        populateTMTermJobMenu();
    }
    else if (tabName == "TM TARRAGON") {
        populateTMTarragonJobMenu();
    }
    else {
        // For tabs that don't support job loading
        QAction* notAvailableAction = openJobMenu->addAction("Not available for this tab");
        notAvailableAction->setEnabled(false);
    }
}

void MainWindow::onSaveJobTriggered()
{
    Logger::instance().info("Save job triggered.");

    // Get current tab to determine which controller to use
    int currentIndex = ui->tabWidget->currentIndex();
    QString tabName = ui->tabWidget->tabText(currentIndex);

    if (tabName == "TM WEEKLY PC" && m_tmWeeklyPCController) {
        // Validate job data first
        QString jobNumber = ui->jobNumberBoxTMWPC->text();
        QString year = ui->yearDDboxTMWPC->currentText();
        QString month = ui->monthDDboxTMWPC->currentText();
        QString week = ui->weekDDboxTMWPC->currentText();

        if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty() || week.isEmpty()) {
            logToTerminal("Cannot save job: missing required data");
            return;
        }

        // Save the job via the controller
        TMWeeklyPCDBManager* dbManager = TMWeeklyPCDBManager::instance();
        if (dbManager && dbManager->saveJob(jobNumber, year, month, week)) {
            logToTerminal("TMWPC job saved successfully");
        } else {
            logToTerminal("Failed to save TMWPC job");
        }
    }
    else if (tabName == "TM TERM" && m_tmTermController) {
        // Validate job data first
        QString jobNumber = ui->jobNumberBoxTMTERM->text();
        QString year = ui->yearDDboxTMTERM->currentText();
        QString month = ui->monthDDboxTMTERM->currentText();

        if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty()) {
            logToTerminal("Cannot save job: missing required data");
            return;
        }

        // Save the job via the controller
        TMTermDBManager* dbManager = TMTermDBManager::instance();
        if (dbManager && dbManager->saveJob(jobNumber, year, month)) {
            logToTerminal("TMTERM job saved successfully");
        } else {
            logToTerminal("Failed to save TMTERM job");
        }
    }
    else if (tabName == "TM TARRAGON" && m_tmTarragonController) {
        // Validate job data first
        QString jobNumber = ui->jobNumberBoxTMTH->text();
        QString year = ui->yearDDboxTMTH->currentText();
        QString month = ui->monthDDboxTMTH->currentText();
        QString dropNumber = ui->dropNumberddBoxTMTH->currentText();

        if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty() || dropNumber.isEmpty()) {
            logToTerminal("Cannot save job: missing required data");
            return;
        }

        // Save the job via the controller
        TMTarragonDBManager* dbManager = TMTarragonDBManager::instance();
        if (dbManager && dbManager->saveJob(jobNumber, year, month, dropNumber)) {
            logToTerminal("TMTARRAGON job saved successfully");
        } else {
            logToTerminal("Failed to save TMTARRAGON job");
        }
    }
    else if (tabName == "TM WEEKLY PACK/IDO") {
        // No save functionality for PIDO tab
        logToTerminal("Save not available for TM WEEKLY PACK/IDO tab");
        return;
    }
    else {
        logToTerminal("Save job: Unknown tab");
        return;
    }

    // Note: Open Job menu will auto-refresh on next hover
    // No manual refresh needed
}

void MainWindow::onCloseJobTriggered()
{
    Logger::instance().info("Close job triggered.");

    // Get current tab to determine which controller to use
    int currentIndex = ui->tabWidget->currentIndex();
    QString tabName = ui->tabWidget->tabText(currentIndex);

    // Auto-save before closing
    onSaveJobTriggered();

    if (tabName == "TM WEEKLY PC" && m_tmWeeklyPCController) {
        // Reset the entire controller and UI to defaults
        m_tmWeeklyPCController->resetToDefaults();
        logToTerminal("TMWPC job closed - all fields reset to defaults");
    }
    else if (tabName == "TM TERM" && m_tmTermController) {
        // Reset the entire controller and UI to defaults
        m_tmTermController->resetToDefaults();
        logToTerminal("TMTERM job closed - all fields reset to defaults");
    }
    else if (tabName == "TM TARRAGON" && m_tmTarragonController) {
        // Reset the entire controller and UI to defaults
        m_tmTarragonController->resetToDefaults();
        logToTerminal("TMTARRAGON job closed - all fields reset to defaults");
    }
    else if (tabName == "TM WEEKLY PACK/IDO") {
        // For PIDO, just clear any relevant fields manually since no job state to track
        logToTerminal("TMWEEKLYPIDO job closed");
    }

    logToTerminal("Job closed and saved successfully");

    // Refresh the Open Job menu to show the newly saved job
    onTabChanged(currentIndex);
}
