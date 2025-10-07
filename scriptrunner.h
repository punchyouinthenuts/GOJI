#ifndef SCRIPTRUNNER_H
#define SCRIPTRUNNER_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QStringList>

class ScriptRunner : public QObject
{
    Q_OBJECT
public:
    explicit ScriptRunner(QObject *parent = nullptr);
    ~ScriptRunner();

    // API expected by existing controllers
    bool runScript(const QString &scriptPath, const QStringList &arguments);
    bool isRunning() const;
    void terminate();
    void writeToScript(const QString &text); // must exist with this exact signature
    QString getLastActualScript() const;

    // Optional toggles
    bool inputWrapperEnabled { true };
    void setInputWrapperEnabled(bool enabled);

signals:
    void scriptOutput(const QString &line);
    void scriptError(const QString &line);
    void scriptFinished(int exitCode, QProcess::ExitStatus exitStatus);

public slots:
    void handleReadyReadStandardOutput();
    void handleReadyReadStandardError();
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void resetBuffers();
    void startInputWrapper();
    void stopInputWrapper();
    void processNewData(QByteArray &accumulator, const QByteArray &newData, bool isStdErr);

private:
    QProcess *m_process { nullptr };
    QString   m_lastScriptPath;
    QByteArray m_stdoutBuf;
    QByteArray m_stderrBuf;
    QTimer    m_inputWrapperTimer;
};

#endif // SCRIPTRUNNER_H
