// Standard library includes
#include <QEvent>
#include <cfloat>   // For DBL_MAX, FLT_MAX, etc.
#include <climits>  // For INT_MAX, INT_MIN, etc.
#include <stdexcept> // For std::exception, std::runtime_error

// Include the mainwindow.h first
#include "mainwindow.h"
#include "fhcontroller.h"
#include "tmfarmcontroller.h"

// Use specific Qt includes instead of module includes
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QFileIconProvider>
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
#include <QScopedValueRollback>

// Custom includes
#include "dropwindow.h"
#include "logger.h"
#include "ui_GOJI.h"
#include "updatedialog.h"
#include "updatesettingsdialog.h"
#include "tmweeklypccontroller.h"
#include "tmweeklypcdbmanager.h"
#include "tmtermdbmanager.h"
#include "tmweeklypidocontroller.h"
#include "tmtermcontroller.h"
#include "tmtarragoncontroller.h"
#include "tmtarragondbmanager.h"
#include "databasemanager.h"
#include "tmflercontroller.h"
#include "tmflerdbmanager.h"
#include "tmhealthycontroller.h"
#include "tmhealthydbmanager.h"
#include "fhdbmanager.h"
#include "tmbrokencontroller.h"
#include "tmbrokendbmanager.h"

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
    // Inactivity timer setup (single-shot) and global event filter
    if (!m_inactivityTimer) {
        m_inactivityTimer = new QTimer(this);
    }
    m_inactivityTimer->setSingleShot(true);
    // For example: 15 minutes; adjust if a constant exists
    const int INACTIVITY_MS = 15 * 60 * 1000;
    m_inactivityTimer->start(INACTIVITY_MS);
    qApp->installEventFilter(this);

    try {
        // Setup UI first
        ui->setupUi(this);
        
        // Apply global ALL-CAPS font policy for QPushButton and QToolButton
        QFont push = QApplication::font("QPushButton");
        push.setCapitalization(QFont::AllUppercase);
        QApplication::setFont(push, "QPushButton");
        QFont tool = QApplication::font("QToolButton");
        tool.setCapitalization(QFont::AllUppercase);
        QApplication::setFont(tool, "QToolButton");
    setWindowState(windowState() | Qt::WindowMaximized);  // start maximized
        ui->tabWidget->setCurrentIndex(0);
        setWindowTitle(tr("Goji v%1").arg(VERSION));

        // Replace dropWindowTMWPIDO if present
        if (ui->dropWindowTMWPIDO) {
            QWidget* parent = ui->dropWindowTMWPIDO->parentWidget();
            QRect geometry = ui->dropWindowTMWPIDO->geometry();
            QString objectName = ui->dropWindowTMWPIDO->objectName();
            delete ui->dropWindowTMWPIDO;
            ui->dropWindowTMWPIDO = new DropWindow(parent);
            ui->dropWindowTMWPIDO->setObjectName(objectName);
            ui->dropWindowTMWPIDO->setGeometry(geometry);
        } else {
            ui->dropWindowTMWPIDO = new DropWindow(this);
            ui->dropWindowTMWPIDO->setObjectName("dropWindowTMWPIDO");
        }

        // Replace dropWindowTMHB if present
        if (ui->dropWindowTMHB) {
            QWidget* parent = ui->dropWindowTMHB->parentWidget();
            QRect geometry = ui->dropWindowTMHB->geometry();
            QString objectName = ui->dropWindowTMHB->objectName();
            delete ui->dropWindowTMHB;
            ui->dropWindowTMHB = new DropWindow(parent);
            ui->dropWindowTMHB->setObjectName(objectName);
            ui->dropWindowTMHB->setGeometry(geometry);
        } else {
            ui->dropWindowTMHB = new DropWindow(this);
            ui->dropWindowTMHB->setObjectName("dropWindowTMHB");
        }

        // Replace dropWindowTMBA if present
        if (ui->dropWindowTMBA) {
            QWidget* parent = ui->dropWindowTMBA->parentWidget();
            QRect geometry = ui->dropWindowTMBA->geometry();
            QString objectName = ui->dropWindowTMBA->objectName();
            delete ui->dropWindowTMBA;
            ui->dropWindowTMBA = new DropWindow(parent);
            ui->dropWindowTMBA->setObjectName(objectName);
            ui->dropWindowTMBA->setGeometry(geometry);
        } else {
            ui->dropWindowTMBA = new DropWindow(this);
            ui->dropWindowTMBA->setObjectName("dropWindowTMBA");
        }

        // Replace dropWindowTMFLER if present
        if (ui->dropWindowTMFLER) {
            QWidget* parent = ui->dropWindowTMFLER->parentWidget();
            QRect geometry = ui->dropWindowTMFLER->geometry();
            QString objectName = ui->dropWindowTMFLER->objectName();
            delete ui->dropWindowTMFLER;
            ui->dropWindowTMFLER = new DropWindow(parent);
            ui->dropWindowTMFLER->setObjectName(objectName);
            ui->dropWindowTMFLER->setGeometry(geometry);
        } else {
            ui->dropWindowTMFLER = new DropWindow(this);
            ui->dropWindowTMFLER->setObjectName("dropWindowTMFLER");
        }

        // Replace dropWindowFH if present
        if (ui->dropWindowFH) {
            QWidget* parent = ui->dropWindowFH->parentWidget();
            QRect geometry = ui->dropWindowFH->geometry();
            QString objectName = ui->dropWindowFH->objectName();
            delete ui->dropWindowFH;
            ui->dropWindowFH = new DropWindow(parent);
            ui->dropWindowFH->setObjectName(objectName);
            ui->dropWindowFH->setGeometry(geometry);
        } else {
            ui->dropWindowFH = new DropWindow(this);
            ui->dropWindowFH->setObjectName("dropWindowFH");
        }

        // Initialize settings
        m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope,
                                   QCoreApplication::organizationName(),
                                   QCoreApplication::applicationName(), this);

        QString dbPath = "C:/Goji/database/goji.db";

        // Ensure database directory exists
        QFileInfo fileInfo(dbPath);
        QDir dbDir = fileInfo.dir();
        if (!dbDir.exists()) {
            if (!dbDir.mkpath(".")) {
                throw std::runtime_error("Failed to create database directory");
            }
        }

        // Get DatabaseManager instance
        m_dbManager = DatabaseManager::instance();
        if (!m_dbManager) {
            throw std::runtime_error("DatabaseManager instance is null");
        }
        if (!m_dbManager->isInitialized()) {
            if (!m_dbManager->initialize(dbPath)) {
                throw std::runtime_error("Failed to initialize database");
            }
        }

        // Create managers
        m_fileManager = new FileSystemManager(m_settings);
        if (!m_fileManager) throw std::runtime_error("Failed to create FileSystemManager");

        m_scriptRunner = new ScriptRunner(this);
        if (!m_scriptRunner) throw std::runtime_error("Failed to create ScriptRunner");

        m_updateManager = new UpdateManager(m_settings, this);
        if (!m_updateManager) throw std::runtime_error("Failed to create UpdateManager");

        // Create controllers
        try { m_fhController = new FHController(this); } catch (...) { m_fhController = nullptr; }
        try { m_tmWeeklyPCController = new TMWeeklyPCController(this); } catch (...) { m_tmWeeklyPCController = nullptr; }
        try { m_tmWeeklyPIDOController = new TMWeeklyPIDOController(this); } catch (...) { m_tmWeeklyPIDOController = nullptr; }
        try { m_tmTermController = new TMTermController(this); } catch (...) { m_tmTermController = nullptr; }
        try { m_tmTarragonController = new TMTarragonController(this); } catch (...) { m_tmTarragonController = nullptr; }
        try { m_tmFlerController = new TMFLERController(this); } catch (...) { m_tmFlerController = nullptr; }
        try { m_tmHealthyController = new TMHealthyController(this); } catch (...) { m_tmHealthyController = nullptr; }
        try { m_tmBrokenController = new TMBrokenController(this); } catch (...) { m_tmBrokenController = nullptr; }
        try { m_tmFarmController = new TMFarmController(this); } catch (...) { m_tmFarmController = nullptr; }


        // Initialize database managers
        if (!TMWeeklyPCDBManager::instance()->initialize()) throw std::runtime_error("Failed to initialize TM Weekly PC database manager");
        if (!TMTermDBManager::instance()->initialize()) throw std::runtime_error("Failed to initialize TM Term database manager");
        if (!TMTarragonDBManager::instance()->initialize()) throw std::runtime_error("Failed to initialize TM Tarragon database manager");
        if (!TMFLERDBManager::instance()->initializeTables()) throw std::runtime_error("Failed to initialize TM FLER database manager");
        if (!TMHealthyDBManager::instance()->initializeDatabase()) throw std::runtime_error("Failed to initialize TM HEALTHY database manager");
        if (!TMBrokenDBManager::instance()->initializeDatabase()) throw std::runtime_error("Failed to initialize TM BROKEN database manager");
        if (!FHDBManager::instance()->initializeTables()) throw std::runtime_error("Failed to initialize FOUR HANDS database manager");

        // Connect UpdateManager signals
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

        // Update check on startup
        bool checkUpdatesOnStartup = m_settings->value("Updates/CheckOnStartup", true).toBool();
        if (checkUpdatesOnStartup) {
            QDateTime lastCheck = m_settings->value("Updates/LastCheckTime").toDateTime();
            QDateTime currentTime = QDateTime::currentDateTime();
            int checkInterval = m_settings->value("Updates/CheckIntervalDays", 1).toInt();
            if (!lastCheck.isValid() || lastCheck.daysTo(currentTime) >= checkInterval) {
                QTimer::singleShot(5000, this, [this]() {
                    logToTerminal(tr("Checking updates from %1/%2")
                                      .arg(m_settings->value("UpdateServerUrl").toString(),
                                           m_settings->value("UpdateInfoFile").toString()));
                    m_updateManager->checkForUpdates(true);
                    connect(m_updateManager, &UpdateManager::updateCheckFinished, this,
                            [this](bool available) {
                                if (available) {
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

        // Setup UI elements
        setupUi();
        setupSignalSlots();
        setupKeyboardShortcuts();
        setupMenus();
        initWatchersAndTimers();

        // Enable jobs for TRACHMAR (and others if needed)
        if (ui->customerTab && ui->customerTab->count() > 0) {
            for (int i = 0; i < ui->customerTab->count(); ++i) {
                QWidget* tabWidget = ui->customerTab->widget(i);
                if (tabWidget && tabWidget->objectName() == "TRACHMAR") {
                    tabWidget->setProperty("supportsJobs", true);
                    break;
                }
            }
        }

        // Ensure meter rates table exists
        ensureMeterRatesTableExists();

        logToTerminal(tr("Goji started: %1").arg(QDateTime::currentDateTime().toString()));

    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Startup Error",
                              QString("A critical error occurred during application startup: %1").arg(e.what()));
        throw;
    } catch (...) {
        QMessageBox::critical(this, "Startup Error",
                              "An unknown critical error occurred during application startup");
        throw;
    }
}

// ScriptOpenDialog implementation
ScriptOpenDialog::ScriptOpenDialog(const QString& filePath, QWidget* parent)
    : QDialog(parent), m_filePath(filePath)
{
    setWindowTitle(tr("Open Script With..."));
    setModal(true);
    setFixedSize(400, 300);
    setupUI();
}

QString ScriptOpenDialog::getSelectedProgram() const
{
    return m_selectedProgram;
}

void ScriptOpenDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Header label
    QFileInfo fileInfo(m_filePath);
    QLabel* headerLabel = new QLabel(tr("Choose a program to open:"));
    headerLabel->setStyleSheet("font-weight: bold; margin-bottom: 10px;");
    mainLayout->addWidget(headerLabel);
    
    // File name label
    QLabel* fileLabel = new QLabel(fileInfo.fileName());
    fileLabel->setStyleSheet("font-size: 12px; color: #555; margin-bottom: 15px;");
    mainLayout->addWidget(fileLabel);
    
    // Get available programs for this file type
    QStringList programs = getAvailablePrograms(fileInfo.suffix().toLower());
    
    // Create buttons for each program
    for (const QString& program : programs) {
        QPushButton* button = new QPushButton();
        
        // Extract program name from full path for display
        QFileInfo progInfo(program);
        QString displayName = progInfo.baseName();
        
        // Set special display names for known programs
        if (displayName.toLower() == "pythonw") {
            displayName = "IDLE (Python)";
        } else if (displayName.toLower() == "emeditor") {
            displayName = "EmEditor";
        } else if (displayName.toLower() == "notepad++") {
            displayName = "Notepad++";
        } else if (displayName.toLower() == "code") {
            displayName = "Visual Studio Code";
        }
        
        button->setText(displayName);
        button->setStyleSheet(
            "QPushButton {"
            "    text-align: left;"
            "    padding: 10px 15px;"
            "    border: 1px solid #ccc;"
            "    border-radius: 5px;"
            "    background-color: #f9f9f9;"
            "    margin: 2px;"
            "}"
            "QPushButton:hover {"
            "    background-color: #e9e9e9;"
            "    border-color: #999;"
            "}"
            "QPushButton:pressed {"
            "    background-color: #d9d9d9;"
            "}"
        );
        
        // Store the full program path in the button's data
        button->setProperty("programPath", program);
        
        connect(button, &QPushButton::clicked, this, &ScriptOpenDialog::onProgramSelected);
        mainLayout->addWidget(button);
    }
    
    // Add spacer
    mainLayout->addStretch();
    
    // Cancel button
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* cancelButton = new QPushButton(tr("Cancel"));
    cancelButton->setStyleSheet(
        "QPushButton {"
        "    padding: 8px 20px;"
        "    border: 1px solid #ccc;"
        "    border-radius: 3px;"
        "    background-color: #f0f0f0;"
        "}"
        "QPushButton:hover {"
        "    background-color: #e0e0e0;"
        "}"
    );
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);
}

QStringList ScriptOpenDialog::getAvailablePrograms(const QString& extension)
{
    QStringList programs;
    
    if (extension == "py") {
        // Python files
        programs << "C:/Users/JCox/AppData/Local/Programs/Python/Python313/pythonw.exe";
        programs << "C:/Users/JCox/AppData/Local/Programs/EmEditor/EmEditor.exe";
        programs << "C:/Program Files/Notepad++/notepad++.exe";
    }
    else if (extension == "ps1") {
        // PowerShell files
        programs << "C:/Users/JCox/AppData/Local/Programs/Microsoft VS Code/Code.exe";
        programs << "C:/Users/JCox/AppData/Local/Programs/EmEditor/EmEditor.exe";
    }
    else if (extension == "bat") {
        // Batch files
        programs << "C:/Users/JCox/AppData/Local/Programs/EmEditor/EmEditor.exe";
        programs << "C:/Program Files/Notepad++/notepad++.exe";
    }
    
    // Filter out programs that don't exist
    QStringList validPrograms;
    for (const QString& program : programs) {
        if (QFileInfo::exists(program)) {
            validPrograms << program;
        }
    }
    
    return validPrograms;
}

void ScriptOpenDialog::onProgramSelected()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (button) {
        m_selectedProgram = button->property("programPath").toString();
        
        // Store additional info for IDLE handling
        QString buttonText = button->text();
        if (buttonText == "IDLE (Python)") {
            // Mark this as an IDLE selection for special argument handling
            setProperty("isIdleSelection", true);
        } else {
            setProperty("isIdleSelection", false);
        }
        
        accept();
    }
}

bool MainWindow::isScriptFile(const QString& fileName)
{
    QString extension = QFileInfo(fileName).suffix().toLower();
    return (extension == "ps1" || extension == "bat" || extension == "py" || 
            extension == "cmd" || extension == "vbs" || extension == "js");
}

QAction* MainWindow::createScriptFileAction(const QFileInfo& fileInfo)
{
    QString fileName = fileInfo.fileName();
    QString extension = fileInfo.suffix().toLower();
    
    // Try to get the system icon for the file type
    QFileIconProvider iconProvider;
    QIcon fileIcon = iconProvider.icon(fileInfo);
    
    // Create the action
    QAction* action = new QAction(fileName, this);
    
    // Set the icon if we got one
    if (!fileIcon.isNull()) {
        action->setIcon(fileIcon);
    }
    
    // Store the full file path in the action data
    action->setData(fileInfo.absoluteFilePath());
    
    // Connect to the slot that will handle opening the file with Windows Open With dialog
    connect(action, &QAction::triggered, this, [this, fileInfo]() {
        openScriptFileWithDialog(fileInfo.absoluteFilePath());
    });
    
    return action;
}

void MainWindow::openScriptFileWithDialog(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    
    // Check if file exists
    if (!fileInfo.exists()) {
        QMessageBox::warning(this, tr("File Not Found"),
                           tr("The script file does not exist: %1").arg(filePath));
        Logger::instance().error("Script file not found: " + filePath);
        return;
    }
    
    // Log the action
    logToTerminal(tr("Opening script file: %1").arg(fileInfo.fileName()));
    Logger::instance().info("Opening script file: " + filePath);
    
    // Show the custom dialog to choose which program to use
    ScriptOpenDialog dialog(filePath, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString selectedProgram = dialog.getSelectedProgram();
        
        if (!selectedProgram.isEmpty()) {
            // Launch the script with the selected program
            QStringList arguments;
            
            // Check if IDLE was selected - requires special argument handling
            bool isIdleSelection = dialog.property("isIdleSelection").toBool();
            if (isIdleSelection) {
                // For IDLE: pythonw.exe -m idlelib script.py
                arguments << "-m" << "idlelib" << filePath;
            } else {
                // For other programs: program.exe script.py
                arguments << filePath;
            }
            
            bool success = QProcess::startDetached(selectedProgram, arguments);
            
            if (success) {
                QFileInfo progInfo(selectedProgram);
                logToTerminal(tr("Opened script with %1: %2").arg(progInfo.baseName(), fileInfo.fileName()));
                Logger::instance().info(QString("Opened script file '%1' with program '%2'").arg(filePath, selectedProgram));
            } else {
                logToTerminal(tr("Failed to open script with selected program"));
                Logger::instance().error(QString("Failed to open script file '%1' with program '%2'").arg(filePath, selectedProgram));
                
                // Show error message to user
                QMessageBox::warning(this, tr("Launch Failed"),
                                   tr("Failed to launch the selected program.\n\nPlease verify the program is properly installed."));
            }
        }
    } else {
        // User cancelled the dialog
        logToTerminal(tr("Script opening cancelled by user"));
        Logger::instance().info("Script opening cancelled by user");
    }
}

void MainWindow::openScriptFileWithWindowsDialog(const QString& filePath)
{
#ifdef Q_OS_WIN
    // Convert QString to wide string for Windows API
    std::wstring wFilePath = filePath.toStdWString();
    
    // Use ShellExecute with "openas" verb to show the "Open With" dialog
    HINSTANCE result = ShellExecuteW(
        reinterpret_cast<HWND>(this->winId()),  // Parent window handle
        L"openas",                               // Verb - "openas" shows the "Open With" dialog
        wFilePath.c_str(),                       // File path
        nullptr,                                 // Parameters
        nullptr,                                 // Working directory
        SW_SHOWNORMAL                            // Show command
    );
    
    // Check if the operation was successful
    if (reinterpret_cast<uintptr_t>(result) <= 32) {
        // ShellExecute failed, fall back to default behavior
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
            QMessageBox::warning(this, tr("Unable to Open"),
                               tr("Unable to open the script file: %1").arg(filePath));
            Logger::instance().error("Failed to open script file: " + filePath);
        }
    }
    
    logToTerminal(tr("Opened script with Windows dialog: %1").arg(QFileInfo(filePath).fileName()));
    Logger::instance().info("Opened script file with Windows dialog: " + filePath);
#else
    // For non-Windows systems, use the cross-platform approach
    openScriptFileWithDialog(filePath);
#endif
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
    delete m_tmFlerController;
    delete m_tmHealthyController;
    delete m_tmBrokenController;
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

    // NEW FEATURE: Close all active jobs across all tabs before exit
    bool anyJobsClosed = false;
    
    // Iterate through all tab controllers and auto-close any active jobs
    if (m_tmWeeklyPCController && m_tmWeeklyPCController->isJobDataLocked()) {
        Logger::instance().info("Auto-closing TM WEEKLY PC job before app exit");
        m_tmWeeklyPCController->autoSaveAndCloseCurrentJob();
        anyJobsClosed = true;
    }
    
    if (m_tmTermController && m_tmTermController->isJobDataLocked()) {
        Logger::instance().info("Auto-closing TM TERM job before app exit");
        m_tmTermController->autoSaveAndCloseCurrentJob();
        anyJobsClosed = true;
    }
    
    if (m_tmTarragonController && m_tmTarragonController->isJobDataLocked()) {
        Logger::instance().info("Auto-closing TM TARRAGON job before app exit");
        m_tmTarragonController->autoSaveAndCloseCurrentJob();
        anyJobsClosed = true;
    }
    
    if (m_tmFlerController && m_tmFlerController->isJobDataLocked()) {
        Logger::instance().info("Auto-closing TM FL ER job before app exit");
        m_tmFlerController->autoSaveAndCloseCurrentJob();
        anyJobsClosed = true;
    }
    
    if (m_tmHealthyController && m_tmHealthyController->isJobDataLocked()) {
        Logger::instance().info("Auto-closing TM HEALTHY BEGINNINGS job before app exit");
        m_tmHealthyController->autoSaveAndCloseCurrentJob();
        anyJobsClosed = true;
    }
    
    if (m_tmBrokenController && m_tmBrokenController->isJobDataLocked()) {
        Logger::instance().info("Auto-closing TM BROKEN APPOINTMENTS job before app exit");
        m_tmBrokenController->autoSaveAndCloseCurrentJob();
        anyJobsClosed = true;
    }
    
    if (anyJobsClosed) {
        Logger::instance().info("Successfully auto-closed active jobs before app exit");
    } else {
        Logger::instance().info("No active jobs found to close on app exit");
    }

    event->accept();
}

void MainWindow::setupUi()
{
    Logger::instance().info("Setting up UI elements...");

    // Setup TM WEEKLY PC controller if available
    if (m_tmWeeklyPCController) {
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
    } else {
        Logger::instance().warning("TMWeeklyPCController is null, skipping UI setup");
    }

    // Connect timer and reset signals for TM WEEKLY PC
    if (m_tmWeeklyPCController) {
        connect(m_tmWeeklyPCController, &TMWeeklyPCController::jobOpened, this, [this]() {
            if (m_inactivityTimer) {
                m_inactivityTimer->start();
                logToTerminal("Auto-save timer started (15 minutes)");
            }
        });
        connect(m_tmWeeklyPCController, &TMWeeklyPCController::jobClosed,
                this, &MainWindow::onJobClosed, Qt::UniqueConnection);
    }

    // Setup TM WEEKLY PACK/IDO controller if available
    if (m_tmWeeklyPIDOController) {
        // Connect the textBrowser to the PIDO controller FIRST
        m_tmWeeklyPIDOController->setTextBrowser(ui->textBrowserTMWPIDO);

        // Initialize TM WEEKLY PACK/IDO controller with UI elements
        // Safely cast the widget to DropWindow, with null check
        DropWindow* dropWindow = nullptr;
        if (ui->dropWindowTMWPIDO) {
            dropWindow = qobject_cast<DropWindow*>(ui->dropWindowTMWPIDO);
            if (!dropWindow) {
                Logger::instance().warning("Failed to cast dropWindowTMWPIDO to DropWindow type");
            }
        }
        
        m_tmWeeklyPIDOController->initializeUI(
            ui->runInitialTMWPIDO,
            ui->processIndv01TMWPIDO,
            ui->processIndv02TMWPIDO,
            ui->dpzipTMWPIDO,
            ui->dpzipbackupTMWPIDO,
            ui->bulkMailerTMWPIDO,
            nullptr,
            ui->printTMWPIDO,     // <-- print button now in the 8th slot (required by 12-parameter overload)
            ui->fileListTMWPIDO,
            ui->terminalWindowTMWPIDO,
            ui->textBrowserTMWPIDO,
            dropWindow
        );
    } else {
        Logger::instance().warning("TMWeeklyPIDOController is null, skipping UI setup");
    }

    // Setup TM TERM controller if available
    if (m_tmTermController) {
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
        
        // Connect auto-save timer signals for TM TERM
        connect(m_tmTermController, &TMTermController::jobOpened, this, [this]() {
            if (m_inactivityTimer) {
                m_inactivityTimer->start();
                logToTerminal("Auto-save timer started (15 minutes)");
            }
        });
        connect(m_tmTermController, &TMTermController::jobClosed,
                this, &MainWindow::onJobClosed, Qt::UniqueConnection);
    } else {
        Logger::instance().warning("TMTermController is null, skipping UI setup");
    }

    // Setup TM TARRAGON controller if available
    if (m_tmTarragonController) {
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
        connect(m_tmTarragonController, &TMTarragonController::jobClosed,
                this, &MainWindow::onJobClosed, Qt::UniqueConnection);
    } else {
        Logger::instance().warning("TMTarragonController is null, skipping UI setup");
    }

    // Set up TMFLER controller with UI widgets
    if (m_tmFlerController) {
        // Safely cast the widget to DropWindow, with null check
        DropWindow* dropWindowTMFLER = nullptr;
        if (ui->dropWindowTMFLER) {
            dropWindowTMFLER = qobject_cast<DropWindow*>(ui->dropWindowTMFLER);
            if (!dropWindowTMFLER) {
                Logger::instance().warning("Failed to cast dropWindowTMFLER to DropWindow type");
            }
        }

        // Connect UI widgets to controller
        m_tmFlerController->setJobNumberBox(ui->jobNumberBoxTMFLER);
        m_tmFlerController->setYearDropdown(ui->yearDDboxTMFLER);
        m_tmFlerController->setMonthDropdown(ui->monthDDboxTMFLER);
        m_tmFlerController->setPostageBox(ui->postageBoxTMFLER);        // CRITICAL: Connect postage widget
        m_tmFlerController->setCountBox(ui->countBoxTMFLER);            // CRITICAL: Connect count widget
        m_tmFlerController->setJobDataLockButton(ui->lockButtonTMFLER);
        m_tmFlerController->setEditButton(ui->editButtonTMFLER);
        m_tmFlerController->setPostageLockButton(ui->postageLockTMFLER);
        m_tmFlerController->setRunInitialButton(ui->runInitialTMFLER);
        m_tmFlerController->setFinalStepButton(ui->finalStepTMFLER);
        m_tmFlerController->setTerminalWindow(ui->terminalWindowTMFLER);
        m_tmFlerController->setTextBrowser(ui->textBrowserTMFLER);
        m_tmFlerController->setTracker(ui->trackerTMFLER);
        m_tmFlerController->setDropWindow(dropWindowTMFLER);  // CRITICAL: Connect drop window

        // Connect auto-save timer signals for TMFLER
        connect(m_tmFlerController, &TMFLERController::jobOpened, this, [this]() {
            if (m_inactivityTimer) {
                m_inactivityTimer->start();
                logToTerminal("Auto-save timer started (15 minutes)");
            }
        });
        connect(m_tmFlerController, &TMFLERController::jobClosed,
                this, &MainWindow::onJobClosed, Qt::UniqueConnection);

        Logger::instance().info("TMFLER controller UI setup complete");
    } else {
        Logger::instance().warning("TMFLERController is null, skipping UI setup");
    }

    // Setup TM HEALTHY controller if available
    if (m_tmHealthyController) {
        m_tmHealthyController->setTextBrowser(ui->textBrowserTMHB);

        // Safely cast the widget to DropWindow, with null check
        DropWindow* dropWindowTMHB = nullptr;
        if (ui->dropWindowTMHB) {
            dropWindowTMHB = qobject_cast<DropWindow*>(ui->dropWindowTMHB);
            if (!dropWindowTMHB) {
                Logger::instance().warning("Failed to cast dropWindowTMHB to DropWindow type");
            }
        }

        // Initialize TM HEALTHY controller with UI elements including drop window
        m_tmHealthyController->initializeUI(
            ui->openBulkMailerTMHB,
            ui->runInitialTMHB,
            ui->finalStepTMHB,
            ui->lockButtonTMHB,
            ui->editButtonTMHB,
            ui->postageLockTMHB,
            ui->yearDDboxTMHB,
            ui->monthDDboxTMHB,
            ui->jobNumberBoxTMHB,
            ui->postageBoxTMHB,
            ui->countBoxTMHB,
            ui->terminalWindowTMHB,
            ui->trackerTMHB,
            ui->textBrowserTMHB,
            dropWindowTMHB
            );

        // Auto-save timer signals for TM HEALTHY
        connect(m_tmHealthyController, &TMHealthyController::jobOpened, this, [this]() {
            if (m_inactivityTimer) {
                m_inactivityTimer->start();
                logToTerminal("Auto-save timer started (15 minutes)");
            }
        });
        connect(m_tmHealthyController, &TMHealthyController::jobClosed,
                this, &MainWindow::onJobClosed, Qt::UniqueConnection);

        Logger::instance().info("TM HEALTHY controller UI setup complete");
    } else {
        Logger::instance().warning("TMHealthyController is null, skipping UI setup");
    }

    // Setup TM BROKEN controller if available
    if (m_tmBrokenController) {
        m_tmBrokenController->setTextBrowser(ui->textBrowserTMBA);

        // Safely cast the widget to DropWindow, with null check
        DropWindow* dropWindowTMBA = nullptr;
        if (ui->dropWindowTMBA) {
            dropWindowTMBA = qobject_cast<DropWindow*>(ui->dropWindowTMBA);
            if (!dropWindowTMBA) {
                Logger::instance().warning("Failed to cast dropWindowTMBA to DropWindow type");
            }
        }

        // Initialize TM BROKEN controller with UI elements including drop window
        m_tmBrokenController->initializeUI(
            ui->openBulkMailerTMBA,
            ui->runInitialTMBA,
            ui->finalStepTMBA,
            ui->lockButtonTMBA,
            ui->editButtonTMBA,
            ui->postageLockTMBA,
            ui->yearDDboxTMBA,
            ui->monthDDboxTMBA,
            ui->jobNumberBoxTMBA,
            ui->postageBoxTMBA,
            ui->countBoxTMBA,
            ui->terminalWindowTMBA,
            ui->trackerTMBA,
            ui->textBrowserTMBA,
            dropWindowTMBA
            );

        // Auto-save timer signals for TM BROKEN
        connect(m_tmBrokenController, &TMBrokenController::jobOpened, this, [this]() {
            if (m_inactivityTimer) {
                m_inactivityTimer->start();
                logToTerminal("Auto-save timer started (15 minutes)");
            }
        });
        connect(m_tmBrokenController, &TMBrokenController::jobClosed,
                this, &MainWindow::onJobClosed, Qt::UniqueConnection);

        Logger::instance().info("TM BROKEN controller UI setup complete");
    } else {
        Logger::instance().warning("TMBrokenController is null, skipping UI setup");
    }
    // Setup TM FARM WORKERS controller if available
    if (m_tmFarmController) {
        // Connect the textBrowser to the controller FIRST
        m_tmFarmController->setTextBrowser(ui->textBrowserTMFW);

        // THEN initialize TM FARM WORKERS controller with UI elements
        m_tmFarmController->initializeUI(
            ui->openBulkMailerTMFW,
            ui->runInitialTMFW,
            ui->finalStepTMFW,
            ui->lockButtonTMFW,
            ui->editButtonTMFW,
            ui->postageLockTMFW,
            ui->yearDDboxTMFW,
            ui->quarterDDboxTMFW,
            ui->jobNumberBoxTMFW,
            ui->postageBoxTMFW,
            ui->countBoxTMFW,
            ui->terminalWindowTMFW,
            ui->trackerTMFW,
            ui->textBrowserTMFW
        );
    } else {
        Logger::instance().warning("TMFarmController is null, skipping UI setup");
    }

    // Setup FOUR HANDS controller if available
    if (m_fhController) {
        // Safely cast the widget to DropWindow, with null check
        DropWindow* dropWindowFH = nullptr;
        if (ui->dropWindowFH) {
            dropWindowFH = qobject_cast<DropWindow*>(ui->dropWindowFH);
            if (!dropWindowFH) {
                Logger::instance().warning("Failed to cast dropWindowFH to DropWindow type");
            }
        }

        // Connect UI widgets to controller
        m_fhController->setJobNumberBox(ui->jobNumberBoxFH);
        m_fhController->setYearDropdown(ui->yearDDboxFH);
        m_fhController->setMonthDropdown(ui->monthDDboxFH);
        m_fhController->setDropNumberDropdown(ui->dropNumberddBoxFH);
        m_fhController->setPostageBox(ui->postageBoxFH);  // ✅ Added: Connect postage box
        m_fhController->setCountBox(ui->countBoxFH);  // ✅ Added: Connect count box
        m_fhController->setJobDataLockButton(ui->lockButtonFH);
        m_fhController->setEditButton(ui->editButtonFH);  // ✅ Added: Connect edit button
        m_fhController->setPostageLockButton(ui->postageLockFH);  // ✅ Added: Connect postage lock button
        m_fhController->setRunInitialButton(ui->runInitialFH);
        m_fhController->setFinalStepButton(ui->finalStepFH);
        m_fhController->setTerminalWindow(ui->terminalWindowFH);
        m_fhController->setTracker(ui->trackerFH);
        m_fhController->setDropWindow(dropWindowFH);

        // Connect auto-save timer signals for FOUR HANDS
        connect(m_fhController, &FHController::jobOpened, this, [this]() {
            if (m_inactivityTimer) {
                m_inactivityTimer->start();
                logToTerminal("Auto-save timer started (15 minutes)");
            }
        });
        connect(m_fhController, &FHController::jobClosed,
                this, &MainWindow::onJobClosed, Qt::UniqueConnection);

        Logger::instance().info("FOUR HANDS controller UI setup complete");
    } else {
        Logger::instance().warning("FHController is null, skipping UI setup");
    }
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
    const QStringList currentPaths = m_printWatcher->directories();
    if (!currentPaths.isEmpty()) {
        m_printWatcher->removePaths(currentPaths);
    }

    // Identify current tab by stable objectName
    const int currentIndex = ui->tabWidget->currentIndex();
    QWidget* page = ui->tabWidget->widget(currentIndex);
    const QString obj = page ? page->objectName() : QString();

    QString printPath;

    // Determine the appropriate print path based on current tab
    if (obj == "TMWEEKLYPC" && m_tmWeeklyPCController) {
        // TM WEEKLY PC print path
        printPath = "C:/Goji/TRACHMAR/WEEKLY PC/JOB/PRINT";
        Logger::instance().info("Setting up print watcher for TM WEEKLY PC");
    }
    else if (obj == "TMWPIDO" && m_tmWeeklyPIDOController) {
        // TM WEEKLY PACK/IDO output path (generated files)
        printPath = "C:/Goji/TRACHMAR/WEEKLY IDO FULL/PROCESSED";
        Logger::instance().info("Setting up print watcher for TM WEEKLY PACK/IDO");
    }
    else if (obj == "TMTERM" && m_tmTermController) {
        // TM TERM archive path (generated files)
        printPath = "C:/Goji/TRACHMAR/TERM/ARCHIVE";
        Logger::instance().info("Setting up print watcher for TM TERM");
    }
    else if (obj == "TMTARRAGON" && m_tmTarragonController) {
        // TM TARRAGON archive path
        printPath = "C:/Goji/TRACHMAR/TARRAGON HOMES/ARCHIVE";
        Logger::instance().info("Setting up print watcher for TM TARRAGON");
    }
    else if (obj == "TMFLER" && m_tmFlerController) {
        // TM FL ER archive path
        printPath = "C:/Goji/TRACHMAR/FL ER/ARCHIVE";
        Logger::instance().info("Setting up print watcher for TM FL ER");
    }
    else if (obj == "TMHEALTHY" && m_tmHealthyController) {
        // TM HEALTHY BEGINNINGS archive path
        printPath = "C:/Goji/TRACHMAR/HEALTHY BEGINNINGS/ARCHIVE";
        Logger::instance().info("Setting up print watcher for TM HEALTHY BEGINNINGS");
    }
    else if ((obj == "TMBA" || obj == "TMBROKEN") && m_tmBrokenController) {
        // TM BROKEN APPOINTMENTS archive path
        printPath = "C:/Goji/TRACHMAR/BROKEN APPOINTMENTS/ARCHIVE";
        Logger::instance().info("Setting up print watcher for TM BROKEN APPOINTMENTS");
    }
    else if (obj == "FOURHANDS" && m_fhController) {
        // FOUR HANDS archive path
        printPath = "C:/Goji/AUTOMATION/FOUR HANDS/ARCHIVE";
        Logger::instance().info("Setting up print watcher for FOUR HANDS");
    }
    else {
        // Default fallback - use a generic path
        printPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Goji_Output";
        Logger::instance().warning("Unknown tab or controller not initialized, using fallback path");
    }

    // Ensure the directory exists, then watch it
    QDir dir(printPath);
    if (dir.exists()) {
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

void MainWindow::onCustomerTabChanged(int index)
{
    QString customerName = ui->customerTab ? ui->customerTab->tabText(index) : QString();
    logToTerminal("Switched to customer tab: " + customerName);
    Logger::instance().info(QString("Customer tab changed to index: %1 (%2)")
                                .arg(index)
                                .arg(customerName));
    setupPrintWatcher();
}

void MainWindow::onPrintDirChanged(const QString &path)
{
    logToTerminal(tr("Print directory changed: %1").arg(path));
}

void MainWindow::onInactivityTimeout()
{
    // Only act if a job is actually open/locked on the active tab
    if (hasOpenJobForCurrentTab()) {
        Logger::instance().info("Inactivity timeout: attempting auto-close via helper");
        (void)requestCloseCurrentJob(false); // idempotent via m_closingJob; controllers handle UI/timers via jobClosed
    } else {
        Logger::instance().info("Inactivity timeout: no locked job to auto-close");
    }
}

void MainWindow::onJobClosed()
{
    if (m_inOnJobClosed) return;
    QScopedValueRollback<bool> _rollback(m_inOnJobClosed, true);

    // Always stop inactivity timer if non-null
    if (m_inactivityTimer) {
        m_inactivityTimer->stop();
        logToTerminal("Auto-save timer stopped");
    }

    // Use sender() to dispatch to the correct reset helper
    QObject *src = sender();
    if (src == m_tmWeeklyPCController) {
        resetTMWeeklyPCUI();
    } else if (src == m_tmTermController) {
        resetTMTermUI();
    } else if (src == m_tmTarragonController) {
        resetTMTarragonUI();
    } else if (src == m_tmFlerController) {
        resetTMFLERUI();
    } else if (src == m_tmHealthyController) {
        resetTMHealthyUI();
    }
    else if (src == m_tmBrokenController) {
        resetTMBrokenUI();
    }
    else if (src == m_fhController) {
        resetFHUI();
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
        m_scriptRunner->runScript(filePath, QStringList());
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

    // Create nested menu structure: Year -> JUL -> Drop (Job#)
    for (auto yearIt = groupedJobs.constBegin(); yearIt != groupedJobs.constEnd(); ++yearIt) {
        QMenu* yearMenu = openJobMenu->addMenu(yearIt.key());

        for (auto monthIt = yearIt.value().constBegin(); monthIt != yearIt.value().constEnd(); ++monthIt) {
            // Convert month number to 3-letter abbreviation
            QString monthAbbrev = convertMonthToAbbreviation(monthIt.key());
            QMenu* monthMenu = yearMenu->addMenu(monthAbbrev);

            for (const auto& job : monthIt.value()) {
                QString actionText = QString("Drop %1 (%2)").arg(job["drop_number"], job["job_number"]);

                QAction* jobAction = monthMenu->addAction(actionText);

                // Store job data in action for later use
                jobAction->setData(QStringList() << job["year"] << job["month"] << job["drop_number"]);

                // Connect to load job function
                connect(jobAction, &QAction::triggered, this, [this, job]() {
                    // CRITICAL FIX: Auto-close current job before opening new one
                    if (m_tmTarragonController) {
                        m_tmTarragonController->autoSaveAndCloseCurrentJob();
                    }
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

    if (ui->terminalWindowTMFLER) {
        ui->terminalWindowTMFLER->append(message);
        ui->terminalWindowTMFLER->ensureCursorVisible();
    }

    if (ui->terminalWindowTMHB) {
        ui->terminalWindowTMHB->append(message);
        ui->terminalWindowTMHB->ensureCursorVisible();
    }

    if (ui->terminalWindowTMBA) {
        ui->terminalWindowTMBA->append(message);
        ui->terminalWindowTMBA->ensureCursorVisible();
    }

    // Log to system logger
    Logger::instance().info(message);
}

double MainWindow::getCurrentMeterRate()
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        return 0.69; // Return default if database not available
    }

    // Ensure the meter_rates table exists
    if (!ensureMeterRatesTableExists()) {
        return 0.69; // Return default if table creation fails
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT rate_value FROM meter_rates ORDER BY created_at DESC LIMIT 1");

    if (m_dbManager->executeQuery(query) && query.next()) {
        return query.value("rate_value").toDouble();
    }

    return 0.69; // Return default if no rate found in database
}

bool MainWindow::updateMeterRateInDatabase(double newRate)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        return false;
    }

    // Ensure the meter_rates table exists
    if (!ensureMeterRatesTableExists()) {
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("INSERT INTO meter_rates (rate_value, created_at, updated_at) VALUES (?, ?, ?)");
    
    QString currentTime = QDateTime::currentDateTime().toString(Qt::ISODate);
    query.addBindValue(newRate);
    query.addBindValue(currentTime);
    query.addBindValue(currentTime);

    if (!m_dbManager->executeQuery(query)) {
        Logger::instance().error("Failed to insert new meter rate: " + query.lastError().text());
        return false;
    }

    Logger::instance().info(QString("Successfully updated meter rate to %1").arg(newRate));
    return true;
}

bool MainWindow::ensureMeterRatesTableExists()
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        return false;
    }

    QSqlQuery query(m_dbManager->getDatabase());
    QString createTableSQL = "CREATE TABLE IF NOT EXISTS meter_rates ("
                             "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                             "rate_value REAL NOT NULL, "
                             "created_at TEXT NOT NULL, "
                             "updated_at TEXT NOT NULL"
                             ")";

    if (!query.exec(createTableSQL)) {
        Logger::instance().error("Failed to create meter_rates table: " + query.lastError().text());
        return false;
    }

    // Check if table is empty and insert default rate if needed
    query.prepare("SELECT COUNT(*) FROM meter_rates");
    if (m_dbManager->executeQuery(query) && query.next()) {
        int count = query.value(0).toInt();
        if (count == 0) {
            // Insert default rate
            QString currentTime = QDateTime::currentDateTime().toString(Qt::ISODate);
            query.prepare("INSERT INTO meter_rates (rate_value, created_at, updated_at) VALUES (?, ?, ?)");
            query.addBindValue(0.69);
            query.addBindValue(currentTime);
            query.addBindValue(currentTime);
            
            if (!m_dbManager->executeQuery(query)) {
                Logger::instance().error("Failed to insert default meter rate: " + query.lastError().text());
                return false;
            }
            
            Logger::instance().info("Inserted default meter rate of 0.69");
        }
    }

    return true;
}

void MainWindow::onUpdateMeteredRateTriggered()
{
    Logger::instance().info("Update metered rate triggered.");
    
    // Get current rate from database
    double currentRate = getCurrentMeterRate();
    
    bool ok;
    double newRate = QInputDialog::getDouble(this, 
                                             tr("Update Metered Rate"),
                                             tr("Enter new meter rate (current: $%1):").arg(currentRate, 0, 'f', 3),
                                             currentRate, 
                                             0.001, 10.000, 3, &ok);
    
    if (ok && newRate > 0) {
        if (updateMeterRateInDatabase(newRate)) {
            logToTerminal(tr("Meter rate updated successfully to $%1").arg(newRate, 0, 'f', 3));
            QMessageBox::information(this, tr("Success"), 
                                     tr("Meter rate has been updated to $%1").arg(newRate, 0, 'f', 3));
        } else {
            logToTerminal(tr("Failed to update meter rate in database"));
            QMessageBox::warning(this, tr("Error"), 
                                 tr("Failed to update meter rate in database"));
        }
    } else if (ok) {
        QMessageBox::warning(this, tr("Invalid Input"), 
                             tr("Please enter a valid rate greater than 0"));
    }
}

void MainWindow::onManageEditDatabaseTriggered()
{
    Logger::instance().info("Manage Edit Database action triggered.");
    
    QString databasePath = "C:/Goji/database/goji.db";
    QString applicationPath = "C:/Program Files/DB Browser for SQLite/DB Browser for SQLite.exe";
    
    // Check if the database file exists
    if (!QFileInfo::exists(databasePath)) {
        logToTerminal(tr("Database file not found: %1").arg(databasePath));
        QMessageBox::warning(this, tr("Database Not Found"), 
                             tr("Database file not found at: %1").arg(databasePath));
        return;
    }
    
    // Check if DB Browser for SQLite exists
    if (!QFileInfo::exists(applicationPath)) {
        logToTerminal(tr("DB Browser for SQLite not found: %1").arg(applicationPath));
        QMessageBox::warning(this, tr("Application Not Found"), 
                             tr("DB Browser for SQLite not found at: %1\n\nPlease install DB Browser for SQLite or verify the installation path.").arg(applicationPath));
        return;
    }
    
    // Launch DB Browser for SQLite with the database file
    QStringList arguments;
    arguments << databasePath;
    
    bool success = QProcess::startDetached(applicationPath, arguments);
    
    if (success) {
        logToTerminal(tr("Successfully opened database in DB Browser for SQLite"));
        Logger::instance().info(QString("Opened database %1 with DB Browser for SQLite").arg(databasePath));
    } else {
        logToTerminal(tr("Failed to open DB Browser for SQLite"));
        QMessageBox::warning(this, tr("Launch Failed"), 
                             tr("Failed to launch DB Browser for SQLite.\n\nPlease check if the application is properly installed."));
        Logger::instance().error("Failed to launch DB Browser for SQLite");
    }
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

    // Apply to existing menus
    ui->menuFile->setStyleSheet(menuStyleSheet);
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

    // Setup Script Management menu with dynamic directory structure
    setupScriptsMenu();

    // Connect tab change handler
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    connect(ui->customerTab, &QTabWidget::currentChanged, this, &MainWindow::onCustomerTabChanged);

    Logger::instance().info("Menus setup complete.");
}

void MainWindow::setupSignalSlots()
{
    Logger::instance().info("Setting up signal slots...");

    // Menu connections
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::onActionExitTriggered);
    connect(ui->actionCheck_for_updates, &QAction::triggered, this, &MainWindow::onCheckForUpdatesTriggered);
    connect(ui->actionUpdate_Metered_Rate, &QAction::triggered, this, &MainWindow::onUpdateMeteredRateTriggered);
    
    // Connect to actionManage_Edit_Database using findChild since it's not exposed in ui
    QAction* manageEditDatabaseAction = this->findChild<QAction*>("actionManage_Edit_Database");
    if (manageEditDatabaseAction) {
        connect(manageEditDatabaseAction, &QAction::triggered, this, &MainWindow::onManageEditDatabaseTriggered);
    } else {
        Logger::instance().warning("actionManage_Edit_Database not found in UI");
    }
    
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
    m_inactivityTimer->setSingleShot(true);
    connect(m_inactivityTimer, &QTimer::timeout, this, &MainWindow::onInactivityTimeout);
    m_inactivityTimer->stop(); // keep it stopped until a job opens
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

    // Create nested menu structure: Year -> JUL -> Week (Job#)
    for (auto yearIt = groupedJobs.constBegin(); yearIt != groupedJobs.constEnd(); ++yearIt) {
        QMenu* yearMenu = openJobMenu->addMenu(yearIt.key());

        for (auto monthIt = yearIt.value().constBegin(); monthIt != yearIt.value().constEnd(); ++monthIt) {
            // Convert month number to 3-letter abbreviation
            QString monthAbbrev = convertMonthToAbbreviation(monthIt.key());
            QMenu* monthMenu = yearMenu->addMenu(monthAbbrev);

            for (const auto& job : monthIt.value()) {
                QString actionText = QString("%1 (%2)").arg(job["week"], job["job_number"]);

                QAction* jobAction = monthMenu->addAction(actionText);

                // Store job data in action for later use
                jobAction->setData(QStringList() << job["year"] << job["month"] << job["week"]);

                // Connect to load job function
                connect(jobAction, &QAction::triggered, this, [this, job]() {
                    // CRITICAL FIX: Auto-close current job before opening new one
                    if (m_tmWeeklyPCController) {
                        m_tmWeeklyPCController->autoSaveAndCloseCurrentJob();
                    }
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

    // Create nested menu structure: Year -> JUL (Job#)
    for (auto yearIt = groupedJobs.constBegin(); yearIt != groupedJobs.constEnd(); ++yearIt) {
        QMenu* yearMenu = openJobMenu->addMenu(yearIt.key());

        for (const auto& job : yearIt.value()) {
            // Convert month number to 3-letter abbreviation
            QString monthAbbrev = convertMonthToAbbreviation(job["month"]);
            QString actionText = QString("%1 (%2)").arg(monthAbbrev, job["job_number"]);

            QAction* jobAction = yearMenu->addAction(actionText);

            // Store job data in action for later use
            jobAction->setData(QStringList() << job["year"] << job["month"]);

            // Connect to load job function
            connect(jobAction, &QAction::triggered, this, [this, job]() {
                // CRITICAL FIX: Auto-close current job before opening new one
                if (m_tmTermController) {
                    m_tmTermController->autoSaveAndCloseCurrentJob();
                }
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

    // Check outer customer tab context first
    int cIdx = (ui->customerTab ? ui->customerTab->currentIndex() : -1);
    QWidget* customer = (ui->customerTab && cIdx >= 0) ? ui->customerTab->widget(cIdx) : nullptr;
    bool supportsJobs = customer && customer->property("supportsJobs").toBool();
    if (!supportsJobs) {
        QAction* a = openJobMenu->addAction(tr("Jobs not available for this customer"));
        a->setEnabled(false);
        return;
    }

    // Get current tab to determine which controller to use
    int currentIndex = ui->tabWidget->currentIndex();
    QWidget* page = ui->tabWidget->widget(currentIndex);
    const QString obj = page ? page->objectName() : QString();

    if (obj == "TMWEEKLYPC") {
        populateTMWPCJobMenu();
    }
    else if (obj == "TMTERM") {
        populateTMTermJobMenu();
    }
    else if (obj == "TMTARRAGON") {
        populateTMTarragonJobMenu();
    }
    else if (obj == "TMFLER") {
        populateTMFLERJobMenu();
    }
    else if (obj == "TMHEALTHY") {
        populateTMHealthyJobMenu();
    }
    else if (obj == "TMBA" || obj == "TMBROKEN") {
        populateTMBrokenJobMenu();
    }
    else if (obj == "FOURHANDS") {
        populateFHJobMenu();
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
    QWidget* page = ui->tabWidget->widget(currentIndex);
    const QString obj = page ? page->objectName() : QString();

    if (obj == "TMWEEKLYPC" && m_tmWeeklyPCController) {
        // Validate job data first
        QString jobNumber = ui->jobNumberBoxTMWPC->text();
        QString year = ui->yearDDboxTMWPC->currentText();
        QString month = ui->monthDDboxTMWPC->currentText();
        QString week = ui->weekDDboxTMWPC->currentText();

        if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty() || week.isEmpty()) {
            logToTerminal("Cannot save job: missing required data");
            return;
        }

        // Save the job via the controller/db
        TMWeeklyPCDBManager* dbManager = TMWeeklyPCDBManager::instance();
        if (dbManager && dbManager->saveJob(jobNumber, year, month, week)) {
            logToTerminal("TMWPC job saved successfully");
        } else {
            logToTerminal("Failed to save TMWPC job");
        }
    }
    else if (obj == "TMTERM" && m_tmTermController) {
        // Validate job data first
        QString jobNumber = ui->jobNumberBoxTMTERM->text();
        QString year = ui->yearDDboxTMTERM->currentText();
        QString month = ui->monthDDboxTMTERM->currentText();

        if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty()) {
            logToTerminal("Cannot save job: missing required data");
            return;
        }

        // Save the job via the controller/db
        TMTermDBManager* dbManager = TMTermDBManager::instance();
        if (dbManager && dbManager->saveJob(jobNumber, year, month)) {
            logToTerminal("TMTERM job saved successfully");
        } else {
            logToTerminal("Failed to save TMTERM job");
        }
    }
    else if (obj == "TMTARRAGON" && m_tmTarragonController) {
        // Validate job data first
        QString jobNumber = ui->jobNumberBoxTMTH->text();
        QString year = ui->yearDDboxTMTH->currentText();
        QString month = ui->monthDDboxTMTH->currentText();
        QString dropNumber = ui->dropNumberddBoxTMTH->currentText();

        if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty() || dropNumber.isEmpty()) {
            logToTerminal("Cannot save job: missing required data");
            return;
        }

        // Save the job via the controller/db
        TMTarragonDBManager* dbManager = TMTarragonDBManager::instance();
        if (dbManager && dbManager->saveJob(jobNumber, year, month, dropNumber)) {
            logToTerminal("TMTARRAGON job saved successfully");
        } else {
            logToTerminal("Failed to save TMTARRAGON job");
        }
    }
    else if (obj == "TMFLER" && m_tmFlerController) {
        // Validate job data first
        QString jobNumber = ui->jobNumberBoxTMFLER->text();
        QString year = ui->yearDDboxTMFLER->currentText();
        QString month = ui->monthDDboxTMFLER->currentText();

        if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty()) {
            logToTerminal("Cannot save job: missing required data");
            return;
        }

        // Save the job via the controller/db
        TMFLERDBManager* dbManager = TMFLERDBManager::instance();
        if (dbManager && dbManager->saveJob(jobNumber, year, month)) {
            logToTerminal("TMFLER job saved successfully");
        } else {
            logToTerminal("Failed to save TMFLER job");
        }
    }
    else if (obj == "TMHEALTHY" && m_tmHealthyController) {
        // Validate job data first
        QString jobNumber = ui->jobNumberBoxTMHB->text();
        QString year = ui->yearDDboxTMHB->currentText();
        QString month = ui->monthDDboxTMHB->currentText();

        if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty()) {
            logToTerminal("Cannot save job: missing required data");
            return;
        }

        // Save the job via the controller/db
        TMHealthyDBManager* dbManager = TMHealthyDBManager::instance();
        if (dbManager && dbManager->saveJob(jobNumber, year, month)) {
            logToTerminal("TMHEALTHY job saved successfully");
        } else {
            logToTerminal("Failed to save TMHEALTHY job");
        }
    }
    else if ((obj == "TMBA" || obj == "TMBROKEN") && m_tmBrokenController) {
        // Validate job data first
        QString jobNumber = ui->jobNumberBoxTMBA->text();
        QString year = ui->yearDDboxTMBA->currentText();
        QString month = ui->monthDDboxTMBA->currentText();

        if (jobNumber.isEmpty() || year.isEmpty() || month.isEmpty()) {
            logToTerminal("Cannot save job: missing required data");
            return;
        }

        // Save the job via the controller/db
        TMBrokenDBManager* dbManager = TMBrokenDBManager::instance();
        if (dbManager && dbManager->saveJob(jobNumber, year, month)) {
            logToTerminal("TMBROKEN job saved successfully");
        } else {
            logToTerminal("Failed to save TMBROKEN job");
        }
    }
    else if (obj == "TMWPIDO") {
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

    const bool closed = requestCloseCurrentJob(false);
    if (closed) {
        logToTerminal("Job closed and saved successfully");
    } else {
        logToTerminal("No job is currently open to close");
        return;
    }

    // No per-tab resetToDefaults() here; controllers emit jobClosed and handle UI/timers.
}

void MainWindow::populateTMFLERJobMenu()
{
    if (!openJobMenu) return;

    // Get all TMFLER jobs from database
    TMFLERDBManager* dbManager = TMFLERDBManager::instance();
    if (!dbManager) {
        QAction* errorAction = openJobMenu->addAction("Database not available");
        errorAction->setEnabled(false);
        logToTerminal("Open Job: TMFLER Database manager not available");
        return;
    }

    QList<QMap<QString, QString>> jobs = dbManager->getAllJobs();
    logToTerminal(QString("Open Job: Found %1 TMFLER jobs in database").arg(jobs.size()));

    if (jobs.isEmpty()) {
        QAction* noJobsAction = openJobMenu->addAction("No saved jobs found");
        noJobsAction->setEnabled(false);
        logToTerminal("Open Job: No TMFLER jobs found in database");
        return;
    }

    // Group jobs by year
    QMap<QString, QList<QMap<QString, QString>>> groupedJobs;
    for (const auto& job : std::as_const(jobs)) {
        groupedJobs[job["year"]].append(job);
        logToTerminal(QString("Open Job: Adding TMFLER job %1 for %2-%3").arg(job["job_number"], job["year"], job["month"]));
    }

    // Create nested menu structure: Year -> MON (Job#)
    for (auto yearIt = groupedJobs.constBegin(); yearIt != groupedJobs.constEnd(); ++yearIt) {
        QMenu* yearMenu = openJobMenu->addMenu(yearIt.key());

        for (const auto& job : yearIt.value()) {
            // Convert month number to 3-letter abbreviation
            QString monthAbbrev = convertMonthToAbbreviation(job["month"]);
            QString actionText = QString("%1 (%2)").arg(monthAbbrev, job["job_number"]);

            QAction* jobAction = yearMenu->addAction(actionText);

            // Store job data in action for later use
            jobAction->setData(QStringList() << job["year"] << job["month"]);

            // Connect to load job function
            connect(jobAction, &QAction::triggered, this, [this, job]() {
                // CRITICAL FIX: Auto-close current job before opening new one
                if (m_tmFlerController) {
                    m_tmFlerController->autoSaveAndCloseCurrentJob();
                }
                loadTMFLERJob(job["year"], job["month"]);
            });
        }
    }
}

void MainWindow::loadTMFLERJob(const QString& year, const QString& month)
{
    if (!m_tmFlerController) return;

    // Switch to TMFLER tab first
    ui->tabWidget->setCurrentWidget(ui->TMFLER);

    // Load the job
    if (m_tmFlerController->loadJob(year, month)) {
        logToTerminal(QString("TMFLER job loaded: %1/%2").arg(year, month));
    } else {
        logToTerminal(QString("Failed to load TMFLER job: %1/%2").arg(year, month));
    }
}

void MainWindow::populateTMHealthyJobMenu()
{
    if (!openJobMenu) return;

    // Get all TMHEALTHY jobs from database
    TMHealthyDBManager* dbManager = TMHealthyDBManager::instance();
    if (!dbManager) {
        QAction* errorAction = openJobMenu->addAction("Database not available");
        errorAction->setEnabled(false);
        logToTerminal("Open Job: TMHEALTHY Database manager not available");
        return;
    }

    QList<QMap<QString, QString>> jobs = dbManager->getAllJobs();
    logToTerminal(QString("Open Job: Found %1 TMHEALTHY jobs in database").arg(jobs.size()));

    if (jobs.isEmpty()) {
        QAction* noJobsAction = openJobMenu->addAction("No saved jobs found");
        noJobsAction->setEnabled(false);
        logToTerminal("Open Job: No TMHEALTHY jobs found in database");
        return;
    }

    // Group jobs by year
    QMap<QString, QList<QMap<QString, QString>>> groupedJobs;
    for (const auto& job : std::as_const(jobs)) {
        groupedJobs[job["year"]].append(job);
        logToTerminal(QString("Open Job: Adding TMHEALTHY job %1 for %2-%3").arg(job["job_number"], job["year"], job["month"]));
    }

    // Create nested menu structure: Year -> JUL (Job#)
    for (auto yearIt = groupedJobs.constBegin(); yearIt != groupedJobs.constEnd(); ++yearIt) {
        QMenu* yearMenu = openJobMenu->addMenu(yearIt.key());

        for (const auto& job : yearIt.value()) {
            // Convert month number to 3-letter abbreviation
            QString monthAbbrev = convertMonthToAbbreviation(job["month"]);
            QString actionText = QString("%1 (%2)").arg(monthAbbrev, job["job_number"]);

            QAction* jobAction = yearMenu->addAction(actionText);

            // Store job data in action for later use
            jobAction->setData(QStringList() << job["year"] << job["month"]);

            // Connect to load job function
            connect(jobAction, &QAction::triggered, this, [this, job]() {
                // CRITICAL FIX: Auto-close current job before opening new one
                if (m_tmHealthyController) {
                    m_tmHealthyController->autoSaveAndCloseCurrentJob();
                }
                loadTMHealthyJob(job["year"], job["month"]);
            });
        }
    }
}

void MainWindow::loadTMHealthyJob(const QString& year, const QString& month)
{
    if (m_tmHealthyController) {
        bool success = m_tmHealthyController->loadJob(year, month);
        if (success) {
            logToTerminal(QString("Loaded TMHEALTHY job for %1-%2").arg(year, month));
        } else {
            logToTerminal(QString("Failed to load TMHEALTHY job for %1-%2").arg(year, month));
        }
    }
}

void MainWindow::setupScriptsMenu()
{
    Logger::instance().info("Setting up scripts menu...");
    
    // Find or create the "Manage Scripts" menu
    QMenu* manageScriptsMenu = nullptr;
    
    // Look for existing menu in the menu bar
    for (QAction* action : ui->menubar->actions()) {
        if (action->text() == "Manage Scripts") {
            manageScriptsMenu = action->menu();
            break;
        }
    }
    
    // If not found, create new menu under Tools instead of menubar
    if (!manageScriptsMenu) {
        manageScriptsMenu = ui->menuTools->addMenu(tr("Manage Scripts"));
    }
    
    // Apply consistent menu styling
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
    
    manageScriptsMenu->setStyleSheet(menuStyleSheet);
    
    // Connect the aboutToShow signal to dynamically populate the menu
    connect(manageScriptsMenu, &QMenu::aboutToShow, this, [this, manageScriptsMenu, menuStyleSheet]() {
        // Clear existing items
        manageScriptsMenu->clear();
        
        // Define the base scripts directory - try both paths
        QString scriptsPath = "C:/Goji/scripts";
        if (!QDir(scriptsPath).exists()) {
            scriptsPath = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../scripts");
            if (!QDir(scriptsPath).exists()) {
                // Final fallback to current project directory
                scriptsPath = QDir::currentPath() + "/scripts";
            }
        }
        
        // Build the menu structure recursively
        buildScriptMenuRecursively(manageScriptsMenu, scriptsPath, menuStyleSheet);
    });
    
    Logger::instance().info("Scripts menu setup complete.");
}

void MainWindow::buildScriptMenuRecursively(QMenu* parentMenu, const QString& dirPath, const QString& styleSheet)
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        QAction* notFoundAction = new QAction(tr("Directory not found: %1").arg(dirPath), this);
        notFoundAction->setEnabled(false);
        parentMenu->addAction(notFoundAction);
        return;
    }
    
    // Get all subdirectories and files
    QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
    
    // Lists to hold different types of entries
    QList<QFileInfo> directories;
    QList<QFileInfo> scriptFiles;
    
    // Separate directories and script files
    for (const QFileInfo& entry : entries) {
        if (entry.isDir()) {
            directories.append(entry);
        } else if (isScriptFile(entry.fileName())) {
            scriptFiles.append(entry);
        }
    }
    
    // Add directories as submenus
    for (const QFileInfo& dirInfo : directories) {
        QMenu* submenu = parentMenu->addMenu(dirInfo.fileName());
        submenu->setStyleSheet(styleSheet);
        
        // Recursively build the submenu
        buildScriptMenuRecursively(submenu, dirInfo.absoluteFilePath(), styleSheet);
    }
    
    // Add script files as actions
    for (const QFileInfo& fileInfo : scriptFiles) {
        QAction* fileAction = createScriptFileAction(fileInfo);
        parentMenu->addAction(fileAction);
    }
    
    // If the menu is empty, add a "No scripts found" action
    if (parentMenu->actions().isEmpty()) {
        QAction* emptyAction = new QAction(tr("No scripts found"), this);
        emptyAction->setEnabled(false);
        parentMenu->addAction(emptyAction);
    }
}

QString MainWindow::convertMonthToAbbreviation(const QString& monthNumber) const
{
    QMap<QString, QString> monthMap = {
        {"01", "JAN"}, {"02", "FEB"}, {"03", "MAR"}, {"04", "APR"},
        {"05", "MAY"}, {"06", "JUN"}, {"07", "JUL"}, {"08", "AUG"},
        {"09", "SEP"}, {"10", "OCT"}, {"11", "NOV"}, {"12", "DEC"}
    };
    return monthMap.value(monthNumber, monthNumber);
}

bool MainWindow::setCurrentJobTab(int index) {
    if (!ui || !ui->tabWidget)
        return false;
    ui->tabWidget->setCurrentIndex(index);
    return true;
}

bool MainWindow::requestCloseCurrentJob(bool viaAppExit)
{
    if (m_closingJob) return false;
    CloseGuard guard(m_closingJob);

    const int currentIndex = ui->tabWidget->currentIndex();
    QWidget* page = ui->tabWidget->widget(currentIndex);
    const QString obj = page ? page->objectName() : QString();

    bool ok = false;

    // Replace any isJobDataLocked() condition with a broader open/has-data check:
    if (obj == "TMWEEKLYPC" && m_tmWeeklyPCController) {
        if (m_tmWeeklyPCController->isJobDataLocked()) {  // or isJobOpen() if available
            Logger::instance().info(viaAppExit ? "Auto-closing TM WEEKLY PC job before exit"
                                               : "Closing TM WEEKLY PC job");
            m_tmWeeklyPCController->autoSaveAndCloseCurrentJob();
            ok = true;
        } else {
            ok = true; // nothing to close
        }
    }
    else if ((obj == "TMBA" || obj == "TMBROKEN") && m_tmBrokenController) {
        if (m_tmBrokenController->isJobDataLocked()) {
            Logger::instance().info(viaAppExit ? "Auto-closing TM BROKEN APPOINTMENTS job before exit"
                                               : "Closing TM BROKEN APPOINTMENTS job");
            m_tmBrokenController->autoSaveAndCloseCurrentJob();
            ok = true;
        } else {
            ok = true; // nothing to close
        }
    } else if (obj == "TMTERM" && m_tmTermController) {
        if (m_tmTermController->isJobDataLocked()) {
            Logger::instance().info(viaAppExit ? "Auto-closing TM TERM job before exit"
                                               : "Closing TM TERM job");
            m_tmTermController->autoSaveAndCloseCurrentJob();
            ok = true;
        } else {
            ok = true; // nothing to close
        }
    } else if (obj == "TMTARRAGON" && m_tmTarragonController) {
        if (m_tmTarragonController->isJobDataLocked()) {
            Logger::instance().info(viaAppExit ? "Auto-closing TM TARRAGON job before exit"
                                               : "Closing TM TARRAGON job");
            m_tmTarragonController->autoSaveAndCloseCurrentJob();
            ok = true;
        } else {
            ok = true; // nothing to close
        }
    } else if (obj == "TMFLER" && m_tmFlerController) {
        if (m_tmFlerController->isJobDataLocked()) {
            Logger::instance().info(viaAppExit ? "Auto-closing TM FL ER job before exit"
                                               : "Closing TM FL ER job");
            m_tmFlerController->autoSaveAndCloseCurrentJob();
            ok = true;
        } else {
            ok = true; // nothing to close
        }
    } else if (obj == "TMHEALTHY" && m_tmHealthyController) {
        if (m_tmHealthyController->isJobDataLocked()) {
            Logger::instance().info(viaAppExit ? "Auto-closing TM HEALTHY BEGINNINGS job before exit"
                                               : "Closing TM HEALTHY BEGINNINGS job");
            m_tmHealthyController->autoSaveAndCloseCurrentJob();
            ok = true;
        } else {
            ok = true; // nothing to close
        }
    }
    // PIDO intentionally excluded (no job state)

    return ok;
}

bool MainWindow::hasOpenJobForCurrentTab() const
{
    const int currentIndex = ui->tabWidget->currentIndex();
    QWidget* page = ui->tabWidget->widget(currentIndex);
    const QString obj = page ? page->objectName() : QString();

    if (obj == "TMWEEKLYPC" && m_tmWeeklyPCController) {
        return m_tmWeeklyPCController->isJobDataLocked();
    } else if (obj == "TMTERM" && m_tmTermController) {
        return m_tmTermController->isJobDataLocked();
    } else if (obj == "TMTARRAGON" && m_tmTarragonController) {
        return m_tmTarragonController->isJobDataLocked();
    } else if (obj == "TMFLER" && m_tmFlerController) {
        return m_tmFlerController->isJobDataLocked();
    } else if (obj == "TMHEALTHY" && m_tmHealthyController) {
        return m_tmHealthyController->isJobDataLocked();
    }
    else if ((obj == "TMBA" || obj == "TMBROKEN") && m_tmBrokenController) {
        return m_tmBrokenController->isJobDataLocked();
    }
    else if (obj == "FOURHANDS" && m_fhController) {
        return m_fhController->hasJobData();
    }
    // PIDO intentionally excluded
    return false;
}

void MainWindow::resetCurrentTabUI()
{
    // This method is no longer used - dispatch now handled by sender() in onJobClosed()
    // Keeping for backwards compatibility but functionality moved to onJobClosed()
}

void MainWindow::resetTMWeeklyPCUI()
{
    // Reset TMWEEKLYPC tab UI widgets to default state
    if (ui->jobNumberBoxTMWPC) ui->jobNumberBoxTMWPC->clear();
    if (ui->yearDDboxTMWPC) ui->yearDDboxTMWPC->setCurrentIndex(0);
    if (ui->monthDDboxTMWPC) ui->monthDDboxTMWPC->setCurrentIndex(0);
    if (ui->weekDDboxTMWPC) ui->weekDDboxTMWPC->setCurrentIndex(0);
    if (ui->classDDboxTMWPC) ui->classDDboxTMWPC->setCurrentIndex(0);
    if (ui->permitDDboxTMWPC) ui->permitDDboxTMWPC->setCurrentIndex(0);
    if (ui->proofDDboxTMWPC) ui->proofDDboxTMWPC->setCurrentIndex(0);
    if (ui->printDDboxTMWPC) ui->printDDboxTMWPC->setCurrentIndex(0);
    if (ui->postageBoxTMWPC) ui->postageBoxTMWPC->clear();
    if (ui->countBoxTMWPC) ui->countBoxTMWPC->clear();
    if (ui->runInitialTMWPC) { ui->runInitialTMWPC->setEnabled(false); ui->runInitialTMWPC->setText(tr("RUN INITIAL")); }
    if (ui->openBulkMailerTMWPC) { ui->openBulkMailerTMWPC->setEnabled(false); ui->openBulkMailerTMWPC->setText(tr("Open Bulk Mailer")); }
    if (ui->runProofDataTMWPC) { ui->runProofDataTMWPC->setEnabled(false); ui->runProofDataTMWPC->setText(tr("RUN PROOF DATA")); }
    if (ui->openProofFileTMWPC) { ui->openProofFileTMWPC->setEnabled(false); ui->openProofFileTMWPC->setText(tr("OPEN PRINT FILE")); }
    if (ui->runWeeklyMergedTMWPC) { ui->runWeeklyMergedTMWPC->setEnabled(false); ui->runWeeklyMergedTMWPC->setText(tr("RUN WEEKLY MERGED")); }
    if (ui->openPrintFileTMWPC) { ui->openPrintFileTMWPC->setEnabled(false); ui->openPrintFileTMWPC->setText(tr("OPEN PRINT FILE")); }
    if (ui->runPostPrintTMWPC) { ui->runPostPrintTMWPC->setEnabled(false); ui->runPostPrintTMWPC->setText(tr("RUN POST PRINT")); }
    if (ui->lockButtonTMWPC) ui->lockButtonTMWPC->setChecked(false);
    if (ui->editButtonTMWPC) ui->editButtonTMWPC->setChecked(false);
    if (ui->postageLockTMWPC) ui->postageLockTMWPC->setChecked(false);
    if (ui->pacbTMWPC) ui->pacbTMWPC->setChecked(false);

    if (ui->terminalWindowTMWPC) ui->terminalWindowTMWPC->clear();
    // (intentionally keeping tracker model populated on close)
// Generic widget reset based on objectName prefixes
    const QStringList prefixes = { "jobNumberBox","postageBox","countBox","classDDbox","permitDDbox","yearDDbox","monthDDbox","weekDDbox","dropNumberddBox" };
    auto needsReset = [&](const QString& n){ for (auto& p : prefixes) if (n.startsWith(p)) return true; return false; };
    auto clearUnlockByName = [&](QObject* root){
        for (auto *e : root->findChildren<QLineEdit*>()) {
            if (needsReset(e->objectName())) { e->clear(); e->setReadOnly(false); e->setEnabled(true); }
        }
        for (auto *c : root->findChildren<QComboBox*>()) {
            if (needsReset(c->objectName())) {
                if (c->isEditable()) c->clearEditText();
                c->setCurrentIndex(-1);
                c->setEnabled(true);
            }
        }
        for (auto *sp : root->findChildren<QSpinBox*>()) {
            if (needsReset(sp->objectName())) { sp->setValue(sp->minimum()); sp->setEnabled(true); }
        }
        for (auto *dp : root->findChildren<QDoubleSpinBox*>()) {
            if (needsReset(dp->objectName())) { dp->setValue(dp->minimum()); dp->setEnabled(true); }
        }
    };
    clearUnlockByName(this);
}

void MainWindow::populateTMBrokenJobMenu()
{
    if (!openJobMenu) return;

    // Get all TMBROKEN jobs from database
    TMBrokenDBManager* dbManager = TMBrokenDBManager::instance();
    if (!dbManager) {
        QAction* errorAction = openJobMenu->addAction("Database not available");
        errorAction->setEnabled(false);
        logToTerminal("Open Job: TMBROKEN Database manager not available");
        return;
    }

    QList<QMap<QString, QString>> jobs = dbManager->getAllJobs();
    logToTerminal(QString("Open Job: Found %1 TMBROKEN jobs in database").arg(jobs.size()));

    if (jobs.isEmpty()) {
        QAction* noJobsAction = openJobMenu->addAction("No saved jobs found");
        noJobsAction->setEnabled(false);
        logToTerminal("Open Job: No TMBROKEN jobs found in database");
        return;
    }

    // Group jobs by year, then month (same structure as TMHEALTHY)
    QMap<QString, QList<QMap<QString, QString>>> groupedJobs;
    for (const auto& job : std::as_const(jobs)) {
        groupedJobs[job["year"]].append(job);
        logToTerminal(QString("Open Job: Adding TMBROKEN job %1 for %2-%3").arg(job["job_number"], job["year"], job["month"]));
    }

    // Create nested menu structure: Year -> Month Abbreviation (Job Number) - same as TMHEALTHY
    for (auto yearIt = groupedJobs.constBegin(); yearIt != groupedJobs.constEnd(); ++yearIt) {
        QMenu* yearMenu = openJobMenu->addMenu(yearIt.key());

        for (const auto& job : yearIt.value()) {
            // Convert month number to 3-letter abbreviation
            QString monthAbbrev = convertMonthToAbbreviation(job["month"]);
            QString actionText = QString("%1 (%2)").arg(monthAbbrev, job["job_number"]);

            QAction* jobAction = yearMenu->addAction(actionText);

            // Store job data in action for later use
            jobAction->setData(QStringList() << job["year"] << job["month"]);

            // Connect to load job function
            connect(jobAction, &QAction::triggered, this, [this, job]() {
                // CRITICAL FIX: Auto-close current job before opening new one
                if (m_tmBrokenController) {
                    m_tmBrokenController->autoSaveAndCloseCurrentJob();
                }
                loadTMBrokenJob(job["year"], job["month"]);
            });
        }
    }
}

void MainWindow::loadTMBrokenJob(const QString& year, const QString& month)
{
    if (m_tmBrokenController) {
        bool success = m_tmBrokenController->loadJob(year, month);
        if (success) {
            logToTerminal(QString("Loaded TMBROKEN job for %1-%2").arg(year, month));
        } else {
            logToTerminal(QString("Failed to load TMBROKEN job for %1-%2").arg(year, month));
        }
    }
}

void MainWindow::populateFHJobMenu()
{
    if (!openJobMenu) return;

    // Get all FH jobs from database
    FHDBManager* dbManager = FHDBManager::instance();
    if (!dbManager) {
        QAction* errorAction = openJobMenu->addAction("Database not available");
        errorAction->setEnabled(false);
        logToTerminal("Open Job: FH Database manager not available");
        return;
    }

    QList<QMap<QString, QString>> jobs = dbManager->getAllJobs();
    logToTerminal(QString("Open Job: Found %1 FH jobs in database").arg(jobs.size()));

    if (jobs.isEmpty()) {
        QAction* noJobsAction = openJobMenu->addAction("No saved jobs found");
        noJobsAction->setEnabled(false);
        logToTerminal("Open Job: No FH jobs found in database");
        return;
    }

    // ✅ Updated: Removed "week" references - FOUR HANDS uses year/month only
    // Group jobs by year, then month
    QMap<QString, QMap<QString, QList<QMap<QString, QString>>>> groupedJobs;
    for (const auto& job : std::as_const(jobs)) {
        groupedJobs[job["year"]][job["month"]].append(job);
        logToTerminal(QString("Open Job: Adding job %1 for %2-%3").arg(job["job_number"], job["year"], job["month"]));
    }

    // Create nested menu structure: Year -> Month -> Job#
    for (auto yearIt = groupedJobs.constBegin(); yearIt != groupedJobs.constEnd(); ++yearIt) {
        QMenu* yearMenu = openJobMenu->addMenu(yearIt.key());

        for (auto monthIt = yearIt.value().constBegin(); monthIt != yearIt.value().constEnd(); ++monthIt) {
            // Convert month number to 3-letter abbreviation
            QString monthAbbrev = convertMonthToAbbreviation(monthIt.key());
            QMenu* monthMenu = yearMenu->addMenu(monthAbbrev);

            for (const auto& job : monthIt.value()) {
                QString actionText = QString("Job %1").arg(job["job_number"]);

                QAction* jobAction = monthMenu->addAction(actionText);

                // Store job data in action for later use (no week for FOUR HANDS)
                jobAction->setData(QStringList() << job["year"] << job["month"]);

                // Connect to load job function
                connect(jobAction, &QAction::triggered, this, [this, job]() {
                    // CRITICAL FIX: Auto-close current job before opening new one
                    if (m_fhController) {
                        m_fhController->autoSaveAndCloseCurrentJob();
                    }
                    loadFHJob(job["year"], job["month"]);
                });
            }
        }
    }
}

void MainWindow::loadFHJob(const QString& year, const QString& month)
{
    if (m_fhController) {
        bool success = m_fhController->loadJob(year, month);
        if (success) {
            logToTerminal(QString("Loaded FH job for %1-%2").arg(year, month));
        } else {
            logToTerminal(QString("Failed to load FH job for %1-%2").arg(year, month));
        }
    }
}

void MainWindow::resetTMBrokenUI()
{
    // Reset TMBROKEN tab UI widgets to default state
    if (ui->jobNumberBoxTMBA) ui->jobNumberBoxTMBA->clear();
    if (ui->yearDDboxTMBA) ui->yearDDboxTMBA->setCurrentIndex(0);
    if (ui->monthDDboxTMBA) ui->monthDDboxTMBA->setCurrentIndex(0);
    if (ui->postageBoxTMBA) ui->postageBoxTMBA->clear();
    if (ui->countBoxTMBA) ui->countBoxTMBA->clear();
    if (ui->runInitialTMBA) { ui->runInitialTMBA->setEnabled(false); }
    if (ui->finalStepTMBA) { ui->finalStepTMBA->setEnabled(false); }
    if (ui->lockButtonTMBA) ui->lockButtonTMBA->setChecked(false);
    if (ui->editButtonTMBA) ui->editButtonTMBA->setChecked(false);
    if (ui->postageLockTMBA) ui->postageLockTMBA->setChecked(false);

    if (ui->terminalWindowTMBA) ui->terminalWindowTMBA->clear();
    if (m_tmBrokenController) {
        m_tmBrokenController->refreshTrackerTable();
    }

    // Generic widget reset based on objectName prefixes
    const QStringList prefixes = { "jobNumberBox","postage","yearDDbox","monthDDbox","weekDDbox","dropNumberddBox" };
    auto needsReset = [&](const QString& n){ for (auto& p : prefixes) if (n.startsWith(p)) return true; return false; };
    auto clearUnlockByName = [&](QObject* root){
        for (auto *e : root->findChildren<QLineEdit*>()) {
            if (needsReset(e->objectName())) { e->clear(); e->setReadOnly(false); e->setEnabled(true); }
        }
        for (auto *c : root->findChildren<QComboBox*>()) {
            if (needsReset(c->objectName())) {
                if (c->isEditable()) c->clearEditText();
                c->setCurrentIndex(-1);
                c->setEnabled(true);
            }
        }
        for (auto *sp : root->findChildren<QSpinBox*>()) {
            if (needsReset(sp->objectName())) { sp->setValue(sp->minimum()); sp->setEnabled(true); }
        }
        for (auto *dp : root->findChildren<QDoubleSpinBox*>()) {
            if (needsReset(dp->objectName())) { dp->setValue(dp->minimum()); dp->setEnabled(true); }
        }
    };
    clearUnlockByName(this);
}

void MainWindow::resetTMTermUI()
{
    // Reset TMTERM tab UI widgets to default state
    if (ui->jobNumberBoxTMTERM) ui->jobNumberBoxTMTERM->clear();
    if (ui->yearDDboxTMTERM) ui->yearDDboxTMTERM->setCurrentIndex(0);
    if (ui->monthDDboxTMTERM) ui->monthDDboxTMTERM->setCurrentIndex(0);
    if (ui->postageBoxTMTERM) ui->postageBoxTMTERM->clear();
    if (ui->countBoxTMTERM) ui->countBoxTMTERM->clear();
    if (ui->runInitialTMTERM) { ui->runInitialTMTERM->setEnabled(false); ui->runInitialTMTERM->setText(tr("Run Initial")); }
    if (ui->finalStepTMTERM) { ui->finalStepTMTERM->setEnabled(false); ui->finalStepTMTERM->setText(tr("Final Step")); }
    if (ui->lockButtonTMTERM) ui->lockButtonTMTERM->setChecked(false);
    if (ui->editButtonTMTERM) ui->editButtonTMTERM->setChecked(false);
    if (ui->postageLockTMTERM) ui->postageLockTMTERM->setChecked(false);

    if (ui->terminalWindowTMTERM) ui->terminalWindowTMTERM->clear();
    if (m_tmTermController) {
        m_tmTermController->refreshTrackerTable();
    }
    
    // Generic widget reset based on objectName prefixes
    const QStringList prefixes = { "jobNumberBox","postageBox","countBox","classDDbox","permitDDbox","yearDDbox","monthDDbox","weekDDbox","dropNumberddBox" };
    auto needsReset = [&](const QString& n){ for (auto& p : prefixes) if (n.startsWith(p)) return true; return false; };
    auto clearUnlockByName = [&](QObject* root){
        for (auto *e : root->findChildren<QLineEdit*>()) {
            if (needsReset(e->objectName())) { e->clear(); e->setReadOnly(false); e->setEnabled(true); }
        }
        for (auto *c : root->findChildren<QComboBox*>()) {
            if (needsReset(c->objectName())) {
                if (c->isEditable()) c->clearEditText();
                c->setCurrentIndex(-1);
                c->setEnabled(true);
            }
        }
        for (auto *sp : root->findChildren<QSpinBox*>()) {
            if (needsReset(sp->objectName())) { sp->setValue(sp->minimum()); sp->setEnabled(true); }
        }
        for (auto *dp : root->findChildren<QDoubleSpinBox*>()) {
            if (needsReset(dp->objectName())) { dp->setValue(dp->minimum()); dp->setEnabled(true); }
        }
    };
    clearUnlockByName(this);
}

void MainWindow::resetTMTarragonUI()
{
    // Reset TMTARRAGON tab UI widgets to default state
    if (ui->jobNumberBoxTMTH) ui->jobNumberBoxTMTH->clear();
    if (ui->yearDDboxTMTH) ui->yearDDboxTMTH->setCurrentIndex(0);
    if (ui->monthDDboxTMTH) ui->monthDDboxTMTH->setCurrentIndex(0);
    if (ui->dropNumberddBoxTMTH) ui->dropNumberddBoxTMTH->setCurrentIndex(0);
    if (ui->postageBoxTMTH) ui->postageBoxTMTH->clear();
    if (ui->countBoxTMTH) ui->countBoxTMTH->clear();
    if (ui->runInitialTMTH) { ui->runInitialTMTH->setEnabled(false); ui->runInitialTMTH->setText(tr("Run Initial")); }
    if (ui->finalStepTMTH) { ui->finalStepTMTH->setEnabled(false); ui->finalStepTMTH->setText(tr("Final Step")); }
    if (ui->lockButtonTMTH) ui->lockButtonTMTH->setChecked(false);
    if (ui->editButtonTMTH) ui->editButtonTMTH->setChecked(false);
    if (ui->postageLockTMTH) ui->postageLockTMTH->setChecked(false);

    if (ui->terminalWindowTMTH) ui->terminalWindowTMTH->clear();
    if (ui->trackerTMTH) {
        if (QAbstractItemModel* model = ui->trackerTMTH->model()) {
            if (QSqlTableModel* sqlModel = qobject_cast<QSqlTableModel*>(model)) {
                sqlModel->clear();
            }
        }
    }
    
    // Generic widget reset based on objectName prefixes
    const QStringList prefixes = { "jobNumberBox","postageBox","countBox","classDDbox","permitDDbox","yearDDbox","monthDDbox","weekDDbox","dropNumberddBox" };
    auto needsReset = [&](const QString& n){ for (auto& p : prefixes) if (n.startsWith(p)) return true; return false; };
    auto clearUnlockByName = [&](QObject* root){
        for (auto *e : root->findChildren<QLineEdit*>()) {
            if (needsReset(e->objectName())) { e->clear(); e->setReadOnly(false); e->setEnabled(true); }
        }
        for (auto *c : root->findChildren<QComboBox*>()) {
            if (needsReset(c->objectName())) {
                if (c->isEditable()) c->clearEditText();
                c->setCurrentIndex(-1);
                c->setEnabled(true);
            }
        }
        for (auto *sp : root->findChildren<QSpinBox*>()) {
            if (needsReset(sp->objectName())) { sp->setValue(sp->minimum()); sp->setEnabled(true); }
        }
        for (auto *dp : root->findChildren<QDoubleSpinBox*>()) {
            if (needsReset(dp->objectName())) { dp->setValue(dp->minimum()); dp->setEnabled(true); }
        }
    };
    clearUnlockByName(this);
}

void MainWindow::resetTMFLERUI()
{
    // Reset TMFLER tab UI widgets to default state
    if (ui->jobNumberBoxTMFLER) ui->jobNumberBoxTMFLER->clear();
    if (ui->yearDDboxTMFLER) ui->yearDDboxTMFLER->setCurrentIndex(0);
    if (ui->monthDDboxTMFLER) ui->monthDDboxTMFLER->setCurrentIndex(0);
    if (ui->postageBoxTMFLER) ui->postageBoxTMFLER->clear();
    if (ui->countBoxTMFLER) ui->countBoxTMFLER->clear();
    if (ui->runInitialTMFLER) { ui->runInitialTMFLER->setEnabled(false); ui->runInitialTMFLER->setText(tr("Run Initial")); }
    if (ui->finalStepTMFLER) { ui->finalStepTMFLER->setEnabled(false); ui->finalStepTMFLER->setText(tr("Final Step")); }
    if (ui->lockButtonTMFLER) ui->lockButtonTMFLER->setChecked(false);
    if (ui->editButtonTMFLER) ui->editButtonTMFLER->setChecked(false);
    if (ui->postageLockTMFLER) ui->postageLockTMFLER->setChecked(false);

    if (ui->terminalWindowTMFLER) ui->terminalWindowTMFLER->clear();
    if (ui->trackerTMFLER) {
        // Do not clear the model here; let the controller refresh to preserve headers
    }

    
    // Generic widget reset based on objectName prefixes
    const QStringList prefixes = { "jobNumberBox","postageBox","countBox","classDDbox","permitDDbox","yearDDbox","monthDDbox","weekDDbox","dropNumberddBox" };
    auto needsReset = [&](const QString& n){ for (auto& p : prefixes) if (n.startsWith(p)) return true; return false; };
    auto clearUnlockByName = [&](QObject* root){
        for (auto *e : root->findChildren<QLineEdit*>()) {
            if (needsReset(e->objectName())) { e->clear(); e->setReadOnly(false); e->setEnabled(true); }
        }
        for (auto *c : root->findChildren<QComboBox*>()) {
            if (needsReset(c->objectName())) {
                if (c->isEditable()) c->clearEditText();
                c->setCurrentIndex(-1);
                c->setEnabled(true);
            }
        }
        for (auto *sp : root->findChildren<QSpinBox*>()) {
            if (needsReset(sp->objectName())) { sp->setValue(sp->minimum()); sp->setEnabled(true); }
        }
        for (auto *dp : root->findChildren<QDoubleSpinBox*>()) {
            if (needsReset(dp->objectName())) { dp->setValue(dp->minimum()); dp->setEnabled(true); }
        }
    };
    clearUnlockByName(this);
    // Ensure tracker headers persist like TMFARM by refreshing via controller
    if (m_tmFlerController) m_tmFlerController->refreshTrackerTable();
}


void MainWindow::resetTMHealthyUI()
{
    // Reset TMHEALTHY tab UI widgets to default state
    if (ui->jobNumberBoxTMHB) ui->jobNumberBoxTMHB->clear();
    if (ui->yearDDboxTMHB) ui->yearDDboxTMHB->setCurrentIndex(0);
    if (ui->monthDDboxTMHB) ui->monthDDboxTMHB->setCurrentIndex(0);
    {
        QSignalBlocker b1(ui->postageBoxTMHB);
        QSignalBlocker b2(ui->countBoxTMHB);
        if (ui->postageBoxTMHB) ui->postageBoxTMHB->clear();
        if (ui->countBoxTMHB) ui->countBoxTMHB->clear();
    }
    if (ui->runInitialTMHB) { ui->runInitialTMHB->setEnabled(false); }
    if (ui->finalStepTMHB) { ui->finalStepTMHB->setEnabled(false); }
    if (ui->lockButtonTMHB) ui->lockButtonTMHB->setChecked(false);
    if (ui->editButtonTMHB) ui->editButtonTMHB->setChecked(false);
    if (ui->postageLockTMHB) ui->postageLockTMHB->setChecked(false);

    if (ui->terminalWindowTMHB) ui->terminalWindowTMHB->clear();
    if (ui->trackerTMHB) {
        // sqlModel->clear() removed per instructions
    }
    if (m_tmHealthyController) {
        m_tmHealthyController->refreshTrackerTable();
    }
    // Clear drop window if present
    if (ui->dropWindowTMHB) {
        // Assuming DropWindow has a method to clear its contents
        // This would need to be implemented based on DropWindow's interface
    }
    
    // Generic widget reset based on objectName prefixes
    const QStringList prefixes = { "jobNumberBox","postageBox","countBox","classDDbox","permitDDbox","yearDDbox","monthDDbox","weekDDbox","dropNumberddBox" };
    auto needsReset = [&](const QString& n){ for (auto& p : prefixes) if (n.startsWith(p)) return true; return false; };
    auto clearUnlockByName = [&](QObject* root){
        for (auto *e : root->findChildren<QLineEdit*>()) {
            if (needsReset(e->objectName())) { e->clear(); e->setReadOnly(false); e->setEnabled(true); }
        }
        for (auto *c : root->findChildren<QComboBox*>()) {
            if (needsReset(c->objectName())) {
                if (c->isEditable()) c->clearEditText();
                c->setCurrentIndex(-1);
                c->setEnabled(true);
            }
        }
        for (auto *sp : root->findChildren<QSpinBox*>()) {
            if (needsReset(sp->objectName())) { sp->setValue(sp->minimum()); sp->setEnabled(true); }
        }
        for (auto *dp : root->findChildren<QDoubleSpinBox*>()) {
            if (needsReset(dp->objectName())) { dp->setValue(dp->minimum()); dp->setEnabled(true); }
        }
    };
    clearUnlockByName(this);
}

void MainWindow::resetFHUI()
{
    // Reset FH tab UI widgets to default state
    if (ui->jobNumberBoxFH) ui->jobNumberBoxFH->clear();
    if (ui->yearDDboxFH) ui->yearDDboxFH->setCurrentIndex(0);
    if (ui->monthDDboxFH) ui->monthDDboxFH->setCurrentIndex(0);
    if (ui->dropNumberddBoxFH) ui->dropNumberddBoxFH->setCurrentIndex(0); // This is the week dropdown
    if (ui->runInitialFH) { ui->runInitialFH->setEnabled(false); }
    if (ui->finalStepFH) { ui->finalStepFH->setEnabled(false); }
    if (ui->lockButtonFH) ui->lockButtonFH->setChecked(false);

    if (ui->terminalWindowFH) ui->terminalWindowFH->clear();
    if (m_fhController) {
        m_fhController->refreshTrackerTable();
    }

    // Generic widget reset based on objectName prefixes
    const QStringList prefixes = { "jobNumberBox","postageBox","countBox","classDDbox","permitDDbox","yearDDbox","monthDDbox","weekDDbox","dropNumberddBox" };
    auto needsReset = [&](const QString& n){ for (auto& p : prefixes) if (n.startsWith(p)) return true; return false; };
    auto clearUnlockByName = [&](QObject* root){
        for (auto *e : root->findChildren<QLineEdit*>()) {
            if (needsReset(e->objectName())) { e->clear(); e->setReadOnly(false); e->setEnabled(true); }
        }
        for (auto *c : root->findChildren<QComboBox*>()) {
            if (needsReset(c->objectName())) {
                if (c->isEditable()) c->clearEditText();
                c->setCurrentIndex(-1);
                c->setEnabled(true);
            }
        }
        for (auto *sp : root->findChildren<QSpinBox*>()) {
            if (needsReset(sp->objectName())) { sp->setValue(sp->minimum()); sp->setEnabled(true); }
        }
        for (auto *dp : root->findChildren<QDoubleSpinBox*>()) {
            if (needsReset(dp->objectName())) { dp->setValue(dp->minimum()); dp->setEnabled(true); }
        }
    };
    clearUnlockByName(this);
}


void MainWindow::restartInactivityTimer() {
    if (!m_inactivityTimer) return;
    const int INACTIVITY_MS = 15 * 60 * 1000;
    m_inactivityTimer->start(INACTIVITY_MS);
}


bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    Q_UNUSED(obj);
    switch (event->type()) {
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseMove:
        case QEvent::Wheel:
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
            restartInactivityTimer();
            break;
        default:
            break;
    }
    return QMainWindow::eventFilter(obj, event);
}
