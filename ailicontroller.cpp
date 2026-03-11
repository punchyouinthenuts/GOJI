#include "ailicontroller.h"

#include "ailifilemanager.h"
#include "ailiemaildialog.h"
#include "dropwindow.h"

#include "mainwindow.h"
#include "ui_GOJI.h"

#include <QDesktopServices>
#include <QDate>
#include <QDoubleValidator>
#include <QFileInfo>
#include <QIntValidator>
#include <QMessageBox>
#include <QTimer>
#include <QUrl>

AILIController::AILIController(MainWindow *mainWindow,
                               Ui::MainWindow *ui,
                               QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_ui(ui)
    , m_dbManager(nullptr)
    , m_fileManager(nullptr)
    , m_emailDialog(nullptr)
    , m_scriptProcess(nullptr)
    , m_autoResetTimer(nullptr)
    , m_pendingAction(NoPendingAction)
    , m_jobActive(false)
    , m_initialLocked(false)
    , m_postageLocked(false)
{
}

AILIController::~AILIController()
{
}

bool AILIController::initializeAfterConstruction()
{
    if (!initializeManagers()) {
        return false;
    }

    initializeValidators();
    initializeUiState();
    connectSignals();
    ensureVersionBoxReadOnly();

    m_scriptProcess = new QProcess(this);

    connect(m_scriptProcess, &QProcess::started,
            this, &AILIController::handleScriptStarted);

    connect(m_scriptProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            &AILIController::handleScriptFinished);

    connect(m_scriptProcess,
            &QProcess::errorOccurred,
            this,
            &AILIController::handleScriptErrorOccurred);

    connect(m_scriptProcess,
            &QProcess::readyReadStandardOutput,
            this,
            &AILIController::handleScriptReadyReadStandardOutput);

    connect(m_scriptProcess,
            &QProcess::readyReadStandardError,
            this,
            &AILIController::handleScriptReadyReadStandardError);

    m_autoResetTimer = new QTimer(this);
    m_autoResetTimer->setSingleShot(true);

    connect(m_autoResetTimer,
            &QTimer::timeout,
            this,
            &AILIController::handleAutoResetTimeout);

    loadDefaultHtml();
    updateButtonStates();

    return true;
}

bool AILIController::initializeManagers()
{
    m_dbManager = AILIDBManager::instance();
    m_fileManager = new AILIFileManager(this);

    if (!m_dbManager || !m_dbManager->initializeTables()) {
        return false;
    }

    return m_fileManager->initializeDirectories();
}

void AILIController::initializeUiState()
{
    if (m_ui->yearDDboxAILI) {
        m_ui->yearDDboxAILI->clear();
        m_ui->yearDDboxAILI->addItem("");

        const int currentYear = QDate::currentDate().year();
        m_ui->yearDDboxAILI->addItem(QString::number(currentYear - 1));
        m_ui->yearDDboxAILI->addItem(QString::number(currentYear));
        m_ui->yearDDboxAILI->addItem(QString::number(currentYear + 1));
        m_ui->yearDDboxAILI->setCurrentIndex(0);
    }

    lockJobMetadataFields(false);
    lockPostageFields(false);
    clearTerminal();
    clearDisplayedResults();
}

void AILIController::initializeValidators()
{
    m_ui->jobNumberBoxAILI->setValidator(new QIntValidator(0, 99999, this));
    m_ui->issueNumberBoxAILI->setValidator(new QIntValidator(0, 99, this));
    m_ui->countBoxAILI->setValidator(new QIntValidator(0, 999999, this));
    m_ui->postageBoxAILI->setValidator(new QDoubleValidator(0, 999999, 2, this));
}

void AILIController::connectSignals()
{
    connect(m_ui->lockButtonAILI,
            &QToolButton::clicked,
            this,
            &AILIController::handleLockButtonClicked);

    connect(m_ui->runInitialAILI,
            &QPushButton::clicked,
            this,
            &AILIController::handleRunInitialClicked);

    connect(m_ui->postageLockAILI,
            &QToolButton::clicked,
            this,
            &AILIController::handlePostageLockClicked);

    connect(m_ui->finalStepAILI,
            &QPushButton::clicked,
            this,
            &AILIController::handleFinalStepClicked);

    connect(m_ui->openBulkMailerAILI,
            &QPushButton::clicked,
            this,
            &AILIController::handleOpenBulkMailerClicked);

    if (m_ui->dropWindowAILI) {
        m_ui->dropWindowAILI->setTargetDirectory(m_fileManager->originalPath());
        m_ui->dropWindowAILI->setSupportedExtensions(QStringList() << "xlsx" << "xls");

        connect(m_ui->dropWindowAILI,
                &DropWindow::filesDropped,
                this,
                [this](const QStringList &filePaths) {
                    if (!filePaths.isEmpty()) {
                        handleDropWindowFileDropped(filePaths.first());
                    }
                });
    }
}

void AILIController::ensureVersionBoxReadOnly()
{
    m_ui->versionBoxAILI->setReadOnly(true);
}

void AILIController::handleDropWindowFileDropped(const QString &filePath)
{
    if (!validateDroppedFile(filePath)) {
        QMessageBox::warning(m_mainWindow, "AILI", "Please drop a valid Excel file (.xlsx or .xls).");
        return;
    }

    if (!captureDroppedFile(filePath)) {
        QMessageBox::warning(m_mainWindow, "AILI", "Failed to copy dropped file to AILI ORIGINAL folder.");
        return;
    }

    appendTerminalMessage(QString("Captured file: %1").arg(QFileInfo(filePath).fileName()));
    updateButtonStates();
}

void AILIController::handleLockButtonClicked()
{
    QString error;

    if (!validateJobMetadata(error)) {
        QMessageBox::warning(m_mainWindow, "AILI", error);
        return;
    }

    if (!persistInitialJob()) {
        QMessageBox::warning(m_mainWindow, "AILI", "Database save failed.");
        return;
    }

    m_initialLocked = true;
    setJobActive(true);

    lockJobMetadataFields(true);
    loadInstructionHtmlForVersion(currentVersion());
    updateButtonStates();

    appendTerminalMessage("Job metadata locked and saved.");
}

void AILIController::handleRunInitialClicked()
{
    QStringList args;
    args << currentVersion();

    if (!startPythonScript(script01Path(), args, PendingInitialProcess)) {
        appendTerminalError("Unable to start initial script.");
    }
}

void AILIController::handleOpenBulkMailerClicked()
{
    if (!openBulkMailerIfNeeded()) {
        QMessageBox::warning(m_mainWindow, "AILI", "Failed to open Bulk Mailer.");
    }
}

void AILIController::handlePostageLockClicked()
{
    QString error;

    if (!validatePostageAndCount(error)) {
        QMessageBox::warning(m_mainWindow, "AILI", error);
        return;
    }

    if (!persistPostageAndCount()) {
        QMessageBox::warning(m_mainWindow, "AILI", "Database update failed.");
        return;
    }

    m_postageLocked = true;
    lockPostageFields(true);
    updateButtonStates();

    appendTerminalMessage("Postage/count locked and saved.");
}

void AILIController::handleFinalStepClicked()
{
    QStringList args;
    args << currentVersion()
         << currentJobNumber()
         << QString::number(currentPostage(), 'f', 2);

    if (!startPythonScript(script02Path(), args, PendingFinalProcess)) {
        appendTerminalError("Unable to start final script.");
    }
}

void AILIController::handleScriptStarted()
{
    appendTerminalMessage("Script started.");
}

void AILIController::handleScriptFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus)

    if (exitCode != 0) {
        appendTerminalError("Script failed.");
        QMessageBox::warning(m_mainWindow, "AILI", "Script failed.");
        m_pendingAction = NoPendingAction;
        return;
    }

    if (m_pendingAction == PendingInitialProcess) {
        appendTerminalMessage("Initial processing complete.");
        QMessageBox::information(m_mainWindow, "AILI", "Initial processing complete.");
    } else if (m_pendingAction == PendingFinalProcess) {
        const QString invalidFile = m_fileManager->findInvalidAddressFile();
        const QVector<QStringList> tableData = buildEmailTableDataFromFinalProcess();

        if (showEmailDialogAndWait(tableData, invalidFile)) {
            beginAutoResetTimer();
        }
    }

    m_pendingAction = NoPendingAction;
}

void AILIController::handleScriptErrorOccurred(QProcess::ProcessError error)
{
    Q_UNUSED(error)
    appendTerminalError("Script execution error.");
    QMessageBox::warning(m_mainWindow, "AILI", "Script execution error.");
    m_pendingAction = NoPendingAction;
}

void AILIController::handleScriptReadyReadStandardOutput()
{
    if (!m_scriptProcess) {
        return;
    }

    const QString text = QString::fromUtf8(m_scriptProcess->readAllStandardOutput()).trimmed();
    if (!text.isEmpty()) {
        appendTerminalMessage(text);
    }
}

void AILIController::handleScriptReadyReadStandardError()
{
    if (!m_scriptProcess) {
        return;
    }

    const QString text = QString::fromUtf8(m_scriptProcess->readAllStandardError()).trimmed();
    if (!text.isEmpty()) {
        appendTerminalError(text);
    }
}

void AILIController::handleAutoResetTimeout()
{
    resetJob();
}

bool AILIController::validateDroppedFile(const QString &filePath) const
{
    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        return false;
    }

    const QString suffix = info.suffix().toLower();
    return suffix == "xlsx" || suffix == "xls";
}

QString AILIController::detectVersionFromFilename(const QString &filePath) const
{
    const QString name = QFileInfo(filePath).fileName().toUpper();

    if (name.contains("AO SPOTLIGHT") || name.contains("AO_SPOTLIGHT") || name.contains("AOSPOTLIGHT")) {
        return "AO SPOTLIGHT";
    }

    return "SPOTLIGHT";
}

bool AILIController::captureDroppedFile(const QString &filePath)
{
    const QString normalizedPath = QFileInfo(filePath).absoluteFilePath();

    QString copiedPath;
    if (!m_fileManager->copyOriginalFile(normalizedPath, copiedPath)) {
        return false;
    }

    setOriginalFilePath(copiedPath);
    setDetectedVersion(detectVersionFromFilename(normalizedPath));

    return true;
}

bool AILIController::validateJobMetadata(QString &errorMessage) const
{
    if (m_originalFilePath.isEmpty()) {
        errorMessage = "Drop a source file before locking job data.";
        return false;
    }

    if (m_ui->jobNumberBoxAILI->text().trimmed().isEmpty()) {
        errorMessage = "Job number required.";
        return false;
    }

    if (m_ui->issueNumberBoxAILI->text().trimmed().isEmpty()) {
        errorMessage = "Issue number required.";
        return false;
    }

    if (m_ui->yearDDboxAILI->currentText().trimmed().isEmpty()) {
        errorMessage = "Year required.";
        return false;
    }

    if (m_ui->monthDDboxAILI->currentText().trimmed().isEmpty()) {
        errorMessage = "Month required.";
        return false;
    }

    if (m_ui->pageCountddBoxAILI->currentIndex() <= 0) {
        errorMessage = "Page count required.";
        return false;
    }

    if (m_ui->versionBoxAILI->text().trimmed().isEmpty()) {
        errorMessage = "Detected version is required.";
        return false;
    }

    return true;
}

bool AILIController::validatePostageAndCount(QString &errorMessage) const
{
    if (m_ui->countBoxAILI->text().trimmed().isEmpty()) {
        errorMessage = "Count required.";
        return false;
    }

    if (m_ui->postageBoxAILI->text().trimmed().isEmpty()) {
        errorMessage = "Postage required.";
        return false;
    }

    return true;
}

AILIJobData AILIController::buildCurrentJobData() const
{
    AILIJobData job;

    job.jobNumber = currentJobNumber();
    job.issueNumber = currentIssueNumber();
    job.month = currentMonth();
    job.year = currentYear();
    job.version = currentVersion();
    job.pageCount = currentPageCount();
    job.postage = currentPostage();
    job.count = currentCount();

    return job;
}

bool AILIController::persistInitialJob()
{
    const AILIJobData job = buildCurrentJobData();

    return m_dbManager->saveJob(
        job.jobNumber,
        job.issueNumber,
        QString::number(job.year),
        QString("%1").arg(job.month, 2, 10, QChar('0')),
        job.version,
        QString::number(job.pageCount));
}

bool AILIController::persistPostageAndCount()
{
    return m_dbManager->savePostageData(
        currentJobNumber(),
        currentIssueNumber(),
        QString::number(currentYear()),
        QString("%1").arg(currentMonth(), 2, 10, QChar('0')),
        currentVersion(),
        QString::number(currentPostage(), 'f', 2),
        QString::number(currentCount()),
        true);
}

void AILIController::lockJobMetadataFields(bool locked)
{
    m_ui->jobNumberBoxAILI->setEnabled(!locked);
    m_ui->issueNumberBoxAILI->setEnabled(!locked);
    m_ui->yearDDboxAILI->setEnabled(!locked);
    m_ui->monthDDboxAILI->setEnabled(!locked);
    m_ui->pageCountddBoxAILI->setEnabled(!locked);
}

void AILIController::lockPostageFields(bool locked)
{
    m_ui->countBoxAILI->setEnabled(!locked);
    m_ui->postageBoxAILI->setEnabled(!locked);
}

void AILIController::updateButtonStates()
{
    m_ui->runInitialAILI->setEnabled(m_initialLocked);
    m_ui->finalStepAILI->setEnabled(m_postageLocked);
}

void AILIController::loadDefaultHtml()
{
    m_ui->textBrowserAILI->setSource(QUrl("qrc:/resources/aili/default.html"));
}

void AILIController::loadInstructionHtmlForVersion(const QString &version)
{
    if (version == "AO SPOTLIGHT") {
        m_ui->textBrowserAILI->setSource(QUrl("qrc:/resources/aili/instructionsAOSL.html"));
    } else {
        m_ui->textBrowserAILI->setSource(QUrl("qrc:/resources/aili/instructionsSL.html"));
    }
}

void AILIController::appendTerminalMessage(const QString &message)
{
    if (m_ui->terminalWindowAILI) {
        m_ui->terminalWindowAILI->append(message);
    }
}

void AILIController::appendTerminalError(const QString &message)
{
    if (m_ui->terminalWindowAILI) {
        m_ui->terminalWindowAILI->append(QString("ERROR: %1").arg(message));
    }
}

void AILIController::clearTerminal()
{
    if (m_ui->terminalWindowAILI) {
        m_ui->terminalWindowAILI->clear();
    }
}

void AILIController::clearDisplayedResults()
{
    if (m_ui->countBoxAILI) {
        m_ui->countBoxAILI->clear();
    }

    if (m_ui->postageBoxAILI) {
        m_ui->postageBoxAILI->clear();
    }
}

QString AILIController::script01Path() const
{
    return "C:/Goji/scripts/AILI/01 PROCESS EXCEL LIST.py";
}

QString AILIController::script02Path() const
{
    return "C:/Goji/scripts/AILI/02 FINAL PROCESS.py";
}

QString AILIController::pythonExecutablePath() const
{
    return "python";
}

bool AILIController::startPythonScript(const QString &scriptPath,
                                       const QStringList &arguments,
                                       PendingAction pendingAction)
{
    if (!m_scriptProcess) {
        return false;
    }

    if (!QFileInfo::exists(scriptPath)) {
        QMessageBox::warning(m_mainWindow, "AILI", "Script not found.");
        return false;
    }

    if (m_scriptProcess->state() != QProcess::NotRunning) {
        QMessageBox::warning(m_mainWindow, "AILI", "A script is already running.");
        return false;
    }

    m_pendingAction = pendingAction;

    QStringList args;
    args << scriptPath;
    args << arguments;

    m_scriptProcess->start(pythonExecutablePath(), args);
    return true;
}

QString AILIController::currentJobNumber() const
{
    return m_ui->jobNumberBoxAILI->text().trimmed();
}

QString AILIController::currentIssueNumber() const
{
    return m_ui->issueNumberBoxAILI->text().trimmed();
}

int AILIController::currentMonth() const
{
    return m_ui->monthDDboxAILI->currentText().toInt();
}

int AILIController::currentYear() const
{
    return m_ui->yearDDboxAILI->currentText().toInt();
}

int AILIController::currentPageCount() const
{
    return m_ui->pageCountddBoxAILI->currentText().toInt();
}

QString AILIController::currentVersion() const
{
    return m_ui->versionBoxAILI->text().trimmed();
}

int AILIController::currentCount() const
{
    return m_ui->countBoxAILI->text().toInt();
}

double AILIController::currentPostage() const
{
    return m_ui->postageBoxAILI->text().toDouble();
}

bool AILIController::openBulkMailerIfNeeded()
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile("C:/BulkMailer/BulkMailer.exe"));
}

QVector<QStringList> AILIController::buildEmailTableDataFromFinalProcess() const
{
    return QVector<QStringList>();
}

bool AILIController::showEmailDialogAndWait(const QVector<QStringList> &tableData,
                                            const QString &invalidAddressFilePath)
{
    m_emailDialog = new AILIEmailDialog(m_mainWindow);
    m_emailDialog->setPostageTable(tableData);
    m_emailDialog->setInvalidAddressFile(invalidAddressFilePath);

    const int result = m_emailDialog->exec();
    return result == QDialog::Accepted;
}

void AILIController::beginAutoResetTimer()
{
    if (m_autoResetTimer) {
        m_autoResetTimer->start(60000);
    }
}

void AILIController::stopAutoResetTimer()
{
    if (m_autoResetTimer) {
        m_autoResetTimer->stop();
    }
}

void AILIController::setJobActive(bool active)
{
    if (m_jobActive == active) {
        return;
    }

    m_jobActive = active;

    if (m_jobActive) {
        emit jobOpened();
    } else {
        emit jobClosed();
    }
}

void AILIController::setOriginalFilePath(const QString &path)
{
    m_originalFilePath = path;
}

void AILIController::setDetectedVersion(const QString &version)
{
    m_detectedVersion = version;
    m_ui->versionBoxAILI->setText(version);
}

void AILIController::resetJob()
{
    stopAutoResetTimer();

    m_ui->jobNumberBoxAILI->clear();
    m_ui->issueNumberBoxAILI->clear();
    m_ui->countBoxAILI->clear();
    m_ui->postageBoxAILI->clear();
    m_ui->versionBoxAILI->clear();
    m_ui->yearDDboxAILI->setCurrentIndex(0);
    m_ui->monthDDboxAILI->setCurrentIndex(0);
    m_ui->pageCountddBoxAILI->setCurrentIndex(0);

    m_initialLocked = false;
    m_postageLocked = false;
    setJobActive(false);
    m_pendingAction = NoPendingAction;
    m_originalFilePath.clear();
    m_detectedVersion.clear();

    lockJobMetadataFields(false);
    lockPostageFields(false);
    updateButtonStates();
    loadDefaultHtml();
}

bool AILIController::hasActiveJob() const
{
    return m_jobActive;
}
