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
    // Ensure separate channels and R/W unbuffered mode on start
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &ScriptRunner::handleReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &ScriptRunner::handleReadyReadStandardError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ScriptRunner::handleFinished);

    m_inputWrapperTimer.setInterval(1500);
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
        if (m_process->state() == QProcess::Running) {
            m_process->terminate();
            m_process->waitForFinished(1500);
        }
    }
}

bool ScriptRunner::runScript(const QString &scriptPath, const QStringList &arguments)
{
    if (!m_process) return false;

    if (m_process->state() != QProcess::NotRunning) {
        return false;
    }

    resetBuffers();
    m_lastScriptPath = scriptPath;

    QString program = QStringLiteral("python");
    QStringList procArgs;
    procArgs << scriptPath;
    procArgs << arguments;

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
    }
}

void ScriptRunner::writeToScript(const QString &text)
{
    if (!m_process || m_process->state() != QProcess::Running) {
        return;
    }

    QByteArray payload = text.toUtf8();
    m_process->write(payload);
    m_process->waitForBytesWritten(50);

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
    QList<QByteArray> lines = data.split('\n');
    for (const QByteArray &line : lines) {
        QString qline = QString::fromLocal8Bit(line).trimmed();
        if (!qline.isEmpty()) {
            emit scriptError(qline);
            emit scriptOutput(qline); // Forward stderr to terminal
        }
    }
}

void ScriptRunner::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    stopInputWrapper();

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
        else emit scriptOutput(qline);
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
