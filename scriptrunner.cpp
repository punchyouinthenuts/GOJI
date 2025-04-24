#include "scriptrunner.h"
#include <QCoreApplication>
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

    // Set process to run without creating a window
    process->setProcessChannelMode(QProcess::MergedChannels);

    QStringList modifiedArgs = arguments;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // Configure based on program type
    if (program.contains("powershell", Qt::CaseInsensitive)) {
        // PowerShell-specific configuration
        if (!modifiedArgs.contains("-WindowStyle")) {
            modifiedArgs.insert(1, "-WindowStyle");
            modifiedArgs.insert(2, "Hidden");
        }
    }
    else if (program.contains("python", Qt::CaseInsensitive)) {
        // Python-specific configuration
        env.insert("PYTHONUNBUFFERED", "1"); // Ensure Python output is unbuffered
    }
    else if (program == "cmd.exe") {
        // Batch file configuration - add quiet mode
        if (!modifiedArgs.contains("/q")) {
            modifiedArgs.insert(1, "/q"); // Quiet mode
        }
    }

    // Apply environment settings
    process->setProcessEnvironment(env);

    // Start the process with the modified arguments
    running = true;
    process->start(program, modifiedArgs);
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
