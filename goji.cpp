#include "goji.h"
#include "ui_GOJI.h"
#include <QLineEdit>
#include <QCheckBox>
#include <QMessageBox>
#include <QDateTime>
#include <QDebug>
#include <QSignalBlocker>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
#include <QProcess>
#include <QFile>
#include <QStringList>
#include <QSqlQuery>
#include <QSqlError>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QDialog>
#include <QClipboard>
#include <QApplication>
#include <QLocale>
#include <QRegularExpression>
#include <QDate>
#include <QStandardPaths>
#include <QDir>
#include <QEventLoop>
#include <QIcon>
#include <QFileInfo>
#include <QDirIterator>
#include <QValidator>
#include <QToolButton>
#include <QComboBox>
#include <QAction>

// Use version defined in GOJI.pro
const QString VERSION = QString(APP_VERSION);

/**
* @brief Constructor: Initializes the Goji main window with UI setup, database, and settings.
* @param parent Parent widget, typically nullptr for main window.
*/
Goji::Goji(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    db(),
    openJobMenu(nullptr),
    weeklyMenu(nullptr),
    validator(nullptr),
    m_printWatcher(nullptr),
    m_inactivityTimer(nullptr),
    settings(new QSettings("GojiApp", "Goji")),
    currentJobType("RAC WEEKLY"),
    originalYear(""),
    originalMonth(""),
    originalWeek(""),
    isJobSaved(false),
    isJobDataLocked(false),
    isOpenIZComplete(false),
    isRunInitialComplete(false),
    isRunPreProofComplete(false),
    isOpenProofFilesComplete(false),
    isRunPostProofComplete(false),
    isOpenPrintFilesComplete(false),
    isRunPostPrintComplete(false),
    isProofRegenMode(false),
    isPostageLocked(false),
    proofFiles(),
    printFiles(),
    regenCheckboxes(),
    checkboxFileMap(),
    stepWeights({2.0, 9.0, 13.0, 13.0, 20.0, 10.0, 3.0, 20.0, 10.0}),
    totalSubtasks(),
    completedSubtasks()
{
    ui->setupUi(this);
    ui->tabWidget->setCurrentIndex(0);  // Set to RAC WEEKLY tab (first tab)
    setWindowTitle(tr("Goji v%1").arg(VERSION));
    setWindowIcon(QIcon(":/icons/ShinGoji.ico"));

    // Set regenTab to default to CBC tab (index 0)
    ui->regenTab->setCurrentIndex(0);

    // Create the "Open Job" menu
    openJobMenu = new QMenu(this);
    openJobMenu->setTitle(tr("Open Job"));
    weeklyMenu = new QMenu(this);
    weeklyMenu->setTitle(tr("Weekly"));
    openJobMenu->addMenu(weeklyMenu);

    connect(weeklyMenu, &QMenu::aboutToShow, this, &Goji::buildWeeklyMenu);
    ui->menuFile->insertMenu(ui->actionSave_Job, openJobMenu);

    // Disable "Open Job" menu and actions unless "RAC WEEKLY" is active
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        currentJobType = ui->tabWidget->tabText(index);
        openJobMenu->setEnabled(currentJobType == "RAC WEEKLY");
        ui->actionSave_Job->setEnabled(currentJobType == "RAC WEEKLY");
        ui->actionClose_Job->setEnabled(currentJobType == "RAC WEEKLY");
    });

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
    QWidget::setTabOrder(ui->ncwo1APostage, ui->ncwo2APostage);
    QWidget::setTabOrder(ui->ncwo2APostage, ui->ncwo1APPostage);
    QWidget::setTabOrder(ui->ncwo1APPostage, ui->ncwo2APPostage);
    QWidget::setTabOrder(ui->ncwo2APPostage, ui->prepifPostage);

    // Connect signals for buttons
    connect(ui->openIZ, &QPushButton::clicked, this, &Goji::onOpenIZClicked);
    connect(ui->runInitial, &QPushButton::clicked, this, &Goji::onRunInitialClicked);
    connect(ui->runPreProof, &QPushButton::clicked, this, &Goji::onRunPreProofClicked);
    connect(ui->openProofFiles, &QPushButton::clicked, this, &Goji::onOpenProofFilesClicked);
    connect(ui->runPostProof, &QPushButton::clicked, this, &Goji::onRunPostProofClicked);
    connect(ui->openPrintFiles, &QPushButton::clicked, this, &Goji::onOpenPrintFilesClicked);
    connect(ui->runPostPrint, &QPushButton::clicked, this, &Goji::onRunPostPrintClicked);
    connect(ui->lockButton, &QToolButton::toggled, this, &Goji::onLockButtonToggled);
    connect(ui->editButton, &QToolButton::toggled, this, &Goji::onEditButtonToggled);
    connect(ui->proofRegen, &QToolButton::toggled, this, &Goji::onProofRegenToggled);
    connect(ui->postageLock, &QToolButton::toggled, this, &Goji::onPostageLockToggled);
    connect(ui->proofDDbox, &QComboBox::currentTextChanged, this, &Goji::onProofDDboxChanged);
    connect(ui->printDDbox, &QComboBox::currentTextChanged, this, &Goji::onPrintDDboxChanged);
    connect(ui->yearDDbox, &QComboBox::currentTextChanged, this, &Goji::onYearDDboxChanged);
    connect(ui->monthDDbox, &QComboBox::currentTextChanged, this, &Goji::onMonthDDboxChanged);
    connect(ui->weekDDbox, &QComboBox::currentTextChanged, this, &Goji::onWeekDDboxChanged);

    // Connect checkbox signals for Qt 6.9 compatibility
    connect(ui->allCB, &QCheckBox::checkStateChanged, this, &Goji::onAllCBStateChanged);
    connect(ui->cbcCB, &QCheckBox::checkStateChanged, this, &Goji::updateAllCBState);
    connect(ui->excCB, &QCheckBox::checkStateChanged, this, &Goji::updateAllCBState);
    connect(ui->inactiveCB, &QCheckBox::checkStateChanged, this, &Goji::updateAllCBState);
    connect(ui->ncwoCB, &QCheckBox::checkStateChanged, this, &Goji::updateAllCBState);
    connect(ui->prepifCB, &QCheckBox::checkStateChanged, this, &Goji::updateAllCBState);

    // Connect menu action signals
    connect(ui->actionExit, &QAction::triggered, this, &Goji::onActionExitTriggered);
    connect(ui->actionClose_Job, &QAction::triggered, this, &Goji::onActionCloseJobTriggered);
    connect(ui->actionSave_Job, &QAction::triggered, this, &Goji::onActionSaveJobTriggered);
    connect(ui->actionCheck_for_updates, &QAction::triggered, this, &Goji::onCheckForUpdatesTriggered);

    // Initialize file mappings for proof files
    proofFiles = {
        {"CBC", {"/RAC/CBC/ART/CBC2 PROOF.indd", "/RAC/CBC/ART/CBC3 PROOF.indd"}},
        {"EXC", {"/RAC/EXC/ART/EXC PROOF.indd"}},
        {"INACTIVE", {"/RAC/INACTIVE/ART/A-PU PROOF.indd", "/RAC/INACTIVE/ART/FZA-PO PROOF.indd",
                      "/RAC/INACTIVE/ART/FZA-PU PROOF.indd", "/RAC/INACTIVE/ART/PR-PO PROOF.indd",
                      "/RAC/INACTIVE/ART/PR-PU PROOF.indd", "/RAC/INACTIVE/ART/A-PO PROOF.indd"}},
        {"NCWO", {"/RAC/NCWO/ART/NCWO 2-PR PROOF.indd", "/RAC/NCWO/ART/NCWO 1-A PROOF.indd",
                  "/RAC/NCWO/ART/NCWO 1-AP PROOF.indd", "/RAC/INACTIVE/ART/NCWO 1-APPR PROOF.indd",
                  "/RAC/NCWO/ART/NCWO 1-PR PROOF.indd", "/RAC/NCWO/ART/NCWO 2-A PROOF.indd",
                  "/RAC/NCWO/ART/NCWO 2-AP PROOF.indd", "/RAC/NCWO/ART/NCWO 2-APPR PROOF.indd"}},
        {"PREPIF", {"/RAC/PREPIF/ART/PREPIF US PROOF.indd", "/RAC/PREPIF/ART/PREPIF PR PROOF.indd"}}
    };

    // Initialize file mappings for print files
    printFiles = {
        {"CBC", {"/RAC/CBC/ART/CBC2 PRINT.indd", "/RAC/CBC/ART/CBC3 PRINT.indd"}},
        {"EXC", {"/RAC/EXC/ART/EXC PRINT.indd"}},
        {"NCWO", {"/RAC/NCWO/ART/NCWO 2-PR PRINT.indd", "/RAC/NCWO/ART/NCWO 1-A PRINT.indd",
                  "/RAC/NCWO/ART/NCWO 1-AP PRINT.indd", "/RAC/NCWO/ART/NCWO 1-APPR PRINT.indd",
                  "/RAC/NCWO/ART/NCWO 1-PR PRINT.indd", "/RAC/NCWO/ART/NCWO 2-A PRINT.indd",
                  "/RAC/NCWO/ART/NCWO 2-AP PRINT.indd", "/RAC/NCWO/ART/NCWO 2-APPR PRINT.indd"}},
        {"PREPIF", {"/RAC/PREPIF/ART/PREPIF US PRINT.indd", "/RAC/PREPIF/ART/PREPIF PR PRINT.indd"}}
    };

    // Initialize checkbox mappings for regeneration
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

    // Initialize UI state
    logToTerminal(tr("Goji started: %1").arg(QDateTime::currentDateTime().toString()));

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

    // Set up validator for postage QLineEdit widgets
    validator = new QRegularExpressionValidator(QRegularExpression("[0-9]*\\.?[0-9]*"));

    // Apply validator and connect editingFinished signal
    QList<QLineEdit*> postageLineEdits;
    postageLineEdits << ui->cbc2Postage << ui->cbc3Postage << ui->excPostage << ui->inactivePOPostage
                     << ui->inactivePUPostage << ui->ncwo1APostage << ui->ncwo2APostage
                     << ui->ncwo1APPostage << ui->ncwo2APPostage << ui->prepifPostage;
    for (QLineEdit *lineEdit : postageLineEdits) {
        lineEdit->setValidator(validator);
        connect(lineEdit, &QLineEdit::editingFinished, this, &Goji::formatCurrencyOnFinish);
    }

    // Initialize database with dynamic path based on build type
#ifdef QT_DEBUG
    QString defaultDbDirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Goji/SQL/debug";
#else
    QString defaultDbDirPath = "C:/Goji/database";
#endif
    QString dbDirPath = settings->value("DatabasePath", defaultDbDirPath).toString();
    QDir dbDir(dbDirPath);
    if (!dbDir.exists()) {
        if (!dbDir.mkpath(".")) {
            QMessageBox::critical(this, tr("Directory Error"), tr("Failed to create directory: %1").arg(dbDirPath));
            return;
        }
    }
    QString dbPath = dbDirPath + "/jobs.db";
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        QMessageBox::critical(this, tr("Database Error"), tr("Failed to open database: %1").arg(db.lastError().text()));
        return;
    }

    // Log database path for debugging
    logToTerminal(tr("Database path: %1").arg(dbPath));

    // Create jobs_rac_weekly table
    QSqlQuery query(db);
    query.exec("CREATE TABLE IF NOT EXISTS jobs_rac_weekly ("
               "year INTEGER, "
               "month INTEGER, "
               "week INTEGER, "
               "cbc_job_number TEXT, "
               "ncwo_job_number TEXT, "
               "inactive_job_number TEXT, "
               "prepif_job_number TEXT, "
               "exc_job_number TEXT, "
               "cbc2_postage TEXT, "
               "cbc3_postage TEXT, "
               "exc_postage TEXT, "
               "inactive_po_postage TEXT, "
               "inactive_pu_postage TEXT, "
               "ncwo1_a_postage TEXT, "
               "ncwo2_a_postage TEXT, "
               "ncwo1_ap_postage TEXT, "
               "ncwo2_ap_postage TEXT, "
               "prepif_postage TEXT, "
               "progress TEXT, "
               "step0_complete INTEGER DEFAULT 0, "
               "step1_complete INTEGER DEFAULT 0, "
               "step2_complete INTEGER DEFAULT 0, "
               "step3_complete INTEGER DEFAULT 0, "
               "step4_complete INTEGER DEFAULT 0, "
               "step5_complete INTEGER DEFAULT 0, "
               "step6_complete INTEGER DEFAULT 0, "
               "step7_complete INTEGER DEFAULT 0, "
               "step8_complete INTEGER DEFAULT 0, "
               "PRIMARY KEY (year, month, week)"
               ")");
    if (query.lastError().isValid()) {
        qDebug() << tr("Error creating table:") << query.lastError().text();
        QMessageBox::critical(this, tr("Database Error"), tr("Failed to create jobs table: %1").arg(query.lastError().text()));
    }

    // Create proof_versions table
    query.exec("CREATE TABLE IF NOT EXISTS proof_versions ("
               "file_path TEXT PRIMARY KEY, "
               "version INTEGER DEFAULT 1"
               ")");
    if (query.lastError().isValid()) {
        qDebug() << tr("Error creating proof_versions table:") << query.lastError().text();
        QMessageBox::critical(this, tr("Database Error"), tr("Failed to create proof_versions table: %1").arg(query.lastError().text()));
    }

    // Create post_proof_counts table
    query.exec("CREATE TABLE IF NOT EXISTS post_proof_counts ("
               "job_number TEXT, "
               "week TEXT, "
               "project TEXT, "
               "pr_count INTEGER, "
               "canc_count INTEGER, "
               "us_count INTEGER, "
               "postage TEXT)");
    if (query.lastError().isValid()) {
        qDebug() << tr("Error creating post_proof_counts table:") << query.lastError().text();
        QMessageBox::critical(this, tr("Database Error"), tr("Failed to create post_proof_counts table: %1").arg(query.lastError().text()));
    }

    // Create count_comparison table
    query.exec("CREATE TABLE IF NOT EXISTS count_comparison ("
               "group_name TEXT, "
               "input_count INTEGER, "
               "output_count INTEGER, "
               "difference INTEGER)");
    if (query.lastError().isValid()) {
        qDebug() << tr("Error creating count_comparison table:") << query.lastError().text();
        QMessageBox::critical(this, tr("Database Error"), tr("Failed to create count_comparison table: %1").arg(query.lastError().text()));
    }

    // Initialize checkbox mapping for regeneration
    regenCheckboxes.insert("CBC", ui->cbcCB);
    regenCheckboxes.insert("EXC", ui->excCB);
    regenCheckboxes.insert("INACTIVE", ui->inactiveCB);
    regenCheckboxes.insert("NCWO", ui->ncwoCB);
    regenCheckboxes.insert("PREPIF", ui->prepifCB);

    // Disable regeneration checkboxes and "ALL" checkbox by default
    for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
        it.value()->setEnabled(false);
    }
    ui->allCB->setEnabled(false);

    // Populate yearDDbox (no blank item, as Qt Designer provides one)
    int currentYear = QDate::currentDate().year();
    ui->yearDDbox->addItem(QString::number(currentYear - 1));
    ui->yearDDbox->addItem(QString::number(currentYear));
    ui->yearDDbox->addItem(QString::number(currentYear + 1));

    // Initialize LEDs
    updateLEDs();

    // Set initial widget states based on isJobSaved
    updateWidgetStatesBasedOnJobState();

    // Initialize step weights
    stepWeights = {2.0, 9.0, 13.0, 13.0, 20.0, 10.0, 3.0, 20.0, 10.0}; // Steps 1-9

    // Initialize subtask trackers
    for (size_t i = 0; i < NUM_STEPS; ++i) {
        totalSubtasks[i] = 1; // Default to 1 for binary steps
        completedSubtasks[i] = 0; // Nothing completed yet
    }

    // Set progress bar range and initial value
    ui->progressBarWeekly->setRange(0, 100);
    ui->progressBarWeekly->setValue(0);

    // Initialize watchers and timers
    initWatchersAndTimers();

    // Set up "Manage Scripts" menu
    QMenu* manageScriptsMenu = ui->menuManage_Scripts;
    if (manageScriptsMenu) {
        manageScriptsMenu->clear();
        QMenu* racMenu = manageScriptsMenu->addMenu("RAC");
        racMenu->addMenu("Weekly")->addAction("Placeholder");
        racMenu->addMenu("Monthly")->addAction("Placeholder");
        racMenu->addMenu("Quarterly")->addAction("Placeholder");
        racMenu->addMenu("Bi-Annual")->addAction("Placeholder");
        QMenu* trachmarMenu = manageScriptsMenu->addMenu("Trachmar");
        trachmarMenu->addMenu("Weekly PC")->addAction("Placeholder");
        trachmarMenu->addMenu("Weekly Packets/IDO")->addAction("Placeholder");
        trachmarMenu->addMenu("Term")->addAction("Placeholder");
    }
}

// Slot: Open InputZIP directory
void Goji::onOpenIZClicked()
{
    if (currentJobType != "RAC WEEKLY") return;
    QString izPath = settings->value("IZPath", QCoreApplication::applicationDirPath() + "/RAC/WEEKLY/INPUTZIP").toString();
    QDesktopServices::openUrl(QUrl::fromLocalFile(izPath));
    isOpenIZComplete = true;
    completedSubtasks[0] = 1;
    updateProgressBar();
    updateLEDs();
    logToTerminal(tr("Opened IZ directory: %1").arg(izPath));
}

// Slot: Run initial script for processing
void Goji::onRunInitialClicked()
{
    if (currentJobType != "RAC WEEKLY") return;
    if (!isOpenIZComplete) {
        QMessageBox::warning(this, tr("Step Incomplete"), tr("Please open InputZIP first."));
        return;
    }
    if (!isJobSaved) {
        QMessageBox::warning(this, tr("Warning"), tr("Please save the job before running initial processing."));
        return;
    }

    logToTerminal(tr("Running initial processing..."));
    QString scriptPath = settings->value("InitialScript", "C:/Goji/Scripts/RAC/WEEKLIES/01RUNFIRST.py").toString();
    runScript("python", QStringList() << scriptPath);
    isRunInitialComplete = true;
    completedSubtasks[1] = 1;
    updateProgressBar();
    updateLEDs();
}

// Slot: Run pre-proof processing
void Goji::onRunPreProofClicked()
{
    if (currentJobType != "RAC WEEKLY") return;
    if (!isRunInitialComplete) {
        QMessageBox::warning(this, tr("Step Incomplete"), tr("Please run Initial Script first."));
        return;
    }
    if (!isPostageLocked) {
        QMessageBox::warning(this, tr("Postage Not Locked"), tr("Please lock the postage data first."));
        return;
    }

    QMap<QString, QStringList> requiredFiles;
    requiredFiles["CBC"] = {"CBC2_WEEKLY.csv", "CBC3_WEEKLY.csv"};
    requiredFiles["EXC"] = {"EXC_OUTPUT.csv"};
    requiredFiles["INACTIVE"] = {"A-PO.txt", "A-PU.txt"};
    requiredFiles["NCWO"] = {"1-A_OUTPUT.csv", "1-AP_OUTPUT.csv", "2-A_OUTPUT.csv", "2-AP_OUTPUT.csv"};
    requiredFiles["PREPIF"] = {"PRE_PIF.csv"};

    QString basePath = settings->value("BasePath", QCoreApplication::applicationDirPath()).toString();
    QStringList missingFiles;
    for (auto it = requiredFiles.constBegin(); it != requiredFiles.constEnd(); ++it) {
        QString jobType = it.key();
        QString outputDir = basePath + "/RAC/" + jobType + "/JOB/OUTPUT";
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

    logToTerminal(tr("Running pre-proof processing..."));
    QString scriptPath = settings->value("PreProofScript", QCoreApplication::applicationDirPath() + "/Scripts/RAC/WEEKLIES/02RUNSECOND.bat").toString();
    QString week = ui->monthDDbox->currentText() + "." + ui->weekDDbox->currentText();
    QStringList arguments;
    arguments << "/c" << scriptPath << basePath << ui->cbcJobNumber->text() << week;
    runScript("cmd.exe", arguments);
    isRunPreProofComplete = true;
    completedSubtasks[2] = 1;
    completedSubtasks[3] = 1;
    updateProgressBar();
    updateLEDs();
}

// Slot: Open proof files for selected job type
void Goji::onOpenProofFilesClicked()
{
    if (currentJobType != "RAC WEEKLY") return;
    if (!isRunPreProofComplete) {
        QMessageBox::warning(this, tr("Step Incomplete"), tr("Please run Pre-Proof first."));
        return;
    }
    QString selection = ui->proofDDbox->currentText();
    if (selection.isEmpty()) {
        logToTerminal(tr("Please select a job type from proofDDbox."));
        return;
    }

    logToTerminal(tr("Checking proof files for: %1").arg(selection));
    checkProofFiles(selection);
    if (isOpenProofFilesComplete) {
        completedSubtasks[4] = 1;
        updateProgressBar();
    }
    updateLEDs();
}

// Slot: Run post-proof processing
void Goji::onRunPostProofClicked()
{
    if (currentJobType != "RAC WEEKLY") return;
    if (!isOpenProofFilesComplete) {
        QMessageBox::warning(this, tr("Step Incomplete"), tr("Please open proof files first."));
        return;
    }

    QMap<QString, QStringList> expectedProofFiles;
    expectedProofFiles["CBC"] = {"CBC2 PROOF.pdf", "CBC3 PROOF.pdf"};
    expectedProofFiles["EXC"] = {"EXC PROOF.pdf"};
    expectedProofFiles["INACTIVE"] = {"INACTIVE A-PO PROOF.pdf", "INACTIVE A-PU PROOF.pdf", "INACTIVE AT-PO PROOF.pdf",
                                      "INACTIVE AT-PU PROOF.pdf", "INACTIVE PR-PO PROOF.pdf", "INACTIVE PR-PU PROOF.pdf"};
    expectedProofFiles["NCWO"] = {"NCWO 1-A PROOF.pdf", "NCWO 1-AP PROOF.pdf", "NCWO 1-APPR PROOF.pdf", "NCWO 1-PR PROOF.pdf",
                                  "NCWO 2-A PROOF.pdf", "NCWO 2-AP PROOF.pdf", "NCWO 2-APPR PROOF.pdf", "NCWO 2-PR PROOF.pdf"};
    expectedProofFiles["PREPIF"] = {"PREPIF US PROOF.pdf", "PREPIF PR PROOF.pdf"};

    QString basePath = settings->value("BasePath", QCoreApplication::applicationDirPath()).toString();
    QStringList missingFiles;
    for (const QString& jobType : QStringList{"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"}) {
        const QStringList& fileList = expectedProofFiles.value(jobType);
        for (const QString& file : fileList) {
            QString proofDir = basePath + "/RAC/" + jobType + "/JOB/PROOF";
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

    logToTerminal(tr("Running post-proof processing..."));
    if (isProofRegenMode) {
        regenerateProofs();
    } else {
        QString scriptPath = settings->value("PostProofScript", QCoreApplication::applicationDirPath() + "/Scripts/RAC/WEEKLIES/04POSTPROOF.py").toString();
        QString week = ui->monthDDbox->currentText() + "." + ui->weekDDbox->currentText();

        QProcess *process = new QProcess();
        connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
            ui->terminalWindow->append(QString::fromUtf8(process->readAllStandardOutput()));
        });
        connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
            ui->terminalWindow->append("<font color=\"red\">" + QString::fromUtf8(process->readAllStandardError()) + "</font>");
        });
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
                    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                        ui->terminalWindow->append(tr("Script completed successfully."));
                        savePostProofCounts();
                        isRunPostProofComplete = true;
                        completedSubtasks[5] = 1;
                        updateProgressBar();
                        enableProofApprovalCheckboxes();
                        updateLEDs();
                    } else {
                        ui->terminalWindow->append(tr("Script failed with exit code %1").arg(exitCode));
                    }
                    process->deleteLater();
                });

        QStringList arguments;
        arguments << scriptPath
                  << "--base_path" << basePath
                  << "--week" << week
                  << "--cbc_job" << ui->cbcJobNumber->text()
                  << "--exc_job" << ui->excJobNumber->text()
                  << "--inactive_job" << ui->inactiveJobNumber->text()
                  << "--ncwo_job" << ui->ncwoJobNumber->text()
                  << "--prepif_job" << ui->prepifJobNumber->text()
                  << "--cbc2_postage" << ui->cbc2Postage->text()
                  << "--cbc3_postage" << ui->cbc3Postage->text()
                  << "--exc_postage" << ui->excPostage->text()
                  << "--inactive_po_postage" << ui->inactivePOPostage->text()
                  << "--inactive_pu_postage" << ui->inactivePUPostage->text()
                  << "--ncwo1_a_postage" << ui->ncwo1APostage->text()
                  << "--ncwo2_a_postage" << ui->ncwo2APostage->text()
                  << "--ncwo1_ap_postage" << ui->ncwo1APPostage->text()
                  << "--ncwo2_ap_postage" << ui->ncwo2APPostage->text()
                  << "--prepif_postage" << ui->prepifPostage->text();
        process->start("python", arguments);
    }
}

// Slot: Open print files for selected job type
void Goji::onOpenPrintFilesClicked()
{
    if (currentJobType != "RAC WEEKLY") return;
    if (!isRunPostProofComplete) {
        QMessageBox::warning(this, tr("Step Incomplete"), tr("Please run Post-Proof first."));
        return;
    }
    QString selection = ui->printDDbox->currentText();
    if (selection.isEmpty()) {
        logToTerminal(tr("Please select a job type from printDDbox."));
        return;
    }

    logToTerminal(tr("Checking print files for: %1").arg(selection));
    checkPrintFiles(selection);
    if (isOpenPrintFilesComplete) {
        completedSubtasks[7] = 1;
        updateProgressBar();
    }
    updateLEDs();
}

// Slot: Run post-print processing
void Goji::onRunPostPrintClicked()
{
    if (currentJobType != "RAC WEEKLY") return;
    if (!isOpenPrintFilesComplete) {
        QMessageBox::warning(this, tr("Step Incomplete"), tr("Please open print files first."));
        return;
    }
    if (completedSubtasks[6] != 1) {
        QMessageBox::warning(this, tr("Step Incomplete"), tr("Please approve all proofs first."));
        return;
    }

    logToTerminal(tr("Running post-print processing..."));
    QString scriptPath = settings->value("PostPrintScript", QCoreApplication::applicationDirPath() + "/Scripts/RAC/WEEKLIES/05POSTPRINT.ps1").toString();
    QStringList arguments;
    arguments << "-ExecutionPolicy" << "Bypass" << "-File" << scriptPath;
    runScript("powershell.exe", arguments);
    isRunPostPrintComplete = true;
    completedSubtasks[8] = 1;
    updateProgressBar();
    updateLEDs();
}

// Slot: Handle exit action
void Goji::onActionExitTriggered()
{
    QApplication::quit();
}

// Slot: Handle proof dropdown changes
void Goji::onProofDDboxChanged(const QString &text)
{
    if (currentJobType != "RAC WEEKLY") return;
    logToTerminal(tr("Proof selection changed to: %1").arg(text));
    if (!text.isEmpty()) {
        checkProofFiles(text);
    }
}

// Slot: Handle print dropdown changes
void Goji::onPrintDDboxChanged(const QString &text)
{
    if (currentJobType != "RAC WEEKLY") return;
    logToTerminal(tr("Print selection changed to: %1").arg(text));
    if (!text.isEmpty()) {
        checkPrintFiles(text);
    }
}

// Slot: Handle year dropdown changes
void Goji::onYearDDboxChanged(const QString &text)
{
    if (currentJobType != "RAC WEEKLY") return;
    originalYear = text;
    logToTerminal(tr("Year changed to: %1").arg(text));
    populateWeekDDbox();
}

// Slot: Handle month dropdown changes
void Goji::onMonthDDboxChanged(const QString &text)
{
    if (currentJobType != "RAC WEEKLY") return;
    originalMonth = text;
    logToTerminal(tr("Month changed to: %1").arg(text));
    populateWeekDDbox();
}

// Slot: Handle week dropdown changes
void Goji::onWeekDDboxChanged(const QString &text)
{
    if (currentJobType != "RAC WEEKLY") return;
    originalWeek = text;
    logToTerminal(tr("Week changed to: %1").arg(text));
}

// Slot: Handle lock button toggle for job data
void Goji::onLockButtonToggled(bool checked)
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

        // Check if the job exists and handle overwrite
        bool isNewJob = !isJobSaved;
        bool isSameJob = isJobSaved && year == originalYear && month == originalMonth && week == originalWeek;
        if (jobExists(year, month, week) && !isSameJob) {
            if (!confirmOverwrite(year, month, week)) {
                ui->lockButton->setChecked(false);
                return;
            }
            // Delete existing job to allow overwrite
            deleteJob(year, month, week);
        }

        if (isNewJob) {
            insertJob();
            isJobSaved = true;
            originalYear = year;
            originalMonth = month;
            originalWeek = week;

            // Create job folders using helper function
            createJobFolders(year, month, week);
            // Check if folder creation failed
            if (!QDir(settings->value("BasePath", "C:/Goji/RAC").toString() + "/" + QString("CBC/%1.%2").arg(month, week)).exists()) {
                ui->lockButton->setChecked(false);
                return;
            }

            logToTerminal(tr("New job created for year %1, month %2, week %3").arg(year, month, week));
        } else {
            // Update existing job
            if (!isSameJob) {
                // Delete the original job if year/month/week changed
                deleteJob(originalYear, originalMonth, originalWeek);
            }
            updateJob();
            originalYear = year;
            originalMonth = month;
            originalWeek = week;
            logToTerminal(tr("Job updated for year %1, month %2, week %3").arg(year, month, week));
        }

        lockJobDataFields(true);
        ui->lockButton->setEnabled(true);
        ui->editButton->setChecked(false);
        ui->editButton->setEnabled(true);
        updateWidgetStatesBasedOnJobState();
    } else {
        lockJobDataFields(false);
        logToTerminal(tr("Job data unlocked for editing"));
        ui->lockButton->setEnabled(true);
        ui->editButton->setEnabled(true);
    }
}

// Slot: Handle edit button toggle for job data
void Goji::onEditButtonToggled(bool checked)
{
    if (currentJobType != "RAC WEEKLY") return;
    lockJobDataFields(!checked);
    if (checked) {
        ui->lockButton->setChecked(false);
        logToTerminal(tr("Job data editing enabled"));
    } else {
        logToTerminal(tr("Job data editing disabled"));
    }
}

// Slot: Handle proof regeneration toggle
void Goji::onProofRegenToggled(bool checked)
{
    if (currentJobType != "RAC WEEKLY") return;
    isProofRegenMode = checked;
    ui->regenTab->setEnabled(checked);
    for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
        it.value()->setEnabled(checked);
    }
    ui->allCB->setEnabled(checked);
    logToTerminal(tr("Proof regeneration mode %1").arg(checked ? tr("enabled") : tr("disabled")));
}

// Slot: Handle postage lock toggle
void Goji::onPostageLockToggled(bool checked)
{
    if (currentJobType != "RAC WEEKLY") return;
    isPostageLocked = checked;
    lockPostageFields(checked);
    logToTerminal(tr("Postage fields %1").arg(checked ? tr("locked") : tr("unlocked")));
}

// Slot: Handle "Close Job" menu action
void Goji::onActionCloseJobTriggered()
{
    if (currentJobType != "RAC WEEKLY") return;
    isJobSaved = false;
    isJobDataLocked = false;
    clearJobNumbers();
    updateWidgetStatesBasedOnJobState();
    logToTerminal(tr("Job closed."));
}

// Slot: Handle "Save Job" menu action
void Goji::onActionSaveJobTriggered()
{
    if (currentJobType != "RAC WEEKLY") return;
    if (isJobSaved) {
        updateJob();
    } else {
        insertJob();
    }
    logToTerminal(tr("Job saved successfully."));
}

// Slot: Handle "Check for Updates" menu action
void Goji::onCheckForUpdatesTriggered()
{
    QMessageBox::information(this, tr("Updates"), tr("Checking for updates is not yet implemented."));
    logToTerminal(tr("Checked for updates."));
}

// Slot: Handle print directory changes
void Goji::onPrintDirChanged(const QString &path)
{
    logToTerminal(tr("Print directory changed: %1").arg(path));
    checkPrintFiles(ui->printDDbox->currentText());
}

// Slot: Format postage fields on editing finished
void Goji::formatCurrencyOnFinish()
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

// Slot: Handle "Get Count Table" button click
void Goji::onGetCountTableClicked()
{
    if (currentJobType != "RAC WEEKLY") return;
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Post-Proof Counts and Comparison"));
    QVBoxLayout *layout = new QVBoxLayout(dialog);
    QTableWidget *countsTable = new QTableWidget(dialog);
    countsTable->setColumnCount(7);
    countsTable->setHorizontalHeaderLabels({tr("Job Number"), tr("Week"), tr("Project"), tr("PR Count"), tr("CANC Count"), tr("US Count"), tr("Postage")});
    countsTable->setStyleSheet("QTableWidget { border: 1px solid black; } QTableWidget::item { border: 1px solid black; }");

    QSqlQuery countsQuery("SELECT job_number, week, project, pr_count, canc_count, us_count, postage FROM post_proof_counts", db);
    int row = 0;
    countsTable->setRowCount(countsQuery.size());
    while (countsQuery.next()) {
        countsTable->setItem(row, 0, new QTableWidgetItem(countsQuery.value("job_number").toString()));
        countsTable->setItem(row, 1, new QTableWidgetItem(countsQuery.value("week").toString()));
        countsTable->setItem(row, 2, new QTableWidgetItem(countsQuery.value("project").toString()));
        countsTable->setItem(row, 3, new QTableWidgetItem(countsQuery.value("pr_count").toString()));
        countsTable->setItem(row, 4, new QTableWidgetItem(countsQuery.value("canc_count").toString()));
        countsTable->setItem(row, 5, new QTableWidgetItem(countsQuery.value("us_count").toString()));
        countsTable->setItem(row, 6, new QTableWidgetItem(countsQuery.value("postage").toString()));
        row++;
    }

    QPushButton *copyCountsButton = new QPushButton(tr("Copy Counts"), dialog);
    connect(copyCountsButton, &QPushButton::clicked, [countsTable]() {
        QString html = "<table border='1'>";
        for (int i = 0; i < countsTable->rowCount(); ++i) {
            html += "<tr>";
            for (int j = 0; j < countsTable->columnCount(); ++j) {
                html += "<td>" + (countsTable->item(i, j) ? countsTable->item(i, j)->text() : "") + "</td>";
            }
            html += "</tr>";
        }
        html += "</table>";
        QApplication::clipboard()->setText(html, QClipboard::Clipboard);
    });
    layout->addWidget(copyCountsButton);
    layout->addWidget(countsTable);

    QTableWidget *comparisonTable = new QTableWidget(dialog);
    comparisonTable->setColumnCount(4);
    comparisonTable->setHorizontalHeaderLabels({tr("Group"), tr("Input Count"), tr("Output Count"), tr("Difference")});
    comparisonTable->setStyleSheet("QTableWidget { border: 1px solid black; } QTableWidget::item { border: 1px solid black; }");

    QSqlQuery comparisonQuery("SELECT group_name, input_count, output_count, difference FROM count_comparison", db);
    row = 0;
    comparisonTable->setRowCount(comparisonQuery.size());
    while (comparisonQuery.next()) {
        comparisonTable->setItem(row, 0, new QTableWidgetItem(comparisonQuery.value("group_name").toString()));
        comparisonTable->setItem(row, 1, new QTableWidgetItem(comparisonQuery.value("input_count").toString()));
        comparisonTable->setItem(row, 2, new QTableWidgetItem(comparisonQuery.value("output_count").toString()));
        comparisonTable->setItem(row, 3, new QTableWidgetItem(comparisonQuery.value("difference").toString()));
        row++;
    }

    QPushButton *copyComparisonButton = new QPushButton(tr("Copy Comparison"), dialog);
    connect(copyComparisonButton, &QPushButton::clicked, [comparisonTable]() {
        QString html = "<table border='1'>";
        for (int i = 0; i < comparisonTable->rowCount(); ++i) {
            html += "<tr>";
            for (int j = 0; j < comparisonTable->columnCount(); ++j) {
                html += "<td>" + (comparisonTable->item(i, j) ? comparisonTable->item(i, j)->text() : "") + "</td>";
            }
            html += "</tr>";
        }
        html += "</table>";
        QApplication::clipboard()->setText(html, QClipboard::Clipboard);
    });
    layout->addWidget(copyComparisonButton);
    layout->addWidget(comparisonTable);

    dialog->setLayout(layout);
    dialog->resize(600, 400);
    dialog->exec();
}

// Slot: Handle regen proof button click
void Goji::onRegenProofButtonClicked()
{
    if (currentJobType != "RAC WEEKLY") return;
    if (!isProofRegenMode) {
        QMessageBox::warning(this, tr("Regen Mode Disabled"), tr("Please enable Proof Regeneration mode first."));
        return;
    }
    regenerateProofs();
    logToTerminal(tr("Regen Proof button clicked."));
}

// Slot: Handle inactivity timeout
void Goji::onInactivityTimeout()
{
    if (currentJobType != "RAC WEEKLY") return;
    logToTerminal(tr("Inactivity timeout reached."));
    if (isJobSaved && !isJobDataLocked) {
        onActionSaveJobTriggered();
        logToTerminal(tr("Auto-saved job due to inactivity."));
    }
}

// Helper: Log messages to terminal window
void Goji::logToTerminal(const QString &message)
{
    ui->terminalWindow->append(QString("[%1] %2").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"), message));
}

// Helper: Run external scripts
void Goji::runScript(const QString &program, const QStringList &arguments)
{
    QProcess *process = new QProcess();
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        ui->terminalWindow->append(QString::fromUtf8(process->readAllStandardOutput()));
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        ui->terminalWindow->append("<font color=\"red\">" + QString::fromUtf8(process->readAllStandardError()) + "</font>");
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
                if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                    ui->terminalWindow->append(tr("Script completed successfully."));
                } else {
                    ui->terminalWindow->append(tr("Script failed with exit code %1").arg(exitCode));
                }
                process->deleteLater();
            });
    process->start(program, arguments);
}

// Helper: Check proof files for a job type
void Goji::checkProofFiles(const QString &selection)
{
    if (currentJobType != "RAC WEEKLY") return;
    if (!proofFiles.contains(selection)) {
        logToTerminal(tr("No proof files defined for %1").arg(selection));
        return;
    }

    QString proofPath = settings->value("ProofPath", QCoreApplication::applicationDirPath() + "/RAC/" + selection + "/JOB/PROOF").toString();
    QDir proofDir(proofPath);
    if (!proofDir.exists()) {
        logToTerminal(tr("Proof directory does not exist: %1").arg(proofPath));
        return;
    }

    QStringList expectedFiles = proofFiles[selection];
    bool allFilesPresent = true;
    for (const QString &file : expectedFiles) {
        if (!proofDir.exists(file)) {
            logToTerminal(tr("Missing proof file: %1").arg(file));
            allFilesPresent = false;
        }
    }

    if (allFilesPresent) {
        logToTerminal(tr("All proof files present for %1").arg(selection));
        isOpenProofFilesComplete = true;
    } else {
        logToTerminal(tr("Some proof files missing for %1").arg(selection));
        isOpenProofFilesComplete = false;
    }
}

// Helper: Check print files for a job type
void Goji::checkPrintFiles(const QString &selection)
{
    if (currentJobType != "RAC WEEKLY") return;
    if (!printFiles.contains(selection)) {
        logToTerminal(tr("No print files defined for %1").arg(selection));
        return;
    }

    QString printPath = settings->value("PrintPath", QCoreApplication::applicationDirPath() + "/RAC/" + selection + "/JOB/PRINT").toString();
    QDir printDir(printPath);
    if (!printDir.exists()) {
        logToTerminal(tr("Print directory does not exist: %1").arg(printPath));
        return;
    }

    QStringList expectedFiles = printFiles[selection];
    bool allFilesPresent = true;
    for (const QString &file : expectedFiles) {
        if (!printDir.exists(file)) {
            logToTerminal(tr("Missing print file: %1").arg(file));
            allFilesPresent = false;
        }
    }

    if (allFilesPresent) {
        logToTerminal(tr("All print files present for %1").arg(selection));
        isOpenPrintFilesComplete = true;
    } else {
        logToTerminal(tr("Some print files missing for %1").arg(selection));
        isOpenPrintFilesComplete = false;
    }
}

// Helper: Regenerate proofs for selected job types
void Goji::regenerateProofs()
{
    if (currentJobType != "RAC WEEKLY") return;
    logToTerminal(tr("Regenerating proofs..."));
    QStringList jobTypes = {"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"};
    for (const QString& jobType : jobTypes) {
        if (!regenCheckboxes[jobType]->isChecked()) {
            QStringList filesToRegen;
            for (auto it = checkboxFileMap.constBegin(); it != checkboxFileMap.constEnd(); ++it) {
                if (it.value().first == jobType && it.key()->isChecked()) {
                    filesToRegen << it.value().second;
                }
            }
            if (!filesToRegen.isEmpty()) {
                int nextVersion = getNextProofVersion(filesToRegen.first());
                runProofRegenScript(jobType, filesToRegen, nextVersion);
            }
        }
    }
    logToTerminal(tr("Proof regeneration complete."));
}

// Helper: Get the next proof version for a file
int Goji::getNextProofVersion(const QString& filePath)
{
    if (currentJobType != "RAC WEEKLY") return 1;
    QSqlQuery query(db);
    query.prepare("SELECT version FROM proof_versions WHERE file_path = :filePath");
    query.bindValue(":filePath", filePath);
    if (query.exec() && query.next()) {
        return query.value(0).toInt() + 1;
    }
    return 2;
}

// Helper: Run the proof regeneration script
void Goji::runProofRegenScript(const QString& jobType, const QStringList& files, int version)
{
    if (currentJobType != "RAC WEEKLY") return;
    QString scriptPath = settings->value("PostProofScript", QCoreApplication::applicationDirPath() + "/Scripts/RAC/WEEKLIES/04POSTPROOF.py").toString();
    QString week = ui->monthDDbox->currentText() + "." + ui->weekDDbox->currentText();
    QString jobNumber = getJobNumberForJobType(jobType);
    QString basePath = settings->value("BasePath", "C:/Goji/RAC").toString();

    QStringList arguments;
    arguments << scriptPath
              << "--base_path" << basePath
              << "--job_type" << jobType
              << "--job_number" << jobNumber
              << "--week" << week
              << "--version" << QString::number(version);
    for (const QString& file : files) {
        arguments << "--proof_files" << file;
    }

    runScript("python", arguments);

    QSqlQuery query(db);
    for (const QString& file : files) {
        query.prepare("INSERT OR REPLACE INTO proof_versions (file_path, version) VALUES (:filePath, :version)");
        query.bindValue(":filePath", file);
        query.bindValue(":version", version);
        if (!query.exec()) {
            logToTerminal(tr("Failed to update proof version for %1: %2").arg(file, query.lastError().text()));
        }
    }
}

// Database: Insert a new job into the database
void Goji::insertJob()
{
    if (currentJobType != "RAC WEEKLY") return;
    QSqlQuery query(db);
    query.prepare("INSERT INTO jobs_rac_weekly (year, month, week, cbc_job_number, ncwo_job_number, inactive_job_number, "
                  "prepif_job_number, exc_job_number, cbc2_postage, cbc3_postage, exc_postage, inactive_po_postage, "
                  "inactive_pu_postage, ncwo1_a_postage, ncwo2_a_postage, ncwo1_ap_postage, ncwo2_ap_postage, prepif_postage, "
                  "progress, step0_complete, step1_complete, step2_complete, step3_complete, step4_complete, "
                  "step5_complete, step6_complete, step7_complete, step8_complete) "
                  "VALUES (:year, :month, :week, :cbc, :ncwo, :inactive, :prepif, :exc, :cbc2, :cbc3, :exc_p, :in_po, "
                  ":in_pu, :nc1a, :nc2a, :nc1ap, :nc2ap, :prepif_p, :progress, 0, 0, 0, 0, 0, 0, 0, 0, 0)");
    query.bindValue(":year", originalYear.toInt());
    query.bindValue(":month", originalMonth.toInt());
    query.bindValue(":week", originalWeek.toInt());
    query.bindValue(":cbc", ui->cbcJobNumber->text());
    query.bindValue(":ncwo", ui->ncwoJobNumber->text());
    query.bindValue(":inactive", ui->inactiveJobNumber->text());
    query.bindValue(":prepif", ui->prepifJobNumber->text());
    query.bindValue(":exc", ui->excJobNumber->text());
    query.bindValue(":cbc2", ui->cbc2Postage->text());
    query.bindValue(":cbc3", ui->cbc3Postage->text());
    query.bindValue(":exc_p", ui->excPostage->text());
    query.bindValue(":in_po", ui->inactivePOPostage->text());
    query.bindValue(":in_pu", ui->inactivePUPostage->text());
    query.bindValue(":nc1a", ui->ncwo1APostage->text());
    query.bindValue(":nc2a", ui->ncwo2APostage->text());
    query.bindValue(":nc1ap", ui->ncwo1APPostage->text());
    query.bindValue(":nc2ap", ui->ncwo2APPostage->text());
    query.bindValue(":prepif_p", ui->prepifPostage->text());
    query.bindValue(":progress", "created");

    if (!query.exec()) {
        logToTerminal(tr("Failed to insert job: %1").arg(query.lastError().text()));
        QMessageBox::critical(this, tr("Database Error"), tr("Failed to insert job: %1").arg(query.lastError().text()));
    } else {
        logToTerminal(tr("Job inserted successfully."));
    }
}

// Database: Update an existing job in the database
void Goji::updateJob()
{
    if (currentJobType != "RAC WEEKLY") return;
    QSqlQuery query(db);
    query.prepare("UPDATE jobs_rac_weekly SET cbc_job_number = :cbc, ncwo_job_number = :ncwo, inactive_job_number = :inactive, "
                  "prepif_job_number = :prepif, exc_job_number = :exc, cbc2_postage = :cbc2, cbc3_postage = :cbc3, "
                  "exc_postage = :exc_p, inactive_po_postage = :in_po, inactive_pu_postage = :in_pu, "
                  "ncwo1_a_postage = :nc1a, ncwo2_a_postage = :nc2a, ncwo1_ap_postage = :nc1ap, "
                  "ncwo2_ap_postage = :nc2ap, prepif_postage = :prepif_p, progress = :progress "
                  "WHERE year = :year AND month = :month AND week = :week");
    query.bindValue(":cbc", ui->cbcJobNumber->text());
    query.bindValue(":ncwo", ui->ncwoJobNumber->text());
    query.bindValue(":inactive", ui->inactiveJobNumber->text());
    query.bindValue(":prepif", ui->prepifJobNumber->text());
    query.bindValue(":exc", ui->excJobNumber->text());
    query.bindValue(":cbc2", ui->cbc2Postage->text());
    query.bindValue(":cbc3", ui->cbc3Postage->text());
    query.bindValue(":exc_p", ui->excPostage->text());
    query.bindValue(":in_po", ui->inactivePOPostage->text());
    query.bindValue(":in_pu", ui->inactivePUPostage->text());
    query.bindValue(":nc1a", ui->ncwo1APostage->text());
    query.bindValue(":nc2a", ui->ncwo2APostage->text());
    query.bindValue(":nc1ap", ui->ncwo1APPostage->text());
    query.bindValue(":nc2ap", ui->ncwo2APPostage->text());
    query.bindValue(":prepif_p", ui->prepifPostage->text());
    query.bindValue(":progress", "updated");
    query.bindValue(":year", originalYear.toInt());
    query.bindValue(":month", originalMonth.toInt());
    query.bindValue(":week", originalWeek.toInt());

    if (!query.exec()) {
        logToTerminal(tr("Failed to update job: %1").arg(query.lastError().text()));
        QMessageBox::critical(this, tr("Database Error"), tr("Failed to update job: %1").arg(query.lastError().text()));
    } else {
        logToTerminal(tr("Job updated successfully."));
    }
}

// Database: Delete a job from the database
void Goji::deleteJob(const QString& year, const QString& month, const QString& week)
{
    if (currentJobType != "RAC WEEKLY") return;
    QSqlQuery query(db);
    query.prepare("DELETE FROM jobs_rac_weekly WHERE year = :year AND month = :month AND week = :week");
    query.bindValue(":year", year.toInt());
    query.bindValue(":month", month.toInt());
    query.bindValue(":week", week.toInt());
    if (!query.exec()) {
        logToTerminal(tr("Failed to delete job: %1").arg(query.lastError().text()));
        QMessageBox::critical(this, tr("Database Error"), tr("Failed to delete job: %1").arg(query.lastError().text()));
    } else {
        logToTerminal(tr("Job deleted successfully."));
    }
}

// Database: Check if a job exists in the database
bool Goji::jobExists(const QString& year, const QString& month, const QString& week)
{
    if (currentJobType != "RAC WEEKLY") return false;
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM jobs_rac_weekly WHERE year = :year AND month = :month AND week = :week");
    query.bindValue(":year", year.toInt());
    query.bindValue(":month", month.toInt());
    query.bindValue(":week", week.toInt());
    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}

// Helper: Show dialog to confirm overwriting an existing job
bool Goji::confirmOverwrite(const QString& year, const QString& month, const QString& week)
{
    if (currentJobType != "RAC WEEKLY") return false;
    QString message = tr("A job with %1|%2|%3 already exists.\nWould you like to overwrite it?").arg(year, month, week);
    QMessageBox dialog(this);
    dialog.setWindowTitle(tr("Job Exists"));
    dialog.setText(message);
    dialog.addButton(tr("Yes, overwrite"), QMessageBox::AcceptRole);
    dialog.addButton(tr("No, go back"), QMessageBox::RejectRole);
    dialog.setIcon(QMessageBox::Warning);
    return dialog.exec() == QMessageBox::AcceptRole;
}

// Menu: Build the "Weekly" submenu dynamically from database entries
void Goji::buildWeeklyMenu()
{
    if (currentJobType != "RAC WEEKLY") {
        weeklyMenu->clear();
        return;
    }
    weeklyMenu->clear();
    QSqlQuery query("SELECT year, month, week FROM jobs_rac_weekly ORDER BY year DESC, month DESC, week DESC", db);
    if (!query.exec()) {
        logToTerminal(tr("Failed to query jobs: %1").arg(query.lastError().text()));
        return;
    }

    QMap<int, QMenu*> yearMenus;
    QMap<QString, QMenu*> monthMenus;
    while (query.next()) {
        int year = query.value(0).toInt();
        int month = query.value(1).toInt();
        int week = query.value(2).toInt();
        QString yearStr = QString::number(year);
        QString monthStr = QString("%1").arg(month, 2, 10, QChar('0')); // Format as 01-12
        QString weekStr = QString("%1").arg(week, 2, 10, QChar('0')); // Format as 01-53

        // Create year submenu if it doesn't exist
        if (!yearMenus.contains(year)) {
            QMenu* yearMenu = new QMenu(yearStr, weeklyMenu);
            yearMenus[year] = yearMenu;
            weeklyMenu->addMenu(yearMenu);
        }

        // Create month submenu if it doesn't exist
        QString monthKey = QString("%1_%2").arg(yearStr, monthStr);
        if (!monthMenus.contains(monthKey)) {
            QMenu* monthMenu = new QMenu(monthStr, yearMenus[year]);
            monthMenus[monthKey] = monthMenu;
            yearMenus[year]->addMenu(monthMenu);
        }

        // Add week action
        QAction *action = new QAction(weekStr, monthMenus[monthKey]);
        connect(action, &QAction::triggered, this, [=]() {
            openJobFromWeekly(yearStr, monthStr, weekStr);
        });
        monthMenus[monthKey]->addAction(action);
    }
}

// Menu: Open a job from the "Weekly" submenu
void Goji::openJobFromWeekly(const QString& year, const QString& month, const QString& week)
{
    if (currentJobType != "RAC WEEKLY") return;
    QSqlQuery query(db);
    query.prepare("SELECT * FROM jobs_rac_weekly WHERE year = :year AND month = :month AND week = :week");
    query.bindValue(":year", year.toInt());
    query.bindValue(":month", month.toInt());
    query.bindValue(":week", week.toInt());
    if (query.exec() && query.next()) {
        ui->yearDDbox->setCurrentText(year);
        ui->monthDDbox->setCurrentText(month);
        ui->weekDDbox->setCurrentText(week);
        ui->cbcJobNumber->setText(query.value("cbc_job_number").toString());
        ui->excJobNumber->setText(query.value("exc_job_number").toString());
        ui->inactiveJobNumber->setText(query.value("inactive_job_number").toString());
        ui->ncwoJobNumber->setText(query.value("ncwo_job_number").toString());
        ui->prepifJobNumber->setText(query.value("prepif_job_number").toString());
        ui->cbc2Postage->setText(query.value("cbc2_postage").toString());
        ui->cbc3Postage->setText(query.value("cbc3_postage").toString());
        ui->excPostage->setText(query.value("exc_postage").toString());
        ui->inactivePOPostage->setText(query.value("inactive_po_postage").toString());
        ui->inactivePUPostage->setText(query.value("inactive_pu_postage").toString());
        ui->ncwo1APostage->setText(query.value("ncwo1_a_postage").toString());
        ui->ncwo2APostage->setText(query.value("ncwo2_a_postage").toString());
        ui->ncwo1APPostage->setText(query.value("ncwo1_ap_postage").toString());
        ui->ncwo2APPostage->setText(query.value("ncwo2_ap_postage").toString());
        ui->prepifPostage->setText(query.value("prepif_postage").toString());

        originalYear = year;
        originalMonth = month;
        originalWeek = week;
        isJobSaved = true;

        completedSubtasks[0] = query.value("step0_complete").toInt();
        completedSubtasks[1] = query.value("step1_complete").toInt();
        completedSubtasks[2] = query.value("step2_complete").toInt();
        completedSubtasks[3] = query.value("step3_complete").toInt();
        completedSubtasks[4] = query.value("step4_complete").toInt();
        completedSubtasks[5] = query.value("step5_complete").toInt();
        completedSubtasks[6] = query.value("step6_complete").toInt();
        completedSubtasks[7] = query.value("step7_complete").toInt();
        completedSubtasks[8] = query.value("step8_complete").toInt();

        isOpenIZComplete = completedSubtasks[0] == 1;
        isRunInitialComplete = completedSubtasks[1] == 1;
        isRunPreProofComplete = completedSubtasks[2] == 1 && completedSubtasks[3] == 1;
        isOpenProofFilesComplete = completedSubtasks[4] == 1;
        isRunPostProofComplete = completedSubtasks[5] == 1;
        isOpenPrintFilesComplete = completedSubtasks[7] == 1;
        isRunPostPrintComplete = completedSubtasks[8] == 1;

        ui->tabWidget->setCurrentIndex(0);
        copyFilesFromHomeToWorking(month, week);
        updateWidgetStatesBasedOnJobState();
        updateProgressBar();
        updateLEDs();
        logToTerminal(tr("Opened job: Year %1, Month %2, Week %3").arg(year, month, week));
    } else {
        logToTerminal(tr("Failed to load job: %1").arg(query.lastError().text()));
        QMessageBox::warning(this, tr("Load Error"), tr("Failed to load job: %1").arg(query.lastError().text()));
    }
}

// Helper: Copy files from home folders to working folders
void Goji::copyFilesFromHomeToWorking(const QString& month, const QString& week)
{
    if (currentJobType != "RAC WEEKLY") return;
    QString basePath = settings->value("BasePath", "C:/Goji/RAC").toString();
    QStringList jobTypes = {"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"};
    QString homeFolder = month + "." + week;

    for (const QString& jobType : jobTypes) {
        QString homeDir = basePath + "/" + jobType + "/" + homeFolder;
        QString workingDir = basePath + "/" + jobType + "/JOB";

        for (const QString& subDir : QStringList{"INPUT", "OUTPUT", "PRINT", "PROOF"}) {
            QDir homeSubDir(homeDir + "/" + subDir);
            QDir workingSubDir(workingDir + "/" + subDir);
            if (!workingSubDir.exists()) {
                workingSubDir.mkpath(".");
            }

            const QStringList& files = homeSubDir.entryList(QDir::Files);
            for (const QString& file : files) {
                QString src = homeSubDir.filePath(file);
                QString dest = workingSubDir.filePath(file);
                if (QFile::exists(dest)) {
                    if (!QFile::remove(dest)) {
                        logToTerminal(tr("Failed to remove existing file: %1").arg(dest));
                        continue;
                    }
                }
                if (!QFile::copy(src, dest)) {
                    logToTerminal(tr("Failed to copy %1 to %2").arg(src, dest));
                }
            }
        }
    }
    logToTerminal(tr("Files copied from home to working directories for month %1, week %2").arg(month, week));
}

// Helper: Copy files to working folders
void Goji::copyFilesToWorkingFolders(const QString& month, const QString& week)
{
    if (currentJobType != "RAC WEEKLY") return;
    copyFilesFromHomeToWorking(month, week);
}

// Helper: Move files from working folders back to home folders
void Goji::moveFilesToHomeFolders(const QString& month, const QString& week)
{
    if (currentJobType != "RAC WEEKLY") return;
    QString basePath = settings->value("BasePath", "C:/Goji/RAC").toString();
    QStringList jobTypes = {"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"};
    QString homeFolder = month + "." + week;

    for (const QString& jobType : jobTypes) {
        QString homeDir = basePath + "/" + jobType + "/" + homeFolder;
        QString workingDir = basePath + "/" + jobType + "/JOB";

        for (const QString& subDir : QStringList{"INPUT", "OUTPUT", "PRINT", "PROOF"}) {
            QDir workingSubDir(workingDir + "/" + subDir);
            QDir homeSubDir(homeDir + "/" + subDir);
            if (!homeSubDir.exists()) {
                homeSubDir.mkpath(".");
            }

            const QStringList& files = workingSubDir.entryList(QDir::Files);
            for (const QString& file : files) {
                QString src = workingSubDir.filePath(file);
                QString dest = homeSubDir.filePath(file);
                if (QFile::exists(dest)) {
                    if (!QFile::remove(dest)) {
                        logToTerminal(tr("Failed to remove existing file: %1").arg(dest));
                        continue;
                    }
                }
                if (!QFile::rename(src, dest)) {
                    logToTerminal(tr("Failed to move %1 to %2").arg(src, dest));
                }
            }
        }
    }
    logToTerminal(tr("Files moved to home directory: %1/%2.%3").arg(basePath, month, week));
}

// Helper: Save post-proof counts to the database
void Goji::savePostProofCounts()
{
    if (currentJobType != "RAC WEEKLY") return;
    logToTerminal(tr("Saving post-proof counts..."));
    QString week = ui->monthDDbox->currentText() + "." + ui->weekDDbox->currentText();
    QString basePath = settings->value("BasePath", "C:/Goji/RAC").toString();
    QStringList jobTypes = {"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"};

    QSqlQuery query(db);
    query.exec("DELETE FROM post_proof_counts WHERE week = '" + week + "'");

    for (const QString& jobType : jobTypes) {
        QString jobNumber = getJobNumberForJobType(jobType);
        if (jobNumber.isEmpty()) continue;

        QString projectFile = basePath + "/RAC/" + jobType + "/JOB/OUTPUT/" + jobType + "_PROJECT.csv";
        QFile file(projectFile);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            logToTerminal(tr("Failed to open project file: %1").arg(projectFile));
            continue;
        }

        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            QStringList fields = line.split(",");
            if (fields.size() >= 4) {
                query.prepare("INSERT INTO post_proof_counts (job_number, week, project, pr_count, canc_count, us_count, postage) "
                              "VALUES (:job, :week, :project, :pr, :canc, :us, :postage)");
                query.bindValue(":job", jobNumber);
                query.bindValue(":week", week);
                query.bindValue(":project", fields[0]);
                query.bindValue(":pr", fields[1].toInt());
                query.bindValue(":canc", fields[2].toInt());
                query.bindValue(":us", fields[3].toInt());
                query.bindValue(":postage", fields.size() > 4 ? fields[4] : "");
                if (!query.exec()) {
                    logToTerminal(tr("Failed to insert post-proof count: %1").arg(query.lastError().text()));
                }
            }
        }
        file.close();
    }
    logToTerminal(tr("Post-proof counts saved."));
}

// Helper: Update LED indicators based on completion status
void Goji::updateLEDs()
{
    if (currentJobType != "RAC WEEKLY") return;
    ui->preProofLED->setStyleSheet(isRunPreProofComplete ? "background-color: #00ff15; border-radius: 2px;" : "background-color: red; border-radius: 2px;");
    ui->proofFilesLED->setStyleSheet(isOpenProofFilesComplete ? "background-color: #00ff15; border-radius: 2px;" : "background-color: red; border-radius: 2px;");
    ui->postProofLED->setStyleSheet(isRunPostProofComplete ? "background-color: #00ff15; border-radius: 2px;" : "background-color: red; border-radius: 2px;");
    ui->proofApprovalLED->setStyleSheet(completedSubtasks[6] == 1 ? "background-color: #00ff15; border-radius: 2px;" : "background-color: red; border-radius: 2px;");
    ui->printFilesLED->setStyleSheet(isOpenPrintFilesComplete ? "background-color: #00ff15; border-radius: 2px;" : "background-color: red; border-radius: 2px;");
    ui->postPrintLED->setStyleSheet(isRunPostPrintComplete ? "background-color: #00ff15; border-radius: 2px;" : "background-color: red; border-radius: 2px;");
    logToTerminal(tr("LED indicators updated."));
}

// Helper: Enable or disable proof approval checkboxes
void Goji::enableProofApprovalCheckboxes()
{
    if (currentJobType != "RAC WEEKLY") return;
    bool enable = isRunPostProofComplete;
    for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
        it.value()->setEnabled(enable);
    }
    ui->allCB->setEnabled(enable);
    logToTerminal(tr("Proof approval checkboxes %1").arg(enable ? tr("enabled") : tr("disabled")));
}

// Helper: Lock or unlock job data fields
void Goji::lockJobDataFields(bool lock)
{
    if (currentJobType != "RAC WEEKLY") return;
    QList<QLineEdit*> jobLineEdits;
    jobLineEdits << ui->cbcJobNumber << ui->excJobNumber << ui->inactiveJobNumber << ui->ncwoJobNumber << ui->prepifJobNumber;
    for (QLineEdit* lineEdit : jobLineEdits) {
        lineEdit->setReadOnly(lock);
    }
    ui->yearDDbox->setEnabled(!lock);
    ui->monthDDbox->setEnabled(!lock);
    ui->weekDDbox->setEnabled(!lock);
}

// Helper: Lock or unlock postage fields
void Goji::lockPostageFields(bool lock)
{
    if (currentJobType != "RAC WEEKLY") return;
    QList<QLineEdit*> postageLineEdits;
    postageLineEdits << ui->cbc2Postage << ui->cbc3Postage << ui->excPostage << ui->inactivePOPostage
                     << ui->inactivePUPostage << ui->ncwo1APostage << ui->ncwo2APostage
                     << ui->ncwo1APPostage << ui->ncwo2APPostage << ui->prepifPostage;
    for (QLineEdit* lineEdit : postageLineEdits) {
        lineEdit->setReadOnly(lock);
    }
}

// Helper: Update widget states based on job state
void Goji::updateWidgetStatesBasedOnJobState()
{
    if (currentJobType != "RAC WEEKLY") return;
    bool jobActive = isJobSaved;
    ui->runInitial->setEnabled(jobActive);
    ui->runPreProof->setEnabled(jobActive);
    ui->openProofFiles->setEnabled(jobActive);
    ui->runPostProof->setEnabled(jobActive);
    ui->openPrintFiles->setEnabled(jobActive);
    ui->runPostPrint->setEnabled(jobActive);
    ui->openIZ->setEnabled(true);
    ui->proofDDbox->setEnabled(jobActive);
    ui->printDDbox->setEnabled(jobActive);
    ui->yearDDbox->setEnabled(true);
    ui->monthDDbox->setEnabled(true);
    ui->weekDDbox->setEnabled(true);
    ui->editButton->setEnabled(jobActive);
    ui->proofRegen->setEnabled(jobActive);
    ui->postageLock->setEnabled(jobActive);
    ui->lockButton->setEnabled(true);
    ui->regenTab->setEnabled(isProofRegenMode);
}

// Helper: Initialize watchers and timers
void Goji::initWatchersAndTimers()
{
    m_printWatcher = new QFileSystemWatcher();
    QString printPath = settings->value("PrintPath", QCoreApplication::applicationDirPath() + "/RAC").toString();
    if (QDir(printPath).exists()) {
        m_printWatcher->addPath(printPath);
        logToTerminal(tr("Watching print directory: %1").arg(printPath));
    } else {
        logToTerminal(tr("Print directory not found: %1").arg(printPath));
    }
    connect(m_printWatcher, &QFileSystemWatcher::directoryChanged, this, &Goji::onPrintDirChanged);

    m_inactivityTimer = new QTimer();
    m_inactivityTimer->setInterval(300000); // 5 minutes
    m_inactivityTimer->setSingleShot(false);
    connect(m_inactivityTimer, &QTimer::timeout, this, &Goji::onInactivityTimeout);
    m_inactivityTimer->start();
    logToTerminal(tr("Inactivity timer started (5 minutes)."));
}

// Helper: Clear job numbers
void Goji::clearJobNumbers()
{
    if (currentJobType != "RAC WEEKLY") return;
    ui->cbcJobNumber->clear();
    ui->excJobNumber->clear();
    ui->inactiveJobNumber->clear();
    ui->ncwoJobNumber->clear();
    ui->prepifJobNumber->clear();
    logToTerminal(tr("Job numbers cleared."));
}

// Helper: Get proof folder path for a job type
QString Goji::getProofFolderPath(const QString &jobType)
{
    return settings->value("ProofPath", QCoreApplication::applicationDirPath() + "/RAC/" + jobType + "/JOB/PROOF").toString();
}

// Helper: Ensure InDesign is open before proceeding
void Goji::ensureInDesignIsOpen(const std::function<void()>& callback)
{
    if (currentJobType != "RAC WEEKLY") return;
    QProcess *process = new QProcess();
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, callback, process](int exitCode, QProcess::ExitStatus exitStatus) {
                if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                    callback();
                } else {
                    logToTerminal(tr("InDesign is not open. Please open InDesign and try again."));
                    QMessageBox::warning(this, tr("InDesign Not Open"), tr("Please open InDesign and try again."));
                }
                process->deleteLater();
            });
    process->start("tasklist", QStringList() << "|" << "findstr" << "InDesign.exe");
}

// Helper: Update button states based on job state
void Goji::updateButtonStates(bool enabled)
{
    if (currentJobType != "RAC WEEKLY") return;
    ui->openIZ->setEnabled(enabled);
    ui->runInitial->setEnabled(enabled);
    ui->runPreProof->setEnabled(enabled && isPostageLocked);
    ui->openProofFiles->setEnabled(enabled);
    ui->runPostProof->setEnabled(enabled && isPostageLocked);
    ui->openPrintFiles->setEnabled(enabled);
    ui->runPostPrint->setEnabled(enabled);
}

// Helper: Open proof files for a selection
void Goji::openProofFiles(const QString& selection)
{
    if (currentJobType != "RAC WEEKLY") return;
    QString proofPath = getProofFolderPath(selection);
    QDesktopServices::openUrl(QUrl::fromLocalFile(proofPath));
    logToTerminal(tr("Opened proof files for: %1").arg(selection));
}

// Helper: Open print files for a selection
void Goji::openPrintFiles(const QString& selection)
{
    if (currentJobType != "RAC WEEKLY") return;
    QString printPath = settings->value("PrintPath", QCoreApplication::applicationDirPath() + "/RAC/" + selection + "/JOB/PRINT").toString();
    QDesktopServices::openUrl(QUrl::fromLocalFile(printPath));
    logToTerminal(tr("Opened print files for: %1").arg(selection));
}

// Helper: Get job number for a specific job type
QString Goji::getJobNumberForJobType(const QString& jobType)
{
    if (currentJobType != "RAC WEEKLY") return QString();
    if (jobType == "CBC") return ui->cbcJobNumber->text();
    if (jobType == "EXC") return ui->excJobNumber->text();
    if (jobType == "INACTIVE") return ui->inactiveJobNumber->text();
    if (jobType == "NCWO") return ui->ncwoJobNumber->text();
    if (jobType == "PREPIF") return ui->prepifJobNumber->text();
    return QString();
}

// Helper: Create job folders
void Goji::createJobFolders(const QString& year, const QString& month, const QString& week)
{
    if (currentJobType != "RAC WEEKLY") return;
    QString basePath = settings->value("BasePath", "C:/Goji/RAC").toString();
    QStringList jobTypes = {"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"};
    QString homeFolder = month + "." + week;

    for (const QString& jobType : jobTypes) {
        QString fullPath = basePath + "/" + jobType + "/" + homeFolder;
        QDir dir(fullPath);
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                logToTerminal(tr("Failed to create home folder: %1").arg(fullPath));
                QMessageBox::critical(this, tr("File Error"), tr("Failed to create home folder: %1").arg(fullPath));
                return;
            }
            logToTerminal(tr("Created home folder: %1").arg(fullPath));

            for (const QString& subDir : QStringList{"INPUT", "OUTPUT", "PRINT", "PROOF"}) {
                QDir subDirPath(fullPath + "/" + subDir);
                if (!subDirPath.exists()) {
                    if (!subDirPath.mkdir(".")) {
                        logToTerminal(tr("Failed to create subdirectory: %1").arg(subDirPath.path()));
                    } else {
                        logToTerminal(tr("Created subdirectory: %1").arg(subDirPath.path()));
                    }
                }
            }
        }
    }
    logToTerminal(tr("Job folders created for %1-%2-%3").arg(year, month, week));
}

// Helper: Update progress bar based on completed subtasks
void Goji::updateProgressBar()
{
    if (currentJobType != "RAC WEEKLY") return;
    double totalWeight = 0.0;
    double completedWeight = 0.0;
    for (size_t i = 0; i < NUM_STEPS; ++i) {
        totalWeight += stepWeights[i];
        completedWeight += completedSubtasks[i] * stepWeights[i];
    }
    int progress = static_cast<int>((completedWeight / totalWeight) * 100);
    ui->progressBarWeekly->setValue(progress);
    logToTerminal(tr("Progress updated to %1%").arg(progress));
}

// Helper: Populate weekDDbox with Mondays for the selected year and month
void Goji::populateWeekDDbox()
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

// Slot: Handle "All" checkbox state change for proof approval
void Goji::onAllCBStateChanged(int state)
{
    if (currentJobType != "RAC WEEKLY") return;
    QSignalBlocker blocker(ui->allCB);
    for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
        it.value()->setCheckState(static_cast<Qt::CheckState>(state));
    }
    if (state == Qt::Checked) {
        completedSubtasks[6] = 1;
        updateProgressBar();
        updateLEDs();
    }
    logToTerminal(tr("All checkbox state changed to: %1").arg(state == Qt::Checked ? tr("checked") : tr("unchecked")));
}

// Slot: Update "All" checkbox based on individual checkbox states
void Goji::updateAllCBState()
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
    if (allChecked) {
        completedSubtasks[6] = 1;
    } else {
        completedSubtasks[6] = 0;
    }
    updateProgressBar();
    updateLEDs();
}

// Destructor: Cleans up dynamically allocated resources
Goji::~Goji()
{
    db.close();
    delete ui;
    delete openJobMenu;
    delete validator;
    delete m_printWatcher;
    delete m_inactivityTimer;
    delete settings;
}
