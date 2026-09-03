#include "tmmacontroller.h"

#include "databasemanager.h"
#include "dropbindinghelper.h"
#include "dropwindow.h"
#include "mailclasspermitbindinghelper.h"
#include "meterrateservice.h"
#include "monthcomboboxhelper.h"
#include "scriptrunner.h"
#include "scriptrunnerbindinghelper.h"
#include "terminaloutputhelper.h"
#include "tmmadbmanager.h"
#include "tmmaemaildialog.h"
#include "tmmafilemanager.h"
#include "yearcomboboxhelper.h"

#include <QAbstractItemView>
#include <QAction>
#include <QComboBox>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>
#include <QSqlDatabase>
#include <QSqlTableModel>
#include <QTextBrowser>
#include <QTextEdit>
#include <QToolButton>

#include <cmath>
#include <limits>

namespace {
class TMMAFormattedSqlModel final : public QSqlTableModel
{
public:
    TMMAFormattedSqlModel(QObject* parent, QSqlDatabase database, TMMAController* controller)
        : QSqlTableModel(parent, database)
        , m_controller(controller)
    {
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        const QVariant value = QSqlTableModel::data(index, role);
        if (role == Qt::DisplayRole && m_controller) {
            return m_controller->formatCellData(index.column(), value.toString());
        }
        return value;
    }

private:
    TMMAController* m_controller;
};

TerminalSeverity terminalSeverity(BaseTrackerController::MessageType type)
{
    switch (type) {
    case BaseTrackerController::Success:
        return TerminalSeverity::Success;
    case BaseTrackerController::Warning:
        return TerminalSeverity::Warning;
    case BaseTrackerController::Error:
        return TerminalSeverity::Error;
    case BaseTrackerController::Info:
    default:
        return TerminalSeverity::Info;
    }
}

QString withoutCurrencyFormatting(QString value)
{
    value.remove(QLatin1Char('$'));
    value.remove(QLatin1Char(','));
    value.remove(QLatin1Char(' '));
    return value;
}

QString trackerClassDisplay(const QString& mailClass)
{
    if (mailClass == QStringLiteral("STANDARD")
        || mailClass == QStringLiteral("FIRST CLASS")) {
        return QStringLiteral("STD");
    }
    return mailClass;
}
} // namespace

TMMAController::TMMAController(QObject* parent)
    : BaseTrackerController(parent)
    , m_fileManager(new TMMAFileManager(nullptr))
    , m_dbManager(TMMADBManager::instance())
    , m_scriptRunner(new ScriptRunner(this))
    , m_meterRateService(new MeterRateService(DatabaseManager::instance(), this))
    , m_jobNumberBox(nullptr)
    , m_yearDropdown(nullptr)
    , m_monthDropdown(nullptr)
    , m_lockButton(nullptr)
    , m_editButton(nullptr)
    , m_postageLockButton(nullptr)
    , m_dropWindow(nullptr)
    , m_runInitialButton(nullptr)
    , m_openBulkMailerButton(nullptr)
    , m_classDropdown(nullptr)
    , m_permitDropdown(nullptr)
    , m_postageBox(nullptr)
    , m_countBox(nullptr)
    , m_tracker(nullptr)
    , m_terminalWindow(nullptr)
    , m_textBrowser(nullptr)
    , m_finalStepButton(nullptr)
    , m_trackerModel(nullptr)
    , m_uiInitialized(false)
    , m_jobDataLocked(false)
    , m_postageDataLocked(false)
    , m_scriptRunning(false)
    , m_meterRateErrorShown(false)
    , m_identityRecoveryBlocked(false)
    , m_htmlDisplayState(DefaultState)
{
    m_scriptRunner->setInputWrapperEnabled(false);
    ScriptRunnerBindingHelper::setupBaselineBindings(
        m_scriptRunner,
        this,
        [this](const QString& output) { onScriptOutput(output); },
        [this](int exitCode, QProcess::ExitStatus exitStatus) {
            onScriptFinished(exitCode, exitStatus);
        });
}

TMMAController::~TMMAController()
{
    delete m_fileManager;
    m_fileManager = nullptr;
}

void TMMAController::initializeUI(QLineEdit* jobNumberBox,
                                  QComboBox* yearDropdown,
                                  QComboBox* monthDropdown,
                                  QToolButton* lockButton,
                                  QToolButton* editButton,
                                  QToolButton* postageLockButton,
                                  DropWindow* dropWindow,
                                  QPushButton* runInitialButton,
                                  QPushButton* openBulkMailerButton,
                                  QComboBox* classDropdown,
                                  QComboBox* permitDropdown,
                                  QLineEdit* postageBox,
                                  QLineEdit* countBox,
                                  QTableView* tracker,
                                  QTextEdit* terminalWindow,
                                  QTextBrowser* textBrowser,
                                  QPushButton* finalStepButton)
{
    if (m_uiInitialized) {
        return;
    }

    m_jobNumberBox = jobNumberBox;
    m_yearDropdown = yearDropdown;
    m_monthDropdown = monthDropdown;
    m_lockButton = lockButton;
    m_editButton = editButton;
    m_postageLockButton = postageLockButton;
    m_dropWindow = dropWindow;
    m_runInitialButton = runInitialButton;
    m_openBulkMailerButton = openBulkMailerButton;
    m_classDropdown = classDropdown;
    m_permitDropdown = permitDropdown;
    m_postageBox = postageBox;
    m_countBox = countBox;
    m_tracker = tracker;
    m_terminalWindow = terminalWindow;
    m_textBrowser = textBrowser;
    m_finalStepButton = finalStepButton;

    if (m_yearDropdown) {
        YearComboBoxHelper::populateWithBlankAndAdjacentYears(m_yearDropdown);
    }
    if (m_monthDropdown) {
        MonthComboBoxHelper::populateWithBlankAndMonths(m_monthDropdown);
    }
    if (m_jobNumberBox) {
        m_jobNumberBox->setValidator(new QRegularExpressionValidator(
            QRegularExpression(QStringLiteral("\\d{0,5}")), this));
    }

    MailClassPermitBindingHelper::bind(m_classDropdown, m_permitDropdown, this);

    if (m_lockButton) {
        connect(m_lockButton, &QToolButton::clicked, this, &TMMAController::onJobDataLockClicked);
    }
    if (m_editButton) {
        connect(m_editButton, &QToolButton::clicked, this, &TMMAController::onEditButtonClicked);
    }
    if (m_postageLockButton) {
        connect(m_postageLockButton, &QToolButton::clicked, this, &TMMAController::onPostageLockClicked);
    }
    if (m_runInitialButton) {
        connect(m_runInitialButton, &QPushButton::clicked, this, &TMMAController::onRunInitialClicked);
    }
    if (m_openBulkMailerButton) {
        connect(m_openBulkMailerButton, &QPushButton::clicked, this, &TMMAController::onOpenBulkMailerClicked);
    }
    if (m_finalStepButton) {
        connect(m_finalStepButton, &QPushButton::clicked, this, &TMMAController::onFinalStepClicked);
    }
    if (m_countBox) {
        connect(m_countBox, &QLineEdit::textChanged, this, &TMMAController::calculateMeteredPostage);
    }
    if (m_classDropdown) {
        connect(m_classDropdown, &QComboBox::currentTextChanged, this, [this]() {
            calculateMeteredPostage();
            if (m_jobDataLocked) {
                saveJobState();
            }
        });
    }
    if (m_permitDropdown) {
        connect(m_permitDropdown, &QComboBox::currentTextChanged, this, [this]() {
            calculateMeteredPostage();
            if (m_jobDataLocked) {
                saveJobState();
            }
        });
    }
    if (m_countBox) {
        connect(m_countBox, &QLineEdit::editingFinished, this, [this]() {
            qint64 count = 0;
            if (parsePositiveCount(count)) {
                const QSignalBlocker blocker(m_countBox);
                m_countBox->setText(QStringLiteral("%L1").arg(count));
            }
            if (m_jobDataLocked) {
                saveJobState();
            }
        });
    }
    if (m_postageBox) {
        connect(m_postageBox, &QLineEdit::editingFinished, this, [this]() {
            double postage = 0.0;
            if (parsePostage(postage)) {
                const QSignalBlocker blocker(m_postageBox);
                m_postageBox->setText(QStringLiteral("$%1").arg(postage, 0, 'f', 2));
            }
            if (m_jobDataLocked) {
                saveJobState();
            }
        });
    }

    setupDropWindow();
    setupTrackerModel();
    m_uiInitialized = true;

    QString directoryError;
    if (!m_fileManager->createBaseDirectories(&directoryError)) {
        outputToTerminal(directoryError, Error);
    }

    recoverPendingIdentityMigration();

    updateControlStates();
    updateHtmlDisplay();
}

QString TMMAController::getJobNumber() const
{
    return m_jobNumberBox ? m_jobNumberBox->text().trimmed() : QString();
}

QString TMMAController::getYear() const
{
    return m_yearDropdown ? m_yearDropdown->currentText().trimmed() : QString();
}

QString TMMAController::getMonth() const
{
    return m_monthDropdown ? m_monthDropdown->currentText().trimmed() : QString();
}

bool TMMAController::isJobDataLocked() const
{
    return m_jobDataLocked;
}

bool TMMAController::isPostageDataLocked() const
{
    return m_postageDataLocked;
}

bool TMMAController::hasActiveJob() const
{
    return hasActiveIdentity();
}

bool TMMAController::validateJobData() const
{
    const QString jobNumber = getJobNumber();
    const QString year = getYear();
    const QString month = getMonth();
    return QRegularExpression(QStringLiteral("^\\d{5}$")).match(jobNumber).hasMatch()
        && QRegularExpression(QStringLiteral("^\\d{4}$")).match(year).hasMatch()
        && TMMAFileManager::monthAbbreviation(month).size() == 3;
}

bool TMMAController::hasActiveIdentity() const
{
    return !m_activeJobNumber.isEmpty() && !m_activeYear.isEmpty() && !m_activeMonth.isEmpty();
}

TMMAJobState TMMAController::currentJobState(bool jobDataLocked) const
{
    TMMAJobState state;
    state.htmlDisplayState = static_cast<int>(jobDataLocked ? InstructionsState : DefaultState);
    state.jobDataLocked = jobDataLocked;
    state.postageDataLocked = m_postageDataLocked;
    state.postage = m_postageBox ? m_postageBox->text() : QString();
    state.count = m_countBox ? m_countBox->text() : QString();
    state.mailClass = m_classDropdown ? m_classDropdown->currentText() : QString();
    state.permit = m_permitDropdown
        ? MailClassPermitBindingHelper::normalizePermitForUi(m_permitDropdown->currentText())
        : QString();
    state.lastExecutedScript = m_lastExecutedScript;
    return state;
}

void TMMAController::blockForIdentityRecovery(const QString& errorMessage)
{
    m_identityRecoveryBlocked = true;
    const QString markerDetail = m_fileManager && m_fileManager->identityMigrationMarkerExists()
        ? QStringLiteral(" Recovery marker: %1")
              .arg(QDir::toNativeSeparators(m_fileManager->identityMigrationMarkerPath()))
        : QString();
    outputToTerminal(
        QStringLiteral("TMMA IDENTITY RECOVERY REQUIRED. Lock, Close, and processing are blocked. %1%2")
            .arg(errorMessage, markerDetail),
        Error);
    updateControlStates();
}

bool TMMAController::recoverPendingIdentityMigration(bool* recoveredNewIdentity)
{
    if (recoveredNewIdentity) {
        *recoveredNewIdentity = false;
    }
    if (!m_fileManager || !m_fileManager->identityMigrationMarkerExists()) {
        return true;
    }

    TMMAIdentityMigrationRecord record;
    QString recoveryError;
    if (!m_fileManager->readIdentityMigrationMarker(record, &recoveryError)) {
        blockForIdentityRecovery(recoveryError);
        return false;
    }

    bool oldJobExists = false;
    bool newJobExists = false;
    bool oldLogExists = false;
    bool newLogExists = false;
    if (!m_dbManager
        || !m_dbManager->jobIdentityExists(record.oldJobNumber, record.oldYear, record.oldMonth, oldJobExists)
        || !m_dbManager->jobIdentityExists(record.newJobNumber, record.newYear, record.newMonth, newJobExists)
        || !m_dbManager->logIdentityExists(record.oldJobNumber, record.oldYear, record.oldMonth, oldLogExists)
        || !m_dbManager->logIdentityExists(record.newJobNumber, record.newYear, record.newMonth, newLogExists)) {
        blockForIdentityRecovery(m_dbManager
                                     ? m_dbManager->lastError()
                                     : QStringLiteral("TMMA database manager is unavailable."));
        return false;
    }

    TMMAIdentityRecoveryOutcome outcome = TMMAIdentityRecoveryOutcome::Blocked;
    if (!m_fileManager->recoverIdentityMigration(
            record, oldJobExists, newJobExists, oldLogExists, newLogExists,
            outcome, &recoveryError)) {
        blockForIdentityRecovery(
            QStringLiteral("Old: %1 / %2 / %3 (%4). New: %5 / %6 / %7 (%8). %9")
                .arg(record.oldJobNumber, record.oldYear, record.oldMonth,
                     QDir::toNativeSeparators(record.oldArchivePath),
                     record.newJobNumber, record.newYear, record.newMonth,
                     QDir::toNativeSeparators(record.newArchivePath), recoveryError));
        return false;
    }

    m_identityRecoveryBlocked = false;
    const bool newIdentityWon = outcome == TMMAIdentityRecoveryOutcome::NewIdentity;
    if (recoveredNewIdentity) {
        *recoveredNewIdentity = newIdentityWon;
    }
    if (!recoveredNewIdentity) {
        outputToTerminal(
            newIdentityWon
                ? QStringLiteral("Recovered a completed TMMA identity migration to %1 / %2 / %3.")
                      .arg(record.newJobNumber, record.newYear, record.newMonth)
                : QStringLiteral("Recovered the original TMMA identity %1 / %2 / %3; the incomplete migration was rolled back.")
                      .arg(record.oldJobNumber, record.oldYear, record.oldMonth),
            Warning);
    }
    return true;
}

void TMMAController::finishSuccessfulLock(const QString& jobNumber,
                                          const QString& year,
                                          const QString& month,
                                          bool identityChanged)
{
    m_activeJobNumber = jobNumber;
    m_activeYear = year;
    m_activeMonth = month;
    m_jobDataLocked = true;
    m_htmlDisplayState = InstructionsState;
    if (m_editButton) {
        m_editButton->setChecked(false);
    }
    updateControlStates();
    updateHtmlDisplay();
    refreshTracker();
    outputToTerminal(
        identityChanged
            ? QStringLiteral("TMMA identity migrated and job data locked for %1 / %2 / %3.")
                  .arg(jobNumber, year, month)
            : QStringLiteral("TMMA job data locked for %1 / %2 / %3.")
                  .arg(jobNumber, year, month),
        Success);
    emit jobOpened();
}

bool TMMAController::parsePositiveCount(qint64& count) const
{
    if (!m_countBox) {
        return false;
    }
    QString value = m_countBox->text();
    value.remove(QLatin1Char(','));
    value.remove(QLatin1Char(' '));
    bool ok = false;
    count = value.toLongLong(&ok);
    return ok && count > 0;
}

bool TMMAController::parsePostage(double& postage) const
{
    if (!m_postageBox) {
        return false;
    }
    bool ok = false;
    postage = withoutCurrencyFormatting(m_postageBox->text()).toDouble(&ok);
    return ok && std::isfinite(postage) && postage >= 0.0;
}

bool TMMAController::validatePostageData() const
{
    qint64 count = 0;
    double postage = 0.0;
    return parsePositiveCount(count)
        && parsePostage(postage)
        && m_classDropdown && !m_classDropdown->currentText().trimmed().isEmpty()
        && m_permitDropdown && !m_permitDropdown->currentText().trimmed().isEmpty();
}

void TMMAController::onJobDataLockClicked()
{
    if (m_identityRecoveryBlocked) {
        if (m_lockButton) {
            m_lockButton->setChecked(m_jobDataLocked);
        }
        outputToTerminal(QStringLiteral("Cannot lock TMMA job while identity recovery is required."), Error);
        return;
    }
    if (!m_lockButton || !m_lockButton->isChecked()) {
        if (m_lockButton) {
            m_lockButton->setChecked(m_jobDataLocked);
        }
        return;
    }

    if (!validateJobData()) {
        m_lockButton->setChecked(false);
        outputToTerminal(QStringLiteral("Cannot lock TMMA job: enter a five-digit job number, a year, and a month."), Error);
        return;
    }

    const QString jobNumber = getJobNumber();
    const QString year = getYear();
    const QString month = getMonth();
    const TMMAJobState lockedState = currentJobState(true);

    if (!hasActiveIdentity()) {
        bool jobExists = false;
        bool logExists = false;
        if (!m_dbManager->jobIdentityExists(jobNumber, year, month, jobExists)
            || !m_dbManager->logIdentityExists(jobNumber, year, month, logExists)) {
            m_lockButton->setChecked(false);
            outputToTerminal(m_dbManager->lastError(), Error);
            updateControlStates();
            return;
        }
        if (jobExists) {
            m_lockButton->setChecked(false);
            outputToTerminal(
                QStringLiteral("TMMA job %1 / %2 / %3 already exists. Open it using File > Open Job. No data was changed.")
                    .arg(jobNumber, year, month),
                Error);
            updateControlStates();
            return;
        }
        if (logExists) {
            m_lockButton->setChecked(false);
            outputToTerminal(
                QStringLiteral("TMMA tracker identity %1 / %2 / %3 already exists without an available job. No data was changed.")
                    .arg(jobNumber, year, month),
                Error);
            updateControlStates();
            return;
        }

        QString archiveError;
        if (!m_fileManager->createNewArchiveSkeleton(jobNumber, year, month, &archiveError)) {
            m_lockButton->setChecked(false);
            outputToTerminal(QStringLiteral("Cannot establish TMMA job: %1").arg(archiveError), Error);
            updateControlStates();
            return;
        }
        if (!m_dbManager->createJobWithState(jobNumber, year, month, lockedState)) {
            const QString databaseError = m_dbManager->lastError();
            bool unexpectedlyExists = false;
            const bool lookupSucceeded = m_dbManager->jobIdentityExists(
                jobNumber, year, month, unexpectedlyExists);
            if (!lookupSucceeded || unexpectedlyExists) {
                blockForIdentityRecovery(
                    QStringLiteral("First Lock could not confirm a clean database rollback for %1 / %2 / %3. %4")
                        .arg(jobNumber, year, month, databaseError));
                return;
            }
            QString cleanupError;
            if (!m_fileManager->removeArchiveJobDirectoryIfEmpty(
                    jobNumber, year, month, &cleanupError)) {
                blockForIdentityRecovery(
                    QStringLiteral("First Lock failed and its archive skeleton could not be removed. %1 Database error: %2")
                        .arg(cleanupError, databaseError));
                return;
            }
            m_lockButton->setChecked(false);
            outputToTerminal(QStringLiteral("Cannot establish TMMA job: %1 No data was changed.")
                                 .arg(databaseError), Error);
            updateControlStates();
            return;
        }
        finishSuccessfulLock(jobNumber, year, month, false);
        return;
    }

    const bool identityChanged = jobNumber != m_activeJobNumber
        || year != m_activeYear || month != m_activeMonth;
    if (!identityChanged) {
        if (!m_dbManager->saveJobState(
                m_activeJobNumber, m_activeYear, m_activeMonth, lockedState)) {
            m_lockButton->setChecked(false);
            outputToTerminal(m_dbManager->lastError(), Error);
            updateControlStates();
            updateHtmlDisplay();
            return;
        }
        finishSuccessfulLock(jobNumber, year, month, false);
        return;
    }

    bool oldJobExists = false;
    bool newJobExists = false;
    bool newLogExists = false;
    if (!m_dbManager->jobIdentityExists(
            m_activeJobNumber, m_activeYear, m_activeMonth, oldJobExists)
        || !m_dbManager->jobIdentityExists(jobNumber, year, month, newJobExists)
        || !m_dbManager->logIdentityExists(jobNumber, year, month, newLogExists)) {
        m_lockButton->setChecked(false);
        outputToTerminal(m_dbManager->lastError(), Error);
        updateControlStates();
        return;
    }
    if (!oldJobExists) {
        m_lockButton->setChecked(false);
        outputToTerminal(
            QStringLiteral("Cannot migrate TMMA identity because the committed old job %1 / %2 / %3 is missing. No data was changed.")
                .arg(m_activeJobNumber, m_activeYear, m_activeMonth),
            Error);
        updateControlStates();
        return;
    }
    if (newJobExists || newLogExists) {
        m_lockButton->setChecked(false);
        outputToTerminal(
            (newJobExists
                 ? QStringLiteral("TMMA job %1 / %2 / %3 already exists. No data was changed.")
                 : QStringLiteral("TMMA tracker identity %1 / %2 / %3 already exists. No data was changed."))
                .arg(jobNumber, year, month),
            Error);
        updateControlStates();
        return;
    }

    TMMAIdentityMigrationRecord record;
    record.oldJobNumber = m_activeJobNumber;
    record.oldYear = m_activeYear;
    record.oldMonth = m_activeMonth;
    record.newJobNumber = jobNumber;
    record.newYear = year;
    record.newMonth = month;
    record.oldArchivePath = m_fileManager->getArchiveJobPath(
        record.oldJobNumber, record.oldYear, record.oldMonth);
    record.newArchivePath = m_fileManager->getArchiveJobPath(
        record.newJobNumber, record.newYear, record.newMonth);

    QString migrationError;
    if (!m_fileManager->preflightIdentityMigration(record, &migrationError)) {
        m_lockButton->setChecked(false);
        outputToTerminal(migrationError, Error);
        updateControlStates();
        return;
    }
    if (!m_fileManager->writeIdentityMigrationMarker(record, &migrationError)) {
        m_lockButton->setChecked(false);
        outputToTerminal(QStringLiteral("Cannot begin TMMA identity migration: %1 No data was changed.")
                             .arg(migrationError), Error);
        updateControlStates();
        return;
    }
    if (!m_fileManager->migrateArchiveIdentity(record, &migrationError)) {
        QString markerError;
        if (!m_fileManager->clearIdentityMigrationMarker(&markerError)) {
            blockForIdentityRecovery(QStringLiteral("%1 %2").arg(migrationError, markerError));
            return;
        }
        m_lockButton->setChecked(false);
        outputToTerminal(migrationError, Error);
        updateControlStates();
        return;
    }

    const bool databaseMigrated = m_dbManager->migrateJobIdentity(
        record.oldJobNumber, record.oldYear, record.oldMonth,
        record.newJobNumber, record.newYear, record.newMonth, lockedState);
    const QString databaseError = m_dbManager->lastError();
    bool recoveredNewIdentity = false;
    if (!recoverPendingIdentityMigration(&recoveredNewIdentity)) {
        return;
    }
    if (!databaseMigrated && !recoveredNewIdentity) {
        m_lockButton->setChecked(false);
        outputToTerminal(
            QStringLiteral("TMMA identity migration failed and was rolled back to %1 / %2 / %3. %4 No committed identity change occurred.")
                .arg(record.oldJobNumber, record.oldYear, record.oldMonth, databaseError),
            Error);
        updateControlStates();
        updateHtmlDisplay();
        return;
    }
    if (!recoveredNewIdentity) {
        blockForIdentityRecovery(QStringLiteral("TMMA database migration reported success, but recovery verification did not confirm the new identity."));
        return;
    }
    finishSuccessfulLock(jobNumber, year, month, true);
}

void TMMAController::onEditButtonClicked()
{
    if (m_identityRecoveryBlocked) {
        if (m_editButton) {
            m_editButton->setChecked(false);
        }
        outputToTerminal(QStringLiteral("Cannot edit TMMA job while identity recovery is required."), Error);
        return;
    }
    if (!m_jobDataLocked) {
        if (m_editButton) {
            m_editButton->setChecked(false);
        }
        outputToTerminal(QStringLiteral("Cannot edit TMMA job data until it is locked."), Error);
        return;
    }

    m_jobDataLocked = false;
    m_htmlDisplayState = DefaultState;
    if (m_lockButton) {
        m_lockButton->setChecked(false);
    }
    updateControlStates();
    updateHtmlDisplay();
    if (!saveJobState()) {
        m_jobDataLocked = true;
        m_htmlDisplayState = InstructionsState;
        if (m_editButton) m_editButton->setChecked(false);
        if (m_lockButton) m_lockButton->setChecked(true);
        updateControlStates();
        updateHtmlDisplay();
        outputToTerminal(QStringLiteral("TMMA job remained locked because Edit state could not be persisted."), Error);
        return;
    }
    outputToTerminal(QStringLiteral("TMMA job data unlocked for editing; its committed identity remains unchanged until re-lock."), Info);
}

void TMMAController::onPostageLockClicked()
{
    if (m_identityRecoveryBlocked) {
        if (m_postageLockButton) {
            m_postageLockButton->setChecked(m_postageDataLocked);
        }
        outputToTerminal(QStringLiteral("Cannot change TMMA postage lock while identity recovery is required."), Error);
        return;
    }
    if (!m_jobDataLocked) {
        if (m_postageLockButton) {
            m_postageLockButton->setChecked(m_postageDataLocked);
        }
        outputToTerminal(QStringLiteral("Cannot lock TMMA postage: lock the job data first."), Error);
        return;
    }

    if (m_postageLockButton && m_postageLockButton->isChecked()) {
        calculateMeteredPostage();
        if (!validatePostageData()) {
            m_postageLockButton->setChecked(false);
            outputToTerminal(QStringLiteral("Cannot lock TMMA postage: enter a positive count, valid postage, class, and permit."), Error);
            return;
        }
        m_postageDataLocked = true;
        if (!addOrUpdateLogEntry() || !saveJobState()) {
            m_postageDataLocked = false;
            m_postageLockButton->setChecked(false);
            updateControlStates();
            return;
        }
        outputToTerminal(QStringLiteral("TMMA postage data locked and tracker row saved."), Success);
    } else {
        m_postageDataLocked = false;
        saveJobState();
        outputToTerminal(QStringLiteral("TMMA postage data unlocked."), Info);
    }
    updateControlStates();
}

void TMMAController::calculateMeteredPostage()
{
    if (!m_classDropdown || !m_permitDropdown || !m_countBox || !m_postageBox) {
        return;
    }

    const QString permit = MailClassPermitBindingHelper::normalizePermitForUi(
        m_permitDropdown->currentText());
    if (m_classDropdown->currentText() != QStringLiteral("FIRST CLASS")
        || permit != QStringLiteral("METERED")) {
        m_meterRateErrorShown = false;
        return;
    }

    qint64 count = 0;
    if (!parsePositiveCount(count)) {
        return;
    }

    const double noFallback = std::numeric_limits<double>::quiet_NaN();
    const double meterRate = m_meterRateService->getCurrentMeterRate(noFallback);
    if (!std::isfinite(meterRate) || meterRate <= 0.0) {
        if (!m_meterRateErrorShown) {
            outputToTerminal(
                QStringLiteral("Cannot calculate TMMA METERED postage: the stored current METERED rate is unavailable or invalid."),
                Error);
            m_meterRateErrorShown = true;
        }
        return;
    }

    m_meterRateErrorShown = false;
    const QSignalBlocker blocker(m_postageBox);
    m_postageBox->setText(QStringLiteral("$%1").arg(meterRate * static_cast<double>(count), 0, 'f', 2));
}

bool TMMAController::saveJobState()
{
    if (m_identityRecoveryBlocked || !hasActiveIdentity() || !m_dbManager) {
        return false;
    }

    const TMMAJobState state = currentJobState(m_jobDataLocked);

    if (!m_dbManager->saveJobState(
            m_activeJobNumber, m_activeYear, m_activeMonth, state)) {
        outputToTerminal(m_dbManager->lastError(), Error);
        return false;
    }
    return true;
}

bool TMMAController::loadJob(const QString& jobNumber,
                             const QString& year,
                             const QString& month)
{
    if (m_fileManager->identityMigrationMarkerExists()
        && !recoverPendingIdentityMigration()) {
        outputToTerminal(QStringLiteral("Cannot open a TMMA job while identity recovery is required."), Error);
        return false;
    }
    if (m_identityRecoveryBlocked) {
        outputToTerminal(QStringLiteral("Cannot open a TMMA job while identity recovery is required."), Error);
        return false;
    }
    if (!m_dbManager || !m_dbManager->jobExists(jobNumber, year, month)) {
        outputToTerminal(m_dbManager ? m_dbManager->lastError()
                                     : QStringLiteral("TMMA database manager is unavailable."), Error);
        return false;
    }

    TMMAJobState state;
    if (!m_dbManager->loadJobState(jobNumber, year, month, state)) {
        outputToTerminal(m_dbManager->lastError(), Error);
        return false;
    }

    QString restoreError;
    if (!m_fileManager->copyArchiveToWorking(jobNumber, year, month, &restoreError)) {
        outputToTerminal(restoreError, Error);
        return false;
    }

    m_activeJobNumber = jobNumber;
    m_activeYear = year;
    m_activeMonth = month;
    m_jobDataLocked = state.jobDataLocked;
    m_postageDataLocked = state.postageDataLocked;
    m_htmlDisplayState = m_jobDataLocked ? InstructionsState : DefaultState;
    m_lastExecutedScript = state.lastExecutedScript;

    const QSignalBlocker jobBlocker(m_jobNumberBox);
    const QSignalBlocker yearBlocker(m_yearDropdown);
    const QSignalBlocker monthBlocker(m_monthDropdown);
    const QSignalBlocker postageBlocker(m_postageBox);
    const QSignalBlocker countBlocker(m_countBox);
    const QSignalBlocker classBlocker(m_classDropdown);
    const QSignalBlocker permitBlocker(m_permitDropdown);
    m_jobNumberBox->setText(jobNumber);
    m_yearDropdown->setCurrentText(year);
    m_monthDropdown->setCurrentText(month);
    m_postageBox->setText(state.postage);
    m_countBox->setText(state.count);
    m_classDropdown->setCurrentText(state.mailClass);
    m_permitDropdown->setCurrentText(
        MailClassPermitBindingHelper::normalizePermitForUi(state.permit));
    if (m_lockButton) {
        m_lockButton->setChecked(m_jobDataLocked);
    }
    if (m_editButton) {
        m_editButton->setChecked(!m_jobDataLocked);
    }
    if (m_postageLockButton) {
        m_postageLockButton->setChecked(m_postageDataLocked);
    }

    updateControlStates();
    updateHtmlDisplay();
    refreshTracker();
    outputToTerminal(QStringLiteral("TMMA job %1 / %2 / %3 reopened; archive files were copied to working directories.")
                         .arg(jobNumber, year, month), Success);
    emit jobOpened();
    return true;
}

bool TMMAController::addOrUpdateLogEntry()
{
    qint64 count = 0;
    double postage = 0.0;
    if (!parsePositiveCount(count) || !parsePostage(postage)) {
        return false;
    }

    const QString formattedPostage = QStringLiteral("$%1").arg(postage, 0, 'f', 2);
    const QString formattedCount = QString::number(count);
    const QString perPiece = QString::number(postage / static_cast<double>(count), 'f', 3);
    const QString mailClass = m_classDropdown->currentText();
    const QString trackerClass = trackerClassDisplay(mailClass);
    const QString permit = MailClassPermitBindingHelper::normalizePermitForUi(
        m_permitDropdown->currentText());

    if (!m_dbManager->upsertLogEntry(
            m_activeJobNumber,
            m_activeYear,
            m_activeMonth,
            QStringLiteral("TM MA"),
            formattedPostage,
            formattedCount,
            perPiece,
            trackerClass,
            QStringLiteral("LTR"),
            permit,
            QDate::currentDate().toString(QStringLiteral("MM/dd/yyyy")))) {
        outputToTerminal(m_dbManager->lastError(), Error);
        return false;
    }

    refreshTracker();
    return true;
}

void TMMAController::onRunInitialClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal(QStringLiteral("Cannot run TMMA INITIAL: lock the job data first."), Error);
        return;
    }
    startScript(QStringLiteral("01 INITIAL"));
}

void TMMAController::onFinalStepClicked()
{
    if (!m_jobDataLocked || !m_postageDataLocked || !hasActiveIdentity()) {
        outputToTerminal(QStringLiteral("Cannot run TMMA FINAL: active job and postage data must be locked."), Error);
        return;
    }
    qint64 count = 0;
    if (!parsePositiveCount(count)) {
        outputToTerminal(QStringLiteral("Cannot run TMMA FINAL: count must be a positive integer."), Error);
        return;
    }
    startScript(QStringLiteral("02 FINAL PROCESS"),
                {m_activeJobNumber, QString::number(count)});
}

bool TMMAController::startScript(const QString& scriptName, const QStringList& arguments)
{
    if (m_identityRecoveryBlocked) {
        outputToTerminal(QStringLiteral("Cannot run TMMA processing while identity recovery is required."), Error);
        return false;
    }
    if (m_scriptRunning) {
        outputToTerminal(QStringLiteral("A TMMA script is already running."), Warning);
        return false;
    }

    const QString scriptPath = m_fileManager->getScriptPath(scriptName);
    if (!QFileInfo::exists(scriptPath)) {
        outputToTerminal(QStringLiteral("TMMA script not found: %1").arg(QDir::toNativeSeparators(scriptPath)), Error);
        return false;
    }

    m_activeScript = scriptName;
    m_lastExecutedScript = scriptName;
    m_finalOutputFilePath.clear();
    m_finalMergedFilePath.clear();
    m_scriptRunning = true;
    updateControlStates();
    saveJobState();
    outputToTerminal(QStringLiteral("Executing TMMA script: %1").arg(scriptName), Info);

    if (!m_scriptRunner->runScript(scriptPath, arguments)) {
        m_scriptRunning = false;
        updateControlStates();
        outputToTerminal(QStringLiteral("Failed to start TMMA script: %1").arg(QDir::toNativeSeparators(scriptPath)), Error);
        return false;
    }
    return true;
}

void TMMAController::onScriptOutput(const QString& output)
{
    const QString line = output.trimmed();
    if (line.startsWith(QStringLiteral("TMMA_FINAL_OUTPUT_FILE="))) {
        m_finalOutputFilePath = QDir::fromNativeSeparators(
            line.mid(QStringLiteral("TMMA_FINAL_OUTPUT_FILE=").size()));
        return;
    }
    if (line.startsWith(QStringLiteral("TMMA_FINAL_MERGED_FILE="))) {
        m_finalMergedFilePath = QDir::fromNativeSeparators(
            line.mid(QStringLiteral("TMMA_FINAL_MERGED_FILE=").size()));
        return;
    }
    if (line.startsWith(QStringLiteral("ERROR:"), Qt::CaseInsensitive)) {
        outputToTerminal(line, Error);
    } else if (line.startsWith(QStringLiteral("WARNING:"), Qt::CaseInsensitive)) {
        outputToTerminal(line, Warning);
    } else if (line.startsWith(QStringLiteral("SUCCESS:"), Qt::CaseInsensitive)) {
        outputToTerminal(line, Success);
    } else {
        outputToTerminal(line, Info);
    }
}

void TMMAController::onScriptFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    const QString completedScript = m_activeScript;
    m_activeScript.clear();
    m_scriptRunning = false;
    updateControlStates();

    if (exitStatus == QProcess::CrashExit) {
        outputToTerminal(QStringLiteral("TMMA script crashed unexpectedly."), Error);
        return;
    }
    if (exitCode != 0) {
        outputToTerminal(QStringLiteral("TMMA script failed with exit code %1.").arg(exitCode), Error);
        return;
    }

    outputToTerminal(QStringLiteral("TMMA script completed successfully."), Success);
    saveJobState();
    if (completedScript == QStringLiteral("02 FINAL PROCESS")) {
        const QFileInfo outputFileInfo(m_finalOutputFilePath);
        const QFileInfo mergedFileInfo(m_finalMergedFilePath);
        if (m_finalOutputFilePath.isEmpty()
            || m_finalMergedFilePath.isEmpty()
            || m_finalOutputFilePath == m_finalMergedFilePath
            || !outputFileInfo.isFile()
            || !mergedFileInfo.isFile()) {
            outputToTerminal(
                QStringLiteral("TMMA FINAL completed without reporting both valid attachment files."),
                Error);
            return;
        }
        showFinalEmailDialog(m_finalOutputFilePath, m_finalMergedFilePath);
    }
}

void TMMAController::onOpenBulkMailerClicked()
{
    if (!m_jobDataLocked) {
        outputToTerminal(QStringLiteral("Please lock TMMA job data before opening Bulk Mailer."), Warning);
        return;
    }

    const QStringList candidates = {
        QStringLiteral("C:/Program Files (x86)/BCC Software/Bulk Mailer/BulkMailer.exe"),
        QStringLiteral("C:/Program Files (x86)/Satori Software/Bulk Mailer/BulkMailer.exe")
    };
    QString executable;
    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            executable = candidate;
            break;
        }
    }
    if (executable.isEmpty()) {
        outputToTerminal(
            QStringLiteral("Bulk Mailer was not found in the established BCC or Satori installation locations."),
            Error);
        return;
    }
    if (!QProcess::startDetached(executable, QStringList())) {
        outputToTerminal(QStringLiteral("Bulk Mailer was found but could not be launched: %1")
                             .arg(QDir::toNativeSeparators(executable)), Error);
        return;
    }
    outputToTerminal(QStringLiteral("Bulk Mailer launched successfully."), Success);
}

void TMMAController::setupDropWindow()
{
    if (!m_dropWindow) {
        return;
    }
    DropBindingHelper::setupDropWindow(
        m_dropWindow,
        m_fileManager->getRawInputPath(),
        {QStringLiteral("xlsx"), QStringLiteral("xls"), QStringLiteral("csv")},
        this,
        [this](const QStringList& files) { onFilesDropped(files); },
        [this](const QString& error) { onFileDropError(error); });
}

void TMMAController::onFilesDropped(const QStringList& filePaths)
{
    outputToTerminal(QStringLiteral("Copied %1 TMMA source file(s) into RAW INPUT. INITIAL requires exactly one active file.")
                         .arg(filePaths.size()), Info);
}

void TMMAController::onFileDropError(const QString& errorMessage)
{
    outputToTerminal(QStringLiteral("TMMA file drop failed: %1").arg(errorMessage), Error);
}

bool TMMAController::autoSaveAndCloseCurrentJob()
{
    if (m_identityRecoveryBlocked) {
        outputToTerminal(QStringLiteral("TMMA close is blocked until the pending identity recovery is resolved."), Error);
        return false;
    }
    if (!hasActiveIdentity()) {
        return false;
    }

    if (!saveJobState()) {
        outputToTerminal(QStringLiteral("TMMA close stopped because the job state could not be saved."), Error);
        return false;
    }

    QString archiveError;
    if (!m_fileManager->moveWorkingToArchive(
            m_activeJobNumber, m_activeYear, m_activeMonth, &archiveError)) {
        outputToTerminal(QStringLiteral("TMMA close stopped: %1").arg(archiveError), Error);
        outputToTerminal(QStringLiteral("Working source files were left recoverable; the job remains active."), Warning);
        return false;
    }

    outputToTerminal(QStringLiteral("TMMA working files moved to %1.")
                         .arg(QDir::toNativeSeparators(m_fileManager->getArchiveJobPath(
                             m_activeJobNumber, m_activeYear, m_activeMonth))), Success);
    resetToDefaults();
    emit jobClosed();
    return true;
}

void TMMAController::clearActiveIdentity()
{
    m_activeJobNumber.clear();
    m_activeYear.clear();
    m_activeMonth.clear();
}

void TMMAController::resetToDefaults()
{
    m_jobDataLocked = false;
    m_postageDataLocked = false;
    m_htmlDisplayState = DefaultState;
    m_lastExecutedScript.clear();
    m_finalOutputFilePath.clear();
    m_finalMergedFilePath.clear();
    clearActiveIdentity();

    if (m_jobNumberBox) {
        const QSignalBlocker blocker(m_jobNumberBox);
        m_jobNumberBox->clear();
    }
    if (m_yearDropdown) {
        const QSignalBlocker blocker(m_yearDropdown);
        m_yearDropdown->setCurrentIndex(0);
    }
    if (m_monthDropdown) {
        const QSignalBlocker blocker(m_monthDropdown);
        m_monthDropdown->setCurrentIndex(0);
    }
    if (m_postageBox) {
        const QSignalBlocker blocker(m_postageBox);
        m_postageBox->clear();
    }
    if (m_countBox) {
        const QSignalBlocker blocker(m_countBox);
        m_countBox->clear();
    }
    if (m_classDropdown) {
        const QSignalBlocker blocker(m_classDropdown);
        m_classDropdown->setCurrentIndex(0);
    }
    if (m_permitDropdown) {
        const QSignalBlocker blocker(m_permitDropdown);
        m_permitDropdown->setCurrentIndex(0);
    }
    if (m_lockButton) {
        m_lockButton->setChecked(false);
    }
    if (m_editButton) {
        m_editButton->setChecked(false);
    }
    if (m_postageLockButton) {
        m_postageLockButton->setChecked(false);
    }
    if (m_dropWindow) {
        m_dropWindow->refreshFromDirectory();
    }
    updateControlStates();
    updateHtmlDisplay();
}

void TMMAController::updateControlStates()
{
    const bool activeJob = hasActiveIdentity();
    if (m_lockButton) m_lockButton->setChecked(m_jobDataLocked);
    if (m_editButton) m_editButton->setChecked(activeJob && !m_jobDataLocked);
    if (m_postageLockButton) m_postageLockButton->setChecked(m_postageDataLocked);

    if (m_identityRecoveryBlocked) {
        if (m_jobNumberBox) m_jobNumberBox->setEnabled(false);
        if (m_yearDropdown) m_yearDropdown->setEnabled(false);
        if (m_monthDropdown) m_monthDropdown->setEnabled(false);
        if (m_lockButton) m_lockButton->setEnabled(false);
        if (m_editButton) m_editButton->setEnabled(false);
        if (m_postageBox) m_postageBox->setEnabled(false);
        if (m_countBox) m_countBox->setEnabled(false);
        if (m_classDropdown) m_classDropdown->setEnabled(false);
        if (m_permitDropdown) m_permitDropdown->setEnabled(false);
        if (m_postageLockButton) m_postageLockButton->setEnabled(false);
        if (m_dropWindow) m_dropWindow->setEnabled(false);
        if (m_runInitialButton) m_runInitialButton->setEnabled(false);
        if (m_openBulkMailerButton) m_openBulkMailerButton->setEnabled(false);
        if (m_finalStepButton) m_finalStepButton->setEnabled(false);
        return;
    }

    const bool editableIdentity = !m_jobDataLocked && !m_scriptRunning;
    if (m_jobNumberBox) m_jobNumberBox->setEnabled(editableIdentity);
    if (m_yearDropdown) m_yearDropdown->setEnabled(editableIdentity);
    if (m_monthDropdown) m_monthDropdown->setEnabled(editableIdentity);
    if (m_lockButton) m_lockButton->setEnabled(!m_scriptRunning);
    if (m_editButton) m_editButton->setEnabled(activeJob && m_jobDataLocked && !m_scriptRunning);
    if (m_dropWindow) m_dropWindow->setEnabled(!m_jobDataLocked && !m_scriptRunning);

    const bool editablePostage = !m_postageDataLocked && !m_scriptRunning;
    if (m_postageBox) m_postageBox->setEnabled(editablePostage);
    if (m_countBox) m_countBox->setEnabled(editablePostage);
    if (m_classDropdown) m_classDropdown->setEnabled(editablePostage);
    if (m_permitDropdown) m_permitDropdown->setEnabled(editablePostage);
    if (m_postageLockButton) m_postageLockButton->setEnabled(activeJob && m_jobDataLocked && !m_scriptRunning);
    if (m_runInitialButton) m_runInitialButton->setEnabled(m_jobDataLocked && !m_scriptRunning);
    if (m_openBulkMailerButton) m_openBulkMailerButton->setEnabled(m_jobDataLocked && !m_scriptRunning);
    if (m_finalStepButton) m_finalStepButton->setEnabled(
        m_jobDataLocked && m_postageDataLocked && !m_scriptRunning);
}

void TMMAController::updateHtmlDisplay()
{
    m_htmlDisplayState = m_jobDataLocked ? InstructionsState : DefaultState;
    loadHtmlFile(m_jobDataLocked
                     ? QStringLiteral(":/resources/tmma/instructions.html")
                     : QStringLiteral(":/resources/tmma/default.html"));
}

void TMMAController::loadHtmlFile(const QString& resourcePath)
{
    if (!m_textBrowser) {
        return;
    }
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        outputToTerminal(QStringLiteral("Unable to load TMMA instructions resource: %1").arg(resourcePath), Error);
        return;
    }
    m_textBrowser->setHtml(QString::fromUtf8(file.readAll()));
}

void TMMAController::outputToTerminal(const QString& message, MessageType type)
{
    TerminalOutputHelper::append(m_terminalWindow, message, terminalSeverity(type));
}

QTableView* TMMAController::getTrackerWidget() const
{
    return m_tracker;
}

QSqlTableModel* TMMAController::getTrackerModel() const
{
    return m_trackerModel;
}

QStringList TMMAController::getTrackerHeaders() const
{
    return {QStringLiteral("JOB"), QStringLiteral("DESCRIPTION"), QStringLiteral("POSTAGE"),
            QStringLiteral("COUNT"), QStringLiteral("AVG RATE"), QStringLiteral("CLASS"),
            QStringLiteral("SHAPE"), QStringLiteral("PERMIT")};
}

QList<int> TMMAController::getVisibleColumns() const
{
    return {1, 2, 3, 4, 5, 6, 7, 8};
}

QString TMMAController::formatCellData(int columnIndex, const QString& cellData) const
{
    if (columnIndex == 3) {
        bool ok = false;
        const double value = withoutCurrencyFormatting(cellData).toDouble(&ok);
        return ok ? QStringLiteral("$%L1").arg(value, 0, 'f', 2) : cellData;
    }
    if (columnIndex == 4) {
        QString clean = cellData;
        clean.remove(QLatin1Char(','));
        bool ok = false;
        const qlonglong value = clean.toLongLong(&ok);
        return ok ? QStringLiteral("%L1").arg(value) : cellData;
    }
    if (columnIndex == 5) {
        bool ok = false;
        const double value = cellData.toDouble(&ok);
        return ok ? QString::number(value, 'f', 3) : cellData;
    }
    return cellData;
}

QString TMMAController::formatCellDataForCopy(int columnIndex, const QString& cellData) const
{
    if (columnIndex == 3) {
        QString clean = cellData;
        clean.remove(QLatin1Char(','));
        bool ok = false;
        const qlonglong value = clean.toLongLong(&ok);
        return ok ? QString::number(value) : cellData;
    }
    return cellData;
}

void TMMAController::setupTrackerModel()
{
    if (!m_tracker || !DatabaseManager::instance()->isInitialized()) {
        return;
    }
    m_trackerModel = new TMMAFormattedSqlModel(
        this, DatabaseManager::instance()->getDatabase(), this);
    m_trackerModel->setTable(QStringLiteral("tm_ma_log"));
    m_trackerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_trackerModel->setSort(0, Qt::DescendingOrder);
    m_trackerModel->select();
    m_tracker->setModel(m_trackerModel);
    applyTrackerHeaders();

    for (int column = 0; column < m_trackerModel->columnCount(); ++column) {
        m_tracker->setColumnHidden(column, !getVisibleColumns().contains(column));
    }
    m_tracker->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tracker->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tracker->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tracker, &QTableView::customContextMenuRequested,
            this, &TMMAController::showTableContextMenu);
    setupOptimizedTableLayout();
}

void TMMAController::applyTrackerHeaders()
{
    if (!m_trackerModel) {
        return;
    }
    const QStringList headers = getTrackerHeaders();
    for (int index = 0; index < headers.size(); ++index) {
        m_trackerModel->setHeaderData(index + 1, Qt::Horizontal, headers.at(index));
    }
}

void TMMAController::refreshTracker()
{
    if (m_trackerModel) {
        m_trackerModel->select();
        applyTrackerHeaders();
    }
}

void TMMAController::setupOptimizedTableLayout()
{
    if (!m_tracker || !m_trackerModel) {
        return;
    }

    const int availableWidth = 611 - 2;
    struct ColumnSpec {
        QString header;
        QString maxContent;
        int minimumWidth;
    };
    const QList<ColumnSpec> columns = {
        {QStringLiteral("JOB"), QStringLiteral("88888"), 56},
        {QStringLiteral("DESCRIPTION"), QStringLiteral("TM MA"), 105},
        {QStringLiteral("POSTAGE"), QStringLiteral("$888,888.88"), 29},
        {QStringLiteral("COUNT"), QStringLiteral("88,888"), 45},
        {QStringLiteral("AVG RATE"), QStringLiteral("0.888"), 45},
        {QStringLiteral("CLASS"), QStringLiteral("STD"), 60},
        {QStringLiteral("SHAPE"), QStringLiteral("LTR"), 33},
        {QStringLiteral("PERMIT"), QStringLiteral("NKLN"), 36}
    };

    int optimalFontSize = 7;
    QFont testFont(QStringLiteral("Blender Pro Bold"), optimalFontSize);
    for (int fontSize = 11; fontSize >= 7; --fontSize) {
        testFont.setPointSize(fontSize);
        const QFontMetrics metrics(testFont);
        int totalWidth = 0;
        for (int index = 0; index < columns.size(); ++index) {
            const ColumnSpec& column = columns.at(index);
            int width = qMax(metrics.horizontalAdvance(column.header) + 12,
                             qMax(metrics.horizontalAdvance(column.maxContent) + 12,
                                  column.minimumWidth));
            if (index == 7) {
                width += 35;
            }
            totalWidth += width;
        }
        if (totalWidth <= availableWidth) {
            optimalFontSize = fontSize;
            break;
        }
    }

    QFont tableFont(QStringLiteral("Blender Pro"), optimalFontSize);
    tableFont.setBold(false);
    tableFont.setWeight(QFont::Normal);
    m_tracker->setFont(tableFont);
    const QFontMetrics metrics(tableFont);
    for (int index = 0; index < columns.size(); ++index) {
        const ColumnSpec& column = columns.at(index);
        int width = qMax(metrics.horizontalAdvance(column.header) + 12,
                         qMax(metrics.horizontalAdvance(column.maxContent) + 12,
                              column.minimumWidth));
        if (index == 7) {
            width += 35;
        }
        m_tracker->setColumnWidth(index + 1, width);
    }

    m_tracker->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_tracker->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tracker->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_tracker->setAlternatingRowColors(true);
    m_tracker->setStyleSheet(QStringLiteral(
        "QTableView { border: 1px solid black; selection-background-color: #d0d0ff; "
        "alternate-background-color: #f8f8f8; gridline-color: #cccccc; }"
        "QHeaderView::section { background-color: #e0e0e0; padding: 4px; "
        "border: 1px solid black; font-weight: bold; font-family: 'Blender Pro Bold'; }"
        "QTableView::item { padding: 3px; border-right: 1px solid #cccccc; }"));
}

void TMMAController::showTableContextMenu(const QPoint& position)
{
    if (!m_tracker) {
        return;
    }
    const QModelIndex clicked = m_tracker->indexAt(position);
    if (!clicked.isValid()) {
        return;
    }
    m_tracker->setCurrentIndex(clicked);
    m_tracker->selectRow(clicked.row());

    QMenu menu(m_tracker);
    QAction* copyAction = menu.addAction(QStringLiteral("Copy Selected Row"));
    if (menu.exec(m_tracker->viewport()->mapToGlobal(position)) == copyAction) {
        const QString result = copyFormattedRow();
        outputToTerminal(result,
                         result == QStringLiteral("Row copied to clipboard") ? Success : Warning);
    }
}

void TMMAController::showFinalEmailDialog(const QString& outputFilePath,
                                          const QString& mergedFilePath)
{
    if (m_emailDialog) {
        m_emailDialog->close();
    }
    m_emailDialog = new TMMAEmailDialog(
        {outputFilePath, mergedFilePath}, qobject_cast<QWidget*>(parent()));
    m_emailDialog->setAttribute(Qt::WA_DeleteOnClose);
    m_emailDialog->show();
}
