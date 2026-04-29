#include "miscscriptcoordinator.h"

#include "scriptrunner.h"

#include <QDir>
#include <QFileInfo>
#include <QPushButton>
#include <QTextEdit>
#include <utility>

namespace {
TerminalSeverity inferMiscSeverity(const QString& message)
{
    const QString text = message.trimmed();
    if (text.isEmpty()) {
        return TerminalSeverity::Info;
    }

    if (text.startsWith("DEBUG", Qt::CaseInsensitive)) {
        return TerminalSeverity::Info;
    }

    if (text.startsWith("ERROR:", Qt::CaseInsensitive)
        || text.contains("error", Qt::CaseInsensitive)
        || text.contains("failed", Qt::CaseInsensitive)
        || text.contains("not found", Qt::CaseInsensitive)
        || text.contains("unavailable", Qt::CaseInsensitive)
        || text.contains("cannot", Qt::CaseInsensitive)
        || text.contains("missing required", Qt::CaseInsensitive)
        || text.contains("invalid", Qt::CaseInsensitive)) {
        return TerminalSeverity::Error;
    }

    if (text.startsWith("WARNING:", Qt::CaseInsensitive)
        || text.contains("warning", Qt::CaseInsensitive)
        || text.contains("cancelled", Qt::CaseInsensitive)
        || text.contains("canceled", Qt::CaseInsensitive)) {
        return TerminalSeverity::Warning;
    }

    if (text.startsWith("SUCCESS:", Qt::CaseInsensitive)
        || text.contains("successfully", Qt::CaseInsensitive)) {
        return TerminalSeverity::Success;
    }

    return TerminalSeverity::Info;
}
}

MiscScriptCoordinator::MiscScriptCoordinator(ScriptRunner* runner,
                                             QTextEdit* terminal,
                                             QObject* parent)
    : QObject(parent)
    , m_runner(runner)
    , m_terminal(terminal)
    , m_scriptRunning(false)
{
    if (m_runner) {
        connect(m_runner, &ScriptRunner::scriptOutput,
                this, &MiscScriptCoordinator::onRunnerOutput);
        connect(m_runner, &ScriptRunner::scriptFinished,
                this, &MiscScriptCoordinator::onRunnerFinished);
    }
}

void MiscScriptCoordinator::registerDirectScript(QPushButton* button,
                                                 const QString& scriptLabel,
                                                 const QString& runtimeScriptPath)
{
    if (!button) {
        return;
    }

    if (!m_buttons.contains(button)) {
        m_buttons.append(button);
    }

    connect(button, &QPushButton::clicked, this, [this, scriptLabel, runtimeScriptPath]() {
        runScript(scriptLabel, runtimeScriptPath, QStringList());
    });
}

void MiscScriptCoordinator::registerCustomWorkflow(QPushButton* button,
                                                   const QString& workflowLabel,
                                                   const std::function<void()>& handler)
{
    if (!button || !handler) {
        return;
    }

    if (!m_buttons.contains(button)) {
        m_buttons.append(button);
    }

    connect(button, &QPushButton::clicked, this, [this, workflowLabel, handler]() {
        if (isBusy()) {
            logToTerminal("A MISC script is already running. Please wait for it to finish.",
                          TerminalSeverity::Warning);
            return;
        }

        logToTerminal(QString("Opening %1 workflow dialog.").arg(workflowLabel), TerminalSeverity::Info);
        handler();
    });
}

bool MiscScriptCoordinator::runScript(const QString& scriptLabel,
                                      const QString& runtimeScriptPath,
                                      const QStringList& arguments)
{
    if (!m_terminal) {
        return false;
    }

    if (!m_runner) {
        logToTerminal("MISC script runner is unavailable.", TerminalSeverity::Error);
        return false;
    }

    if (isBusy()) {
        logToTerminal("A MISC script is already running. Please wait for it to finish.",
                      TerminalSeverity::Warning);
        return false;
    }

    const QFileInfo scriptInfo(runtimeScriptPath);
    if (!scriptInfo.exists()) {
        logToTerminal(QString("Script not found: %1")
                          .arg(QDir::toNativeSeparators(runtimeScriptPath)),
                      TerminalSeverity::Error);
        return false;
    }

    m_scriptRunning = true;
    setButtonsEnabled(false);
    emit busyChanged(true);

    logToTerminal(QString("Starting %1").arg(scriptLabel), TerminalSeverity::Info);
    logToTerminal(QString("Script: %1").arg(QDir::toNativeSeparators(runtimeScriptPath)),
                  TerminalSeverity::Info);

    if (!arguments.isEmpty()) {
        logToTerminal(QString("Args: %1").arg(arguments.join(" | ")), TerminalSeverity::Info);
    }

    emit scriptStarted(scriptLabel, runtimeScriptPath, arguments);

    if (!m_runner->runScript(runtimeScriptPath, arguments)) {
        m_scriptRunning = false;
        setButtonsEnabled(true);
        emit busyChanged(false);
        logToTerminal("Failed to start MISC script process.", TerminalSeverity::Error);
        return false;
    }

    return true;
}

bool MiscScriptCoordinator::isBusy() const
{
    return m_scriptRunning || (m_runner && m_runner->isRunning());
}

void MiscScriptCoordinator::onRunnerOutput(const QString& output)
{
    const QString trimmed = output.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    logToTerminal(trimmed, inferSeverity(trimmed));
    emit scriptOutput(trimmed);
}

void MiscScriptCoordinator::onRunnerFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_scriptRunning = false;
    setButtonsEnabled(true);
    emit busyChanged(false);

    if (exitStatus == QProcess::CrashExit) {
        logToTerminal("MISC script crashed.", TerminalSeverity::Error);
    } else if (exitCode == 0) {
        logToTerminal("MISC script finished successfully.", TerminalSeverity::Success);
    } else {
        logToTerminal(QString("MISC script finished with exit code %1.").arg(exitCode),
                      TerminalSeverity::Error);
    }

    emit scriptFinished(exitCode, exitStatus);
}

void MiscScriptCoordinator::logToTerminal(const QString& message, TerminalSeverity severity)
{
    TerminalOutputHelper::append(m_terminal, message, severity);
}

void MiscScriptCoordinator::setButtonsEnabled(bool enabled)
{
    for (QPushButton* button : std::as_const(m_buttons)) {
        if (button) {
            button->setEnabled(enabled);
        }
    }
}

TerminalSeverity MiscScriptCoordinator::inferSeverity(const QString& message)
{
    return inferMiscSeverity(message);
}
