#include "scriptrunner.h"
#include <QDebug>

ScriptRunner::ScriptRunner(QObject* parent)
    : QObject(parent), process(nullptr), running(false)
{
}

ScriptRunner::~ScriptRunner()
{
    if (process) {
        if (process->state() != QProcess::NotRunning) {
            process->terminate();
            process->waitForFinished(3000);
            if (process->state() != QProcess::NotRunning) {
                process->kill();
            }
        }
        delete process;
    }
}

void ScriptRunner::runScript(const QString& program, const QStringList& arguments)
{
    if (running) {
        qDebug() << "ScriptRunner: Cannot start new process while another is running.";
        return;
    }

    if (!process) {
        process = new QProcess(this);
        connect(process, &QProcess::readyReadStandardOutput, this, &ScriptRunner::handleReadyReadStandardOutput);
        connect(process, &QProcess::readyReadStandardError, this, &ScriptRunner::handleReadyReadStandardError);
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &ScriptRunner::handleFinished);
    }

    running = true;
    process->start(program, arguments);
}

bool ScriptRunner::isRunning() const
{
    return running;
}

void ScriptRunner::terminate()
{
    if (process && running) {
        process->terminate();
        process->waitForFinished(3000);
        if (process->state() != QProcess::NotRunning) {
            process->kill();
        }
        running = false;
    }
}

void ScriptRunner::handleReadyReadStandardOutput()
{
    if (process) {
        QString output = QString::fromUtf8(process->readAllStandardOutput());
        emit scriptOutput(output);
    }
}

void ScriptRunner::handleReadyReadStandardError()
{
    if (process) {
        QString error = QString::fromUtf8(process->readAllStandardError());
        emit scriptError(error);
    }
}

void ScriptRunner::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    running = false;
    emit scriptFinished(exitCode, exitStatus);
}
