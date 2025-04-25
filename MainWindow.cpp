#include "mainwindow.h"
#include "ui_GOJI.h"
#include "countstabledialog.h"
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

// Use version defined in GOJI.pro
const QString VERSION = QString(APP_VERSION);

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_settings(new QSettings("GojiApp", "Goji", this)),
    openJobMenu(nullptr),
    weeklyMenu(nullptr),
    validator(nullptr),
    m_printWatcher(nullptr),
    m_inactivityTimer(nullptr),
    m_currentInstructionState(InstructionState::None)
{
    ui->setupUi(this);
    setWindowTitle(tr("Goji v%1").arg(VERSION));

    // Initialize database manager
    QString defaultDbDirPath;
#ifdef QT_DEBUG
    defaultDbDirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Goji/SQL/debug";
#else
    defaultDbDirPath = "C:/Goji/database";
#endif
    QString dbDirPath = m_settings->value("DatabasePath", defaultDbDirPath).toString();
    QDir dbDir(dbDirPath);
    if (!dbDir.exists()) {
        if (!dbDir.mkpath(".")) {
            QMessageBox::critical(this, tr("Directory Error"), tr("Failed to create directory: %1").arg(dbDirPath));
            return;
        }
    }
    QString dbPath = dbDirPath + "/jobs.db";

    // Create managers and controllers
    m_dbManager = new DatabaseManager(dbPath);
    if (!m_dbManager->initialize()) {
        QMessageBox::critical(this, tr("Database Error"), tr("Failed to initialize database."));
        return;
    }

    m_fileManager = new FileSystemManager(m_settings);
    m_scriptRunner = new ScriptRunner(this);
    m_jobController = new JobController(m_dbManager, m_fileManager, m_scriptRunner, m_settings, this);

    // Set up UI elements and connections
    setupUi();
    setupSignalSlots();
    initializeValidators();
    setupMenus();
    setupRegenCheckboxes();
    initWatchersAndTimers();

    // Initialize instructions
    initializeInstructions();

    // Start on RAC WEEKLY tab
    currentJobType = "RAC WEEKLY";

    // Set default instructions for RAC WEEKLY tab on startup
    if (currentJobType == "RAC WEEKLY") {
        m_currentInstructionState = InstructionState::Default;
        loadInstructionContent(m_currentInstructionState);
    }

    logToTerminal(tr("Goji started: %1").arg(QDateTime::currentDateTime().toString()));
}

MainWindow::~MainWindow()
{
    delete ui;
    delete m_dbManager;
    delete m_fileManager;
    delete m_scriptRunner;
    delete m_jobController;
    delete openJobMenu;
    delete weeklyMenu;
    delete validator;
    delete m_printWatcher;
    delete m_inactivityTimer;
}

void MainWindow::initializeInstructions()
{
    // Load the Iosevka font if not already loaded
    QFontDatabase::addApplicationFont("C:/Users/JCox/AppData/Local/Microsoft/Windows/Fonts/IosevkaCustom-Regular.ttf");

    // Set the font for the textBrowser
    QFont iosevkaFont("Iosevka", 11);
    ui->textBrowser->setFont(iosevkaFont);

    // Map instruction states to their resource paths (using Qt resource system)
    m_instructionFiles[InstructionState::Default] = ":/resources/instructions/default.html";
    m_instructionFiles[InstructionState::Initial] = ":/resources/instructions/initial.html";
    m_instructionFiles[InstructionState::PreProof] = ":/resources/instructions/preproof.html";
    m_instructionFiles[InstructionState::PostProof] = ":/resources/instructions/postproof.html";
    m_instructionFiles[InstructionState::Final] = ":/resources/instructions/final.html";

    // Load default instructions initially
    m_currentInstructionState = InstructionState::Default;
    loadInstructionContent(m_currentInstructionState);
}

void MainWindow::loadInstructionContent(InstructionState state)
{
    if (state == InstructionState::None) {
        ui->textBrowser->clear();
        return;
    }

    if (!m_instructionFiles.contains(state)) {
        logToTerminal("Error: No instruction file found for current state.");
        return;
    }

    QString filePath = m_instructionFiles[state];
    QFile file(filePath);

    if (!file.exists()) {
        logToTerminal("Error: Instruction file not found: " + filePath);
        return;
    }

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = file.readAll();
        ui->textBrowser->setHtml(content);
        file.close();
    } else {
        logToTerminal("Error: Could not open instruction file: " + filePath);
    }
}

InstructionState MainWindow::determineInstructionState()
{
    // If no job is loaded but we're on the RAC WEEKLY tab, show default instructions
    if (currentJobType == "RAC WEEKLY" && (!m_jobController || !m_jobController->isJobSaved())) {
        return InstructionState::Default;
    }

    // If no job is loaded, return None (blank)
    if (!m_jobController || !m_jobController->isJobSaved()) {
        return InstructionState::None;
    }

    JobData* job = m_jobController->currentJob();

    // Check allCB state first (highest priority)
    if (ui->allCB->isChecked()) {
        return InstructionState::Final;
    }

    // Check if post-proof has been run
    if (job->isRunPostProofComplete) {
        return InstructionState::PostProof;
    }

    // Check if pre-proof has been run
    if (job->isRunPreProofComplete) {
        return InstructionState::PreProof;
    }

    // Default state for a loaded job
    return InstructionState::Initial;
}

void MainWindow::updateInstructions()
{
    InstructionState newState = determineInstructionState();

    // Only update if the state has changed
    if (newState != m_currentInstructionState) {
        m_currentInstructionState = newState;
        loadInstructionContent(m_currentInstructionState);
        logToTerminal("Updated instructions for new state.");
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (closeAllJobs()) {
        // If all jobs closed successfully, accept the close event
        event->accept();
    } else {
        // If there was a problem closing jobs, reject the close event
        event->ignore();
    }
}

bool MainWindow::closeAllJobs()
{
    if (m_jobController->isJobSaved()) {
        try {
            // Try to save and close the current job
            bool success = m_jobController->saveJob();
            if (!success) {
                QMessageBox::StandardButton reply = QMessageBox::warning(this,
                                                                         tr("Job Save Error"),
                                                                         tr("There was an error saving the current job. Do you want to exit anyway?\n\nAny unsaved changes will be lost."),
                                                                         QMessageBox::Yes | QMessageBox::No);

                if (reply == QMessageBox::No) {
                    return false;
                }
            }

            // Try to close the job (move files to home folders)
            success = m_jobController->closeJob();
            if (!success) {
                QMessageBox::StandardButton reply = QMessageBox::warning(this,
                                                                         tr("Job Close Error"),
                                                                         tr("There was an error closing the current job. Some files may not have been moved to their home folders. Do you want to exit anyway?"),
                                                                         QMessageBox::Yes | QMessageBox::No);

                if (reply == QMessageBox::No) {
                    return false;
                }
            }
        }
        catch (const std::exception& e) {
            QMessageBox::critical(this,
                                  tr("Fatal Error"),
                                  tr("A fatal error occurred while trying to close the job: %1\n\nThe application will revert to the latest saved state.").arg(e.what()));

            // Revert to saved state
            m_jobController->loadJob(m_jobController->getOriginalYear(),
                                     m_jobController->getOriginalMonth(),
                                     m_jobController->getOriginalWeek());

            QMessageBox::information(this,
                                     tr("Revert Complete"),
                                     tr("The application has reverted to the latest saved state."));

            return false;
        }
    }

    return true;
}

void MainWindow::setupUi()
{
    // Set regenTab to default to CBC tab (index 0)
    ui->regenTab->setCurrentIndex(0);

    // Set custom tab order for QLineEdit widgets
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
    QWidget::setTabOrder(ui->ncwo1APostage, ui->ncwo1APPostage);  // Updated
    QWidget::setTabOrder(ui->ncwo1APPostage, ui->ncwo2APostage);  // Updated
    QWidget::setTabOrder(ui->ncwo2APostage, ui->ncwo2APPostage);  // Updated
    QWidget::setTabOrder(ui->ncwo2APPostage, ui->prepifPostage);  // Updated

    // Set placeholder text for postage QLineEdit widgets
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

    // Populate yearDDbox (no blank item, as Qt Designer provides one)
    int currentYear = QDate::currentDate().year();
    ui->yearDDbox->addItem(QString::number(currentYear - 1));
    ui->yearDDbox->addItem(QString::number(currentYear));
    ui->yearDDbox->addItem(QString::number(currentYear + 1));

    // Set progress bar range and initial valueI am currently running a
    ui->progressBarWeekly->setRange(0, 100);
    ui->progressBarWeekly->setValue(0);

    // Initialize LED indicators
    updateLEDs();

    // Update widget states based on job state
    updateWidgetStatesBasedOnJobState();
}

void MainWindow::initializeValidators()
{
    // Set up validator for postage QLineEdit widgets
    validator = new QRegularExpressionValidator(QRegularExpression("[0-9]*\\.?[0-9]*"), this);
    QList<QLineEdit*> postageLineEdits;
    postageLineEdits << ui->cbc2Postage << ui->cbc3Postage << ui->excPostage << ui->inactivePOPostage
                     << ui->inactivePUPostage << ui->ncwo1APostage << ui->ncwo2APostage
                     << ui->ncwo1APPostage << ui->ncwo2APPostage << ui->prepifPostage;
    for (QLineEdit *lineEdit : postageLineEdits) {
        lineEdit->setValidator(validator);
        connect(lineEdit, &QLineEdit::editingFinished, this, &MainWindow::formatCurrencyOnFinish);
    }
}

void MainWindow::setupMenus()
{
    // Create the "Open Job" menu
    openJobMenu = new QMenu(tr("Open Job"), this);
    weeklyMenu = openJobMenu->addMenu(tr("Weekly"));
    connect(weeklyMenu, &QMenu::aboutToShow, this, &MainWindow::buildWeeklyMenu);
    ui->menuFile->insertMenu(ui->actionSave_Job, openJobMenu);

    // Disable "Open Job" menu and actions unless "RAC WEEKLY" is active
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        currentJobType = ui->tabWidget->tabText(index);
        openJobMenu->setEnabled(currentJobType == "RAC WEEKLY");
        ui->actionSave_Job->setEnabled(currentJobType == "RAC WEEKLY");
        ui->actionClose_Job->setEnabled(currentJobType == "RAC WEEKLY");

        // Show default instructions when RAC WEEKLY tab is active but no job is loaded
        if (currentJobType == "RAC WEEKLY" && (!m_jobController || !m_jobController->isJobSaved())) {
            m_currentInstructionState = InstructionState::Default;
            loadInstructionContent(m_currentInstructionState);
        } else if (currentJobType != "RAC WEEKLY") {
            m_currentInstructionState = InstructionState::None;
            loadInstructionContent(m_currentInstructionState);
        }
    });

    // Set up "Manage Scripts" menu
    QMenu* manageScriptsMenu = ui->menuInput->findChild<QMenu*>("menuManage_Scripts");
    if (manageScriptsMenu) {
        manageScriptsMenu->clear();

        // Define script directories and their menu structure
        QMap<QString, QVariant> scriptDirs;
        // Use QList to preserve insertion order for RAC submenus
        QList<QPair<QString, QString>> racSubmenus = {
            {"Weekly", "C:/Goji/Scripts/RAC/WEEKLIES"},
            {"Monthly", "C:/Goji/Scripts/RAC/MONTHLY"},
            {"Quarterly", "C:/Goji/Scripts/RAC/SWEEPS"},
            {"Bi-Annual", "C:/Goji/Scripts/RAC/PCE"}
        };
        scriptDirs["RAC"] = QVariant::fromValue(racSubmenus);
        // Trachmar submenus as QVariant-wrapped QMap
        scriptDirs["Trachmar"] = QVariant::fromValue(QMap<QString, QString>{
            {"Weekly PC", "C:/Goji/Scripts/TRACHMAR/WEEKLY PC"},
            {"Weekly Packets/IDO", "C:/Goji/Scripts/TRACHMAR/WEEKLY PACKET & IDO"},
            {"Term", "C:/Goji/Scripts/TRACHMAR/TERM"}
        });

        // Populate menus dynamically
        for (auto it = scriptDirs.constBegin(); it != scriptDirs.constEnd(); ++it) {
            QMenu* parentMenu = manageScriptsMenu->addMenu(it.key());
            if (it.key() == "RAC") {
                // Handle RAC submenus with QList for custom order
                const auto& submenus = it.value().value<QList<QPair<QString, QString>>>();
                for (const auto& submenu : submenus) {
                    QMenu* subMenu = parentMenu->addMenu(submenu.first);
                    populateScriptMenu(subMenu, submenu.second);
                }
            } else {
                // Handle Trachmar submenus with QMap (alphabetical)
                const auto& subdirs = it.value().value<QMap<QString, QString>>();
                for (auto subIt = subdirs.constBegin(); subIt != subdirs.constEnd(); ++subIt) {
                    QMenu* subMenu = parentMenu->addMenu(subIt.key());
                    populateScriptMenu(subMenu, subIt.value());
                }
            }
        }
    }
}

void MainWindow::setupSignalSlots()
{
    // Connect menu action signals
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::onActionExitTriggered);
    connect(ui->actionClose_Job, &QAction::triggered, this, &MainWindow::onActionCloseJobTriggered);
    connect(ui->actionSave_Job, &QAction::triggered, this, &MainWindow::onActionSaveJobTriggered);
    connect(ui->actionCheck_for_updates, &QAction::triggered, this, &MainWindow::onCheckForUpdatesTriggered);

    // Connect signals for button actions
    connect(ui->openIZ, &QPushButton::clicked, this, &MainWindow::onOpenIZClicked);
    connect(ui->runInitial, &QPushButton::clicked, this, &MainWindow::onRunInitialClicked);
    connect(ui->runPreProof, &QPushButton::clicked, this, &MainWindow::onRunPreProofClicked);
    connect(ui->openProofFiles, &QPushButton::clicked, this, &MainWindow::onOpenProofFilesClicked);
    connect(ui->runPostProof, &QPushButton::clicked, this, &MainWindow::onRunPostProofClicked);
    connect(ui->openPrintFiles, &QPushButton::clicked, this, &MainWindow::onOpenPrintFilesClicked);
    connect(ui->runPostPrint, &QPushButton::clicked, this, &MainWindow::onRunPostPrintClicked);

    // Connect signals for UI state changes
    connect(ui->lockButton, &QToolButton::toggled, this, &MainWindow::onLockButtonToggled);
    connect(ui->editButton, &QToolButton::toggled, this, &MainWindow::onEditButtonToggled);
    connect(ui->proofRegen, &QToolButton::toggled, this, &MainWindow::onProofRegenToggled);
    connect(ui->postageLock, &QToolButton::toggled, this, &MainWindow::onPostageLockToggled);
    connect(ui->proofDDbox, &QComboBox::currentTextChanged, this, &MainWindow::onProofDDboxChanged);
    connect(ui->printDDbox, &QComboBox::currentTextChanged, this, &MainWindow::onPrintDDboxChanged);
    connect(ui->yearDDbox, &QComboBox::currentTextChanged, this, &MainWindow::onYearDDboxChanged);
    connect(ui->monthDDbox, &QComboBox::currentTextChanged, this, &MainWindow::onMonthDDboxChanged);
    connect(ui->weekDDbox, &QComboBox::currentTextChanged, this, &MainWindow::onWeekDDboxChanged);

    // Connect checkbox signals
    connect(ui->allCB, &QCheckBox::checkStateChanged, this, &MainWindow::onAllCBStateChanged);
    connect(ui->cbcCB, &QCheckBox::checkStateChanged, this, &MainWindow::updateAllCBState);
    connect(ui->excCB, &QCheckBox::checkStateChanged, this, &MainWindow::updateAllCBState);
    connect(ui->inactiveCB, &QCheckBox::checkStateChanged, this, &MainWindow::updateAllCBState);
    connect(ui->ncwoCB, &QCheckBox::checkStateChanged, this, &MainWindow::updateAllCBState);
    connect(ui->prepifCB, &QCheckBox::checkStateChanged, this, &MainWindow::updateAllCBState);

    // Connect signals from JobController
    connect(m_jobController, &JobController::logMessage, this, &MainWindow::onLogMessage);
    connect(m_jobController, &JobController::jobProgressUpdated, this, &MainWindow::onJobProgressUpdated);
    connect(m_jobController, &JobController::scriptStarted, this, &MainWindow::onScriptStarted);
    connect(m_jobController, &JobController::scriptFinished, this, &MainWindow::onScriptFinished);

    // Connect jobLoaded signal to updateInstructions
    connect(m_jobController, &JobController::jobLoaded, this, &MainWindow::updateInstructions);

    // Connect jobClosed signal to clear instructions
    connect(m_jobController, &JobController::jobClosed, this, [this]() {
        m_currentInstructionState = InstructionState::None;
        loadInstructionContent(m_currentInstructionState);
    });

    // Connect step completion to trigger instruction updates
    connect(m_jobController, &JobController::stepCompleted, this, &MainWindow::updateInstructions);
}

void MainWindow::setupRegenCheckboxes()
{
    // Initialize checkbox mapping for regeneration
    regenCheckboxes.insert("CBC", ui->cbcCB);
    regenCheckboxes.insert("EXC", ui->excCB);
    regenCheckboxes.insert("INACTIVE", ui->inactiveCB);
    regenCheckboxes.insert("NCWO", ui->ncwoCB);
    regenCheckboxes.insert("PREPIF", ui->prepifCB);

    // Initialize checkbox-file mapping for regeneration
    checkboxFileMap.insert(ui->regenCBC2CB, QPair<QString, QString>("CBC", "CBC2 PROOF.pdf"));
    checkboxFileMap.insert(ui->regenCBC3CB, QPair<QString, QString>("CBC", "CBC3 PROOF.pdf"));
    checkboxFileMap.insert(ui->regenEXCCB, QPair<QString, QString>("EXC", "EXC PROOF.pdf"));
    checkboxFileMap.insert(ui->regenAPOCB, QPair<QString, QString>("INACTIVE", "INACTIVE A-PO PROOF.pdf"));
    checkboxFileMap.insert(ui->regenAPUCB, QPair<QString, QString>("INACTIVE", "INACTIVE A-PU PROOF.pdf"));
    checkboxFileMap.insert(ui->regenATPOCB, QPair<QString, QString>("INACTIVE", "INACTIVE AT-PO PROOF.pdf"));
    checkboxFileMap.insert(ui->regenATPUCB, QPair<QString, QString>("INACTIVE", "INACTIVE AT-PU PROOF.pdf"));
    checkboxFileMap.insert(ui->regenPRPOCB, QPair<QString, QString>("INACTIVE", "INACTIVE PR-PO PROOF.pdf"));
    checkboxFileMap.insert(ui->regenPRPUCB, QPair<QString, QString>("INACTIVE", "INACTIVE PR-PU PROOF.pdf"));
    checkboxFileMap.insert(ui->regen1ACB, QPair<QString, QString>("NCWO", "NCWO 1-A PROOF.pdf"));
    checkboxFileMap.insert(ui->regen1APCB, QPair<QString, QString>("NCWO", "NCWO 1-AP PROOF.pdf"));
    checkboxFileMap.insert(ui->regen1APPRCB, QPair<QString, QString>("NCWO", "NCWO 1-APPR PROOF.pdf"));
    checkboxFileMap.insert(ui->regen1PRCB, QPair<QString, QString>("NCWO", "NCWO 1-PR PROOF.pdf"));
    checkboxFileMap.insert(ui->regen2ACB, QPair<QString, QString>("NCWO", "NCWO 2-A PROOF.pdf"));
    checkboxFileMap.insert(ui->regen2APCB, QPair<QString, QString>("NCWO", "NCWO 2-AP PROOF.pdf"));
    checkboxFileMap.insert(ui->regen2APPRCB, QPair<QString, QString>("NCWO", "NCWO 2-APPR PROOF.pdf"));
    checkboxFileMap.insert(ui->regen2PRCB, QPair<QString, QString>("NCWO", "NCWO 2-PR PROOF.pdf"));
    checkboxFileMap.insert(ui->regenPPUSCB, QPair<QString, QString>("PREPIF", "PREPIF US PROOF.pdf"));
    checkboxFileMap.insert(ui->regenPPPRCB, QPair<QString, QString>("PREPIF", "PREPIF PR PROOF.pdf"));

    // Disable regeneration checkboxes by default
    for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
        it.value()->setEnabled(false);
    }
    ui->allCB->setEnabled(false);
    ui->regenTab->setEnabled(false);
}

void MainWindow::initWatchersAndTimers()
{
    // Set up print file watcher
    m_printWatcher = new QFileSystemWatcher();
    QString printPath = m_settings->value("PrintPath", QCoreApplication::applicationDirPath() + "/RAC").toString();
    if (QDir(printPath).exists()) {
        m_printWatcher->addPath(printPath);
        logToTerminal(tr("Watching print directory: %1").arg(printPath));
    } else {
        logToTerminal(tr("Print directory not found: %1").arg(printPath));
    }
    connect(m_printWatcher, &QFileSystemWatcher::directoryChanged, this, &MainWindow::onPrintDirChanged);

    // Set up inactivity timer
    m_inactivityTimer = new QTimer();
    m_inactivityTimer->setInterval(300000); // 5 minutes
    m_inactivityTimer->setSingleShot(false);
    connect(m_inactivityTimer, &QTimer::timeout, this, &MainWindow::onInactivityTimeout);
    m_inactivityTimer->start();
    logToTerminal(tr("Inactivity timer started (5 minutes)."));
}

void MainWindow::onActionExitTriggered()
{
    // This will trigger closeEvent
    close();
}

void MainWindow::onActionSaveJobTriggered()
{
    if (currentJobType != "RAC WEEKLY") return;

    // Update job data from UI fields
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

    // Save to database
    if (m_jobController->isJobSaved()) {
        m_jobController->saveJob();
    } else {
        m_jobController->createJob();
    }
}

void MainWindow::onActionCloseJobTriggered()
{
    if (currentJobType != "RAC WEEKLY") return;

    // Confirm with user
    QMessageBox::StandardButton reply = QMessageBox::question(this, tr("Close Job"),
                                                              tr("Are you sure you want to close the current job?"),
                                                              QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_jobController->closeJob();

        // Reset UI fields
        ui->cbcJobNumber->clear();
        ui->excJobNumber->clear();
        ui->inactiveJobNumber->clear();
        ui->ncwoJobNumber->clear();
        ui->prepifJobNumber->clear();
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

        // Reset combo boxes to default state
        ui->yearDDbox->setCurrentIndex(0);
        ui->monthDDbox->setCurrentIndex(0);
        ui->weekDDbox->clear();
        ui->proofDDbox->setCurrentIndex(0);
        ui->printDDbox->setCurrentIndex(0);

        // Reset UI state
        ui->lockButton->setChecked(false);
        ui->editButton->setChecked(false);
        ui->proofRegen->setChecked(false);
        ui->postageLock->setChecked(false);

        // Reset all checkboxes
        ui->allCB->setChecked(false);
        ui->cbcCB->setChecked(false);
        ui->excCB->setChecked(false);
        ui->inactiveCB->setChecked(false);
        ui->ncwoCB->setChecked(false);
        ui->prepifCB->setChecked(false);

        // Reset regeneration checkboxes
        for (QCheckBox* checkbox : findChildren<QCheckBox*>()) {
            if (checkbox->objectName().startsWith("regen")) {
                checkbox->setChecked(false);
            }
        }

        updateWidgetStatesBasedOnJobState();
        updateLEDs();

        // Load default instructions
        m_currentInstructionState = InstructionState::Default;
        loadInstructionContent(m_currentInstructionState);

        logToTerminal("Job closed and UI reset");
    }
}

void MainWindow::onCheckForUpdatesTriggered()
{
    QMessageBox::information(this, tr("Updates"), tr("Checking for updates is not yet implemented."));
    logToTerminal(tr("Checked for updates."));
}

void MainWindow::onOpenIZClicked()
{
    if (currentJobType != "RAC WEEKLY") return;
    m_jobController->openIZ();
    updateLEDs();
    updateInstructions();
}

void MainWindow::onRunInitialClicked()
{
    if (currentJobType != "RAC WEEKLY") return;

    ui->runInitial->setEnabled(false);

    connect(m_scriptRunner, &ScriptRunner::scriptFinished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                ui->runInitial->setEnabled(true);
                if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                    JobData* job = m_jobController->currentJob();
                    job->isRunInitialComplete = true;
                    job->step1_complete = 1;
                    m_jobController->saveJob();
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
    if (currentJobType != "RAC WEEKLY") return;

    // Check for required files before running pre-proof processing
    QString basePath = m_settings->value("BasePath", "C:/Goji/RAC").toString();
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

    // Temporarily disable the button to prevent multiple clicks
    ui->runPreProof->setEnabled(false);

    // Connect to the scriptFinished signal with a single-shot connection
    connect(m_scriptRunner, &ScriptRunner::scriptFinished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                // Always re-enable the button
                ui->runPreProof->setEnabled(true);

                // Only update job state if script was successful
                if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                    updateLEDs();
                    updateInstructions();
                } else {
                    logToTerminal("Pre-proof script execution failed. You can try running it again.");
                }
            }, Qt::SingleShotConnection);

    m_jobController->runPreProofProcessing();
}

void MainWindow::onOpenProofFilesClicked()
{
    if (currentJobType != "RAC WEEKLY") return;
    QString selection = ui->proofDDbox->currentText();
    m_jobController->openProofFiles(selection);
    updateLEDs();
    updateInstructions();
}

void MainWindow::onRunPostProofClicked()
{
    if (currentJobType != "RAC WEEKLY") return;

    // Check for proof files before running post-proof processing
    QString basePath = m_settings->value("BasePath", "C:/Goji/RAC").toString();
    QMap<QString, QStringList> expectedProofFiles;
    expectedProofFiles["CBC"] = {"CBC2 PROOF.pdf", "CBC3 PROOF.pdf"};
    expectedProofFiles["EXC"] = {"EXC PROOF.pdf"};
    expectedProofFiles["INACTIVE"] = {"INACTIVE A-PO PROOF.pdf", "INACTIVE A-PU PROOF.pdf", "INACTIVE AT-PO PROOF.pdf",
                                      "INACTIVE AT-PU PROOF.pdf", "INACTIVE PR-PO PROOF.pdf", "INACTIVE PR-PU PROOF.pdf"};
    expectedProofFiles["NCWO"] = {"NCWO 1-A PROOF.pdf", "NCWO 1-AP PROOF.pdf", "NCWO 1-APPR PROOF.pdf", "NCWO 1-PR PROOF.pdf",
                                  "NCWO 2-A PROOF.pdf", "NCWO 2-AP PROOF.pdf", "NCWO 2-APPR PROOF.pdf", "NCWO 2-PR PROOF.pdf"};
    expectedProofFiles["PREPIF"] = {"PREPIF US PROOF.pdf", "PREPIF PR PROOF.pdf"};

    QStringList missingFiles;
    for (const QString& jobType : QStringList{"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"}) {
        const QStringList& fileList = expectedProofFiles.value(jobType);
        for (const QString& file : fileList) {
            QString proofDir = basePath + "/" + jobType + "/JOB/PROOF";
            if (!QFile::exists(proofDir + "/" + file)) {
                missingFiles.append(proofDir + "/" + file);
            }
        }
    }

    if (!missingFiles.isEmpty()) {
        QString message = tr("The following proof files are missing:\n\n") + missingFiles.join("\n") +
                          tr("\n\nDo you want to proceed anyway?");
        int choice = QMessageBox::warning(this, tr("Missing Proof Files"), message, QMessageBox::Yes | QMessageBox::No);
        if (choice == QMessageBox::No) {
            return;
        }
    }

    // Temporarily disable the button to prevent multiple clicks
    ui->runPostProof->setEnabled(false);

    // Connect to the scriptFinished signal with a single-shot connection
    connect(m_scriptRunner, &ScriptRunner::scriptFinished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                // Always re-enable the button
                ui->runPostProof->setEnabled(true);

                // Only update job state if script was successful
                if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                    updateLEDs();
                    updateInstructions();
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
    if (currentJobType != "RAC WEEKLY") return;
    QString selection = ui->printDDbox->currentText();
    m_jobController->openPrintFiles(selection);
    updateLEDs();
    updateInstructions();
}

void MainWindow::onRunPostPrintClicked()
{
    if (currentJobType != "RAC WEEKLY") return;

    // Temporarily disable the button to prevent multiple clicks
    ui->runPostPrint->setEnabled(false);

    // Connect to the scriptFinished signal with a single-shot connection
    connect(m_scriptRunner, &ScriptRunner::scriptFinished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                // Always re-enable the button
                ui->runPostPrint->setEnabled(true);

                // Only update job state if script was successful
                if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                    updateLEDs();
                    updateInstructions();
                } else {
                    logToTerminal("Post-print script execution failed. You can try running it again.");
                }
            }, Qt::SingleShotConnection);

    m_jobController->runPostPrintProcessing();
}

void MainWindow::onGetCountTableClicked()
{
    if (currentJobType != "RAC WEEKLY") return;
    CountsTableDialog dialog(m_dbManager, this);
    dialog.exec();
}

void MainWindow::onRegenProofButtonClicked()
{
    if (currentJobType != "RAC WEEKLY") return;
    if (!m_jobController->isProofRegenMode()) {
        QMessageBox::warning(this, tr("Regen Mode Disabled"), tr("Please enable Proof Regeneration mode first."));
        return;
    }

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

    if (filesByJobType.isEmpty()) {
        QMessageBox::warning(this, tr("No Files Selected"), tr("Please select at least one proof file to regenerate."));
        return;
    }

    m_jobController->regenerateProofs(filesByJobType);
    logToTerminal(tr("Regen Proof button clicked."));
}

void MainWindow::onProofDDboxChanged(const QString &text)
{
    if (currentJobType != "RAC WEEKLY") return;
    logToTerminal(tr("Proof selection changed to: %1").arg(text));
}

void MainWindow::onPrintDDboxChanged(const QString &text)
{
    if (currentJobType != "RAC WEEKLY") return;
    logToTerminal(tr("Print selection changed to: %1").arg(text));
}

void MainWindow::onYearDDboxChanged(const QString &text)
{
    if (currentJobType != "RAC WEEKLY") return;
    logToTerminal(tr("Year changed to: %1").arg(text));
    populateWeekDDbox();
}

void MainWindow::onMonthDDboxChanged(const QString &text)
{
    if (currentJobType != "RAC WEEKLY") return;
    logToTerminal(tr("Month changed to: %1").arg(text));
    populateWeekDDbox();
}

void MainWindow::onWeekDDboxChanged(const QString &text)
{
    if (currentJobType != "RAC WEEKLY") return;
    logToTerminal(tr("Week changed to: %1").arg(text));
}

void MainWindow::onLockButtonToggled(bool checked)
{
    if (currentJobType != "RAC WEEKLY") return;

    if (checked) {
        QString year = ui->yearDDbox->currentText().trimmed();
        QString month = ui->monthDDbox->currentText().trimmed();
        QString week = ui->weekDDbox->currentText().trimmed();
        QList<QLineEdit*> jobNumberFields;
        jobNumberFields << ui->cbcJobNumber << ui->excJobNumber << ui->inactiveJobNumber
                        << ui->ncwoJobNumber << ui->prepifJobNumber;

        if (year.isEmpty() || month.isEmpty() || week.isEmpty()) {
            QMessageBox::warning(this, tr("Incomplete Data"), tr("Year, month, and week must be selected before locking."));
            ui->lockButton->setChecked(false);
            return;
        }

        for (QLineEdit* field : jobNumberFields) {
            if (field->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, tr("Incomplete Data"), tr("All job number fields must be filled before locking."));
                ui->lockButton->setChecked(false);
                return;
            }
        }

        // Update job data from UI
        JobData* job = m_jobController->currentJob();
        job->year = year;
        job->month = month;
        job->week = week;
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

        // Create or update job
        if (m_jobController->isJobSaved()) {
            m_jobController->saveJob();
        } else {
            m_jobController->createJob();
        }

        m_jobController->setJobDataLocked(true);
        ui->editButton->setChecked(false);

        // Update instructions if job is created/loaded
        updateInstructions();
    } else {
        // Prevent unchecking lockButton unless editButton is checked
        if (!ui->editButton->isChecked()) {
            QMessageBox::warning(this, tr("Edit Mode Required"), tr("You must enable Edit mode to unlock job data."));
            ui->lockButton->setChecked(true); // Revert to checked state
            return;
        }
        m_jobController->setJobDataLocked(false);
    }

    // Update UI
    QList<QLineEdit*> jobNumberFields;
    jobNumberFields << ui->cbcJobNumber << ui->excJobNumber << ui->inactiveJobNumber
                    << ui->ncwoJobNumber << ui->prepifJobNumber;

    for (QLineEdit* field : jobNumberFields) {
        field->setReadOnly(checked);
    }

    ui->yearDDbox->setEnabled(!checked);
    ui->monthDDbox->setEnabled(!checked);
    ui->weekDDbox->setEnabled(!checked);

    updateWidgetStatesBasedOnJobState();
}

void MainWindow::onEditButtonToggled(bool checked)
{
    if (currentJobType != "RAC WEEKLY") return;

    QList<QLineEdit*> jobNumberFields;
    jobNumberFields << ui->cbcJobNumber << ui->excJobNumber << ui->inactiveJobNumber
                    << ui->ncwoJobNumber << ui->prepifJobNumber;

    for (QLineEdit* field : jobNumberFields) {
        field->setReadOnly(!checked);
    }

    ui->yearDDbox->setEnabled(checked);
    ui->monthDDbox->setEnabled(checked);
    ui->weekDDbox->setEnabled(checked);

    if (checked) {
        ui->lockButton->setChecked(false);
        m_jobController->setJobDataLocked(false);
        logToTerminal(tr("Job data editing enabled"));
    } else {
        logToTerminal(tr("Job data editing disabled"));
    }
}

void MainWindow::onProofRegenToggled(bool checked)
{
    if (currentJobType != "RAC WEEKLY") return;

    m_jobController->setProofRegenMode(checked);
    ui->regenTab->setEnabled(checked);

    for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
        it.value()->setEnabled(checked);
    }
    ui->allCB->setEnabled(checked);

    logToTerminal(tr("Proof regeneration mode %1").arg(checked ? tr("enabled") : tr("disabled")));
}

void MainWindow::onPostageLockToggled(bool checked)
{
    if (currentJobType != "RAC WEEKLY") return;

    m_jobController->setPostageLocked(checked);

    QList<QLineEdit*> postageFields;
    postageFields << ui->cbc2Postage << ui->cbc3Postage << ui->excPostage
                  << ui->inactivePOPostage << ui->inactivePUPostage
                  << ui->ncwo1APostage << ui->ncwo2APostage
                  << ui->ncwo1APPostage << ui->ncwo2APPostage << ui->prepifPostage;

    for (QLineEdit* field : postageFields) {
        field->setReadOnly(checked);
    }

    logToTerminal(tr("Postage fields %1").arg(checked ? tr("locked") : tr("unlocked")));
}

void MainWindow::formatCurrencyOnFinish()
{
    if (currentJobType != "RAC WEEKLY") return;

    QLineEdit *lineEdit = qobject_cast<QLineEdit*>(sender());
    if (!lineEdit) return;

    QString text = lineEdit->text().trimmed();
    if (text.isEmpty()) return;

    static const QRegularExpression regex("[^0-9.]");
    text.remove(regex);
    bool ok;
    double value = text.toDouble(&ok);
    if (!ok) {
        lineEdit->clear();
        return;
    }

    QLocale locale(QLocale::English, QLocale::UnitedStates);
    lineEdit->setText(locale.toCurrencyString(value));
    logToTerminal(tr("Formatted %1 as %2").arg(lineEdit->placeholderText(), lineEdit->text()));
}

void MainWindow::onPrintDirChanged(const QString &path)
{
    logToTerminal(tr("Print directory changed: %1").arg(path));

    if (currentJobType == "RAC WEEKLY") {
        QString selection = ui->printDDbox->currentText();
        if (!selection.isEmpty()) {
            QStringList missingFiles;
            m_fileManager->checkPrintFiles(selection, missingFiles);
        }
    }
}

void MainWindow::onInactivityTimeout()
{
    if (currentJobType != "RAC WEEKLY") return;

    logToTerminal(tr("Inactivity timeout reached."));

    if (m_jobController->isJobSaved() && !m_jobController->isJobDataLocked()) {
        onActionSaveJobTriggered();
        logToTerminal(tr("Auto-saved job due to inactivity."));
    }
}

void MainWindow::onAllCBStateChanged(int state)
{
    if (currentJobType != "RAC WEEKLY") return;

    QSignalBlocker blocker(ui->allCB);
    for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
        it.value()->setCheckState(static_cast<Qt::CheckState>(state));
    }

    JobData* job = m_jobController->currentJob();
    if (state == Qt::Checked) {
        job->step6_complete = 1;
    } else {
        job->step6_complete = 0;
    }

    m_jobController->updateProgress();
    updateLEDs();

    // Update instructions when all checkbox is toggled
    updateInstructions();

    logToTerminal(tr("All checkbox state changed to: %1").arg(state == Qt::Checked ? tr("checked") : tr("unchecked")));
}

void MainWindow::updateAllCBState()
{
    if (currentJobType != "RAC WEEKLY") return;

    bool allChecked = true;
    bool anyChecked = false;
    for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
        if (it.value()->isChecked()) {
            anyChecked = true;
        } else {
            allChecked = false;
        }
    }

    QSignalBlocker blocker(ui->allCB);
    ui->allCB->setCheckState(allChecked ? Qt::Checked : (anyChecked ? Qt::PartiallyChecked : Qt::Unchecked));

    JobData* job = m_jobController->currentJob();
    if (allChecked) {
        job->step6_complete = 1;
    } else {
        job->step6_complete = 0;
    }

    m_jobController->updateProgress();
    updateLEDs();

    // Update instructions when checkbox state changes
    updateInstructions();
}

void MainWindow::buildWeeklyMenu()
{
    if (currentJobType != "RAC WEEKLY") {
        weeklyMenu->clear();
        return;
    }

    weeklyMenu->clear();
    QList<QMap<QString, QString>> jobs = m_dbManager->getAllJobs();

    QMap<QString, QMenu*> yearMenus;
    QMap<QString, QMenu*> monthMenus;

    for (const QMap<QString, QString>& job : jobs) {
        QString year = job["year"];
        QString month = job["month"].length() == 1 ? "0" + job["month"] : job["month"];
        QString week = job["week"].length() == 1 ? "0" + job["week"] : job["week"];

        // Create year submenu if it doesn't exist
        if (!yearMenus.contains(year)) {
            QMenu* yearMenu = new QMenu(year, weeklyMenu);
            yearMenus[year] = yearMenu;
            weeklyMenu->addMenu(yearMenu);
        }

        // Create month submenu if it doesn't exist
        QString monthKey = QString("%1_%2").arg(year, month);
        if (!monthMenus.contains(monthKey)) {
            QMenu* monthMenu = new QMenu(month, yearMenus[year]);
            monthMenus[monthKey] = monthMenu;
            yearMenus[year]->addMenu(monthMenu);
        }

        // Add week action
        QAction *action = new QAction(week, monthMenus[monthKey]);
        connect(action, &QAction::triggered, this, [=]() {
            openJobFromWeekly(year, month, week);
        });
        monthMenus[monthKey]->addAction(action);
    }
}

void MainWindow::openJobFromWeekly(const QString& year, const QString& month, const QString& week)
{
    if (currentJobType != "RAC WEEKLY") return;

    if (m_jobController->loadJob(year, month, week)) {
        // Update UI with job data
        JobData* job = m_jobController->currentJob();

        ui->yearDDbox->setCurrentText(job->year);
        ui->monthDDbox->setCurrentText(job->month);
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

        // Load terminal logs
        ui->terminalWindow->clear();
        QStringList logs = m_dbManager->getTerminalLogs(year, month, week);
        for (const QString& log : logs) {
            ui->terminalWindow->append(log);
        }

        // Update UI state
        ui->lockButton->setChecked(true);
        m_jobController->setJobDataLocked(true);
        updateWidgetStatesBasedOnJobState();
        updateLEDs();

        // Update instructions based on loaded job state
        updateInstructions();
    }
}

void MainWindow::populateScriptMenu(QMenu* menu, const QString& dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        menu->addAction(tr("Directory not found"))->setEnabled(false);
        logToTerminal(tr("Script directory not found: %1").arg(dirPath));
        return;
    }

    QStringList scriptFilters = {"*.py", "*.ps1", "*.bat", "*.r"};
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

    // Separate files and folders
    QFileInfoList scriptFiles;
    QFileInfoList subDirs;
    QFileInfoList subscriptDirs;
    for (const QFileInfo& entry : entries) {
        if (entry.isFile() && scriptFilters.contains("*." + entry.suffix(), Qt::CaseInsensitive)) {
            scriptFiles.append(entry);
        } else if (entry.isDir() && entry.fileName().toLower() != "archive") {
            if (entry.fileName().toLower() == "subscripts") {
                subscriptDirs.append(entry);
            } else {
                subDirs.append(entry);
            }
        }
    }

    // Add non-Subscripts folders at the top
    for (const QFileInfo& subDir : subDirs) {
        QMenu* subMenu = menu->addMenu(subDir.fileName());
        populateScriptMenu(subMenu, subDir.absoluteFilePath());
    }

    // Add script files
    for (const QFileInfo& fileInfo : scriptFiles) {
        QAction* action = menu->addAction(fileInfo.fileName());
        connect(action, &QAction::triggered, this, [=]() {
            openScriptFile(fileInfo.absoluteFilePath());
        });
    }

    // Add Subscripts folders at the bottom
    for (const QFileInfo& subDir : subscriptDirs) {
        QMenu* subMenu = menu->addMenu(subDir.fileName());
        populateScriptMenu(subMenu, subDir.absoluteFilePath());
    }

    if (scriptFiles.isEmpty() && subDirs.isEmpty() && subscriptDirs.isEmpty()) {
        menu->addAction(tr("No scripts or folders found"))->setEnabled(false);
    }
}

void MainWindow::openScriptFile(const QString& filePath)
{
    QString editorPath;
    QStringList arguments = {filePath};

    if (filePath.endsWith(".py", Qt::CaseInsensitive) || filePath.endsWith(".bat", Qt::CaseInsensitive)) {
        editorPath = "C:/Users/JCox/AppData/Local/Programs/EmEditor/EmEditor.exe";
    } else if (filePath.endsWith(".ps1", Qt::CaseInsensitive)) {
        editorPath = "C:/Users/JCox/AppData/Local/Programs/Microsoft VS Code/Code.exe";
    } else if (filePath.endsWith(".r", Qt::CaseInsensitive)) {
        editorPath = "C:/Program Files/RStudio/rstudio.exe";
    } else {
        logToTerminal(tr("Unsupported file type: %1").arg(filePath));
        return;
    }

    QProcess *process = new QProcess(this);
    connect(process, &QProcess::started, this, [=]() {
        logToTerminal(tr("Opened %1 in %2").arg(filePath, editorPath));
    });
    connect(process, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error) {
        QString errorType;
        switch (error) {
        case QProcess::FailedToStart: errorType = "Failed to start"; break;
        case QProcess::Crashed: errorType = "Crashed"; break;
        case QProcess::Timedout: errorType = "Timed out"; break;
        case QProcess::ReadError: errorType = "Read error"; break;
        case QProcess::WriteError: errorType = "Write error"; break;
        default: errorType = "Unknown error"; break;
        }
        logToTerminal(tr("Failed to open %1: %2 (%3)").arg(filePath, errorType, process->errorString()));
    });
    process->startDetached(editorPath, arguments);
    process->deleteLater();
}

void MainWindow::updateWidgetStatesBasedOnJobState()
{
    bool jobActive = m_jobController->isJobSaved();
    bool jobLocked = m_jobController->isJobDataLocked();

    ui->runInitial->setEnabled(jobActive);
    ui->runPreProof->setEnabled(jobActive && m_jobController->isPostageLocked());
    ui->openProofFiles->setEnabled(jobActive);
    ui->runPostProof->setEnabled(jobActive);
    ui->openPrintFiles->setEnabled(jobActive);
    ui->runPostPrint->setEnabled(jobActive);
    ui->openIZ->setEnabled(true);
    ui->proofDDbox->setEnabled(jobActive);
    ui->printDDbox->setEnabled(jobActive);
    ui->yearDDbox->setEnabled(!jobLocked);
    ui->monthDDbox->setEnabled(!jobLocked);
    ui->weekDDbox->setEnabled(!jobLocked);
    ui->editButton->setEnabled(jobActive);
    ui->proofRegen->setEnabled(jobActive);
    ui->postageLock->setEnabled(jobActive);
    ui->lockButton->setEnabled(true);
    ui->regenTab->setEnabled(m_jobController->isProofRegenMode());
}

void MainWindow::updateLEDs()
{
    if (currentJobType != "RAC WEEKLY") return;

    JobData* job = m_jobController->currentJob();

    ui->preProofLED->setStyleSheet(job->isRunPreProofComplete ?
                                       "background-color: #00ff15; border-radius: 2px;" :
                                       "background-color: red; border-radius: 2px;");
    ui->proofFilesLED->setStyleSheet(job->isOpenProofFilesComplete ?
                                         "background-color: #00ff15; border-radius: 2px;" :
                                         "background-color: red; border-radius: 2px;");
    ui->postProofLED->setStyleSheet(job->isRunPostProofComplete ?
                                        "background-color: #00ff15; border-radius: 2px;" :
                                        "background-color: red; border-radius: 2px;");
    ui->proofApprovalLED->setStyleSheet(job->step6_complete == 1 ?
                                            "background-color: #00ff15; border-radius: 2px;" :
                                            "background-color: red; border-radius: 2px;");
    ui->printFilesLED->setStyleSheet(job->isOpenPrintFilesComplete ?
                                         "background-color: #00ff15; border-radius: 2px;" :
                                         "background-color: red; border-radius: 2px;");
    ui->postPrintLED->setStyleSheet(job->isRunPostPrintComplete ?
                                        "background-color: #00ff15; border-radius: 2px;" :
                                        "background-color: red; border-radius: 2px;");
}

void MainWindow::populateWeekDDbox()
{
    if (currentJobType != "RAC WEEKLY") return;

    ui->weekDDbox->clear();
    ui->weekDDbox->addItem(""); // Add blank item as the first option

    QString yearStr = ui->yearDDbox->currentText();
    QString monthStr = ui->monthDDbox->currentText();

    // If year or month is not selected, leave weekDDbox with only the blank item
    if (yearStr.isEmpty() || monthStr.isEmpty()) {
        return;
    }

    // Convert strings to integers, checking for validity
    bool yearOk, monthOk;
    int year = yearStr.toInt(&yearOk);
    int month = monthStr.toInt(&monthOk);
    if (!yearOk || !monthOk) {
        return;
    }

    // Calculate Mondays in the selected month
    QDate firstDay(year, month, 1);
    int daysInMonth = firstDay.daysInMonth();

    for (int day = 1; day <= daysInMonth; ++day) {
        QDate date(year, month, day);
        if (date.dayOfWeek() == 1) { // Qt::Monday is 1
            ui->weekDDbox->addItem(QString("%1").arg(day, 2, 10, QChar('0')));
        }
    }
}

void MainWindow::onLogMessage(const QString& message)
{
    logToTerminal(message);
}

void MainWindow::onJobProgressUpdated(int progress)
{
    ui->progressBarWeekly->setValue(progress);
}

void MainWindow::onScriptStarted()
{
    // Disable buttons while script is running
    ui->openIZ->setEnabled(false);
    ui->runInitial->setEnabled(false);
    ui->runPreProof->setEnabled(false);
    ui->openProofFiles->setEnabled(false);
    ui->runPostProof->setEnabled(false);
    ui->openPrintFiles->setEnabled(false);
    ui->runPostPrint->setEnabled(false);

    // Set cursor to wait state
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Log the script start
    logToTerminal("Script execution started");
}

void MainWindow::onScriptFinished(bool success)
{
    // Re-enable buttons based on job state
    updateWidgetStatesBasedOnJobState();

    // Restore cursor
    QApplication::restoreOverrideCursor();

    // Log script completion
    if (success) {
        logToTerminal("<font color=\"green\">Script execution completed successfully</font>");
    } else {
        logToTerminal("<font color=\"red\">Script execution failed</font>");
    }

    // Update visual indicators
    updateLEDs();
    updateInstructions();
}

void MainWindow::logToTerminal(const QString& message)
{
    // Get current cursor position to preserve scroll state
    QTextCursor cursor = ui->terminalWindow->textCursor();
    bool wasAtEnd = cursor.atEnd();

    // Format message with timestamp and process HTML tags safely
    QString formattedMessage;
    if (message.contains("<") && message.contains(">")) {
        formattedMessage = QString("[%1] %2")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
            .arg(message);
    } else {
        formattedMessage = QString("[%1] %2")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
            .arg(message.toHtmlEscaped().replace("\n", "<br/>"));
    }

    // Save to database if a job is active
    if (m_jobController->isJobSaved()) {
        JobData* job = m_jobController->currentJob();
        m_dbManager->saveTerminalLog(job->year, job->month, job->week, message);
    }

    // Append the message
    ui->terminalWindow->append(formattedMessage);

    // Auto-scroll to bottom if we were already at the bottom
    if (wasAtEnd) {
        cursor.movePosition(QTextCursor::End);
        ui->terminalWindow->setTextCursor(cursor);
    }

    // Process events to ensure immediate display
    QCoreApplication::processEvents();
}
