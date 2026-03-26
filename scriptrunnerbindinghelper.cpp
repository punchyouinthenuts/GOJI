#include "scriptrunnerbindinghelper.h"

#include "scriptrunner.h"

#include <QObject>

bool ScriptRunnerBindingHelper::setupBaselineBindings(
    ScriptRunner* scriptRunner,
    QObject* context,
    const ScriptOutputHandler& scriptOutputHandler,
    const ScriptFinishedHandler& scriptFinishedHandler,
    const ScriptErrorHandler& scriptErrorHandler)
{
    if (!scriptRunner || !context || !scriptOutputHandler || !scriptFinishedHandler) {
        return false;
    }

    QObject::connect(scriptRunner, &ScriptRunner::scriptOutput, context,
                     [scriptOutputHandler](const QString& output) {
                         scriptOutputHandler(output);
                     });

    QObject::connect(scriptRunner, &ScriptRunner::scriptFinished, context,
                     [scriptFinishedHandler](int exitCode, QProcess::ExitStatus exitStatus) {
                         scriptFinishedHandler(exitCode, exitStatus);
                     });

    if (scriptErrorHandler) {
        QObject::connect(scriptRunner, &ScriptRunner::scriptError, context,
                         [scriptErrorHandler](const QString& errorOutput) {
                             scriptErrorHandler(errorOutput);
                         });
    }

    return true;
}
