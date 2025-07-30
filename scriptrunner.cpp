#include "scriptrunner.h"
#include "fileutils.h" // For FileUtils::safeRemoveFile
#include "errorhandling.h" // For FileOperationException
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QTemporaryFile>
#include <QTextStream>
#include <type_traits> // For std::as_const in Qt 6

ScriptRunner::ScriptRunner(QObject* parent)
    : QObject(parent), process(nullptr), running(false)
{
    // Create a new process
    createProcess();
}

ScriptRunner::~ScriptRunner()
{
    // Clean up process on destruction
    cleanUpProcess();
}

void ScriptRunner::createProcess()
{
    if (process) {
        cleanUpProcess();
    }

    process = new QProcess(this);

    // Set up environment to handle output properly
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONUNBUFFERED", "1");  // Force Python to run unbuffered
    env.insert("PYTHONIOENCODING", "utf-8"); // Set encoding for Python
    process->setProcessEnvironment(env);

    // Set up process to merge standard output and error channels
    process->setProcessChannelMode(QProcess::SeparateChannels);

    // Connect signals for output handling
    connect(process, &QProcess::readyReadStandardOutput,
            this, &ScriptRunner::handleReadyReadStandardOutput,
            Qt::DirectConnection);  // Direct connection for immediate output

    connect(process, &QProcess::readyReadStandardError,
            this, &ScriptRunner::handleReadyReadStandardError,
            Qt::DirectConnection);  // Direct connection for immediate output

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ScriptRunner::handleFinished);
}

void ScriptRunner::cleanUpProcess()
{
    if (process) {
        if (process->state() != QProcess::NotRunning) {
            // Try to terminate gracefully
            process->terminate();
            if (!process->waitForFinished(3000)) {
                // Force kill if not terminated within timeout
                process->kill();
            }
        }

        // Disconnect all signals
        disconnect(process, nullptr, this, nullptr);

        // Delete the process
        delete process;
        process = nullptr;
    }

    running = false;
}

void ScriptRunner::runScript(const QString& program, const QStringList& arguments)
{
    if (running) {
        emit scriptError("A script is already running. Please wait for it to complete.");
        return;
    }

    // Ensure we have a clean process
    if (!process) {
        createProcess();
    }

    // Log the command line
    QString cmdLine = program + " " + arguments.join(" ");
    emit scriptOutput("Executing: " + cmdLine);

    // Create a wrapper script if this is a Python script to handle user input
    QString actualProgram = program;
    QStringList actualArgs = arguments;
    QString tempScriptPath;

    if (program.endsWith(".py", Qt::CaseInsensitive) ||
        (program.contains("python", Qt::CaseInsensitive) && !arguments.isEmpty() && arguments.first().endsWith(".py", Qt::CaseInsensitive))) {

        // Determine the Python script path
        QString pythonScriptPath;
        if (program.endsWith(".py", Qt::CaseInsensitive)) {
            pythonScriptPath = program;
            actualProgram = "python";
            actualArgs.clear();
            actualArgs << pythonScriptPath;
        } else {
            pythonScriptPath = arguments.first();
        }

        // Create a wrapper script that will handle any user input automatically
        tempScriptPath = createInputHandlerScript(pythonScriptPath, arguments);
        if (!tempScriptPath.isEmpty()) {
            actualProgram = "python";
            actualArgs.clear();
            actualArgs << tempScriptPath;
            emit scriptOutput("Using input handler wrapper for: " + pythonScriptPath);
        }
    }
    else if (program.contains("powershell", Qt::CaseInsensitive)) {
        // Ensure PowerShell runs in hidden mode
        if (!actualArgs.contains("-WindowStyle") && !actualArgs.contains("-NonInteractive")) {
            actualArgs.prepend("-NonInteractive");
            actualArgs.prepend("-WindowStyle");
            actualArgs.prepend("Hidden");
        }
    }
    else if (program == "cmd.exe" || program.endsWith(".bat", Qt::CaseInsensitive) || program.endsWith(".cmd", Qt::CaseInsensitive)) {
        // Batch files should run with echo off and use cmd.exe explicitly
        if (program.endsWith(".bat", Qt::CaseInsensitive) || program.endsWith(".cmd", Qt::CaseInsensitive)) {
            QString batchPath = program;
            actualProgram = "cmd.exe";
            actualArgs.clear();
            actualArgs << "/C" << batchPath;

            // Add arguments for the batch file
            if (!arguments.isEmpty()) {
                actualArgs << arguments;
            }
        }

        // Add /Q for quiet mode if not already present
        if (!actualArgs.contains("/Q")) {
            actualArgs.prepend("/Q");
        }
    }

    // Set the working directory if possible
    QFileInfo fileInfo(program);
    if (fileInfo.exists() && fileInfo.isFile()) {
        process->setWorkingDirectory(fileInfo.absolutePath());
        emit scriptOutput("Working directory: " + fileInfo.absolutePath());
    }

    // Start the process
    running = true;
    emit scriptOutput("Starting process with: " + actualProgram + " " + actualArgs.join(" "));
    process->start(actualProgram, actualArgs);

    if (!process->waitForStarted(5000)) {
        emit scriptError("Failed to start process: " + process->errorString());
        running = false;

        // Clean up temporary script if needed
        if (!tempScriptPath.isEmpty() && QFile::exists(tempScriptPath)) {
            try {
                FileUtils::safeRemoveFile(tempScriptPath);
            } catch (const FileOperationException& e) {
                emit scriptError(QString("Failed to remove temporary script: %1").arg(e.message()));
            }
        }
    }
}

void ScriptRunner::writeToScript(const QString& input)
{
    if (process && process->state() == QProcess::Running) {
        process->write(input.toUtf8());
        process->waitForBytesWritten(1000); // Wait up to 1 second for write to complete
    }
}

QString ScriptRunner::createInputHandlerScript(const QString& pythonScriptPath, const QStringList& arguments)
{
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(false);  // We need the file to persist after this function

    if (!tempFile.open()) {
        emit scriptError("Failed to create temporary input handler script");
        return QString();
    }

    // Get absolute path to the Python script
    QFileInfo fileInfo(pythonScriptPath);
    QString absoluteScriptPath = fileInfo.absoluteFilePath();

    // Get script arguments, excluding the script path if it's the first argument
    QStringList scriptArgs = arguments;
    if (!scriptArgs.isEmpty() && scriptArgs.first().endsWith(".py", Qt::CaseInsensitive)) {
        scriptArgs.removeFirst();
    }

    // Write the Python wrapper script
    QTextStream stream(&tempFile);
    stream << "import sys\n";
    stream << "import subprocess\n";
    stream << "import threading\n";
    stream << "import time\n";
    stream << "import os\n\n";

    // Function to handle input requests automatically
    stream << "def input_handler(process):\n";
    stream << "    while process.poll() is None:\n";
    stream << "        try:\n";
    stream << "            # Send an Enter key press to any waiting input prompt\n";
    stream << "            process.stdin.write(b'\\n')\n";
    stream << "            process.stdin.flush()\n";
    stream << "        except:\n";
    stream << "            pass\n";
    stream << "        time.sleep(0.5)\n\n";

    // Function to continuously read and forward output
    stream << "def output_reader(stream, output_func):\n";
    stream << "    while True:\n";
    stream << "        line = stream.readline()\n";
    stream << "        if not line:\n";
    stream << "            break\n";
    stream << "        output_func(line.decode('utf-8', errors='replace'))\n\n";

    // Main execution
    stream << "def main():\n";
    stream << "    # Build command\n";
    stream << "    cmd = [sys.executable, r'" << absoluteScriptPath << "'";

    // Add script arguments
    for (const QString& arg : scriptArgs) {
        QString safeArg = arg;
        safeArg.replace("\\", "\\\\");  // Escape backslashes for Python
        stream << ", r'" << safeArg << "'";
    }
    stream << "]\n\n";

    // Create process
    stream << "    # Start the process\n";
    stream << "    process = subprocess.Popen(\n";
    stream << "        cmd,\n";
    stream << "        stdin=subprocess.PIPE,\n";
    stream << "        stdout=subprocess.PIPE,\n";
    stream << "        stderr=subprocess.PIPE,\n";
    stream << "        cwd=r'" << fileInfo.absolutePath() << "',\n";
    stream << "        shell=False,\n";
    stream << "        universal_newlines=False,\n";
    stream << "        bufsize=1\n";
    stream << "    )\n\n";

    // Start threads
    stream << "    # Start input handler thread\n";
    stream << "    input_thread = threading.Thread(target=input_handler, args=(process,))\n";
    stream << "    input_thread.daemon = True\n";
    stream << "    input_thread.start()\n\n";

    // Start output reader threads
    stream << "    stdout_thread = threading.Thread(target=output_reader, args=(process.stdout, lambda x: sys.stdout.write(x) or sys.stdout.flush()))\n";
    stream << "    stdout_thread.daemon = True\n";
    stream << "    stdout_thread.start()\n\n";

    stream << "    stderr_thread = threading.Thread(target=output_reader, args=(process.stderr, lambda x: sys.stderr.write(x) or sys.stderr.flush()))\n";
    stream << "    stderr_thread.daemon = True\n";
    stream << "    stderr_thread.start()\n\n";

    // Wait for process to complete
    stream << "    # Wait for process to complete\n";
    stream << "    exit_code = process.wait()\n";
    stream << "    stdout_thread.join()\n";
    stream << "    stderr_thread.join()\n";
    stream << "    return exit_code\n\n";

    // Execute main
    stream << "if __name__ == '__main__':\n";
    stream << "    try:\n";
    stream << "        exit_code = main()\n";
    stream << "        sys.exit(exit_code)\n";
    stream << "    except Exception as e:\n";
    stream << "        print(f'Error in wrapper script: {e}', file=sys.stderr)\n";
    stream << "        sys.exit(1)\n";

    tempFile.close();

    return tempFile.fileName();
}

bool ScriptRunner::isRunning() const
{
    return running;
}

void ScriptRunner::terminate()
{
    if (process && running) {
        emit scriptOutput("Terminating process...");
        process->terminate();
        if (!process->waitForFinished(3000)) {
            emit scriptOutput("Process did not terminate gracefully, forcing kill...");
            process->kill();
        }
        running = false;
    }
}

void ScriptRunner::handleReadyReadStandardOutput()
{
    if (!process) return;

    QByteArray output = process->readAllStandardOutput();
    if (!output.isEmpty()) {
        QString text = QString::fromUtf8(output);

        // Clean up the text - replace carriage returns, etc.
        text = text.replace("\r\n", "\n");

        // Split into lines and emit each line separately
        QStringList lines = text.split('\n', Qt::SkipEmptyParts);
        for (const QString& line : std::as_const(lines)) {
            emit scriptOutput(line);
        }
    }
}

void ScriptRunner::handleReadyReadStandardError()
{
    if (!process) return;

    QByteArray error = process->readAllStandardError();
    if (!error.isEmpty()) {
        QString text = QString::fromUtf8(error);

        // Clean up the text - replace carriage returns, etc.
        text = text.replace("\r\n", "\n");

        // Split into lines and emit each line separately
        QStringList lines = text.split('\n', Qt::SkipEmptyParts);
        for (const QString& line : std::as_const(lines)) {
            emit scriptError(line);
        }
    }
}

void ScriptRunner::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // Read any remaining output
    handleReadyReadStandardOutput();
    handleReadyReadStandardError();

    // Log completion status
    if (exitStatus == QProcess::NormalExit) {
        emit scriptOutput(QString("Process completed with exit code: %1").arg(exitCode));
    } else {
        emit scriptError(QString("Process crashed or was killed. Exit code: %1").arg(exitCode));
    }

    // Update state
    running = false;

    // Clean up the process to ensure a fresh start next time
    QProcess* oldProcess = process;
    process = nullptr;  // Clear the pointer before emitting signals

    // Emit finished signal
    emit scriptFinished(exitCode, exitStatus);

    // Clean up the old process after emitting signals
    if (oldProcess) {
        oldProcess->deleteLater();
    }

    // Clean up any temporary wrapper scripts in the temp directory
    QDir tempDir(QDir::tempPath());
    QStringList filters;
    filters << "temp*"; // Match temporary files created by QTemporaryFile
    QFileInfoList tempFiles = tempDir.entryInfoList(filters, QDir::Files);

    QDateTime currentTime = QDateTime::currentDateTime();
    for (const QFileInfo& fileInfo : std::as_const(tempFiles)) {
        // Only delete files more than 30 minutes old
        if (fileInfo.lastModified().secsTo(currentTime) > 1800) {
            try {
                FileUtils::safeRemoveFile(fileInfo.absoluteFilePath());
            } catch (const FileOperationException& e) {
                emit scriptError(QString("Failed to remove temporary file: %1").arg(e.message()));
            }
        }
    }
}
