#ifndef SCRIPTRUNNER_H
#define SCRIPTRUNNER_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

class ScriptRunner : public QObject
{
    Q_OBJECT

public:
    ScriptRunner(QObject* parent = nullptr);
    ~ScriptRunner();

    void runScript(const QString& program, const QStringList& arguments);
    bool isRunning() const;
    void terminate();
    void writeToScript(const QString& input);

signals:
    void scriptOutput(const QString& output);
    void scriptError(const QString& error);
    void scriptFinished(int exitCode, QProcess::ExitStatus exitStatus);

private slots:
    void handleReadyReadStandardOutput();
    void handleReadyReadStandardError();
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess* process;
    bool running;

    // Add these new method declarations
    void createProcess();
    void cleanUpProcess();
    QString createInputHandlerScript(const QString& pythonScriptPath, const QStringList& arguments);
};

#endif // SCRIPTRUNNER_H
