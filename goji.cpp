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
#include <QMap>
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
#include <QRegExp>
#include <QDate>
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
#include <QFileSystemWatcher>
#include <QEventLoop>

// Define the version number as a constant
const QString VERSION = "0.9.1";

// Constructor with initialization order matching declaration in goji.h
Goji::Goji(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    recentFilesMenu(nullptr),
    openJobMenu(nullptr),
    weeklyMenu(nullptr),
    openIZButton(nullptr),
    runPreProofButton(nullptr),
    openProofFilesButton(nullptr),
    runPostProofButton(nullptr),
    openPrintFilesButton(nullptr),
    runPostPrintButton(nullptr),
    lockButton(nullptr),
    editButton(nullptr),
    proofRegenButton(nullptr),
    progressBar(nullptr),
    m_printDirs(),
    m_printWatcher(nullptr),
    m_inactivityTimer(nullptr),
    proofFiles(),
    printFiles(),
    regenCheckboxes(),
    checkboxFileMap(),
    db(),
    isJobSaved(false),
    originalYear(""),
    originalMonth(""),
    originalWeek(""),
    isPostageLocked(false),
    isProofRegenMode(false),
    validator(nullptr),
    isOpenIZComplete(false),
    isRunInitialComplete(false),
    isRunPreProofComplete(false),
    isOpenProofFilesComplete(false),
    isRunPostProofComplete(false),
    isOpenPrintFilesComplete(false),
    isRunPostPrintComplete(false),
    isJobDataLocked(false)
{
    ui->setupUi(this);
    this->setWindowTitle("Goji v" + VERSION);
    setWindowIcon(QIcon(":/resources/icons/ShinGoji.ico"));

    // Set regenTab to default to CBC tab (index 0)
    ui->regenTab->setCurrentIndex(0);

    // Create the "Open Job" menu
    openJobMenu = new QMenu("Open Job", this);

    // Create the "Weekly" submenu
    weeklyMenu = openJobMenu->addMenu("Weekly");

    // Connect the aboutToShow signal to dynamically build the menu
    if (!weeklyMenu) {
        qDebug() << "weeklyMenu is null!";
        return;
    }
    connect(weeklyMenu, &QMenu::aboutToShow, this, static_cast<void (Goji::*)()>(&Goji::buildWeeklyMenu));

    // Insert "Open Job" menu before "Save Job"
    ui->menuFile->insertMenu(ui->actionSave_Job, openJobMenu);

    // Create the "Recent Files" menu
    recentFilesMenu = new QRecentFilesMenu("Recent Files", this);
    recentFilesMenu->setMaxCount(4);  // Limit to 4 recent files

    // Insert "Recent Files" menu before "Save Job"
    ui->menuFile->insertMenu(ui->actionSave_Job, recentFilesMenu);

    // Connect signals
    connect(recentFilesMenu, &QRecentFilesMenu::triggered, this, &Goji::openRecentFile);
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
    connect(ui->allCB, &QCheckBox::stateChanged, this, &Goji::onAllCBStateChanged);
    connect(ui->cbcCB, &QCheckBox::stateChanged, this, &Goji::updateAllCBState);
    connect(ui->excCB, &QCheckBox::stateChanged, this, &Goji::updateAllCBState);
    connect(ui->inactiveCB, &QCheckBox::stateChanged, this, &Goji::updateAllCBState);
    connect(ui->ncwoCB, &QCheckBox::stateChanged, this, &Goji::updateAllCBState);
    connect(ui->prepifCB, &QCheckBox::stateChanged, this, &Goji::updateAllCBState);
    connect(ui->actionExit, &QAction::triggered, this, &Goji::onActionExitTriggered);
    connect(ui->actionClose_Job, &QAction::triggered, this, &Goji::onActionCloseJobTriggered);
    connect(ui->actionSave_Job, &QAction::triggered, this, &Goji::onActionSaveJobTriggered);

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
    checkboxFileMap[ui->regenCBC2CB] = QPair<QString, QString>("CBC", "CBC2 PROOF.pdf");
    checkboxFileMap[ui->regenCBC3CB] = QPair<QString, QString>("CBC", "CBC3 PROOF.pdf");
    checkboxFileMap[ui->regenEXCCB] = QPair<QString, QString>("EXC", "EXC PROOF.pdf");
    checkboxFileMap[ui->regenAPOCB] = QPair<QString, QString>("INACTIVE", "INACTIVE A-PO PROOF.pdf");
    checkboxFileMap[ui->regenAPUCB] = QPair<QString, QString>("INACTIVE", "INACTIVE A-PU PROOF.pdf");
    checkboxFileMap[ui->regenATPOCB] = QPair<QString, QString>("INACTIVE", "INACTIVE AT-PO PROOF.pdf");
    checkboxFileMap[ui->regenATPUCB] = QPair<QString, QString>("INACTIVE", "INACTIVE AT-PU PROOF.pdf");
    checkboxFileMap[ui->regenPRPOCB] = QPair<QString, QString>("INACTIVE", "INACTIVE PR-PO PROOF.pdf");
    checkboxFileMap[ui->regenPRPUCB] = QPair<QString, QString>("INACTIVE", "INACTIVE PR-PU PROOF.pdf");
    checkboxFileMap[ui->regen1ACB] = QPair<QString, QString>("NCWO", "NCWO 1-A PROOF.pdf");
    checkboxFileMap[ui->regen1APCB] = QPair<QString, QString>("NCWO", "NCWO 1-AP PROOF.pdf");
    checkboxFileMap[ui->regen1APPRCB] = QPair<QString, QString>("NCWO", "NCWO 1-APPR PROOF.pdf");
    checkboxFileMap[ui->regen1PRCB] = QPair<QString, QString>("NCWO", "NCWO 1-PR PROOF.pdf");
    checkboxFileMap[ui->regen2ACB] = QPair<QString, QString>("NCWO", "NCWO 2-A PROOF.pdf"); // Corrected typo
    checkboxFileMap[ui->regen2APCB] = QPair<QString, QString>("NCWO", "NCWO 2-AP PROOF.pdf");
    checkboxFileMap[ui->regen2APPRCB] = QPair<QString, QString>("NCWO", "NCWO 2-APPR PROOF.pdf");
    checkboxFileMap[ui->regen2PRCB] = QPair<QString, QString>("NCWO", "NCWO 2-PR PROOF.pdf");
    checkboxFileMap[ui->regenPPUSCB] = QPair<QString, QString>("PREPIF", "PREPIF US PROOF.pdf");
    checkboxFileMap[ui->regenPPPRCB] = QPair<QString, QString>("PREPIF", "PREPIF PR PROOF.pdf");

    // Initialize UI state
    logToTerminal("Goji started: " + QDateTime::currentDateTime().toString());

    // Set placeholder text for postage QLineEdit widgets
    ui->cbc2Postage->setPlaceholderText("CBC2");
    ui->cbc3Postage->setPlaceholderText("CBC3");
    ui->excPostage->setPlaceholderText("EXC");
    ui->inactivePOPostage->setPlaceholderText("A-PO");
    ui->inactivePUPostage->setPlaceholderText("A-PU");
    ui->ncwo1APostage->setPlaceholderText("1-A");
    ui->ncwo2APostage->setPlaceholderText("2-A");
    ui->prepifPostage->setPlaceholderText("PREPIF");

    // Set up validator for postage QLineEdit widgets
    validator = new QRegExpValidator(QRegExp("[0-9]*\\.?[0-9]*"), this);

    // List of postage QLineEdit widgets
    QList<QLineEdit*> postageLineEdits = {
        ui->cbc2Postage, ui->cbc3Postage, ui->excPostage,
        ui->inactivePOPostage, ui->inactivePUPostage,
        ui->ncwo1APostage, ui->ncwo2APostage, ui->prepifPostage
    };

    // Apply validator and connect editingFinished signal
    for (QLineEdit *lineEdit : postageLineEdits) {
        lineEdit->setValidator(validator);
        connect(lineEdit, &QLineEdit::editingFinished, this, &Goji::formatCurrencyOnFinish);
    }

    // Initialize database with a dynamic path
    QString dbDirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Goji/SQL";
    QDir dbDir(dbDirPath);
    if (!dbDir.exists()) {
        if (!dbDir.mkpath(".")) {
            QMessageBox::critical(this, "Directory Error", "Failed to create directory: " + dbDirPath);
            return;
        }
    }
    QString dbPath = dbDirPath + "/jobs.db";
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        QMessageBox::critical(this, "Database Error", "Failed to open database: " + db.lastError().text());
        return;
    }

    // Create jobs table if it doesn’t exist
    QSqlQuery query;
    query.exec("CREATE TABLE IF NOT EXISTS jobs ("
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
               "prepif_postage TEXT, "
               "progress TEXT, "
               "PRIMARY KEY (year, month, week)"
               ")");
    if (query.lastError().isValid()) {
        qDebug() << "Error creating table:" << query.lastError().text();
    }

    // Create proof_versions table for tracking proof regeneration versions
    query.exec("CREATE TABLE IF NOT EXISTS proof_versions ("
               "file_path TEXT PRIMARY KEY, "
               "version INTEGER DEFAULT 1"
               ")");
    if (query.lastError().isValid()) {
        qDebug() << "Error creating proof_versions table:" << query.lastError().text();
    }

    // Initialize checkbox mapping for regeneration
    regenCheckboxes = {
        {"CBC", ui->cbcCB},
        {"EXC", ui->excCB},
        {"INACTIVE", ui->inactiveCB},
        {"NCWO", ui->ncwoCB},
        {"PREPIF", ui->prepifCB}
    };

    // Disable regeneration checkboxes and "ALL" checkbox by default
    for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
        it.value()->setEnabled(false);
    }
    ui->allCB->setEnabled(false);

    // Populate yearDDbox
    int currentYear = QDate::currentDate().year();
    ui->yearDDbox->addItem("");
    ui->yearDDbox->addItem(QString::number(currentYear - 1));
    ui->yearDDbox->addItem(QString::number(currentYear));
    ui->yearDDbox->addItem(QString::number(currentYear + 1));

    // Initialize LEDs
    updateLEDs();

    // Set initial widget states based on isJobSaved
    updateWidgetStatesBasedOnJobState();

    // Initialize step weights for progress bar
    stepWeights[0] = 2.0;  // Step 1: ZIP file detection
    stepWeights[1] = 9.0;  // Step 2: ZIP file processing
    stepWeights[2] = 13.0; // Step 3: OUTPUT files generation
    stepWeights[3] = 13.0; // Step 4: PreProof (proof data files)
    stepWeights[4] = 20.0; // Step 5: Proof files (PDFs) generation
    stepWeights[5] = 10.0; // Step 6: runPostProof (ZIP files)
    stepWeights[6] = 3.0;  // Step 7: Proof approval
    stepWeights[7] = 20.0; // Step 8: Print files generation
    stepWeights[8] = 10.0; // Step 9: runPostPrint

    // Initialize subtask trackers
    for (int i = 0; i < 9; ++i) {
        totalSubtasks[i] = 1;     // Default to 1 for binary steps
        completedSubtasks[i] = 0; // Nothing completed yet
    }

    // Set progress bar range
    ui->progressBarWeekly->setRange(0, 100);
    ui->progressBarWeekly->setValue(0);
}

void Goji::buildWeeklyMenu()
{
    weeklyMenu->clear();  // Clear existing menu items

    // Query distinct years from the jobs table
    QSqlQuery yearQuery("SELECT DISTINCT year FROM jobs ORDER BY year", db);
    if (!yearQuery.exec()) {
        qDebug() << "Year query failed:" << yearQuery.lastError().text();
        return;
    }

    while (yearQuery.next()) {
        int year = yearQuery.value(0).toInt();
        QMenu *yearMenu = weeklyMenu->addMenu(QString::number(year));

        // Query distinct months for this year
        QSqlQuery monthQuery(db);
        monthQuery.prepare("SELECT DISTINCT month FROM jobs WHERE year = ? ORDER BY month");
        monthQuery.addBindValue(year);
        if (!monthQuery.exec()) {
            qDebug() << "Month query failed for year" << year << ":" << monthQuery.lastError().text();
            continue;
        }

        while (monthQuery.next()) {
            int month = monthQuery.value(0).toInt();
            QMenu *monthMenu = yearMenu->addMenu(QString::number(month).rightJustified(2, '0'));

            // Query distinct weeks for this year and month
            QSqlQuery weekQuery(db);
            weekQuery.prepare("SELECT DISTINCT week FROM jobs WHERE year = ? AND month = ? ORDER BY week");
            weekQuery.addBindValue(year);
            weekQuery.addBindValue(month);
            if (!weekQuery.exec()) {
                qDebug() << "Week query failed for year" << year << "month" << month << ":" << weekQuery.lastError().text();
                continue;
            }

            while (weekQuery.next()) {
                int week = weekQuery.value(0).toInt();
                QAction *weekAction = monthMenu->addAction(QString::number(week).rightJustified(2, '0'));

                // Connect the action to open the job
                connect(weekAction, &QAction::triggered, this, [this, year, month, week]() {
                    openJobFromWeekly(year, month, week);
                });
            }
        }
    }

    // If no data exists, show a placeholder
    if (!yearQuery.first()) {
        weeklyMenu->addAction("No jobs available")->setEnabled(false);
    }
}

void Goji::openJobFromWeekly(int year, int month, int week)
{
    // Reset progress bar for new job
    for (int i = 0; i < 9; ++i) {
        completedSubtasks[i] = 0;
    }
    updateProgressBar();

    // Query the job details and update the UI
    QSqlQuery query(db);
    query.prepare("SELECT * FROM jobs WHERE year = ? AND month = ? AND week = ?");
    query.addBindValue(year);
    query.addBindValue(month);
    query.addBindValue(week);
    if (query.exec() && query.next()) {
        // Update UI elements with job details
        ui->yearDDbox->setCurrentText(QString::number(year));
        ui->monthDDbox->setCurrentText(QString::number(month).rightJustified(2, '0'));
        ui->weekDDbox->setCurrentText(QString::number(week).rightJustified(2, '0'));

        // Load job numbers
        ui->cbcJobNumber->setText(query.value("cbc_job_number").toString());
        ui->ncwoJobNumber->setText(query.value("ncwo_job_number").toString());
        ui->inactiveJobNumber->setText(query.value("inactive_job_number").toString());
        ui->prepifJobNumber->setText(query.value("prepif_job_number").toString());
        ui->excJobNumber->setText(query.value("exc_job_number").toString());

        // Load postage data
        ui->cbc2Postage->setText(query.value("cbc2_postage").toString());
        ui->cbc3Postage->setText(query.value("cbc3_postage").toString());
        ui->excPostage->setText(query.value("exc_postage").toString());
        ui->inactivePOPostage->setText(query.value("inactive_po_postage").toString());
        ui->inactivePUPostage->setText(query.value("inactive_pu_postage").toString());
        ui->ncwo1APostage->setText(query.value("ncwo1_a_postage").toString());
        ui->ncwo2APostage->setText(query.value("ncwo2_a_postage").toString());
        ui->prepifPostage->setText(query.value("prepif_postage").toString());

        // Update state
        isJobSaved = true;
        originalYear = QString::number(year);
        originalMonth = QString::number(month).rightJustified(2, '0');
        originalWeek = QString::number(week).rightJustified(2, '0');

        // Switch to the WEEKLY tab
        ui->tabWidget->setCurrentIndex(0);  // Assuming "WEEKLY" tab is at index 0

        updateWidgetStatesBasedOnJobState();
        logToTerminal("Opened job: " + originalYear + "-" + originalMonth + "-" + originalWeek);
    } else {
        qDebug() << "Failed to load job for" << year << "-" << month << "-" << week << ":" << query.lastError().text();
        QMessageBox::warning(this, "Error", "Failed to load job data from the database.");
    }
}

// Destructor
Goji::~Goji()
{
    db.close();
    delete ui;
}

// **Button Handler Implementations for QPushButton**
void Goji::onOpenIZClicked()
{
    logToTerminal("Opening InputZIP directory...");
    QString inputZipPath = QCoreApplication::applicationDirPath() + "/RAC/WEEKLY/INPUTZIP";
    QDesktopServices::openUrl(QUrl::fromLocalFile(inputZipPath));
    isOpenIZComplete = true;
    completedSubtasks[0] = 1; // Step 1: ZIP file detection
    updateProgressBar();
    updateLEDs();
}

void Goji::onRunInitialClicked()
{
    if (!isOpenIZComplete) {
        QMessageBox::warning(this, "Step Incomplete", "Please open InputZIP first.");
        return;
    }
    logToTerminal("Running Initial Script...");
    QString scriptPath = QCoreApplication::applicationDirPath() + "/Scripts/WEEKLIES/01RUNFIRST.py";
    runScript("python", {scriptPath});
    isRunInitialComplete = true;
    completedSubtasks[1] = 1; // Step 2: ZIP file processing
    updateProgressBar();
    updateLEDs();
}

void Goji::onRunPreProofClicked()
{
    if (!isRunInitialComplete) {
        QMessageBox::warning(this, "Step Incomplete", "Please run Initial Script first.");
        return;
    }
    if (!isPostageLocked) {
        QMessageBox::warning(this, "Postage Not Locked", "Please lock the postage data first.");
        return;
    }

    QMap<QString, QStringList> requiredFiles;
    requiredFiles["CBC"] = QStringList() << "CBC2_WEEKLY.csv" << "CBC3_WEEKLY.csv";
    requiredFiles["EXC"] = QStringList() << "EXC_OUTPUT.csv";
    requiredFiles["INACTIVE"] = QStringList() << "A-PO.txt" << "A-PU.txt";
    requiredFiles["NCWO"] = QStringList() << "1-A_OUTPUT.csv" << "1-AP_OUTPUT.csv" << "2-A_OUTPUT.csv" << "2-AP_OUTPUT.csv";
    requiredFiles["PREPIF"] = QStringList() << "PRE_PIF.csv";

    QString basePath = QCoreApplication::applicationDirPath();
    QStringList missingFiles;
    for (auto it = requiredFiles.constBegin(); it != requiredFiles.constEnd(); ++it) {
        QString jobType = it.key();
        QString outputDir = basePath + "/RAC/" + jobType + "/JOB/OUTPUT";
        for (const QString& fileName : it.value()) {
            QString filePath = outputDir + "/" + fileName;
            if (!QFile::exists(filePath)) {
                missingFiles.append(fileName);
            }
        }
    }

    if (!missingFiles.isEmpty()) {
        QString message = "The following data files are missing from their OUTPUT folders:\n\n";
        message += missingFiles.join("\n");
        message += "\n\nDo you want to proceed?";
        int choice = QMessageBox::warning(this, "Missing Files", message, QMessageBox::Yes | QMessageBox::No);
        if (choice == QMessageBox::No) {
            return;
        }

        QMessageBox confirmBox;
        confirmBox.setText("CONFIRM INCOMPLETE CONTINUE");
        QPushButton *confirmButton = confirmBox.addButton("Confirm", QMessageBox::AcceptRole);
        confirmBox.addButton("Cancel", QMessageBox::RejectRole);
        confirmBox.exec();
        if (confirmBox.clickedButton() != confirmButton) {
            return;
        }
    }

    logToTerminal("Running Pre-Proof...");
    QString scriptPath = QCoreApplication::applicationDirPath() + "/Scripts/WEEKLIES/02RUNSECOND.bat";
    QStringList arguments = {scriptPath, basePath, ui->cbcJobNumber->text(), ui->monthDDbox->currentText() + "." + ui->weekDDbox->currentText()};
    runScript("cmd.exe", QStringList() << "/c" << arguments);
    isRunPreProofComplete = true;
    completedSubtasks[2] = 1; // Step 3: OUTPUT files generation
    completedSubtasks[3] = 1; // Step 4: PreProof (proof data files)
    updateProgressBar();
    updateLEDs();
}

void Goji::onOpenProofFilesClicked()
{
    if (!isRunPreProofComplete) {
        QMessageBox::warning(this, "Step Incomplete", "Please run Pre-Proof first.");
        return;
    }
    QString selection = ui->proofDDbox->currentText();
    if (selection.isEmpty()) {
        logToTerminal("Please select a job type from proofDDbox.");
        return;
    }
    logToTerminal("Checking proof files for: " + selection);
    checkProofFiles(selection);
    if (isOpenProofFilesComplete) {
        completedSubtasks[4] = 1; // Step 5: Proof files (PDFs) generation
        updateProgressBar();
    }
    updateLEDs();
}

void Goji::onRunPostProofClicked()
{
    if (!isOpenProofFilesComplete) {
        QMessageBox::warning(this, "Step Incomplete", "Please open proof files first.");
        return;
    }

    QMap<QString, QStringList> expectedProofFiles;
    expectedProofFiles["CBC"] = QStringList() << "CBC2 PROOF.pdf" << "CBC3 PROOF.pdf";
    expectedProofFiles["EXC"] = QStringList() << "EXC PROOF.pdf";
    expectedProofFiles["INACTIVE"] = QStringList() << "INACTIVE A-PO PROOF.pdf" << "INACTIVE A-PU PROOF.pdf" << "INACTIVE AT-PO PROOF.pdf"
                                                   << "INACTIVE AT-PU PROOF.pdf" << "INACTIVE PR-PO PROOF.pdf" << "INACTIVE PR-PU PROOF.pdf";
    expectedProofFiles["NCWO"] = QStringList() << "NCWO 1-A PROOF.pdf" << "NCWO 1-AP PROOF.pdf" << "NCWO 1-APPR PROOF.pdf" << "NCWO 1-PR PROOF.pdf"
                                               << "NCWO 2-A PROOF.pdf" << "NCWO 2-AP PROOF.pdf" << "NCWO 2-APPR PROOF.pdf" << "NCWO 2-PR PROOF.pdf";
    expectedProofFiles["PREPIF"] = QStringList() << "PREPIF US PROOF.pdf" << "PREPIF PR PROOF.pdf";

    QStringList jobTypesToCheck = {"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"};
    QString basePath = QCoreApplication::applicationDirPath();
    QStringList missingFiles;
    for (const QString& jobType : jobTypesToCheck) {
        QString proofDir = basePath + "/RAC/" + jobType + "/JOB/PROOF";
        for (const QString& file : expectedProofFiles[jobType]) {
            QString filePath = proofDir + "/" + file;
            if (!QFile::exists(filePath)) {
                missingFiles.append(filePath);
            }
        }
    }

    if (!missingFiles.isEmpty()) {
        QString message = "The following proof files are missing:\n\n" + missingFiles.join("\n") +
                          "\n\nDo you want to proceed anyway?";
        int choice = QMessageBox::warning(this, "Missing Proof Files", message, QMessageBox::Yes | QMessageBox::No);
        if (choice == QMessageBox::No) {
            return;
        }
    }

    logToTerminal("Running Post-Proof...");
    if (isProofRegenMode) {
        regenerateProofs();
    } else {
        QString scriptPath = QCoreApplication::applicationDirPath() + "/Scripts/WEEKLIES/04POSTPROOF.py";
        QString week = ui->monthDDbox->currentText() + "." + ui->weekDDbox->currentText();

        QStringList arguments = {
            scriptPath,
            "--base_path", basePath,
            "--week", week,
            "--cbc_job", ui->cbcJobNumber->text(),
            "--exc_job", ui->excJobNumber->text(),
            "--inactive_job", ui->inactiveJobNumber->text(),
            "--ncwo_job", ui->ncwoJobNumber->text(),
            "--prepif_job", ui->prepifJobNumber->text(),
            "--cbc2_postage", ui->cbc2Postage->text(),
            "--cbc3_postage", ui->cbc3Postage->text(),
            "--exc_postage", ui->excPostage->text(),
            "--inactive_po_postage", ui->inactivePOPostage->text(),
            "--inactive_pu_postage", ui->inactivePUPostage->text(),
            "--ncwo_1a_postage", ui->ncwo1APostage->text(),
            "--ncwo_2a_postage", ui->ncwo2APostage->text(),
            "--prepif_postage", ui->prepifPostage->text()
        };

        QProcess *process = new QProcess(this);
        connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
            ui->terminalWindow->append(process->readAllStandardOutput());
        });
        connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
            ui->terminalWindow->append("<font color=\"red\">" + process->readAllStandardError() + "</font>");
        });
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
                    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                        ui->terminalWindow->append("Script completed successfully.");
                        savePostProofCounts();
                        isRunPostProofComplete = true;
                        completedSubtasks[5] = 1; // Step 6: runPostProof (ZIP files)
                        updateProgressBar();
                        enableProofApprovalCheckboxes();
                        updateLEDs();
                    } else {
                        ui->terminalWindow->append("Script failed with exit code " + QString::number(exitCode));
                    }
                    process->deleteLater();
                });
        process->start("python", arguments);
    }
}

void Goji::onOpenPrintFilesClicked()
{
    if (!isRunPostProofComplete) {
        QMessageBox::warning(this, "Step Incomplete", "Please run Post-Proof first.");
        return;
    }
    QString selection = ui->printDDbox->currentText();
    if (selection.isEmpty()) {
        logToTerminal("Please select a job type from printDDBox.");
        return;
    }
    logToTerminal("Checking print files for: " + selection);
    checkPrintFiles(selection);
    if (isOpenPrintFilesComplete) {
        completedSubtasks[7] = 1; // Step 8: Print files generation
        updateProgressBar();
    }
    updateLEDs();
}

void Goji::onRunPostPrintClicked()
{
    if (!isOpenPrintFilesComplete) {
        QMessageBox::warning(this, "Step Incomplete", "Please open print files first.");
        return;
    }
    logToTerminal("Running Post-Print...");
    QString scriptPath = QCoreApplication::applicationDirPath() + "/Scripts/WEEKLIES/05POSTPRINT.ps1";
    runScript("powershell.exe", {"-ExecutionPolicy", "Bypass", "-File", scriptPath});
    isRunPostPrintComplete = true;
    completedSubtasks[8] = 1; // Step 9: runPostPrint
    updateProgressBar();
    updateLEDs();
}

// **Button Handler Implementations for QToolButton**
void Goji::onLockButtonToggled(bool checked)
{
    if (checked) {
        if (!isJobSaved) {
            if (jobExists(ui->yearDDbox->currentText(), ui->monthDDbox->currentText(), ui->weekDDbox->currentText())) {
                QMessageBox::warning(this, "Job Exists", "A job with this year, month, and week already exists.");
                ui->lockButton->setChecked(false);
                return;
            }
            insertJob();
            isJobSaved = true;
            originalYear = ui->yearDDbox->currentText();
            originalMonth = ui->monthDDbox->currentText();
            originalWeek = ui->weekDDbox->currentText();
            createJobFolders(originalYear, originalMonth, originalWeek);  // Create home folders
            logToTerminal("Job Data Saved and Locked");
        } else {
            logToTerminal("Job Data Already Saved");
        }
        lockJobDataFields(true);
        ui->lockButton->setEnabled(false);  // Prevent toggling off directly
        ui->editButton->setEnabled(true);   // Allow editing via editButton
        updateWidgetStatesBasedOnJobState();  // Update widget states when job is saved
    } else {
        if (isJobSaved) {
            QMessageBox::warning(this, "Job Saved", "The job is already saved and cannot be unlocked.");
            ui->lockButton->setChecked(true);
        } else {
            lockJobDataFields(false);
            logToTerminal("Job Data Unlocked");
            ui->lockButton->setEnabled(true);
        }
    }
}

void Goji::onEditButtonToggled(bool checked)
{
    if (!isJobSaved) {
        if (checked) {
            QMessageBox::warning(this, "No Job Saved", "Cannot edit before saving the job.");
            ui->editButton->setChecked(false);
        }
        return;
    }

    if (checked) {
        lockJobDataFields(false);
        logToTerminal("Edit Mode Enabled");
        ui->editLabel->setText("EDITING ENABLED");
    } else {
        QString newYear = ui->yearDDbox->currentText();
        QString newMonth = ui->monthDDbox->currentText();
        QString newWeek = ui->weekDDbox->currentText();
        if (newYear != originalYear || newMonth != originalMonth || newWeek != originalWeek) {
            if (jobExists(newYear, newMonth, newWeek)) {
                QMessageBox::warning(this, "Job Exists", "JOB " + newMonth + "." + newWeek + " ALREADY EXISTS\n"
                                                                                             "In order to change details for " + newMonth + "." + newWeek + " open it from the menu.");
                ui->yearDDbox->setCurrentText(originalYear);
                ui->monthDDbox->setCurrentText(originalMonth);
                ui->weekDDbox->setCurrentText(originalWeek);
            } else {
                deleteJob(originalYear, originalMonth, originalWeek);
                insertJob();
                originalYear = newYear;
                originalMonth = newMonth;
                originalWeek = newWeek;
            }
        } else {
            updateJob();
        }
        lockJobDataFields(true);
        logToTerminal("Edit Mode Disabled");
        ui->editLabel->setText("EDITING DISABLED");
        ui->lockButton->setEnabled(true);  // Ensure lockButton is re-enabled
    }
}

void Goji::onProofRegenToggled(bool checked)
{
    isProofRegenMode = checked;
    ui->regenTab->setEnabled(checked);  // Enable/disable regenTab based on proofRegen toggle
    if (checked) {
        logToTerminal("Proof Regeneration Mode Enabled");
        updateButtonStates(false);
        ui->openPrintFiles->setEnabled(false);  // Disable openPrintFiles in regen mode
        for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
            it.value()->setEnabled(true);
        }
        isOpenProofFilesComplete = false;  // Reset proof files status
        updateLEDs();
    } else {
        logToTerminal("Proof Regeneration Mode Disabled");
        updateButtonStates(true);
        ui->openPrintFiles->setEnabled(true);   // Re-enable openPrintFiles
        for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
            it.value()->setEnabled(false);
            it.value()->setChecked(false);
        }
        updateLEDs();
    }
}

void Goji::onPostageLockToggled(bool checked)
{
    if (checked) {
        isPostageLocked = true;
        QList<QLineEdit*> postageLineEdits = {
            ui->cbc2Postage, ui->cbc3Postage, ui->excPostage,
            ui->inactivePOPostage, ui->inactivePUPostage,
            ui->ncwo1APostage, ui->ncwo2APostage, ui->prepifPostage
        };
        for (QLineEdit *edit : postageLineEdits) {
            edit->setReadOnly(true);
        }
        ui->runPostProof->setEnabled(true);  // Enable runPostProof when postage is locked
        logToTerminal("Postage Data Locked");
    } else {
        if (isRunPreProofComplete) {
            int result = QMessageBox::warning(this, "Warning", "Proof and postage data has already been processed.\n"
                                                               "Editing will require running Pre-Proof again.\nProceed with edit?",
                                              QMessageBox::Yes | QMessageBox::No);
            if (result == QMessageBox::Yes) {
                isPostageLocked = false;
                QList<QLineEdit*> postageLineEdits = {
                    ui->cbc2Postage, ui->cbc3Postage, ui->excPostage,
                    ui->inactivePOPostage, ui->inactivePUPostage,
                    ui->ncwo1APostage, ui->ncwo2APostage, ui->prepifPostage
                };
                for (QLineEdit *edit : postageLineEdits) {
                    edit->setReadOnly(false);
                }
                isRunPreProofComplete = false;
                completedSubtasks[2] = 0; // Reset Step 3
                completedSubtasks[3] = 0; // Reset Step 4
                updateProgressBar();
                updateLEDs();
                ui->runPostProof->setEnabled(false); // Disable runPostProof when unlocked
                logToTerminal("Postage Data Unlocked");
            } else {
                ui->postageLock->setChecked(true);
            }
        } else {
            isPostageLocked = false;
            QList<QLineEdit*> postageLineEdits = {
                ui->cbc2Postage, ui->cbc3Postage, ui->excPostage,
                ui->inactivePOPostage, ui->inactivePUPostage,
                ui->ncwo1APostage, ui->ncwo2APostage, ui->prepifPostage
            };
            for (QLineEdit *edit : postageLineEdits) {
                edit->setReadOnly(false);
            }
            ui->runPostProof->setEnabled(false); // Disable runPostProof when unlocked
            logToTerminal("Postage Data Unlocked");
        }
    }
}

// **Checkbox Handlers**
void Goji::onAllCBStateChanged(int state)
{
    bool checked = (state == Qt::Checked);
    ui->cbcCB->setChecked(checked);
    ui->excCB->setChecked(checked);
    ui->inactiveCB->setChecked(checked);
    ui->ncwoCB->setChecked(checked);
    ui->prepifCB->setChecked(checked);
    if (checked) {
        ui->proofApprovalLED->setStyleSheet("background-color: #00ff15;");
        completedSubtasks[6] = 1; // Step 7: Proof approval
    } else {
        ui->proofApprovalLED->setStyleSheet("background-color: red;");
        completedSubtasks[6] = 0; // Reset Step 7 if unchecked
    }
    updateProgressBar();
}

void Goji::updateAllCBState()
{
    bool allChecked = ui->cbcCB->isChecked() &&
                      ui->excCB->isChecked() &&
                      ui->inactiveCB->isChecked() &&
                      ui->ncwoCB->isChecked() &&
                      ui->prepifCB->isChecked();
    QSignalBlocker blocker(ui->allCB);
    ui->allCB->setChecked(allChecked);
    if (allChecked) {
        ui->proofApprovalLED->setStyleSheet("background-color: #00ff15;");
        completedSubtasks[6] = 1; // Step 7: Proof approval
    } else {
        ui->proofApprovalLED->setStyleSheet("background-color: red;");
        completedSubtasks[6] = 0; // Reset Step 7 if not all checked
    }
    updateProgressBar();
}

// **ComboBox Handlers**
void Goji::onProofDDboxChanged(const QString &text)
{
    logToTerminal("Proof dropdown changed to: " + text);
}

void Goji::onPrintDDboxChanged(const QString &text)
{
    logToTerminal("Print dropdown changed to: " + text);
}

void Goji::onYearDDboxChanged(const QString &text)
{
    logToTerminal("Year changed to: " + text);
}

void Goji::onMonthDDboxChanged(const QString &text)
{
    logToTerminal("Month changed to: " + text);
    ui->weekDDbox->clear();
    ui->weekDDbox->addItem("");
    if (!text.isEmpty()) {
        int month = text.toInt();
        int year = ui->yearDDbox->currentText().toInt();
        if (year > 0 && month > 0) {
            QDate firstDay(year, month, 1);
            QDate lastDay = firstDay.addMonths(1).addDays(-1);
            QDate date = firstDay;
            while (date <= lastDay) {
                if (date.dayOfWeek() == Qt::Monday) {
                    ui->weekDDbox->addItem(QString::number(date.day()).rightJustified(2, '0'));
                }
                date = date.addDays(1);
            }
        }
    }
}

void Goji::onWeekDDboxChanged(const QString &text)
{
    logToTerminal("Week changed to: " + text);
}

// **Menu Action Implementations**
void Goji::onActionExitTriggered()
{
    QCoreApplication::quit();
}

void Goji::onActionCloseJobTriggered()
{
    if (isJobSaved) {
        moveFilesToHomeFolders(originalYear, originalMonth, originalWeek);  // Move files back to home folders
    }
    ui->yearDDbox->setCurrentIndex(0);
    ui->monthDDbox->setCurrentIndex(0);
    ui->weekDDbox->setCurrentIndex(0);
    clearJobNumbers();
    isJobSaved = false;
    proofFiles.clear();
    printFiles.clear();
    ui->lockButton->setChecked(false);
    ui->editButton->setChecked(false);
    ui->postageLock->setChecked(false);
    ui->proofRegen->setChecked(false);
    lockJobDataFields(false);
    lockPostageFields(false);
    logToTerminal("Current job closed. Ready for a new job.");
    updateWidgetStatesBasedOnJobState();

    // Reset progress bar when job is closed
    for (int i = 0; i < 9; ++i) {
        completedSubtasks[i] = 0;
    }
    updateProgressBar();
}

void Goji::onActionSaveJobTriggered()
{
    QString year = ui->yearDDbox->currentText();
    QString month = ui->monthDDbox->currentText();
    QString week = ui->weekDDbox->currentText();

    if (jobExists(year, month, week)) {
        updateJob();
    } else {
        insertJob();
    }
    isJobSaved = true;
    logToTerminal("Job saved: " + year + " " + month + " " + week);
}

// **Recent Files Handler**
void Goji::openRecentFile(QAction* action)
{
    QString filePath = action->data().toString();
    logToTerminal("Opening recent file: " + filePath);
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    recentFilesMenu->addRecentFile(filePath);
}

// **Helper Methods**
void Goji::logToTerminal(const QString &message)
{
    ui->terminalWindow->append(message);
}

void Goji::clearJobNumbers()
{
    ui->cbcJobNumber->clear();
    ui->ncwoJobNumber->clear();
    ui->inactiveJobNumber->clear();
    ui->prepifJobNumber->clear();
    ui->excJobNumber->clear();
}

void Goji::lockJobDataFields(bool lock)
{
    isJobDataLocked = lock;
    ui->cbcJobNumber->setReadOnly(lock);
    ui->ncwoJobNumber->setReadOnly(lock);
    ui->inactiveJobNumber->setReadOnly(lock);
    ui->prepifJobNumber->setReadOnly(lock);
    ui->excJobNumber->setReadOnly(lock);
    ui->yearDDbox->setEnabled(!lock);
    ui->monthDDbox->setEnabled(!lock);
    ui->weekDDbox->setEnabled(!lock);
}

void Goji::lockPostageFields(bool lock)
{
    ui->cbc2Postage->setReadOnly(lock);
    ui->cbc3Postage->setReadOnly(lock);
    ui->excPostage->setReadOnly(lock);
    ui->inactivePOPostage->setReadOnly(lock);
    ui->inactivePUPostage->setReadOnly(lock);
    ui->ncwo1APostage->setReadOnly(lock);
    ui->ncwo2APostage->setReadOnly(lock);
    ui->prepifPostage->setReadOnly(lock);
}

void Goji::runScript(const QString &program, const QStringList &arguments)
{
    QProcess *process = new QProcess(this);
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        ui->terminalWindow->append(process->readAllStandardOutput());
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        ui->terminalWindow->append("<font color=\"red\">" + process->readAllStandardError() + "</font>");
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [process]() {  // Avoid capturing unused parameters
                process->deleteLater();
            });
    process->start(program, arguments);
}

void Goji::ensureInDesignIsOpen(const std::function<void()>& callback)
{
    // Placeholder: Add logic to check if InDesign is open, then call callback
    callback();
}

void Goji::checkProofFiles(const QString& selection)
{
    QString proofPath = QCoreApplication::applicationDirPath() + "/RAC/" + selection + "/JOB/PROOF";
    QDir proofDir(proofPath);
    QStringList csvFiles = proofDir.entryList(QStringList() << "*PROOF*.csv" << "*PD*.csv", QDir::Files);
    QStringList pdfFiles = proofDir.entryList(QStringList() << "*.pdf", QDir::Files);

    QMap<QString, QPair<QStringList, QStringList>> versionMap = {
        {"CBC", {{"CBC2WEEKLYREFORMAT"}, {"CBC2 PROOF"}}},
        {"CBC", {{"CBC3WEEKLYREFORMAT"}, {"CBC3 PROOF"}}},
        {"EXC", {{"EXC_OUTPUT"}, {"EXC PROOF"}}},
        {"INACTIVE", {{"A-PO"}, {"INACTIVE A-PO PROOF"}}},
        {"INACTIVE", {{"A-PU"}, {"INACTIVE A-PU PROOF"}}},
        {"INACTIVE", {{"AT-PO"}, {"INACTIVE AT-PO PROOF"}}},
        {"INACTIVE", {{"AT-PU"}, {"INACTIVE AT-PU PROOF"}}},
        {"INACTIVE", {{"PR-PO"}, {"INACTIVE PR-PO PROOF"}}},
        {"INACTIVE", {{"PR-PU"}, {"INACTIVE PR-PU PROOF"}}},
        {"NCWO", {{"1-A"}, {"NCWO 1-A PROOF"}}},
        {"NCWO", {{"1-AP"}, {"NCWO 1-AP PROOF"}}},
        {"NCWO", {{"1-APPR"}, {"NCWO 1-APPR PROOF"}}},
        {"NCWO", {{"1-PR"}, {"NCWO 1-PR PROOF"}}},
        {"NCWO", {{"2-A"}, {"NCWO 2-A PROOF"}}},
        {"NCWO", {{"2-AP"}, {"NCWO 2-AP PROOF"}}},
        {"NCWO", {{"2-APPR"}, {"NCWO 2-APPR PROOF"}}},
        {"NCWO", {{"2-PR"}, {"NCWO 2-PR PROOF"}}},
        {"PREPIF", {{"PRE_PIF"}, {"PREPIF US PROOF", "PREPIF PR PROOF"}}}
    };

    bool allPresent = true;
    for (auto it = versionMap.constBegin(); it != versionMap.constEnd(); ++it) {
        if (it.key() != selection) continue;
        QStringList csvVersions = it.value().first;
        QStringList pdfVersions = it.value().second;

        for (const QString& csvVersion : csvVersions) {
            bool csvFound = false;
            for (const QString& csv : csvFiles) {
                if (csv.contains(csvVersion, Qt::CaseSensitive)) {
                    csvFound = true;
                    bool pdfFound = false;
                    for (const QString& pdfVersion : pdfVersions) {
                        for (const QString& pdf : pdfFiles) {
                            if (pdf.contains(pdfVersion, Qt::CaseSensitive)) {
                                pdfFound = true;
                                break;
                            }
                        }
                        if (pdfFound) break;
                    }
                    if (!pdfFound) {
                        logToTerminal("Missing PDF for " + csvVersion + " in " + selection + "/PROOF");
                        allPresent = false;
                    }
                    break;
                }
            }
            if (!csvFound) {
                logToTerminal("No CSV found for " + csvVersion + " in " + selection + "/PROOF (optional version missing)");
            }
        }
    }

    isOpenProofFilesComplete = allPresent;
    if (allPresent) {
        logToTerminal("All required proof files present for: " + selection);
    } else {
        logToTerminal("Proof files incomplete for: " + selection);
    }
}

void Goji::checkPrintFiles(const QString& selection)
{
    QStringList jobTypes = (selection.isEmpty()) ? QStringList{"CBC", "EXC", "NCWO", "PREPIF"} : QStringList{selection};

    QMap<QString, QMap<QString, QStringList>> versionMap = {
        {"CBC", {{"CBC2WEEKLYREFORMAT", {"CBC2 PRINT"}}, {"CBC3WEEKLYREFORMAT", {"CBC3 PRINT"}}}},
        {"EXC", {{"EXC_OUTPUT", {"EXC PRINT"}}}},
        {"NCWO", {
                     {"1-A", {"NCWO 1-A PRINT"}},
                     {"1-AP", {"NCWO 1-AP PRINT"}},
                     {"1-APPR", {"NCWO 1-APPR PRINT"}},
                     {"1-PR", {"NCWO 1-PR PRINT"}},
                     {"2-A", {"NCWO 2-A PRINT"}},
                     {"2-AP", {"NCWO 2-AP PRINT"}},
                     {"2-APPR", {"NCWO 2-APPR PRINT"}},
                     {"2-PR", {"NCWO 2-PR PRINT"}}
                 }},
        {"PREPIF", {{"PRE_PIF", {"PREPIF US PRINT", "PREPIF PR PRINT"}}}}
    };

    bool allComplete = true;
    for (const QString& jobType : jobTypes) {
        if (!versionMap.contains(jobType)) continue; // Skip INACTIVE

        QString outputPath = QCoreApplication::applicationDirPath() + "/RAC/" + jobType + "/JOB/OUTPUT";
        QString printPath = QCoreApplication::applicationDirPath() + "/RAC/" + jobType + "/JOB/PRINT";
        QDir outputDir(outputPath);
        QDir printDir(printPath);

        QStringList outputFiles = outputDir.entryList(QStringList() << "*.csv", QDir::Files);
        QStringList printFiles = printDir.entryList(QStringList() << "*.pdf", QDir::Files);

        // Check file stability
        QFileSystemWatcher watcher;
        watcher.addPath(printPath);
        bool filesStable = true;
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(&watcher, &QFileSystemWatcher::directoryChanged, this, [&filesStable, &loop]() {
            filesStable = false;
            loop.quit();
        });
        timer.start(10000); // 10 seconds
        loop.exec();
        if (!filesStable) {
            logToTerminal("Files still being written in " + jobType + "/PRINT");
            allComplete = false;
            continue;
        }

        // Match versions
        QMap<QString, QStringList> versions = versionMap[jobType];
        for (const QString& csvVersion : versions.keys()) {
            bool csvFound = false;
            for (const QString& csv : outputFiles) {
                if (csv.contains(csvVersion, Qt::CaseSensitive)) {
                    csvFound = true;
                    bool pdfFound = false;
                    QStringList pdfVersions = versions[csvVersion];
                    for (const QString& pdfVersion : pdfVersions) {
                        for (const QString& pdf : printFiles) {
                            if (pdf.contains(pdfVersion, Qt::CaseSensitive)) {
                                pdfFound = true;
                                break;
                            }
                        }
                        if (pdfFound) break;
                    }
                    if (!pdfFound) {
                        logToTerminal("Missing print PDF for " + csvVersion + " in " + jobType + "/PRINT");
                        allComplete = false;
                    }
                    break;
                }
            }
            if (!csvFound) {
                logToTerminal("No CSV found for " + csvVersion + " in " + jobType + "/OUTPUT (optional version missing)");
            }
        }
    }

    isOpenPrintFilesComplete = allComplete;
    if (allComplete) {
        logToTerminal("All print files complete and stable for: " + (selection.isEmpty() ? "all jobs" : selection));
    } else {
        logToTerminal("Print files incomplete or unstable for: " + (selection.isEmpty() ? "some jobs" : selection));
    }
}

void Goji::openProofFiles(const QString& selection)
{
    logToTerminal("Opening proof files for: " + selection);
    // Placeholder logic remains unchanged; actual opening not required for LED check
}

void Goji::openPrintFiles(const QString& selection)
{
    logToTerminal("Opening print files for: " + selection);
    // Placeholder logic remains unchanged; actual opening not required for LED check
}

void Goji::updateLEDs()
{
    ui->preProofLED->setStyleSheet(isRunPreProofComplete ? "background-color: #00ff15; border-radius: 2px;" : "background-color: red; border-radius: 2px;");
    ui->proofFilesLED->setStyleSheet(isOpenProofFilesComplete ? "background-color: #00ff15; border-radius: 2px;" : "background-color: red; border-radius: 2px;");
    ui->postProofLED->setStyleSheet(isRunPostProofComplete ? "background-color: #00ff15; border-radius: 2px;" : "background-color: red; border-radius: 2px;");
    ui->printFilesLED->setStyleSheet(isOpenPrintFilesComplete ? "background-color: #00ff15; border-radius: 2px;" : "background-color: red; border-radius: 2px;");
    ui->postPrintLED->setStyleSheet(isRunPostPrintComplete ? "background-color: #00ff15; border-radius: 2px;" : "background-color: red; border-radius: 2px;");
}

void Goji::updateButtonStates(bool enabled)
{
    ui->openIZ->setEnabled(enabled);
    ui->runInitial->setEnabled(enabled);
    ui->runPreProof->setEnabled(enabled && isPostageLocked);
    ui->openProofFiles->setEnabled(enabled);
    ui->runPostProof->setEnabled(enabled && isPostageLocked);
    ui->openPrintFiles->setEnabled(enabled);
    ui->runPostPrint->setEnabled(enabled);
}

void Goji::updateWidgetStatesBasedOnJobState()
{
    bool jobActive = isJobSaved;

    // QPushButtons: Disable all except openIZ when no job is active
    ui->runInitial->setEnabled(jobActive);
    ui->runPreProof->setEnabled(jobActive);
    ui->openProofFiles->setEnabled(jobActive);
    ui->runPostProof->setEnabled(jobActive);
    ui->openPrintFiles->setEnabled(jobActive);
    ui->runPostPrint->setEnabled(jobActive);
    ui->openIZ->setEnabled(true);  // Always enabled

    // QComboBoxes: Disable all except yearDDbox, monthDDbox, weekDDbox
    ui->proofDDbox->setEnabled(jobActive);
    ui->printDDbox->setEnabled(jobActive);
    ui->yearDDbox->setEnabled(true);   // Always enabled
    ui->monthDDbox->setEnabled(true);  // Always enabled
    ui->weekDDbox->setEnabled(true);   // Always enabled

    // QToolButtons: Disable all except lockButton
    ui->editButton->setEnabled(jobActive);
    ui->proofRegen->setEnabled(jobActive);
    ui->postageLock->setEnabled(jobActive);
    ui->lockButton->setEnabled(true);  // Always enabled

    // regenTab: Disabled unless proofRegen is toggled on
    ui->regenTab->setEnabled(isProofRegenMode);
}

void Goji::formatCurrencyOnFinish()
{
    QLineEdit *lineEdit = qobject_cast<QLineEdit*>(sender());
    if (!lineEdit) return;

    QString text = lineEdit->text().trimmed();
    if (text.isEmpty()) {
        lineEdit->clear();
        return;
    }

    text.remove(QRegExp("[^0-9.]"));
    if (text.isEmpty()) {
        lineEdit->clear();
        return;
    }

    bool ok;
    double value = text.toDouble(&ok);
    if (!ok) {
        lineEdit->clear();
        return;
    }

    QLocale locale(QLocale::English, QLocale::UnitedStates);
    QString formatted = locale.toString(value, 'f', 2);
    formatted.prepend("$");
    lineEdit->setText(formatted);
}

void Goji::onGetCountTableClicked()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Post-Proof Counts and Comparison");
    QVBoxLayout *layout = new QVBoxLayout(dialog);

    QTableWidget *countsTable = new QTableWidget(this);
    countsTable->setColumnCount(7);
    countsTable->setHorizontalHeaderLabels({"Job Number", "Week", "Project", "PR Count", "CANC Count", "US Count", "Postage"});
    countsTable->setStyleSheet("QTableWidget { border: 1px solid black; } QTableWidget::item { border: 1px solid black; }");

    QSqlQuery countsQuery("SELECT job_number, week, project, pr_count, canc_count, us_count, postage FROM post_proof_counts");
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

    QPushButton *copyCountsButton = new QPushButton("Copy Counts", dialog);
    connect(copyCountsButton, &QPushButton::clicked, this, [countsTable]() {
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

    QTableWidget *comparisonTable = new QTableWidget(this);
    comparisonTable->setColumnCount(4);
    comparisonTable->setHorizontalHeaderLabels({"Group", "Input Count", "Output Count", "Difference"});
    comparisonTable->setStyleSheet("QTableWidget { border: 1px solid black; } QTableWidget::item { border: 1px solid black; }");

    QSqlQuery comparisonQuery("SELECT group_name, input_count, output_count, difference FROM count_comparison");
    row = 0;
    comparisonTable->setRowCount(comparisonQuery.size());
    while (comparisonQuery.next()) {
        comparisonTable->setItem(row, 0, new QTableWidgetItem(comparisonQuery.value("group_name").toString()));
        comparisonTable->setItem(row, 1, new QTableWidgetItem(comparisonQuery.value("input_count").toString()));
        comparisonTable->setItem(row, 2, new QTableWidgetItem(comparisonQuery.value("output_count").toString()));
        comparisonTable->setItem(row, 3, new QTableWidgetItem(comparisonQuery.value("difference").toString()));
        row++;
    }

    QPushButton *copyComparisonButton = new QPushButton("Copy Comparison", dialog);
    connect(copyComparisonButton, &QPushButton::clicked, this, [comparisonTable]() {
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

int Goji::getNextProofVersion(const QString& filePath)
{
    QSqlQuery query(db);
    query.prepare("SELECT version FROM proof_versions WHERE file_path = :filePath");
    query.bindValue(":filePath", filePath);
    if (query.exec() && query.next()) {
        int currentVersion = query.value(0).toInt();
        return currentVersion + 1; // Increment current version
    } else {
        // If no version exists, start at 2 (original is v1)
        return 2;
    }
}

void Goji::regenerateProofs()
{
    QStringList jobTypes = {"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"};
    for (const QString& jobType : jobTypes) {
        QStringList filesToRegen;
        for (auto it = checkboxFileMap.constBegin(); it != checkboxFileMap.constEnd(); ++it) {
            QCheckBox* checkbox = it.key();
            const QPair<QString, QString>& pair = it.value();
            if (pair.first == jobType && checkbox->isChecked()) {
                QString fileName = pair.second;
                int nextVersion = getNextProofVersion(fileName);
                filesToRegen << fileName;
                runProofRegenScript(jobType, filesToRegen, nextVersion);
                filesToRegen.clear(); // Clear after processing each file
            }
        }
    }
}

void Goji::enableProofApprovalCheckboxes()
{
    for (auto it = regenCheckboxes.begin(); it != regenCheckboxes.end(); ++it) {
        it.value()->setEnabled(true);
    }
    ui->allCB->setEnabled(true);
}

// **Database Operations**
bool Goji::jobExists(const QString& year, const QString& month, const QString& week)
{
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM jobs WHERE year = :year AND month = :month AND week = :week");
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", week);
    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}

void Goji::insertJob()
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO jobs (year, month, week, cbc_job_number, ncwo_job_number, inactive_job_number, prepif_job_number, exc_job_number, "
                  "cbc2_postage, cbc3_postage, exc_postage, inactive_po_postage, inactive_pu_postage, ncwo1_a_postage, ncwo2_a_postage, prepif_postage, progress) "
                  "VALUES (:year, :month, :week, :cbc, :ncwo, :inactive, :prepif, :exc, :cbc2, :cbc3, :exc_p, :in_po, :in_pu, :nc1a, :nc2a, :prepif, :progress)");
    query.bindValue(":year", ui->yearDDbox->currentText());
    query.bindValue(":month", ui->monthDDbox->currentText());
    query.bindValue(":week", ui->weekDDbox->currentText());
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
    query.bindValue(":prepif", ui->prepifPostage->text());
    query.bindValue(":progress", "created");
    if (!query.exec()) {
        qDebug() << "Insert error:" << query.lastError().text();
    }
}

void Goji::updateJob()
{
    QSqlQuery query(db);
    query.prepare("UPDATE jobs SET cbc_job_number = :cbc, ncwo_job_number = :ncwo, inactive_job_number = :inactive, prepif_job_number = :prepif, exc_job_number = :exc, "
                  "cbc2_postage = :cbc2, cbc3_postage = :cbc3, exc_postage = :exc_p, inactive_po_postage = :in_po, inactive_pu_postage = :in_pu, "
                  "ncwo1_a_postage = :nc1a, ncwo2_a_postage = :nc2a, prepif_postage = :prepif, progress = :progress "
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
    query.bindValue(":prepif", ui->prepifPostage->text());
    query.bindValue(":progress", "updated");
    query.bindValue(":year", originalYear);
    query.bindValue(":month", originalMonth);
    query.bindValue(":week", originalWeek);
    if (!query.exec()) {
        qDebug() << "Update error:" << query.lastError().text();
    }
}

void Goji::deleteJob(const QString& year, const QString& month, const QString& week)
{
    QSqlQuery query(db);
    query.prepare("DELETE FROM jobs WHERE year = :year AND month = :month AND week = :week");
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", week);
    if (!query.exec()) {
        qDebug() << "Delete error:" << query.lastError().text();
    }
}

void Goji::savePostProofCounts()
{
    QSqlQuery query(db);
    query.exec("DROP TABLE IF EXISTS post_proof_counts");
    query.exec("DROP TABLE IF EXISTS count_comparison");

    if (!query.exec("CREATE TABLE post_proof_counts ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "job_number TEXT, "
                    "week TEXT, "
                    "project TEXT, "
                    "pr_count INTEGER, "
                    "canc_count INTEGER, "
                    "us_count INTEGER, "
                    "postage REAL)")) {
        qDebug() << "Error creating post_proof_counts table:" << query.lastError().text();
        return;
    }

    if (!query.exec("CREATE TABLE count_comparison ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "group_name TEXT, "
                    "input_count INTEGER, "
                    "output_count INTEGER, "
                    "difference INTEGER)")) {
        qDebug() << "Error creating count_comparison table:" << query.lastError().text();
        return;
    }

    logToTerminal("Post-proof counts saved to database (placeholder implementation).");
}

void Goji::runProofRegenScript(const QString& jobType, const QStringList& files, int version)
{
    QProcess* process = new QProcess(this);
    QString scriptPath = QCoreApplication::applicationDirPath() + "/Scripts/WEEKLIES/04POSTPROOF.py";
    QString basePath = QCoreApplication::applicationDirPath();
    QString week = ui->monthDDbox->currentText() + "." + ui->weekDDbox->currentText();
    QString jobNumber = getJobNumberForJobType(jobType);

    QStringList arguments = {
        scriptPath,
        "--base_path", basePath,
        "--job_type", jobType,
        "--job_number", jobNumber,
        "--week", week,
        "--proof_files"
    };
    arguments << files;
    arguments << "--version" << QString::number(version);

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        ui->terminalWindow->append(process->readAllStandardOutput());
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        ui->terminalWindow->append("<font color=\"red\">" + process->readAllStandardError() + "</font>");
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process, jobType, files, version](int exitCode, QProcess::ExitStatus exitStatus) {
                if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                    ui->terminalWindow->append("Regeneration script completed successfully.");
                    // Update proof_versions table with the new version
                    QSqlQuery updateQuery(db);
                    for (const QString& file : files) {
                        updateQuery.prepare("INSERT OR REPLACE INTO proof_versions (file_path, version) "
                                            "VALUES (:filePath, :version)");
                        updateQuery.bindValue(":filePath", file);
                        updateQuery.bindValue(":version", version);
                        if (!updateQuery.exec()) {
                            qDebug() << "Failed to update proof_versions for" << file << ":" << updateQuery.lastError().text();
                        }
                    }
                } else {
                    ui->terminalWindow->append("Regeneration script failed with exit code " + QString::number(exitCode));
                }
                process->deleteLater();
            });

    process->start("python", arguments);
}

QString Goji::getJobNumberForJobType(const QString& jobType)
{
    if (jobType == "CBC") return ui->cbcJobNumber->text();
    if (jobType == "EXC") return ui->excJobNumber->text();
    if (jobType == "INACTIVE") return ui->inactiveJobNumber->text();
    if (jobType == "NCWO") return ui->ncwoJobNumber->text();
    if (jobType == "PREPIF") return ui->prepifJobNumber->text();
    return "";
}

void Goji::buildWeeklyMenu(QMenu* menu)
{
    QSqlQuery query(db);
    if (!query.exec("SELECT DISTINCT year FROM jobs ORDER BY year DESC")) {
        logToTerminal("Database error: " + query.lastError().text());
        return;
    }

    while (query.next()) {
        QString year = query.value(0).toString();
        QMenu* yearMenu = menu->addMenu(year);

        QSqlQuery monthQuery(db);
        monthQuery.prepare("SELECT DISTINCT month FROM jobs WHERE year = :year ORDER BY month");
        monthQuery.bindValue(":year", year);
        if (!monthQuery.exec()) {
            logToTerminal("Database error: " + monthQuery.lastError().text());
            continue;
        }

        while (monthQuery.next()) {
            QString month = monthQuery.value(0).toString();
            QMenu* monthMenu = yearMenu->addMenu(month);

            QSqlQuery weekQuery(db);
            weekQuery.prepare("SELECT DISTINCT week FROM jobs WHERE year = :year AND month = :month ORDER BY week");
            weekQuery.bindValue(":year", year);
            weekQuery.bindValue(":month", month);
            if (!weekQuery.exec()) {
                logToTerminal("Database error: " + weekQuery.lastError().text());
                continue;
            }

            while (weekQuery.next()) {
                QString week = weekQuery.value(0).toString();
                QAction* weekAction = monthMenu->addAction(week);
                connect(weekAction, &QAction::triggered, this, [this, year, month, week]() {
                    openJobFromWeekly(year, month, week);
                });
            }
        }
    }
}

void Goji::openJobFromWeekly(const QString& year, const QString& month, const QString& week)
{
    if (isJobSaved) {
        moveFilesToHomeFolders(originalYear, originalMonth, originalWeek);  // Close current job
    }
    ui->yearDDbox->setCurrentText(year);
    ui->monthDDbox->setCurrentText(month);
    ui->weekDDbox->setCurrentText(week);
    copyFilesToWorkingFolders(year, month, week);  // Copy files to working folders
    isJobSaved = true;
    originalYear = year;
    originalMonth = month;
    originalWeek = week;
    logToTerminal("Opened job: " + year + " " + month + " " + week);
    // Additional logic to load job data can be added here
}

// **New Slot Implementations (Placeholders)**
void Goji::onPrintDirChanged(const QString &path)
{
    qDebug() << "Print directory changed:" << path;
    // Add logic to handle directory changes (e.g., update UI or file lists)
}

void Goji::onRegenProofButtonClicked()
{
    qDebug() << "Regenerating proofs...";
    // Add logic to regenerate proofs
}

// **Helper Functions for File Management**
void Goji::createJobFolders(const QString& year, const QString& month, const QString& week)
{
    Q_UNUSED(year);  // Year is not used in folder structure but kept for consistency
    QString basePath = QCoreApplication::applicationDirPath() + "/RAC";
    QString weekFolder = month + "." + week;
    QStringList jobTypes = {"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"};
    QStringList subfolders = {"INPUT", "OUTPUT", "PRINT", "PROOF"};

    for (const QString& jobType : jobTypes) {
        QString homePath = basePath + "/" + jobType + "/" + weekFolder;
        for (const QString& subfolder : subfolders) {
            QDir dir(homePath + "/" + subfolder);
            if (!dir.exists()) {
                if (!dir.mkpath(".")) {
                    logToTerminal("Failed to create directory: " + dir.path());
                } else {
                    logToTerminal("Created directory: " + dir.path());
                }
            }
        }
    }
}

void Goji::copyFilesToWorkingFolders(const QString& year, const QString& month, const QString& week)
{
    Q_UNUSED(year);
    QString basePath = QCoreApplication::applicationDirPath() + "/RAC";
    QString weekFolder = month + "." + week;
    QStringList jobTypes = {"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"};
    QStringList subfolders = {"INPUT", "OUTPUT", "PRINT", "PROOF"};

    for (const QString& jobType : jobTypes) {
        QString homePath = basePath + "/" + jobType + "/" + weekFolder;
        QString workingPath = basePath + "/" + jobType + "/JOB";
        for (const QString& subfolder : subfolders) {
            QDir homeDir(homePath + "/" + subfolder);
            QDir workingDir(workingPath + "/" + subfolder);
            if (!workingDir.exists()) {
                workingDir.mkpath(".");
            }
            for (const QFileInfo& fileInfo : homeDir.entryInfoList(QDir::Files)) {
                QString srcFile = fileInfo.filePath();
                QString destFile = workingDir.filePath(fileInfo.fileName());
                if (QFile::exists(destFile)) {
                    if (!QFile::remove(destFile)) {
                        logToTerminal("Failed to remove existing file: " + destFile);
                        continue;
                    }
                }
                if (!QFile::copy(srcFile, destFile)) {
                    logToTerminal("Failed to copy file: " + srcFile + " to " + destFile);
                } else {
                    logToTerminal("Copied file: " + srcFile + " to " + destFile);
                }
            }
        }
    }
}

void Goji::moveFilesToHomeFolders(const QString& year, const QString& month, const QString& week)
{
    Q_UNUSED(year);
    QString basePath = QCoreApplication::applicationDirPath() + "/RAC";
    QString weekFolder = month + "." + week;
    QStringList jobTypes = {"CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"};
    QStringList subfolders = {"INPUT", "OUTPUT", "PRINT", "PROOF"};

    for (const QString& jobType : jobTypes) {
        QString homePath = basePath + "/" + jobType + "/" + weekFolder;
        QString workingPath = basePath + "/" + jobType + "/JOB";
        for (const QString& subfolder : subfolders) {
            QDir workingDir(workingPath + "/" + subfolder);
            QDir homeDir(homePath + "/" + subfolder);
            for (const QFileInfo& fileInfo : workingDir.entryInfoList(QDir::Files)) {
                QString workingFile = fileInfo.filePath();
                QString homeFile = homeDir.filePath(fileInfo.fileName());
                if (QFile::exists(homeFile)) {
                    if (!QFile::remove(homeFile)) {
                        logToTerminal("Failed to remove existing file: " + homeFile);
                        continue;
                    }
                }
                if (!QFile::rename(workingFile, homeFile)) {
                    logToTerminal("Failed to move file: " + workingFile + " to " + homeFile);
                } else {
                    logToTerminal("Moved file: " + workingFile + " to " + homeFile);
                }
            }
        }
    }
}

// **Progress Bar Update Function**
void Goji::updateProgressBar()
{
    double total = 0.0;
    for (int i = 0; i < 9; ++i) {
        if (totalSubtasks[i] > 0) {
            total += (static_cast<double>(completedSubtasks[i]) / totalSubtasks[i]) * stepWeights[i];
        }
    }
    ui->progressBarWeekly->setValue(static_cast<int>(total));
}
