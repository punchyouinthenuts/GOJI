#include "filelocationsdialog.h"
#include "updatedialog.h"
#include "updatesettingsdialog.h"
#include "mainwindow.h"
#include "threadutils.h"
#include "ui_GOJI.h"
#include "countstabledialog.h"
#include "logging.h"
#include "logger.h"
#include "configmanager.h"
#include "errormanager.h"
#include "validator.h"
#include <QLineEdit>
#include <QCheckBox>
#include <QMessageBox>
#include <QDateTime>
#include <QDebug>
#include <QSignalBlocker>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QFile>
#include <QSqlQuery>
#include <QSqlError>
#include <QClipboard>
#include <QApplication>
#include <QLocale>
#include <QRegularExpression>
#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCloseEvent>
#include <QTextStream>
#include <QFontDatabase>
#include <QThread>
#include <QtConcurrent>
#include <QSet>
#include <QMap>
#include <QList>
#include <QMenu>
#include <QAction>
#include <algorithm>

class CountsTableDialog;  // Add this line

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
    weeklyMenu(nullptr),
    validator(nullptr),
    m_printWatcher(nullptr),
    m_inactivityTimer(nullptr),
    m_currentInstructionState(InstructionState::None),
    // Initialize all Bug Nudge related pointers to null
    m_bugNudgeMenu(nullptr),
    m_forcePreProofAction(nullptr),
    m_forceProofFilesAction(nullptr),
    m_forcePostProofAction(nullptr),
    m_forceProofApprovalAction(nullptr),
    m_forcePrintFilesAction(nullptr),
    m_forcePostPrintAction(nullptr)
{
    ::logMessage("Entering MainWindow constructor...");

    try {
        ::logMessage("Initializing QSettings...");
        // Use ConfigManager but keep m_settings pointer for compatibility
        ConfigManager& config = ConfigManager::instance();
        m_settings = config.getSettings();

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
        ::logMessage("QSettings initialized.");

        ::logMessage("Setting up UI...");
        ui->setupUi(this);
        setWindowTitle(tr("Goji v%1").arg(VERSION));
        ::logMessage("UI setup complete.");

        // Initialize database manager
        ::logMessage("Setting up database directory...");
        QString defaultDbDirPath;
#ifdef QT_DEBUG
        defaultDbDirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Goji/SQL/debug";
#else
        defaultDbDirPath = "C:/Goji/database";
#endif
        QString dbDirPath = m_settings->value("DatabasePath", defaultDbDirPath).toString();
        QDir dbDir(dbDirPath);
        if (!dbDir.exists()) {
            ::logMessage("Creating database directory: " + dbDirPath);
            if (!dbDir.mkpath(".")) {
                ::logMessage("Failed to create database directory: " + dbDirPath);
                throw std::runtime_error("Failed to create database directory");
            }
        }
        QString dbPath = dbDirPath + "/jobs.db";
        ::logMessage("Database directory setup complete: " + dbPath);

        ::logMessage("Initializing DatabaseManager...");
        m_dbManager = new DatabaseManager(dbPath);
        if (!m_dbManager->initialize()) {
            ::logMessage("Failed to initialize database.");
            throw std::runtime_error("Failed to initialize database");
        }
        ::logMessage("DatabaseManager initialized.");

        ::logMessage("Creating managers and controllers...");
        m_fileManager = new FileSystemManager(m_settings);
        m_scriptRunner = new ScriptRunner(this);
        m_jobController = new JobController(m_dbManager, m_fileManager, m_scriptRunner, m_settings, this);
        m_updateManager = new UpdateManager(m_settings, this);
        ::logMessage("Managers and controllers created.");

        ::logMessage("Connecting UpdateManager signals...");
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
                    ::logMessage(success ? "Update installation initiated." : "Update installation failed.");
                });
        connect(m_updateManager, &UpdateManager::errorOccurred, this,
                [this](const QString& error) {
                    logToTerminal(tr("Update error: %1").arg(error));
                });
        ::logMessage("UpdateManager signals connected.");

        ::logMessage("Checking for updates...");
        bool checkUpdatesOnStartup = m_settings->value("Updates/CheckOnStartup", true).toBool();
        if (checkUpdatesOnStartup) {
            QDateTime lastCheck = m_settings->value("Updates/LastCheckTime").toDateTime();
            QDateTime currentTime = QDateTime::currentDateTime();
            int checkInterval = m_settings->value("Updates/CheckIntervalDays", 1).toInt();
            if (!lastCheck.isValid() || lastCheck.daysTo(currentTime) >= checkInterval) {
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
        ::logMessage("Update check setup complete.");

        ::logMessage("Setting up UI elements...");
        setupUi();
        setupSignalSlots();
        initializeValidators();
        setupMenus();

        // Set up other elements first, handle Bug Nudge menu with special care
        setupRegenCheckboxes();
        initWatchersAndTimers();

        // Initialize instructions before setting up the Bug Nudge menu
        ::logMessage("Initializing instructions...");
        initializeInstructions();
        ::logMessage("Instructions initialized.");

        // Set current job type before setting up Bug Nudge menu
        ::logMessage("Setting current job type...");
        currentJobType = "RAC WEEKLY";
        if (currentJobType == "RAC WEEKLY") {
            m_currentInstructionState = InstructionState::Default;
            loadInstructionContent(m_currentInstructionState);
        }
        ::logMessage("Current job type set.");

        // Set up Bug Nudge menu last, after everything else is initialized
        ::logMessage("Setting up Bug Nudge menu...");
        setupBugNudgeMenu();
        ::logMessage("Bug Nudge menu setup complete.");

        ::logMessage("UI elements setup complete.");

        ::logMessage("Logging startup...");
        logToTerminal(tr("Goji started: %1").arg(QDateTime::currentDateTime().toString()));
        ::logMessage("MainWindow constructor finished.");
    }
    catch (const std::exception& e) {
        ::logMessage(QString("Critical error in MainWindow constructor: %1").arg(e.what()));
        QMessageBox::critical(this, "Startup Error",
                              QString("A critical error occurred during application startup: %1").arg(e.what()));
        throw; // Re-throw to be handled by main()
    }
    catch (...) {
        ::logMessage("Unknown critical error in MainWindow constructor");
        QMessageBox::critical(this, "Startup Error",
                              "An unknown critical error occurred during application startup");
        throw; // Re-throw to be handled by main()
    }
}

MainWindow::~MainWindow()
{
    ::logMessage("Destroying MainWindow...");
    delete ui;
    delete m_dbManager;
    delete m_fileManager;
    delete m_scriptRunner;
    delete m_jobController;
    delete m_updateManager;
    delete openJobMenu;
    delete weeklyMenu;
    delete m_bugNudgeMenu;
    delete m_forcePreProofAction;
    delete m_forceProofFilesAction;
    delete m_forcePostProofAction;
    delete m_forceProofApprovalAction;
    delete m_forcePrintFilesAction;
    delete m_forcePostPrintAction;
    delete validator;
    delete m_printWatcher;
    delete m_inactivityTimer;
    ::logMessage("MainWindow destroyed.");
}

void MainWindow::onProofDDboxChanged(const QString &text)
{
    logToTerminal("Proof dropdown changed to: " + text);
}

void MainWindow::onPrintDDboxChanged(const QString &text)
{
    logToTerminal("Print dropdown changed to: " + text);
}

void MainWindow::onYearDDboxChanged(const QString &text)
{
    logToTerminal("Year dropdown changed to: " + text);
    // No need to repopulate month dropdown as years don't affect month options
}

void MainWindow::onMonthDDboxChanged(const QString &text)
{
    logToTerminal("Month dropdown changed to: " + text);
    populateWeekDDbox();
}

void MainWindow::onWeekDDboxChanged(const QString &text)
{
    logToTerminal("Week dropdown changed to: " + text);
}

void MainWindow::onLockButtonToggled(bool checked)
{
    if (currentJobType != "RAC WEEKLY") return;

    if (checked) {
        // Validate job data before locking
        bool hasEmptyFields = false;

        if (ui->cbcJobNumber->text().isEmpty() ||
            ui->excJobNumber->text().isEmpty() ||
            ui->inactiveJobNumber->text().isEmpty() ||
            ui->ncwoJobNumber->text().isEmpty() ||
            ui->prepifJobNumber->text().isEmpty() ||
            ui->yearDDbox->currentText().isEmpty() ||
            ui->monthDDbox->currentText().isEmpty() ||
            ui->weekDDbox->currentText().isEmpty()) {
            hasEmptyFields = true;
        }

        if (hasEmptyFields) {
            QMessageBox::warning(this, tr("Empty Fields"),
                                 tr("Please fill in all job numbers and select year, month, and week."));
            ui->lockButton->setChecked(false);
            return;
        }

        m_jobController->setJobDataLocked(true);
        ui->lockButton->setIcon(QIcon(":/lock_locked.svg"));
        logToTerminal("Job data locked.");
    } else {
        if (m_jobController->isJobSaved() &&
            QMessageBox::question(this, tr("Unlock Data"),
                                  tr("Are you sure you want to unlock the job data?"),
                                  QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
            ui->lockButton->setChecked(true);
            return;
        }

        m_jobController->setJobDataLocked(false);
        ui->lockButton->setIcon(QIcon(":/lock_unlocked.svg"));
        logToTerminal("Job data unlocked.");
    }

    updateWidgetStatesBasedOnJobState();
}

void MainWindow::onEditButtonToggled(bool checked)
{
    if (currentJobType != "RAC WEEKLY") return;

    if (!m_jobController->isJobDataLocked()) {
        ui->editButton->setChecked(false);
        QMessageBox::information(this, tr("Edit Mode"),
                                 tr("Please lock job data before enabling edit mode."));
        return;
    }

    if (checked) {
        logToTerminal("Edit mode enabled.");
    } else {
        logToTerminal("Edit mode disabled.");
    }

    updateWidgetStatesBasedOnJobState();
}

void MainWindow::onProofRegenToggled(bool checked)
{
    if (currentJobType != "RAC WEEKLY") return;

    if (!m_jobController->isJobDataLocked()) {
        ui->proofRegen->setChecked(false);
        QMessageBox::information(this, tr("Proof Regeneration"),
                                 tr("Please lock job data before enabling proof regeneration mode."));
        return;
    }

    m_jobController->setProofRegenMode(checked);

    if (checked) {
        logToTerminal("Proof regeneration mode enabled.");
        ui->regenTab->setEnabled(true);
    } else {
        logToTerminal("Proof regeneration mode disabled.");
        ui->regenTab->setEnabled(false);
    }

    updateWidgetStatesBasedOnJobState();
}

void MainWindow::onPostageLockToggled(bool checked)
{
    if (currentJobType != "RAC WEEKLY") return;

    if (!m_jobController->isJobDataLocked()) {
        ui->postageLock->setChecked(false);
        QMessageBox::information(this, tr("Postage Lock"),
                                 tr("Please lock job data before locking postage values."));
        return;
    }

    // Check if all postage fields are filled
    if (checked) {
        QList<QLineEdit*> postageFields;
        postageFields << ui->cbc2Postage << ui->cbc3Postage << ui->excPostage
                      << ui->inactivePOPostage << ui->inactivePUPostage
                      << ui->ncwo1APostage << ui->ncwo2APostage << ui->ncwo1APPostage
                      << ui->ncwo2APPostage << ui->prepifPostage;

        bool hasEmptyPostage = false;
        for (QLineEdit* field : postageFields) {
            if (field->text().trimmed().isEmpty()) {
                hasEmptyPostage = true;
                break;
            }
        }

        if (hasEmptyPostage) {
            if (QMessageBox::question(this, tr("Empty Postage Fields"),
                                      tr("Some postage fields are empty. Do you want to continue?"),
                                      QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
                ui->postageLock->setChecked(false);
                return;
            }
        }
    }

    m_jobController->setPostageLocked(checked);

    if (checked) {
        logToTerminal("Postage values locked.");
    } else {
        logToTerminal("Postage values unlocked.");
    }

    updateWidgetStatesBasedOnJobState();
}

void MainWindow::onAllCBcheckStateChanged(int state)
{
    if (currentJobType != "RAC WEEKLY") return;

    // Block signals to prevent recursive signal handling
    for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
        if (!it.value()) continue;
        QSignalBlocker blocker(it.value());
        it.value()->setChecked(state == Qt::Checked);
    }

    if (state == Qt::Checked) {
        JobData* job = m_jobController->currentJob();
        if (job) {
            job->step6_complete = 1;
            m_jobController->saveJob();
            updateLEDs();
            updateWidgetStatesBasedOnJobState();
            updateBugNudgeMenu();
            updateInstructions();
            logToTerminal("All proofs marked as approved.");
        }
    } else {
        JobData* job = m_jobController->currentJob();
        if (job) {
            job->step6_complete = 0;
            m_jobController->saveJob();
            updateLEDs();
            updateWidgetStatesBasedOnJobState();
            updateBugNudgeMenu();
            updateInstructions();
            logToTerminal("Proofs approval status cleared.");
        }
    }
}

void MainWindow::updateAllCBState()
{
    if (currentJobType != "RAC WEEKLY") return;

    bool allChecked = true;
    bool anyChecked = false;

    for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
        if (!it.value()) continue;
        if (it.value()->isChecked()) {
            anyChecked = true;
        } else {
            allChecked = false;
        }
    }

    QSignalBlocker blocker(ui->allCB);
    if (allChecked) {
        ui->allCB->setCheckState(Qt::Checked);
        // Update job data to mark step6 as complete
        JobData* job = m_jobController->currentJob();
        if (job) {
            job->step6_complete = 1;
            m_jobController->saveJob();
            updateLEDs();
            updateWidgetStatesBasedOnJobState();
            updateBugNudgeMenu();
            updateInstructions();
        }
    } else if (anyChecked) {
        ui->allCB->setCheckState(Qt::PartiallyChecked);
    } else {
        ui->allCB->setCheckState(Qt::Unchecked);
        // Update job data to mark step6 as incomplete
        JobData* job = m_jobController->currentJob();
        if (job) {
            job->step6_complete = 0;
            m_jobController->saveJob();
            updateLEDs();
            updateWidgetStatesBasedOnJobState();
            updateBugNudgeMenu();
            updateInstructions();
        }
    }
}

void MainWindow::initializeInstructions() {
    ::logMessage("Initializing instructions...");

    // Load the Iosevka font if not already loaded
    QString fontPath = "C:/Users/JCox/AppData/Local/Microsoft/Windows/Fonts/IosevkaCustom-Regular.ttf";
    ::logMessage("Loading font: " + fontPath);
    if (QFile::exists(fontPath)) {
        int fontId = QFontDatabase::addApplicationFont(fontPath);
        if (fontId == -1) {
            ::logMessage("Failed to load font: " + fontPath);
        } else {
            ::logMessage("Font loaded successfully.");
        }
    } else {
        ::logMessage("Font file not found: " + fontPath);
    }

    // Set the font for the textBrowser
    ::logMessage("Setting textBrowser font...");
    QFont iosevkaFont("Iosevka", 11);
    ui->textBrowser->setFont(iosevkaFont);
    ::logMessage("textBrowser font set.");

    // Map instruction states to their resource paths
    ::logMessage("Mapping instruction files...");
    m_instructionFiles[InstructionState::None] = ":/resources/instructions/none.html";
    m_instructionFiles[InstructionState::Default] = ":/resources/instructions/default.html";
    m_instructionFiles[InstructionState::Initial] = ":/resources/instructions/initial.html";
    m_instructionFiles[InstructionState::PreProof] = ":/resources/instructions/preproof.html";
    m_instructionFiles[InstructionState::PostProof] = ":/resources/instructions/postproof.html";
    m_instructionFiles[InstructionState::Final] = ":/resources/instructions/final.html";
    ::logMessage("Instruction files mapped.");

    // Load default instructions
    ::logMessage("Loading default instructions...");
    m_currentInstructionState = InstructionState::Default;
    loadInstructionContent(m_currentInstructionState);
    ::logMessage("Default instructions loaded.");
}

void MainWindow::loadInstructionContent(InstructionState state)
{
    ::logMessage("Loading instruction content for state: " + QString::number(static_cast<int>(state)));

    if (state == InstructionState::None) {
        ::logMessage("Clearing textBrowser for None state.");
        if (ui && ui->textBrowser) {
            ui->textBrowser->clear();
        }
        return;
    }

    if (!m_instructionFiles.contains(state)) {
        ::logMessage("Error: No instruction file found for state: " + QString::number(static_cast<int>(state)));
        return;
    }

    QString filePath = m_instructionFiles[state];
    ::logMessage("Loading instruction file: " + filePath);
    QFile file(filePath);

    if (!file.exists()) {
        ::logMessage("Error: Instruction file not found: " + filePath);
        return;
    }

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = file.readAll();
        if (ui && ui->textBrowser) {
            ui->textBrowser->setHtml(content);
        }
        file.close();
        ::logMessage("Instruction file loaded: " + filePath);
    } else {
        ::logMessage("Error: Could not open instruction file: " + filePath);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    ::logMessage("Handling close event...");
    if (closeAllJobs()) {
        ::logMessage("All jobs closed successfully.");
        event->accept();
    } else {
        ::logMessage("Failed to close jobs.");
        event->ignore();
    }
}

bool MainWindow::closeAllJobs()
{
    ::logMessage("Closing all jobs...");
    if (m_jobController->isJobSaved()) {
        try {
            bool success = m_jobController->saveJob();
            if (!success) {
                ::logMessage("Error saving job.");
                return false;
            }
            success = m_jobController->closeJob();
            if (!success) {
                ::logMessage("Error closing job.");
                return false;
            }
        }
        catch (const std::exception& e) {
            ::logMessage("Fatal error closing job: " + QString(e.what()));
            return false;
        }
    }
    ::logMessage("All jobs closed.");
    return true;
}

void MainWindow::setupUi()
{
    ::logMessage("Setting up UI elements...");
    ui->regenTab->setCurrentIndex(0);
    QWidget::setTabOrder(ui->cbcJobNumber, ui->excJobNumber);
    QWidget::setTabOrder(ui->excJobNumber, ui->inactiveJobNumber);
    QWidget::setTabOrder(ui->inactiveJobNumber, ui->ncwoJobNumber);
    QWidget::setTabOrder(ui->ncwoJobNumber, ui->prepifJobNumber);
    QWidget::setTabOrder(ui->prepifJobNumber, ui->cbc2Postage);
    QWidget::setTabOrder(ui->cbc2Postage, ui->cbc3Postage);
    QWidget::setTabOrder(ui->cbc3Postage, ui->excPostage);
    QWidget::setTabOrder(ui->excPostage, ui->inactivePOPostage);
    QWidget::setTabOrder(ui->inactivePOPostage, ui->inactivePUPostage);
    QWidget::setTabOrder(ui->inactivePUPostage, ui->ncwo1APostage);
    QWidget::setTabOrder(ui->ncwo1APostage, ui->ncwo1APPostage);
    QWidget::setTabOrder(ui->ncwo1APPostage, ui->ncwo2APostage);
    QWidget::setTabOrder(ui->ncwo2APostage, ui->ncwo2APPostage);
    QWidget::setTabOrder(ui->ncwo2APPostage, ui->prepifPostage);

    ui->cbc2Postage->setPlaceholderText(tr("CBC2"));
    ui->cbc3Postage->setPlaceholderText(tr("CBC3"));
    ui->excPostage->setPlaceholderText(tr("EXC"));
    ui->inactivePOPostage->setPlaceholderText(tr("A-PO"));
    ui->inactivePUPostage->setPlaceholderText(tr("A-PU"));
    ui->ncwo1APostage->setPlaceholderText(tr("1-A"));
    ui->ncwo2APostage->setPlaceholderText(tr("2-A"));
    ui->ncwo1APPostage->setPlaceholderText(tr("1-AP"));
    ui->ncwo2APPostage->setPlaceholderText(tr("2-AP"));
    ui->prepifPostage->setPlaceholderText(tr("PREPIF"));

    const int currentYear = QDate::currentDate().year();
    ui->yearDDbox->addItem(QString::number(currentYear - 1));
    ui->yearDDbox->addItem(QString::number(currentYear));
    ui->yearDDbox->addItem(QString::number(currentYear + 1));

    ui->progressBarWeekly->setRange(0, 100);
    ui->progressBarWeekly->setValue(0);

    updateLEDs();
    updateWidgetStatesBasedOnJobState();
    ::logMessage("UI elements setup complete.");
}

void MainWindow::initializeValidators()
{
    ::logMessage("Initializing validators...");
    validator = new QRegularExpressionValidator(QRegularExpression("[0-9]*\\.?[0-9]*"));
    QList<QLineEdit*> postageLineEdits;
    postageLineEdits << ui->cbc2Postage << ui->cbc3Postage << ui->excPostage << ui->inactivePOPostage
                     << ui->inactivePUPostage << ui->ncwo1APostage << ui->ncwo2APostage
                     << ui->ncwo1APPostage << ui->ncwo2APPostage << ui->prepifPostage;
    for (QLineEdit *lineEdit : postageLineEdits) {
        lineEdit->setValidator(validator);
        connect(lineEdit, &QLineEdit::editingFinished, this, &MainWindow::formatCurrencyOnFinish);
    }
    ::logMessage("Validators initialized.");
}

void MainWindow::setupMenus()
{
    ::logMessage("Setting up menus...");
    openJobMenu = new QMenu(tr("Open Job"));
    weeklyMenu = openJobMenu->addMenu(tr("Weekly"));
    connect(weeklyMenu, &QMenu::aboutToShow, this, &MainWindow::buildWeeklyMenu);
    ui->menuFile->insertMenu(ui->actionSave_Job, openJobMenu);

    QMenu* settingsMenu = ui->menubar->addMenu(tr("Settings"));
    QAction* updateSettingsAction = new QAction(tr("Update Settings"));
    connect(updateSettingsAction, &QAction::triggered, this, &MainWindow::onUpdateSettingsTriggered);
    settingsMenu->addAction(updateSettingsAction);

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        currentJobType = ui->tabWidget->tabText(index);
        openJobMenu->setEnabled(currentJobType == "RAC WEEKLY");
        ui->actionSave_Job->setEnabled(currentJobType == "RAC WEEKLY");
        ui->actionClose_Job->setEnabled(currentJobType == "RAC WEEKLY");
        if (currentJobType == "RAC WEEKLY" && (!m_jobController || !m_jobController->isJobSaved())) {
            m_currentInstructionState = InstructionState::Default;
            loadInstructionContent(m_currentInstructionState);
        } else if (currentJobType != "RAC WEEKLY") {
            m_currentInstructionState = InstructionState::None;
            loadInstructionContent(m_currentInstructionState);
        }
    });

    QMenu* manageScriptsMenu = ui->menuInput->findChild<QMenu*>("menuManage_Scripts");
    if (manageScriptsMenu) {
        manageScriptsMenu->clear();
        QMap<QString, QVariant> scriptDirs;
        QList<QPair<QString, QString>> racSubmenus = {
            {"Weekly", "C:/Goji/Scripts/RAC/WEEKLIES"},
            {"Monthly", "C:/Goji/Scripts/RAC/MONTHLY"},
            {"Quarterly", "C:/Goji/Scripts/RAC/SWEEPS"},
            {"Bi-Annual", "C:/Goji/Scripts/RAC/PCE"}
        };
        scriptDirs["RAC"] = QVariant::fromValue(racSubmenus);
        scriptDirs["Trachmar"] = QVariant::fromValue(QMap<QString, QString>{
            {"Weekly PC", "C:/Goji/Scripts/TRACHMAR/WEEKLY PC"},
            {"Weekly Packets/IDO", "C:/Goji/Scripts/TRACHMAR/WEEKLY PACKET & IDO"},
            {"Term", "C:/Goji/Scripts/TRACHMAR/TERM"}
        });

        for (auto it = scriptDirs.constBegin(); it != scriptDirs.constEnd(); ++it) {
            QMenu* parentMenu = manageScriptsMenu->addMenu(it.key());
            if (it.key() == "RAC") {
                const auto& submenus = it.value().value<QList<QPair<QString, QString>>>();
                for (const auto& submenu : submenus) {
                    QMenu* subMenu = parentMenu->addMenu(submenu.first);
                    populateScriptMenu(subMenu, submenu.second);
                }
            } else {
                const auto& subdirs = it.value().value<QMap<QString, QString>>();
                for (auto subIt = subdirs.constBegin(); subIt != subdirs.constEnd(); ++subIt) {
                    QMenu* subMenu = parentMenu->addMenu(subIt.key());
                    populateScriptMenu(subMenu, subIt.value());
                }
            }
        }
    }
    ::logMessage("Menus setup complete.");
}

void MainWindow::setupBugNudgeMenu()
{
    ::logMessage("Setting up Bug Nudge menu...");

    // Check if UI components are available
    if (!ui || !ui->menuTools) {
        ::logMessage("Error: ui or ui->menuTools is null");
        return; // Exit early if UI components aren't ready
    }

    // Create or find the Bug Nudge action
    QAction* bugNudgeAction = nullptr;
    bool bugNudgeExists = false;

    // First check if the menu already exists
    for (QAction* action : ui->menuTools->actions()) {
        if (action && (action->text() == "Bug Nudge" || action->objectName() == "actionBug_Nudge")) {
            bugNudgeAction = action;
            bugNudgeExists = true;
            ::logMessage("Found existing Bug Nudge action in menuTools");
            break;
        }
    }

    if (!bugNudgeExists) {
        bugNudgeAction = new QAction(tr("Bug Nudge"), this);
        if (!bugNudgeAction) {
            ::logMessage("Error: Failed to create Bug Nudge action");
            return;
        }
        ui->menuTools->addAction(bugNudgeAction);
        ::logMessage("Added Bug Nudge action to menuTools");
    }

    // Create the menu
    if (m_bugNudgeMenu) {
        ::logMessage("Reusing existing Bug Nudge menu");
        m_bugNudgeMenu->clear();
    } else {
        m_bugNudgeMenu = new QMenu(this);
        if (!m_bugNudgeMenu) {
            ::logMessage("Error: Failed to create Bug Nudge menu");
            return;
        }
        ::logMessage("Created new Bug Nudge menu");
    }

    bugNudgeAction->setMenu(m_bugNudgeMenu);

    // Clean up any existing actions
    if (m_forcePreProofAction) {
        disconnect(m_forcePreProofAction, &QAction::triggered, this, &MainWindow::onForcePreProofComplete);
        delete m_forcePreProofAction;
    }
    if (m_forceProofFilesAction) {
        disconnect(m_forceProofFilesAction, &QAction::triggered, this, &MainWindow::onForceProofFilesComplete);
        delete m_forceProofFilesAction;
    }
    if (m_forcePostProofAction) {
        disconnect(m_forcePostProofAction, &QAction::triggered, this, &MainWindow::onForcePostProofComplete);
        delete m_forcePostProofAction;
    }
    if (m_forceProofApprovalAction) {
        disconnect(m_forceProofApprovalAction, &QAction::triggered, this, &MainWindow::onForceProofApprovalComplete);
        delete m_forceProofApprovalAction;
    }
    if (m_forcePrintFilesAction) {
        disconnect(m_forcePrintFilesAction, &QAction::triggered, this, &MainWindow::onForcePrintFilesComplete);
        delete m_forcePrintFilesAction;
    }
    if (m_forcePostPrintAction) {
        disconnect(m_forcePostPrintAction, &QAction::triggered, this, &MainWindow::onForcePostPrintComplete);
        delete m_forcePostPrintAction;
    }

    // Create menu actions
    m_forcePreProofAction = new QAction(tr("PRE PROOF"), this);
    m_forceProofFilesAction = new QAction(tr("PROOF FILES GENERATED"), this);
    m_forcePostProofAction = new QAction(tr("POST PROOF"), this);
    m_forceProofApprovalAction = new QAction(tr("PROOFS APPROVED"), this);
    m_forcePrintFilesAction = new QAction(tr("PRINT FILES GENERATED"), this);
    m_forcePostPrintAction = new QAction(tr("POST PRINT"), this);

    // Check if all actions were created
    if (!m_forcePreProofAction || !m_forceProofFilesAction ||
        !m_forcePostProofAction || !m_forceProofApprovalAction ||
        !m_forcePrintFilesAction || !m_forcePostPrintAction) {

        ::logMessage("Error: Failed to create one or more Bug Nudge actions");

        // Clean up any actions that were created
        if (m_forcePreProofAction) delete m_forcePreProofAction;
        if (m_forceProofFilesAction) delete m_forceProofFilesAction;
        if (m_forcePostProofAction) delete m_forcePostProofAction;
        if (m_forceProofApprovalAction) delete m_forceProofApprovalAction;
        if (m_forcePrintFilesAction) delete m_forcePrintFilesAction;
        if (m_forcePostPrintAction) delete m_forcePostPrintAction;

        // Reset all pointers to null
        m_forcePreProofAction = nullptr;
        m_forceProofFilesAction = nullptr;
        m_forcePostProofAction = nullptr;
        m_forceProofApprovalAction = nullptr;
        m_forcePrintFilesAction = nullptr;
        m_forcePostPrintAction = nullptr;

        return;
    }

    // Add actions to menu
    m_bugNudgeMenu->addAction(m_forcePreProofAction);
    m_bugNudgeMenu->addAction(m_forceProofFilesAction);
    m_bugNudgeMenu->addAction(m_forcePostProofAction);
    m_bugNudgeMenu->addAction(m_forceProofApprovalAction);
    m_bugNudgeMenu->addAction(m_forcePrintFilesAction);
    m_bugNudgeMenu->addAction(m_forcePostPrintAction);

    // Connect signals
    connect(m_forcePreProofAction, &QAction::triggered, this, &MainWindow::onForcePreProofComplete);
    connect(m_forceProofFilesAction, &QAction::triggered, this, &MainWindow::onForceProofFilesComplete);
    connect(m_forcePostProofAction, &QAction::triggered, this, &MainWindow::onForcePostProofComplete);
    connect(m_forceProofApprovalAction, &QAction::triggered, this, &MainWindow::onForceProofApprovalComplete);
    connect(m_forcePrintFilesAction, &QAction::triggered, this, &MainWindow::onForcePrintFilesComplete);
    connect(m_forcePostPrintAction, &QAction::triggered, this, &MainWindow::onForcePostPrintComplete);

    // Connect to tab changes to update the menu state
    if (ui->tabWidget) {
        connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MainWindow::updateBugNudgeMenu);
    }

    updateBugNudgeMenu();
    ::logMessage("Bug Nudge menu setup completed.");
}

void MainWindow::setupRegenCheckboxes()
{
    ::logMessage("Setting up regeneration checkboxes...");

    // Check for null pointers before inserting into regenCheckboxes map
    if (!ui->cbcCB) {
        ::logMessage("Error: ui->cbcCB is null");
    } else {
        regenCheckboxes.insert("CBC", ui->cbcCB);
    }
    if (!ui->excCB) {
        ::logMessage("Error: ui->excCB is null");
    } else {
        regenCheckboxes.insert("EXC", ui->excCB);
    }
    if (!ui->inactiveCB) {
        ::logMessage("Error: ui->inactiveCB is null");
    } else {
        regenCheckboxes.insert("INACTIVE", ui->inactiveCB);
    }
    if (!ui->ncwoCB) {
        ::logMessage("Error: ui->ncwoCB is null");
    } else {
        regenCheckboxes.insert("NCWO", ui->ncwoCB);
    }
    if (!ui->prepifCB) {
        ::logMessage("Error: ui->prepifCB is null");
    } else {
        regenCheckboxes.insert("PREPIF", ui->prepifCB);
    }

    // Check for null pointers before inserting into checkboxFileMap
    if (!ui->regenCBC2CB) {
        ::logMessage("Error: ui->regenCBC2CB is null");
    } else {
        checkboxFileMap.insert(ui->regenCBC2CB, QPair<QString, QString>("CBC", "CBC2 PROOF.pdf"));
    }
    if (!ui->regenCBC3CB) {
        ::logMessage("Error: ui->regenCBC3CB is null");
    } else {
        checkboxFileMap.insert(ui->regenCBC3CB, QPair<QString, QString>("CBC", "CBC3 PROOF.pdf"));
    }
    if (!ui->regenEXCCB) {
        ::logMessage("Error: ui->regenEXCCB is null");
    } else {
        checkboxFileMap.insert(ui->regenEXCCB, QPair<QString, QString>("EXC", "EXC PROOF.pdf"));
    }
    if (!ui->regenAPOCB) {
        ::logMessage("Error: ui->regenAPOCB is null");
    } else {
        checkboxFileMap.insert(ui->regenAPOCB, QPair<QString, QString>("INACTIVE", "INACTIVE A-PO PROOF.pdf"));
    }
    if (!ui->regenAPUCB) {
        ::logMessage("Error: ui->regenAPUCB is null");
    } else {
        checkboxFileMap.insert(ui->regenAPUCB, QPair<QString, QString>("INACTIVE", "INACTIVE A-PU PROOF.pdf"));
    }
    if (!ui->regenATPOCB) {
        ::logMessage("Error: ui->regenATPOCB is null");
    } else {
        checkboxFileMap.insert(ui->regenATPOCB, QPair<QString, QString>("INACTIVE", "INACTIVE AT-PO PROOF.pdf"));
    }
    if (!ui->regenATPUCB) {
        ::logMessage("Error: ui->regenATPUCB is null");
    } else {
        checkboxFileMap.insert(ui->regenATPUCB, QPair<QString, QString>("INACTIVE", "INACTIVE AT-PU PROOF.pdf"));
    }
    if (!ui->regenPRPOCB) {
        ::logMessage("Error: ui->regenPRPOCB is null");
    } else {
        checkboxFileMap.insert(ui->regenPRPOCB, QPair<QString, QString>("INACTIVE", "INACTIVE PR-PO PROOF.pdf"));
    }
    if (!ui->regenPRPUCB) {
        ::logMessage("Error: ui->regenPRPUCB is null");
    } else {
        checkboxFileMap.insert(ui->regenPRPUCB, QPair<QString, QString>("INACTIVE", "INACTIVE PR-PU PROOF.pdf"));
    }
    if (!ui->regen1ACB) {
        ::logMessage("Error: ui->regen1ACB is null");
    } else {
        checkboxFileMap.insert(ui->regen1ACB, QPair<QString, QString>("NCWO", "NCWO 1-A PROOF.pdf"));
    }
    if (!ui->regen1APCB) {
        ::logMessage("Error: ui->regen1APCB is null");
    } else {
        checkboxFileMap.insert(ui->regen1APCB, QPair<QString, QString>("NCWO", "NCWO 1-AP PROOF.pdf"));
    }
    if (!ui->regen1APPRCB) {
        ::logMessage("Error: ui->regen1APPRCB is null");
    } else {
        checkboxFileMap.insert(ui->regen1APPRCB, QPair<QString, QString>("NCWO", "NCWO 1-APPR PROOF.pdf"));
    }
    if (!ui->regen1PRCB) {
        ::logMessage("Error: ui->regen1PRCB is null");
    } else {
        checkboxFileMap.insert(ui->regen1PRCB, QPair<QString, QString>("NCWO", "NCWO 1-PR PROOF.pdf"));
    }
    if (!ui->regen2ACB) {
        ::logMessage("Error: ui->regen2ACB is null");
    } else {
        checkboxFileMap.insert(ui->regen2ACB, QPair<QString, QString>("NCWO", "NCWO 2-A PROOF.pdf"));
    }
    if (!ui->regen2APCB) {
        ::logMessage("Error: ui->regen2APCB is null");
    } else {
        checkboxFileMap.insert(ui->regen2APCB, QPair<QString, QString>("NCWO", "NCWO 2-AP PROOF.pdf"));
    }
    if (!ui->regen2APPRCB) {
        ::logMessage("Error: ui->regen2APPRCB is null");
    } else {
        checkboxFileMap.insert(ui->regen2APPRCB, QPair<QString, QString>("NCWO", "NCWO 2-APPR PROOF.pdf"));
    }
    if (!ui->regen2PRCB) {
        ::logMessage("Error: ui->regen2PRCB is null");
    } else {
        checkboxFileMap.insert(ui->regen2PRCB, QPair<QString, QString>("NCWO", "NCWO 2-PR PROOF.pdf"));
    }
    if (!ui->regenPPUSCB) {
        ::logMessage("Error: ui->regenPPUSCB is null");
    } else {
        checkboxFileMap.insert(ui->regenPPUSCB, QPair<QString, QString>("PREPIF", "PREPIF US PROOF.pdf"));
    }
    if (!ui->regenPPPRCB) {
        ::logMessage("Error: ui->regenPPPRCB is null");
    } else {
        checkboxFileMap.insert(ui->regenPPPRCB, QPair<QString, QString>("PREPIF", "PREPIF PR PROOF.pdf"));
    }

    // Safely set enabled state for regenCheckboxes
    for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
        if (it.value()) {
            it.value()->setEnabled(false);
        } else {
            ::logMessage("Error: Regen checkbox is null in map");
        }
    }

    // Check for null before setting enabled
    if (ui->allCB) {
        ui->allCB->setEnabled(false);
    } else {
        ::logMessage("Error: ui->allCB is null");
    }

    if (ui->regenTab) {
        ui->regenTab->setEnabled(false);
    } else {
        ::logMessage("Error: ui->regenTab is null");
    }

    ::logMessage("Regeneration checkboxes setup complete.");
}

void MainWindow::formatCurrencyOnFinish()
{
    QLineEdit* lineEdit = qobject_cast<QLineEdit*>(sender());
    if (!lineEdit) return;

    QString text = lineEdit->text().trimmed();
    if (text.isEmpty()) return;

    // Use Validator to format as currency
    QLocale locale(QLocale::English, QLocale::UnitedStates);
    QString formatted = Validator::formatAsCurrency(text, locale);

    // Remove the currency symbol and spaces for storage (just keep the number)
    QString cleaned = formatted;
    cleaned.remove(QRegularExpression("[^0-9.]"));

    lineEdit->setText(cleaned);
}

void MainWindow::onPrintDirChanged(const QString &path)
{
    logToTerminal(tr("Print directory changed: %1").arg(path));

    // You can implement additional file monitoring logic here if needed
}

void MainWindow::onInactivityTimeout()
{
    if (!m_jobController || !m_jobController->isJobSaved()) return;

    // Save the current job state periodically
    m_jobController->saveJob();
    logToTerminal("Auto-saved job state due to inactivity.");
}

void MainWindow::onScriptStarted()
{
    logToTerminal("Script execution started...");
    ui->terminalWindow->ensureCursorVisible();
    QApplication::setOverrideCursor(Qt::WaitCursor);
}

void MainWindow::onScriptFinished(bool success)
{
    QApplication::restoreOverrideCursor();
    if (success) {
        logToTerminal("Script execution completed successfully.");
    } else {
        logToTerminal("Script execution failed. See terminal output for details.");
    }
    ui->terminalWindow->ensureCursorVisible();
}

void MainWindow::buildWeeklyMenu()
{
    // Clear previous menu items
    weeklyMenu->clear();

    try {
        // Get all jobs from the database
        QList<QMap<QString, QString>> jobs = m_dbManager->getAllJobs();

        // Check if the jobs list is valid
        if (jobs.isEmpty()) {
            QAction* noJobsAction = new QAction(tr("No Saved Jobs"), this);
            noJobsAction->setEnabled(false);
            weeklyMenu->addAction(noJobsAction);
            return;
        }

        // Group jobs by year, month, and week (using QSet for unique weeks)
        QMap<QString, QMap<QString, QSet<QString>>> jobsByYearAndMonth;
        for (const QMap<QString, QString>& job : jobs) {
            QString year = job["year"];
            QString month = job["month"];
            QString week = job["week"];
            jobsByYearAndMonth[year][month].insert(week);
        }

        // Add menu items
        // Sort years in descending order (newest first)
        QList<QString> years = jobsByYearAndMonth.keys();
        std::sort(years.begin(), years.end(), std::greater<QString>());

        for (const QString& year : years) {
            QMenu* yearMenu = weeklyMenu->addMenu(year);

            // Sort months in descending order (newest first)
            QList<QString> months = jobsByYearAndMonth[year].keys();
            std::sort(months.begin(), months.end(), std::greater<QString>());

            for (const QString& month : months) {
                QMenu* monthMenu = yearMenu->addMenu(tr("Month %1").arg(month));

                // Manually convert QSet to QList
                QList<QString> weeks;
                for (const QString& week : jobsByYearAndMonth[year][month]) {
                    weeks.append(week);
                }

                // Sort weeks in descending order (newest first)
                std::sort(weeks.begin(), weeks.end(), std::greater<QString>());

                for (const QString& week : weeks) {
                    QAction* weekAction = new QAction(tr("Week %1").arg(week), this);
                    connect(weekAction, &QAction::triggered, this, [=]() {
                        openJobFromWeekly(year, month, week);
                    });
                    monthMenu->addAction(weekAction);
                }
            }
        }
    }
    catch (const std::exception& e) {
        ::logMessage(QString("Error building weekly menu: %1").arg(e.what()));
        QAction* errorAction = new QAction(tr("Error loading jobs"), this);
        errorAction->setEnabled(false);
        weeklyMenu->addAction(errorAction);
    }
}

void MainWindow::openJobFromWeekly(const QString& year, const QString& month, const QString& week)
{
    // Check if any job is already open and confirm closing
    if (m_jobController->isJobSaved()) {
        QMessageBox::StandardButton reply = QMessageBox::question(this, tr("Open Job"),
                                                                  tr("Do you want to close the current job and open the selected one?"),
                                                                  QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }

        // Close current job
        if (!m_jobController->closeJob()) {
            QMessageBox::warning(this, tr("Error"), tr("Failed to close current job."));
            return;
        }
    }

    // Load selected job
    if (!m_jobController->loadJob(year, month, week)) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to load job."));
        return;
    }

    // Update UI with loaded job data
    JobData* job = m_jobController->currentJob();

    ui->yearDDbox->setCurrentText(job->year);
    ui->monthDDbox->setCurrentText(job->month);
    populateWeekDDbox();
    ui->weekDDbox->setCurrentText(job->week);

    ui->cbcJobNumber->setText(job->cbcJobNumber);
    ui->excJobNumber->setText(job->excJobNumber);
    ui->inactiveJobNumber->setText(job->inactiveJobNumber);
    ui->ncwoJobNumber->setText(job->ncwoJobNumber);
    ui->prepifJobNumber->setText(job->prepifJobNumber);

    ui->cbc2Postage->setText(job->cbc2Postage);
    ui->cbc3Postage->setText(job->cbc3Postage);
    ui->excPostage->setText(job->excPostage);
    ui->inactivePOPostage->setText(job->inactivePOPostage);
    ui->inactivePUPostage->setText(job->inactivePUPostage);
    ui->ncwo1APostage->setText(job->ncwo1APostage);
    ui->ncwo2APostage->setText(job->ncwo2APostage);
    ui->ncwo1APPostage->setText(job->ncwo1APPostage);
    ui->ncwo2APPostage->setText(job->ncwo2APPostage);
    ui->prepifPostage->setText(job->prepifPostage);

    // Set UI state based on job progress
    ui->lockButton->setChecked(true);
    ui->postageLock->setChecked(true);
    m_jobController->setJobDataLocked(true);
    m_jobController->setPostageLocked(true);

    // Set proof approval checkboxes
    QSignalBlocker allCBBlocker(ui->allCB);
    ui->allCB->setChecked(job->step6_complete == 1);

    updateWidgetStatesBasedOnJobState();
    updateLEDs();
    updateInstructions();

    logToTerminal(tr("Loaded job: Year %1, Month %2, Week %3").arg(year, month, week));
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

void MainWindow::onLogMessage(const QString& message)
{
    // Log to terminal window
    logToTerminal(message);

    // Also log to file via the global logMessage function
    ::logMessage(message);
}

void MainWindow::logToTerminal(const QString& message)
{
    // Log to terminal UI
    ui->terminalWindow->append(message);
    ui->terminalWindow->ensureCursorVisible();

    // Log to system logger
    LOG_INFO(message);

    // Save to database if there's an active job
    if (m_jobController && m_jobController->isJobSaved()) {
        JobData* job = m_jobController->currentJob();
        m_dbManager->saveTerminalLog(job->year, job->month, job->week, message);
    }
}

void MainWindow::onJobProgressUpdated(int progress)
{
    ui->progressBarWeekly->setValue(progress);

    // You can add more visual feedback here if needed
    QString progressText = tr("Job Progress: %1%").arg(progress);
    ui->statusbar->showMessage(progressText, 3000);
}

void MainWindow::populateWeekDDbox()
{
    ui->weekDDbox->clear();
    ui->weekDDbox->addItem("");  // Empty item

    QString month = ui->monthDDbox->currentText();
    if (month.isEmpty()) {
        return;
    }

    // Days in month lookup
    QMap<QString, int> daysInMonth;
    daysInMonth["01"] = 31; // January
    daysInMonth["02"] = 28; // February (not handling leap years)
    daysInMonth["03"] = 31; // March
    daysInMonth["04"] = 30; // April
    daysInMonth["05"] = 31; // May
    daysInMonth["06"] = 30; // June
    daysInMonth["07"] = 31; // July
    daysInMonth["08"] = 31; // August
    daysInMonth["09"] = 30; // September
    daysInMonth["10"] = 31; // October
    daysInMonth["11"] = 30; // November
    daysInMonth["12"] = 31; // December

    // Calculate weeks based on the month
    int days = daysInMonth.value(month, 30);
    int weeks = (days + 6) / 7;  // Ceiling division

    for (int i = 1; i <= weeks; i++) {
        ui->weekDDbox->addItem(QString::number(i));
    }
}

void MainWindow::setupSignalSlots()
{
    ::logMessage("Setting up signal slots...");

    // Connect JobController signals
    connect(m_jobController, &JobController::logMessage, this, &MainWindow::onLogMessage);
    connect(m_jobController, &JobController::jobProgressUpdated, this, &MainWindow::onJobProgressUpdated);
    connect(m_jobController, &JobController::scriptStarted, this, &MainWindow::onScriptStarted);
    connect(m_jobController, &JobController::scriptFinished, this, &MainWindow::onScriptFinished);
    connect(m_jobController, &JobController::postProofCountsUpdated,
            this,
            [this]() {
                logToTerminal("Post-proof counts updated.");
            }
            );

    // Menu connections
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::onActionExitTriggered);
    connect(ui->actionSave_Job, &QAction::triggered, this, &MainWindow::onActionSaveJobTriggered);
    connect(ui->actionClose_Job, &QAction::triggered, this, &MainWindow::onActionCloseJobTriggered);
    connect(ui->actionCheck_for_updates, &QAction::triggered, this, &MainWindow::onCheckForUpdatesTriggered);

    // Count table menu action connection
    connect(ui->actionGet_Count_Table, &QAction::triggered, this, &MainWindow::onGetCountTableClicked);

    // Button connections
    connect(ui->openIZ, &QPushButton::clicked, this, &MainWindow::onOpenIZClicked);
    connect(ui->runInitial, &QPushButton::clicked, this, &MainWindow::onRunInitialClicked);
    connect(ui->runPreProof, &QPushButton::clicked, this, &MainWindow::onRunPreProofClicked);
    connect(ui->openProofFiles, &QPushButton::clicked, this, &MainWindow::onOpenProofFilesClicked);
    connect(ui->runPostProof, &QPushButton::clicked, this, &MainWindow::onRunPostProofClicked);
    connect(ui->openPrintFiles, &QPushButton::clicked, this, &MainWindow::onOpenPrintFilesClicked);
    connect(ui->runPostPrint, &QPushButton::clicked, this, &MainWindow::onRunPostPrintClicked);

    // ComboBox connections
    connect(ui->proofDDbox, &QComboBox::currentTextChanged, this, &MainWindow::onProofDDboxChanged);
    connect(ui->printDDbox, &QComboBox::currentTextChanged, this, &MainWindow::onPrintDDboxChanged);
    connect(ui->yearDDbox, &QComboBox::currentTextChanged, this, &MainWindow::onYearDDboxChanged);
    connect(ui->monthDDbox, &QComboBox::currentTextChanged, this, &MainWindow::onMonthDDboxChanged);
    connect(ui->weekDDbox, &QComboBox::currentTextChanged, this, &MainWindow::onWeekDDboxChanged);

    // Toggle button connections
    connect(ui->lockButton, &QPushButton::toggled, this, &MainWindow::onLockButtonToggled);
    connect(ui->editButton, &QPushButton::toggled, this, &MainWindow::onEditButtonToggled);
    connect(ui->proofRegen, &QPushButton::toggled, this, &MainWindow::onProofRegenToggled);
    connect(ui->postageLock, &QPushButton::toggled, this, &MainWindow::onPostageLockToggled);

    // Checkbox connections - fixed to use checkStateChanged instead of stateChanged
    connect(ui->allCB, &QCheckBox::checkStateChanged, this, &MainWindow::onAllCBcheckStateChanged);

    // Connect job type checkboxes to updateAllCBState
    for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
        if (!it.value())
            continue;
        // FIX: Use a lambda to properly call the updateAllCBState method
        connect(it.value(), &QCheckBox::checkStateChanged, this, &MainWindow::updateAllCBState);
    }

    // Add this connection for the Regenerate Email action
    connect(ui->actionRegenerate_Email, &QAction::triggered, this, &MainWindow::onRegenerateEmailClicked);

    ::logMessage("Signal slots setup complete.");
}

void MainWindow::initWatchersAndTimers()
{
    ::logMessage("Initializing watchers and timers...");
    m_printWatcher = new QFileSystemWatcher();
    QString printPath = m_settings->value("PrintPath", QCoreApplication::applicationDirPath() + "/RAC").toString();
    if (QDir(printPath).exists()) {
        m_printWatcher->addPath(printPath);
        logToTerminal(tr("Watching print directory: %1").arg(printPath));
    } else {
        logToTerminal(tr("Print directory not found: %1").arg(printPath));
    }
    connect(m_printWatcher, &QFileSystemWatcher::directoryChanged, this, &MainWindow::onPrintDirChanged);

    m_inactivityTimer = new QTimer();
    m_inactivityTimer->setInterval(300000); // 5 minutes
    m_inactivityTimer->setSingleShot(false);
    connect(m_inactivityTimer, &QTimer::timeout, this, &MainWindow::onInactivityTimeout);
    m_inactivityTimer->start();
    logToTerminal(tr("Inactivity timer started (5 minutes)."));
    ::logMessage("Watchers and timers initialized.");
}

void MainWindow::onActionExitTriggered()
{
    ::logMessage("Exit action triggered.");
    close();
}

void MainWindow::onActionSaveJobTriggered()
{
    ::logMessage("Save job action triggered.");
    if (currentJobType != "RAC WEEKLY") return;

    JobData* job = m_jobController->currentJob();
    job->year = ui->yearDDbox->currentText();
    job->month = ui->monthDDbox->currentText();
    job->week = ui->weekDDbox->currentText();
    job->cbcJobNumber = ui->cbcJobNumber->text();
    job->excJobNumber = ui->excJobNumber->text();
    job->inactiveJobNumber = ui->inactiveJobNumber->text();
    job->ncwoJobNumber = ui->ncwoJobNumber->text();
    job->prepifJobNumber = ui->prepifJobNumber->text();
    job->cbc2Postage = ui->cbc2Postage->text();
    job->cbc3Postage = ui->cbc3Postage->text();
    job->excPostage = ui->excPostage->text();
    job->inactivePOPostage = ui->inactivePOPostage->text();
    job->inactivePUPostage = ui->inactivePUPostage->text();
    job->ncwo1APostage = ui->ncwo1APostage->text();
    job->ncwo2APostage = ui->ncwo2APostage->text();
    job->ncwo1APPostage = ui->ncwo1APPostage->text();
    job->ncwo2APPostage = ui->ncwo2APPostage->text();
    job->prepifPostage = ui->prepifPostage->text();

    if (m_jobController->isJobSaved()) {
        m_jobController->saveJob();
    } else {
        m_jobController->createJob();
    }
    ::logMessage("Job saved.");
}

// Also update onActionCloseJobTriggered to properly reset checkbox states
void MainWindow::onActionCloseJobTriggered()
{
    ::logMessage("Close job action triggered.");
    if (currentJobType != "RAC WEEKLY") return;

    QMessageBox::StandardButton reply = QMessageBox::question(this, tr("Close Job"),
                                      tr("Are you sure you want to close the current job?"),
                                      QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_jobController->closeJob();

        const QSignalBlocker lockBlocker(ui->lockButton);
        const QSignalBlocker editBlocker(ui->editButton);
        const QSignalBlocker regenBlocker(ui->proofRegen);
        const QSignalBlocker postageBlocker(ui->postageLock);

        // Clear all job number fields
        ui->cbcJobNumber->clear();
        ui->excJobNumber->clear();
        ui->inactiveJobNumber->clear();
        ui->ncwoJobNumber->clear();
        ui->prepifJobNumber->clear();

        // Make job number fields editable
        ui->cbcJobNumber->setReadOnly(false);
        ui->excJobNumber->setReadOnly(false);
        ui->inactiveJobNumber->setReadOnly(false);
        ui->ncwoJobNumber->setReadOnly(false);
        ui->prepifJobNumber->setReadOnly(false);

        // Clear all postage fields
        ui->cbc2Postage->clear();
        ui->cbc3Postage->clear();
        ui->excPostage->clear();
        ui->inactivePOPostage->clear();
        ui->inactivePUPostage->clear();
        ui->ncwo1APostage->clear();
        ui->ncwo2APostage->clear();
        ui->ncwo1APPostage->clear();
        ui->ncwo2APPostage->clear();
        ui->prepifPostage->clear();

        // Make postage fields editable
        ui->cbc2Postage->setReadOnly(false);
        ui->cbc3Postage->setReadOnly(false);
        ui->excPostage->setReadOnly(false);
        ui->inactivePOPostage->setReadOnly(false);
        ui->inactivePUPostage->setReadOnly(false);
        ui->ncwo1APostage->setReadOnly(false);
        ui->ncwo2APostage->setReadOnly(false);
        ui->ncwo1APPostage->setReadOnly(false);
        ui->ncwo2APPostage->setReadOnly(false);
        ui->prepifPostage->setReadOnly(false);

        ui->yearDDbox->setCurrentIndex(0);
        ui->monthDDbox->setCurrentIndex(0);
        ui->weekDDbox->clear();
        ui->proofDDbox->setCurrentIndex(0);
        ui->printDDbox->setCurrentIndex(0);

        ui->lockButton->setChecked(false);
        ui->editButton->setChecked(false);
        ui->proofRegen->setChecked(false);
        ui->postageLock->setChecked(false);

        // Clear all checkboxes with signal blocking
        const QSignalBlocker allCBBlocker(ui->allCB);
        ui->allCB->setChecked(false);

        const QSignalBlocker cbcCBBlocker(ui->cbcCB);
        ui->cbcCB->setChecked(false);

        const QSignalBlocker excCBBlocker(ui->excCB);
        ui->excCB->setChecked(false);

        const QSignalBlocker inactiveCBBlocker(ui->inactiveCB);
        ui->inactiveCB->setChecked(false);

        const QSignalBlocker ncwoCBBlocker(ui->ncwoCB);
        ui->ncwoCB->setChecked(false);

        const QSignalBlocker prepifCBBlocker(ui->prepifCB);
        ui->prepifCB->setChecked(false);

        QList<QCheckBox*> checkboxes = findChildren<QCheckBox*>();
        for (QCheckBox* checkbox : checkboxes) {
            if (checkbox->objectName().startsWith("regen")) {
                const QSignalBlocker blocker(checkbox);
                checkbox->setChecked(false);
            }
        }

        updateWidgetStatesBasedOnJobState();
        updateLEDs();

        m_currentInstructionState = InstructionState::Default;
        loadInstructionContent(m_currentInstructionState);

        logToTerminal("Job closed and UI reset");
    }
    ::logMessage("Close job action completed.");
}

void MainWindow::onCheckForUpdatesTriggered()
{
    ::logMessage("Check for updates triggered.");
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
    ::logMessage("Update settings triggered.");
    UpdateSettingsDialog dialog(m_settings, this);
    dialog.exec();
    logToTerminal(tr("Update settings updated."));
}

void MainWindow::onOpenIZClicked()
{
    ::logMessage("Open IZ clicked.");
    if (currentJobType != "RAC WEEKLY") return;
    m_jobController->openIZ();
    updateLEDs();
    updateInstructions();
}

void MainWindow::onRunInitialClicked()
{
    ::logMessage("Run initial clicked.");
    if (currentJobType != "RAC WEEKLY") return;

    ui->runInitial->setEnabled(false);

    // Prevent multiple connections if button is clicked rapidly
    disconnect(m_scriptRunner, &ScriptRunner::scriptFinished, this, nullptr);

    connect(m_scriptRunner, &ScriptRunner::scriptFinished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                ui->runInitial->setEnabled(true);
                if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                    JobData* job = m_jobController->currentJob();
                    job->isRunInitialComplete = true;
                    job->step1_complete = 1;
                    m_jobController->saveJob();

                    QString izPath = m_fileManager->getIZPath();
                    QDir izDir(izPath);
                    QStringList zipFiles = izDir.entryList(QStringList() << "*.zip", QDir::Files);
                    for (const QString& zipFile : zipFiles) {
                        QString zipFilePath = izPath + "/" + zipFile;
                        QFile file(zipFilePath);
                        if (file.exists()) {
                            file.setPermissions(QFile::WriteOwner | QFile::WriteUser);
                            bool deleted = false;
                            for (int attempt = 1; attempt <= 3; ++attempt) {
                                if (file.remove()) {
                                    logToTerminal("Deleted ZIP file: " + zipFile);
                                    deleted = true;
                                    break;
                                } else {
                                    logToTerminal(QString("Attempt %1: Failed to delete ZIP file: %2 - Error: %3")
                                                      .arg(attempt).arg(zipFile).arg(file.errorString()));
                                    QThread::msleep(500);
                                }
                            }
                            if (!deleted) {
                                logToTerminal("Failed to delete ZIP file after retries: " + zipFile);
                            }
                        } else {
                            logToTerminal("ZIP file not found: " + zipFile);
                        }
                    }

                    updateLEDs();
                    updateInstructions();
                    updateWidgetStatesBasedOnJobState();
                    logToTerminal("Initial processing completed successfully.");
                } else {
                    logToTerminal("Script execution failed. You can try running it again.");
                }
            }, Qt::SingleShotConnection);

    m_jobController->runInitialProcessing();
}

void MainWindow::onRunPreProofClicked()
{
    ::logMessage("Run pre-proof clicked.");
    if (currentJobType != "RAC WEEKLY") return;

    if (!m_jobController->isPostageLocked()) {
        QMessageBox::warning(this, tr("Postage Not Locked"),
                             tr("Please enter all postage amounts and lock them before running pre-proof processing."));
        return;
    }

    QList<QLineEdit*> postageFields;
    postageFields << ui->cbc2Postage << ui->cbc3Postage << ui->excPostage
                  << ui->inactivePOPostage << ui->inactivePUPostage
                  << ui->ncwo1APostage << ui->ncwo2APostage
                  << ui->ncwo1APPostage << ui->ncwo2APPostage << ui->prepifPostage;

    bool missingPostage = false;
    for (QLineEdit* field : postageFields) {
        if (field->text().trimmed().isEmpty()) {
            missingPostage = true;
            break;
        }
    }

    if (missingPostage) {
        QMessageBox::warning(this, tr("Missing Postage"),
                             tr("Please enter all postage amounts before running pre-proof processing."));
        return;
    }

    QString basePath = ConfigManager::instance().getString("BasePath", "C:/Goji/RAC");
    QMap<QString, QStringList> requiredFiles;
    requiredFiles["CBC"] = {"CBC2_WEEKLY.csv", "CBC3_WEEKLY.csv"};
    requiredFiles["EXC"] = {"EXC_OUTPUT.csv"};
    requiredFiles["INACTIVE"] = {"A-PO.txt", "A-PU.txt"};
    requiredFiles["NCWO"] = {"1-A_OUTPUT.csv", "1-AP_OUTPUT.csv", "2-A_OUTPUT.csv", "2-AP_OUTPUT.csv"};
    requiredFiles["PREPIF"] = {"PRE_PIF.csv"};

    QStringList missingFiles;
    for (auto it = requiredFiles.constBegin(); it != requiredFiles.constEnd(); ++it) {
        QString jobType = it.key();
        QString outputDir = basePath + "/" + jobType + "/JOB/OUTPUT";
        for (const QString& fileName : it.value()) {
            if (!QFile::exists(outputDir + "/" + fileName)) {
                missingFiles.append(fileName);
            }
        }
    }

    if (!missingFiles.isEmpty()) {
        QString message = tr("The following data files are missing from their OUTPUT folders:\n\n") + missingFiles.join("\n") +
                          tr("\n\nDo you want to proceed?");
        int choice = QMessageBox::warning(this, tr("Missing Files"), message, QMessageBox::Yes | QMessageBox::No);
        if (choice == QMessageBox::No) {
            return;
        }

        QMessageBox confirmBox;
        confirmBox.setWindowTitle(tr("Confirm"));
        confirmBox.setText(tr("CONFIRM INCOMPLETE CONTINUE"));
        QPushButton *confirmButton = confirmBox.addButton(tr("Confirm"), QMessageBox::AcceptRole);
        confirmBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
        confirmBox.exec();
        if (confirmBox.clickedButton() != confirmButton) {
            return;
        }
    }

    ui->runPreProof->setEnabled(false);

    connect(m_scriptRunner, &ScriptRunner::scriptFinished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                ui->runPreProof->setEnabled(true);
                if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                    JobData* job = m_jobController->currentJob();
                    job->isRunPreProofComplete = true;
                    job->step2_complete = 1;
                    job->step3_complete = 1;
                    m_jobController->saveJob();
                    updateLEDs();
                    updateInstructions();
                    updateWidgetStatesBasedOnJobState();
                    logToTerminal("Pre-proof processing completed successfully.");
                } else {
                    logToTerminal("Pre-proof script execution failed. You can try running it again.");
                }
            }, Qt::SingleShotConnection);

    m_jobController->runPreProofProcessing();
}

void MainWindow::onOpenProofFilesClicked()
{
    ::logMessage("Open proof files clicked.");
    if (currentJobType != "RAC WEEKLY") return;
    QString selection = ui->proofDDbox->currentText();
    m_jobController->openProofFiles(selection);
    updateLEDs();
    updateInstructions();
}

void MainWindow::onRunPostProofClicked()
{
    ::logMessage("Run post-proof clicked.");
    if (currentJobType != "RAC WEEKLY") return;

    ui->runPostProof->setEnabled(false);

    connect(m_scriptRunner, &ScriptRunner::scriptFinished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                ui->runPostProof->setEnabled(true);
                if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                    JobData* job = m_jobController->currentJob();
                    job->isRunPostProofComplete = true;
                    job->step5_complete = 1;
                    bool saved = m_jobController->saveJob();
                    if (!saved) {
                        logToTerminal("Warning: Failed to save job state after postProof completion.");
                    } else {
                        logToTerminal("Job state saved successfully after postProof completion.");
                    }
                    updateLEDs();
                    updateWidgetStatesBasedOnJobState();
                    updateBugNudgeMenu();
                    updateInstructions();
                    ui->allCB->setEnabled(true);
                    for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
                        it.value()->setEnabled(true);
                    }
                    QList<QCheckBox*> checkboxes = ui->regenTab->findChildren<QCheckBox*>();
                    for (QCheckBox* checkbox : checkboxes) {
                        if (checkbox->objectName().startsWith("regen")) {
                            checkbox->setEnabled(true);
                        }
                    }
                    onJobProgressUpdated(m_jobController->getProgress());
                    logToTerminal("Post-proof processing completed successfully. Proof approval now enabled.");
                } else {
                    logToTerminal("Post-proof script execution failed. You can try running it again.");
                }
            }, Qt::SingleShotConnection);

    if (m_jobController->isProofRegenMode()) {
        QMap<QString, QStringList> filesByJobType;
        for (auto it = checkboxFileMap.begin(); it != checkboxFileMap.end(); ++it) {
            if (it.key()->isChecked()) {
                QString jobType = it.value().first;
                QString fileName = it.value().second;
                if (!filesByJobType.contains(jobType)) {
                    filesByJobType[jobType] = QStringList();
                }
                filesByJobType[jobType].append(fileName);
            }
        }
        m_jobController->regenerateProofs(filesByJobType);
    } else {
        m_jobController->runPostProofProcessing(false);
    }
}

void MainWindow::onOpenPrintFilesClicked()
{
    ::logMessage("Open print files clicked.");
    if (currentJobType != "RAC WEEKLY") return;
    QString selection = ui->printDDbox->currentText();
    m_jobController->openPrintFiles(selection);
    updateLEDs();
    updateInstructions();
}

void MainWindow::onRunPostPrintClicked()
{
    ::logMessage("Run post-print clicked.");
    if (currentJobType != "RAC WEEKLY") return;

    ui->runPostPrint->setEnabled(false);

    connect(m_scriptRunner, &ScriptRunner::scriptFinished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                ui->runPostPrint->setEnabled(true);
                if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                    updateLEDs();
                    updateInstructions();
                } else {
                    logToTerminal("Post-print script execution failed. You can try running it again.");
                }
            }, Qt::SingleShotConnection);

    m_jobController->runPostPrintProcessing();
}

// Enhanced implementation for MainWindow::onGetCountTableClicked
void MainWindow::onGetCountTableClicked()
{
    ::logMessage("Get count table clicked.");
    if (currentJobType != "RAC WEEKLY") return;

    // Check if we have a valid job loaded
    if (!m_jobController || !m_jobController->isJobSaved()) {
        QMessageBox::warning(this, tr("No Job Loaded"), tr("Please load a job before attempting to view counts."));
        return;
    }

    // Get the current job data
    JobData* job = m_jobController->currentJob();
    if (!job) {
        QMessageBox::warning(this, tr("Job Data Error"), tr("Unable to access job data."));
        return;
    }

    // First check if we need to regenerate the counts from the POST-PROOF script
    bool hasExistingCounts = false;
    QList<QMap<QString, QVariant>> existingCounts = m_dbManager->getPostProofCounts();
    if (!existingCounts.isEmpty()) {
        hasExistingCounts = true;
    }

    if (!hasExistingCounts) {
        // If we don't have counts but post-proof has been completed, offer to run it again
        if (job->isRunPostProofComplete) {
            QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                                      tr("Missing Counts Data"),
                                                                      tr("Count data is missing. Would you like to run the post-proof script again to generate count data?"),
                                                                      QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                // Run post-proof with silent mode (just to get counts)
                connect(m_scriptRunner, &ScriptRunner::scriptFinished, this,
                        [this](int exitCode, QProcess::ExitStatus exitStatus) {
                            if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                                // Now show the counts table
                                CountsTableDialog* dialog = new CountsTableDialog(m_dbManager, this);
                                dialog->setAttribute(Qt::WA_DeleteOnClose);
                                dialog->setWindowTitle(tr("Post-Proof Counts"));
                                dialog->show();
                                logToTerminal(tr("Post-proof script completed and counts data generated. Showing counts table."));
                            } else {
                                logToTerminal(tr("Failed to generate counts data. Please try again."));
                            }
                        }, Qt::SingleShotConnection);

                m_jobController->runPostProofProcessing(false);
                return;
            }
        } else {
            QMessageBox::warning(this, tr("Post-Proof Not Complete"),
                                 tr("You need to complete the post-proof step before count data is available."));
            return;
        }
    }

    // Create and show the counts table dialog with existing data
    CountsTableDialog* dialog = new CountsTableDialog(m_dbManager, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Post-Proof Counts"));
    dialog->show();
    logToTerminal(tr("Showing counts table dialog."));
}

void MainWindow::onRegenerateEmailClicked()
{
    ::logMessage("Regenerate Email button clicked.");
    if (currentJobType != "RAC WEEKLY" || !m_jobController->isJobSaved()) {
        QMessageBox::warning(this, tr("No Active Job"),
                             tr("Please open a RAC WEEKLY job first."));
        return;
    }

    JobData* job = m_jobController->currentJob();
    if (!job) {
        QMessageBox::warning(this, tr("No Job Data"),
                             tr("No job data available."));
        return;
    }

    QStringList fileLocations;
    fileLocations << "Inactive data file on Buskro, print files located below\n";
    QStringList jobTypes = {"NCWO", "PREPIF", "CBC", "EXC"};
    QString week = job->month + "." + job->week;

    for (const QString& jobType : jobTypes) {
        QString jobNumber = job->getJobNumberForJobType(jobType);
        QString path = QString("\\\\NAS1069D9\\AMPrintData\\%1_SrcFiles\\I\\Innerworkings\\%2 %3\\%4")
                           .arg(job->year, jobNumber, jobType, week);
        fileLocations << path;
    }

    if (fileLocations.count() < 2) {
        logToTerminal("Error: Could not generate file locations for email");
        return;
    }

    QString locationsText = fileLocations.join("\n");
    FileLocationsDialog dialog(locationsText, FileLocationsDialog::CopyCloseButtons, this);
    dialog.exec();

    logToTerminal("Regenerated email information window.");
}

InstructionState MainWindow::determineInstructionState()
{
    JobData* job = m_jobController->currentJob();

    if (!job) {
        return InstructionState::Default;
    }

    if (job->step6_complete == 1) {
        return InstructionState::Final;
    }
    else if (job->isRunPostProofComplete) {
        return InstructionState::PostProof;
    }
    else if (job->isRunPreProofComplete) {
        return InstructionState::PreProof;
    }
    else if (job->isRunInitialComplete || m_jobController->isJobSaved()) {
        return InstructionState::Initial;
    }

    return InstructionState::Default;
}

void MainWindow::onForcePreProofComplete()
{
    ::logMessage("Force pre-proof complete triggered.");
    if (currentJobType != "RAC WEEKLY" || !m_jobController->isJobSaved())
        return;

    QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                              tr("Force Pre-Proof Complete"),
                                                              tr("Are you sure you want to force the PRE PROOF step to be marked as complete?"),
                                                              QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    JobData* job = m_jobController->currentJob();
    if (!job->isRunInitialComplete) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Initial processing must be completed first."));
        return;
    }

    job->isRunPreProofComplete = true;
    job->step2_complete = 1;
    job->step3_complete = 1;

    if (m_jobController->saveJob()) {
        logToTerminal("Forced PRE PROOF step to complete.");
        updateLEDs();
        updateWidgetStatesBasedOnJobState();
        updateBugNudgeMenu();
        updateInstructions();
    } else {
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to save job state."));
    }
}

void MainWindow::onForceProofFilesComplete()
{
    ::logMessage("Force proof files complete triggered.");
    if (currentJobType != "RAC WEEKLY" || !m_jobController->isJobSaved())
        return;

    QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                              tr("Force Proof Files Generated"),
                                                              tr("Are you sure you want to force the PROOF FILES GENERATED step to be marked as complete?"),
                                                              QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    JobData* job = m_jobController->currentJob();
    if (!job->isRunPreProofComplete) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Pre-Proof processing must be completed first."));
        return;
    }

    job->isOpenProofFilesComplete = true;
    job->step4_complete = 1;

    if (m_jobController->saveJob()) {
        logToTerminal("Forced PROOF FILES GENERATED step to complete.");
        updateLEDs();
        updateWidgetStatesBasedOnJobState();
        updateBugNudgeMenu();
        updateInstructions();
    } else {
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to save job state."));
    }
}

void MainWindow::onForcePostProofComplete()
{
    ::logMessage("Force post-proof complete triggered.");
    if (currentJobType != "RAC WEEKLY" || !m_jobController->isJobSaved())
        return;

    QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                              tr("Force Post Proof Complete"),
                                                              tr("Are you sure you want to force the POST PROOF step to be marked as complete?"),
                                                              QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    JobData* job = m_jobController->currentJob();
    if (!job->isOpenProofFilesComplete) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Proof files must be generated first."));
        return;
    }

    job->isRunPostProofComplete = true;
    job->step5_complete = 1;

    if (m_jobController->saveJob()) {
        logToTerminal("Forced POST PROOF step to complete.");
        updateLEDs();
        updateWidgetStatesBasedOnJobState();
        updateBugNudgeMenu();
        updateInstructions();

        ui->allCB->setEnabled(true);
        for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
            it.value()->setEnabled(true);
        }
    } else {
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to save job state."));
    }
}

void MainWindow::fixCurrentPostProofState()
{
    ::logMessage("Fixing current post-proof state.");
    if (currentJobType != "RAC WEEKLY" || !m_jobController || !m_jobController->isJobSaved())
        return;

    JobData* job = m_jobController->currentJob();
    if (!job->isOpenProofFilesComplete) {
        logToTerminal("Error: Proof files must be generated first. Running force fix for proof files...");
        onForceProofFilesComplete();
    }

    job->isRunPostProofComplete = true;
    job->step5_complete = 1;

    if (m_jobController->saveJob()) {
        logToTerminal("Forced POST PROOF step to complete.");
        ui->allCB->setEnabled(true);
        for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
            it.value()->setEnabled(true);
        }
        QList<QCheckBox*> checkboxes = ui->regenTab->findChildren<QCheckBox*>();
        for (QCheckBox* checkbox : checkboxes) {
            if (checkbox->objectName().startsWith("regen")) {
                checkbox->setEnabled(true);
            }
        }
        updateLEDs();
        updateWidgetStatesBasedOnJobState();
        updateBugNudgeMenu();
        updateInstructions();
        logToTerminal("Successfully fixed application state. You should now have access to proof approval checkboxes.");
    } else {
        logToTerminal("Error: Failed to save job state after fixing post-proof state.");
    }
}

void MainWindow::onForceProofApprovalComplete()
{
    ::logMessage("Force proof approval complete triggered.");
    if (currentJobType != "RAC WEEKLY" || !m_jobController->isJobSaved())
        return;

    QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                              tr("Force Proofs Approved"),
                                                              tr("Are you sure you want to force the PROOFS APPROVED step to be marked as complete?"),
                                                              QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    JobData* job = m_jobController->currentJob();
    if (!job->isRunPostProofComplete) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Post-Proof processing must be completed first."));
        return;
    }

    job->step6_complete = 1;

    const QSignalBlocker allCBBlocker(ui->allCB);
    ui->allCB->setChecked(true);

    for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
        const QSignalBlocker blocker(it.value());
        it.value()->setChecked(true);
    }

    if (m_jobController->saveJob()) {
        logToTerminal("Forced PROOFS APPROVED step to complete.");
        updateLEDs();
        updateWidgetStatesBasedOnJobState();
        updateBugNudgeMenu();
        updateInstructions();
    } else {
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to save job state."));
    }
}

void MainWindow::onForcePrintFilesComplete()
{
    ::logMessage("Force print files complete triggered.");
    if (currentJobType != "RAC WEEKLY" || !m_jobController->isJobSaved())
        return;

    QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                              tr("Force Print Files Generated"),
                                                              tr("Are you sure you want to force the PRINT FILES GENERATED step to be marked as complete?"),
                                                              QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    JobData* job = m_jobController->currentJob();
    if (job->step6_complete != 1) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Proofs must be approved first."));
        return;
    }

    job->isOpenPrintFilesComplete = true;
    job->step7_complete = 1;

    if (m_jobController->saveJob()) {
        logToTerminal("Forced PRINT FILES GENERATED step to complete.");
        updateLEDs();
        updateWidgetStatesBasedOnJobState();
        updateBugNudgeMenu();
        updateInstructions();
    } else {
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to save job state."));
    }
}

void MainWindow::onForcePostPrintComplete()
{
    ::logMessage("Force post-print complete triggered.");
    if (currentJobType != "RAC WEEKLY" || !m_jobController->isJobSaved())
        return;

    QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                              tr("Force Post Print Complete"),
                                                              tr("Are you sure you want to force the POST PRINT step to be marked as complete?"),
                                                              QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    JobData* job = m_jobController->currentJob();
    if (!job->isOpenPrintFilesComplete) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Print files must be generated first."));
        return;
    }

    job->isRunPostPrintComplete = true;
    job->step8_complete = 1;

    if (m_jobController->saveJob()) {
        logToTerminal("Forced POST PRINT step to complete.");
        updateLEDs();
        updateWidgetStatesBasedOnJobState();
        updateBugNudgeMenu();
        updateInstructions();
    } else {
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to save job state."));
    }
}

void MainWindow::updateWidgetStatesBasedOnJobState()
{
    ::logMessage("Updating widget states based on job state...");

    bool jobLoaded = m_jobController && m_jobController->isJobSaved();
    bool jobLocked = jobLoaded && m_jobController->isJobDataLocked();
    bool postageLocked = jobLoaded && m_jobController->isPostageLocked();
    bool proofRegenMode = jobLoaded && m_jobController->isProofRegenMode();

    // Setup UI element state based on job state
    ui->lockButton->setChecked(jobLocked);
    ui->editButton->setChecked(false);  // Always reset edit mode
    ui->proofRegen->setChecked(proofRegenMode);
    ui->postageLock->setChecked(postageLocked);

    // Job data field read-only states
    bool fieldsEditable = jobLoaded && (!jobLocked || ui->editButton->isChecked());

    // Job number fields
    ui->cbcJobNumber->setReadOnly(!fieldsEditable);
    ui->excJobNumber->setReadOnly(!fieldsEditable);
    ui->inactiveJobNumber->setReadOnly(!fieldsEditable);
    ui->ncwoJobNumber->setReadOnly(!fieldsEditable);
    ui->prepifJobNumber->setReadOnly(!fieldsEditable);

    // Postage fields - affected by both job lock and postage lock
    bool postageEditable = fieldsEditable && !postageLocked;
    ui->cbc2Postage->setReadOnly(!postageEditable);
    ui->cbc3Postage->setReadOnly(!postageEditable);
    ui->excPostage->setReadOnly(!postageEditable);
    ui->inactivePOPostage->setReadOnly(!postageEditable);
    ui->inactivePUPostage->setReadOnly(!postageEditable);
    ui->ncwo1APostage->setReadOnly(!postageEditable);
    ui->ncwo2APostage->setReadOnly(!postageEditable);
    ui->ncwo1APPostage->setReadOnly(!postageEditable);
    ui->ncwo2APPostage->setReadOnly(!postageEditable);
    ui->prepifPostage->setReadOnly(!postageEditable);

    // Date fields
    ui->yearDDbox->setEnabled(fieldsEditable);
    ui->monthDDbox->setEnabled(fieldsEditable);
    ui->weekDDbox->setEnabled(fieldsEditable);

    // Job workflow buttons - require job to be locked
    ui->openIZ->setEnabled(jobLocked);
    ui->runInitial->setEnabled(jobLocked && m_jobController->currentJob()->isOpenIZComplete);
    ui->runPreProof->setEnabled(jobLocked && postageLocked && m_jobController->currentJob()->isRunInitialComplete);

    ui->openProofFiles->setEnabled(jobLocked && m_jobController->currentJob()->isRunPreProofComplete);
    ui->proofDDbox->setEnabled(jobLocked && m_jobController->currentJob()->isRunPreProofComplete);

    ui->runPostProof->setEnabled(jobLocked &&
                                 (m_jobController->currentJob()->isOpenProofFilesComplete || proofRegenMode));

    // Proof regen related widgets
    ui->regenTab->setEnabled(proofRegenMode);

    // Proof approval checkboxes
    bool proofApprovalEnabled = jobLocked && m_jobController->currentJob()->isRunPostProofComplete;
    ui->allCB->setEnabled(proofApprovalEnabled);
    ui->cbcCB->setEnabled(proofApprovalEnabled);
    ui->excCB->setEnabled(proofApprovalEnabled);
    ui->inactiveCB->setEnabled(proofApprovalEnabled);
    ui->ncwoCB->setEnabled(proofApprovalEnabled);
    ui->prepifCB->setEnabled(proofApprovalEnabled);

    // Print related widgets - require proof approval
    bool proofApproved = jobLocked && m_jobController->currentJob()->step6_complete == 1;
    ui->openPrintFiles->setEnabled(proofApproved);
    ui->printDDbox->setEnabled(proofApproved);

    ui->runPostPrint->setEnabled(proofApproved && m_jobController->currentJob()->isOpenPrintFilesComplete);

    ::logMessage("Widget states updated successfully.");
}

void MainWindow::updateLEDs()
{
    JobData* job = m_jobController->currentJob();
    if (!job) {
        // No job loaded, set all LEDs to red
        ui->preProofLED->setStyleSheet("background-color: #ff0000;");
        ui->proofFilesLED->setStyleSheet("background-color: #ff0000;");
        ui->postProofLED->setStyleSheet("background-color: #ff0000;");
        ui->proofApprovalLED->setStyleSheet("background-color: #ff0000;");
        ui->printFilesLED->setStyleSheet("background-color: #ff0000;");
        ui->postPrintLED->setStyleSheet("background-color: #ff0000;");
        return;
    }

    // Update each LED based on job step completion
    // Green for completed, Red for incomplete

    // Pre-Proof LED
    ui->preProofLED->setStyleSheet(
        job->isRunPreProofComplete ? "background-color: #00ff15;" : "background-color: #ff0000;");

    // Proof Files Generated LED
    ui->proofFilesLED->setStyleSheet(
        job->isOpenProofFilesComplete ? "background-color: #00ff15;" : "background-color: #ff0000;");

    // Post-Proof LED
    ui->postProofLED->setStyleSheet(
        job->isRunPostProofComplete ? "background-color: #00ff15;" : "background-color: #ff0000;");

    // Proofs Approved LED
    ui->proofApprovalLED->setStyleSheet(
        (job->step6_complete == 1) ? "background-color: #00ff15;" : "background-color: #ff0000;");

    // Print Files Generated LED
    ui->printFilesLED->setStyleSheet(
        job->isOpenPrintFilesComplete ? "background-color: #00ff15;" : "background-color: #ff0000;");

    // Post-Print LED
    ui->postPrintLED->setStyleSheet(
        job->isRunPostPrintComplete ? "background-color: #00ff15;" : "background-color: #ff0000;");
}

void MainWindow::updateBugNudgeMenu()
{
    ::logMessage("Updating Bug Nudge menu based on current tab.");

    if (!m_bugNudgeMenu) {
        ::logMessage("Warning: Bug Nudge menu is null, cannot update.");
        return;
    }

    // Only enable the menu for RAC WEEKLY tab
    bool isRacWeekly = (currentJobType == "RAC WEEKLY");
    m_bugNudgeMenu->setEnabled(isRacWeekly);

    if (!isRacWeekly || !m_jobController || !m_jobController->isJobSaved()) {
        // Disable all actions if not on RAC WEEKLY tab or no job loaded
        if (m_forcePreProofAction) m_forcePreProofAction->setEnabled(false);
        if (m_forceProofFilesAction) m_forceProofFilesAction->setEnabled(false);
        if (m_forcePostProofAction) m_forcePostProofAction->setEnabled(false);
        if (m_forceProofApprovalAction) m_forceProofApprovalAction->setEnabled(false);
        if (m_forcePrintFilesAction) m_forcePrintFilesAction->setEnabled(false);
        if (m_forcePostPrintAction) m_forcePostPrintAction->setEnabled(false);
        return;
    }

    // Get current job state
    JobData* job = m_jobController->currentJob();
    if (!job) {
        ::logMessage("Warning: Job controller has no current job, disabling Bug Nudge menu.");
        return;
    }

    // Enable/disable actions based on job state
    // Actions should only be enabled if the previous step is complete but the current step is not

    // Pre-Proof
    if (m_forcePreProofAction) {
        m_forcePreProofAction->setEnabled(
            job->isRunInitialComplete && !job->isRunPreProofComplete);
    }

    // Proof Files Generated
    if (m_forceProofFilesAction) {
        m_forceProofFilesAction->setEnabled(
            job->isRunPreProofComplete && !job->isOpenProofFilesComplete);
    }

    // Post-Proof
    if (m_forcePostProofAction) {
        m_forcePostProofAction->setEnabled(
            job->isOpenProofFilesComplete && !job->isRunPostProofComplete);
    }

    // Proof Approval
    if (m_forceProofApprovalAction) {
        m_forceProofApprovalAction->setEnabled(
            job->isRunPostProofComplete && job->step6_complete != 1);
    }

    // Print Files Generated
    if (m_forcePrintFilesAction) {
        m_forcePrintFilesAction->setEnabled(
            job->step6_complete == 1 && !job->isOpenPrintFilesComplete);
    }

    // Post-Print
    if (m_forcePostPrintAction) {
        m_forcePostPrintAction->setEnabled(
            job->isOpenPrintFilesComplete && !job->isRunPostPrintComplete);
    }

    ::logMessage("Bug Nudge menu updated successfully.");
}

void MainWindow::updateInstructions()
{
    // Determine the current state based on job progress
    InstructionState newState = determineInstructionState();

    // Only update instructions if the state has changed
    if (newState != m_currentInstructionState) {
        m_currentInstructionState = newState;
        loadInstructionContent(m_currentInstructionState);
        ::logMessage(QString("Instructions updated to state: %1").arg(static_cast<int>(m_currentInstructionState)));
    }
}
