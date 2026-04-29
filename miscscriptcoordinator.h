#ifndef MISCSCRIPTCOORDINATOR_H
#define MISCSCRIPTCOORDINATOR_H

#include <QObject>
#include <QProcess>
#include <QList>
#include <QStringList>
#include <functional>

#include "terminaloutputhelper.h"

class QPushButton;
class QTextEdit;
class ScriptRunner;

class MiscScriptCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit MiscScriptCoordinator(ScriptRunner* runner,
                                   QTextEdit* terminal,
                                   QObject* parent = nullptr);

    void registerDirectScript(QPushButton* button,
                              const QString& scriptLabel,
                              const QString& runtimeScriptPath);

    void registerCustomWorkflow(QPushButton* button,
                                const QString& workflowLabel,
                                const std::function<void()>& handler);

    bool runScript(const QString& scriptLabel,
                   const QString& runtimeScriptPath,
                   const QStringList& arguments = QStringList());

    bool isBusy() const;

signals:
    void busyChanged(bool busy);
    void scriptStarted(const QString& scriptLabel,
                       const QString& runtimeScriptPath,
                       const QStringList& arguments);
    void scriptOutput(const QString& output);
    void scriptFinished(int exitCode, QProcess::ExitStatus exitStatus);

private slots:
    void onRunnerOutput(const QString& output);
    void onRunnerFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void logToTerminal(const QString& message, TerminalSeverity severity);
    void setButtonsEnabled(bool enabled);
    static TerminalSeverity inferSeverity(const QString& message);

    ScriptRunner* m_runner;
    QTextEdit* m_terminal;
    QList<QPushButton*> m_buttons;
    bool m_scriptRunning;
};

#endif // MISCSCRIPTCOORDINATOR_H