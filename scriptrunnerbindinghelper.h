#ifndef SCRIPTRUNNERBINDINGHELPER_H
#define SCRIPTRUNNERBINDINGHELPER_H

#include <QProcess>
#include <QString>

#include <functional>

class QObject;
class ScriptRunner;

class ScriptRunnerBindingHelper
{
public:
    using ScriptOutputHandler = std::function<void(const QString&)>;
    using ScriptFinishedHandler = std::function<void(int, QProcess::ExitStatus)>;
    using ScriptErrorHandler = std::function<void(const QString&)>;

    static bool setupBaselineBindings(
        ScriptRunner* scriptRunner,
        QObject* context,
        const ScriptOutputHandler& scriptOutputHandler,
        const ScriptFinishedHandler& scriptFinishedHandler,
        const ScriptErrorHandler& scriptErrorHandler = ScriptErrorHandler());
};

#endif // SCRIPTRUNNERBINDINGHELPER_H
