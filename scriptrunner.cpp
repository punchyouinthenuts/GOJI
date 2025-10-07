#include "scriptrunner.h"
#include <QCoreApplication>
#include <QTextStream>

// Helper to check newline presence
static inline bool hasNewline(const QString &s) {
    for (QChar c : s) {
        if (c == QLatin1Char('\n')) return true;
    }
    return false;
}

ScriptRunner::ScriptRunner(QObject *parent)
    : QObject(parent),
      m_process(new QProcess(this))
{
    // Ensure separate channels and R/W unbuffered mode on start (explicitly again in runScript)
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &ScriptRunner::handleReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &ScriptRunner::handleReadyReadStandardError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ScriptRunner::handleFinished);

    // Input wrapper: periodically writes a newline to keep some scripts responsive,
    // e.g., to flush prompts or prevent blocking on input() in certain environments.
    m_inputWrapperTimer.setInterval(1500); // 1.5s default tick; can be adjusted if needed
    m_inputWrapperTimer.setSingleShot(false);
    connect(&m_inputWrapperTimer, &QTimer::timeout, [this]() {
        if (!inputWrapperEnabled) return;
        if (m_process && m_process->state() == QProcess::Running) {
            m_process->write("\n");
            m_process->waitForBytesWritten(50);
        }
    });
}

ScriptRunner::~ScriptRunner()
{
    stopInputWrapper();
    if (m_process) {
        // Let the process end gracefully
        if (m_process->state() == QProcess::Running) {
            m_process->terminate();
            m_process->waitForFinished(1500);
        }
    }
}

bool ScriptRunner::runScript(const QString &scriptPath, const QStringList &arguments)
{
    if (!m_process) return false;

    // If already running, refuse to start another
    if (m_process->state() != QProcess::NotRunning) {
        return false;
    }

    resetBuffers();
    m_lastScriptPath = scriptPath;

    // Build program/args: "python" + script + args (keeps legacy behavior)
    // Controllers pass the script path to this method.
    QString program = QStringLiteral("python");
    QStringList procArgs;
    procArgs << scriptPath;
    procArgs << arguments;

    // Explicit channel mode and start with read/write + unbuffered I/O
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->start(program, procArgs, QIODevice::ReadWrite | QIODevice::Unbuffered);

    const bool started = m_process->waitForStarted(5000);
    if (started) {
        startInputWrapper();
    }
    return started;
}

bool ScriptRunner::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

void ScriptRunner::terminate()
{
    if (!m_process) return;

    if (m_process->state() == QProcess::Running) {
        m_process->terminate();
        // Do not close the write channel here; requirement is to keep stdin open
        // for life of the process and only close after finished.
    }
}

void ScriptRunner::writeToScript(const QString &text)
{
    // Verify process valid and running
    if (!m_process || m_process->state() != QProcess::Running) {
        return;
    }

    QByteArray payload = text.toUtf8();
    // Do not append newline automatically unless already present in text
    // (If text contains a newline anywhere, we pass as-is.)
    m_process->write(payload);
    m_process->waitForBytesWritten(50); // immediate flush

    emit scriptOutput(QStringLiteral("[stdin] %1").arg(text));
}

QString ScriptRunner::getLastActualScript() const
{
    return m_lastScriptPath;
}

void ScriptRunner::handleReadyReadStandardOutput()
{
    if (!m_process) return;
    QByteArray data = m_process->readAllStandardOutput();
    processNewData(m_stdoutBuf, data, /*isStdErr=*/false);
}

void ScriptRunner::handleReadyReadStandardError()
{
    if (!m_process) return;
    QByteArray data = m_process->readAllStandardError();
    processNewData(m_stderrBuf, data, /*isStdErr=*/true);
}

void ScriptRunner::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    stopInputWrapper();

    // Drain any remaining buffers as final lines
    if (!m_stdoutBuf.isEmpty()) {
        QList<QByteArray> lines = m_stdoutBuf.split('\n');
        for (int i = 0; i < lines.size(); ++i) {
            QString line = QString::fromLocal8Bit(lines.at(i)).trimmed();
            if (!line.isEmpty())
                emit scriptOutput(line);
        }
        m_stdoutBuf.clear();
    }
    if (!m_stderrBuf.isEmpty()) {
        QList<QByteArray> lines = m_stderrBuf.split('\n');
        for (int i = 0; i < lines.size(); ++i) {
            QString line = QString::fromLocal8Bit(lines.at(i)).trimmed();
            if (!line.isEmpty())
                emit scriptError(line);
        }
        m_stderrBuf.clear();
    }

    // Per requirement, only now close write channel
    if (m_process && m_process->isWritable()) {
        m_process->closeWriteChannel();
    }

    emit scriptFinished(exitCode, exitStatus);
}

void ScriptRunner::resetBuffers()
{
    m_stdoutBuf.clear();
    m_stderrBuf.clear();
}

void ScriptRunner::startInputWrapper()
{
    if (!m_inputWrapperTimer.isActive())
        m_inputWrapperTimer.start();
}

void ScriptRunner::stopInputWrapper()
{
    if (m_inputWrapperTimer.isActive())
        m_inputWrapperTimer.stop();
}

void ScriptRunner::processNewData(QByteArray &accumulator, const QByteArray &newData, bool isStdErr)
{
    accumulator.append(newData);

    // Normalize CRLF to LF for line splitting
    accumulator.replace("\r\n", "\n");
    accumulator.replace('\r', '\n');

    int idx;
    while ((idx = accumulator.indexOf('\n')) != -1) {
        QByteArray line = accumulator.left(idx);
        accumulator.remove(0, idx + 1);
        QString qline = QString::fromLocal8Bit(line).trimmed();
        if (qline.isEmpty())
            continue;
        if (isStdErr) emit scriptError(qline);
        else          emit scriptOutput(qline);
    }
}

void ScriptRunner::setInputWrapperEnabled(bool enabled)
{
    inputWrapperEnabled = enabled;
    if (!enabled) {
        stopInputWrapper();
    } else if (isRunning()) {
        startInputWrapper();
    }
}
