#include "goji.h"
#include "ui_GOJI.h"
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
#include <QRegExpValidator>
#include <QLocale>
#include <QDate>
#include <QStandardPaths>
#include <QDir>
#include <QTimer>

Goji::Goji(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("Goji");
    setWindowIcon(QIcon(":/resources/icons/ShinGoji.ico"));

    // Initialize QRecentFilesMenu
    recentFilesMenu = new QRecentFilesMenu("Recent Files", this);
    ui->menuFile->addMenu(recentFilesMenu);

    // Connect QRecentFilesMenu signal to slot
    connect(recentFilesMenu, &QRecentFilesMenu::triggered, this, &Goji::openRecentFile);

    // Connect QPushButton signals to their respective slots
    connect(ui->openIZ, &QPushButton::clicked, this, &Goji::onOpenIZClicked);
    connect(ui->runInitial, &QPushButton::clicked, this, &Goji::onRunInitialClicked);
    connect(ui->runPreProof, &QPushButton::clicked, this, &Goji::onRunPreProofClicked);
    connect(ui->openProofFiles, &QPushButton::clicked, this, &Goji::onOpenProofFilesClicked);
    connect(ui->runPostProof, &QPushButton::clicked, this, &Goji::onRunPostProofClicked);
    connect(ui->openPrintFiles, &QPushButton::clicked, this, &Goji::onOpenPrintFilesClicked);
    connect(ui->runPostPrint, &QPushButton::clicked, this, &Goji::onRunPostPrintClicked);

    // Connect QToolButton signals to their respective slots
    connect(ui->lockButton, &QToolButton::toggled, this, &Goji::onLockButtonToggled);
    connect(ui->editButton, &QToolButton::toggled, this, &Goji::onEditButtonToggled);
    connect(ui->proofRegen, &QToolButton::toggled, this, &Goji::onProofRegenToggled);
    connect(ui->postageLock, &QToolButton::toggled, this, &Goji::onPostageLockToggled);

    // Connect combobox signals
    connect(ui->proofDDbox, &QComboBox::currentTextChanged, this, &Goji::onProofDDboxChanged);
    connect(ui->printDDbox, &QComboBox::currentTextChanged, this, &Goji::onPrintDDboxChanged);
    connect(ui->yearDDbox, &QComboBox::currentTextChanged, this, &Goji::onYearDDboxChanged);
    connect(ui->monthDDbox, &QComboBox::currentTextChanged, this, &Goji::onMonthDDboxChanged);
    connect(ui->weekDDbox, &QComboBox::currentTextChanged, this, &Goji::onWeekDDboxChanged);

    // Connect "ALL" checkbox state change
    connect(ui->allCB, &QCheckBox::stateChanged, this, &Goji::onAllCBStateChanged);

    // Connect individual checkboxes to update "ALL" checkbox
    connect(ui->cbcCB, &QCheckBox::stateChanged, this, &Goji::updateAllCBState);
    connect(ui->excCB, &QCheckBox::stateChanged, this, &Goji::updateAllCBState);
    connect(ui->inactiveCB, &QCheckBox::stateChanged, this, &Goji::updateAllCBState);
    connect(ui->ncwoCB, &QCheckBox::stateChanged, this, &Goji::updateAllCBState);
    connect(ui->prepifCB, &QCheckBox::stateChanged, this, &Goji::updateAllCBState);

    // Initialize file mappings for proof files
    proofFiles = {
        {"CBC", {"/RAC/CBC/ART/CBC2 PROOF.indd", "/RAC/CBC/ART/CBC3 PROOF.indd"}},
        {"EXC", {"/RAC/EXC/ART/EXC PROOF.indd"}},
        {"INACTIVE", {"/RAC/INACTIVE/ART/A-PU PROOF.indd", "/RAC/INACTIVE/ART/FZA-PO PROOF.indd",
                      "/RAC/INACTIVE/ART/FZA-PU PROOF.indd", "/RAC/INACTIVE/ART/PR-PO PROOF.indd",
                      "/RAC/INACTIVE/ART/PR-PU PROOF.indd", "/RAC/INACTIVE/ART/A-PO PROOF.indd"}},
        {"NCWO", {"/RAC/NCWO/ART/NCWO 2-PR PROOF.indd", "/RAC/NCWO/ART/NCWO 1-A PROOF.indd",
                  "/RAC/NCWO/ART/NCWO 1-AP PROOF.indd", "/RAC/NCWO/ART/NCWO 1-APPR PROOF.indd",
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

    // Set up validator for postage QLineEdit widgets (now a member variable)
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
               "ncwo1_ap_postage TEXT, "
               "ncwo2_a_postage TEXT, "
               "ncwo2_ap_postage TEXT, "
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

    // Initialize state variables
    isJobSaved = false;
    isPostageLocked = false;
    isOpenIZComplete = false;
    isRunInitialComplete = false;
    isRunPreProofComplete = false;
    isOpenProofFilesComplete = false;
    isRunPostProofComplete = false;
    isOpenPrintFilesComplete = false;
    isRunPostPrintComplete = false;
    isProofRegenMode = false;

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
}

Goji::~Goji()
{
    db.close();
    delete ui;
}
