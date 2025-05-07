#ifndef JOBCONTROLLER_H
#define JOBCONTROLLER_H

#include <QObject>
#include <array>
#include <QMap>
#include <QPair>
#include <QCheckBox>
#include <QJsonObject>
#include <QJsonDocument>
#include <exception>
#include <string>
#include <QMetaObject>

#include "jobdata.h"
#include "databasemanager.h"
#include "filesystemmanager.h"
#include "scriptrunner.h"
#include "errorhandling.h"

class JobController : public QObject
{
    Q_OBJECT

public:
    JobController(DatabaseManager* dbManager, FileSystemManager* fileManager,
                  ScriptRunner* scriptRunner, QSettings* settings, QObject* parent = nullptr);
    ~JobController();

    bool loadJob(const QString& year, const QString& month, const QString& week);
    bool saveJob();
    bool createJob();
    bool closeJob();
    bool deleteJob(const QString& year, const QString& month, const QString& week);

    bool openIZ();
    bool runInitialProcessing();
    bool runPreProofProcessing();
    bool openProofFiles(const QString& jobType);
    bool runPostProofProcessing(bool isRegenMode = false);
    bool regenerateProofs(const QMap<QString, QStringList>& filesByJobType);
    bool openPrintFiles(const QString& jobType);
    bool runPostPrintProcessing();

    JobData* currentJob() const;
    bool isJobSaved() const;
    bool isJobDataLocked() const;
    void setJobDataLocked(bool locked);
    bool isProofRegenMode() const;
    void setProofRegenMode(bool enabled);
    bool isPostageLocked() const;
    void setPostageLocked(bool locked);

    QString getOriginalYear() const;
    QString getOriginalMonth() const;
    QString getOriginalWeek() const;

    double getProgress() const;
    void updateProgress();

signals:
    void jobLoaded(const JobData& job);
    void jobSaved();
    void jobClosed();
    void jobProgressUpdated(int progressPercent);
    void stepCompleted(int stepIndex);
    void logMessage(const QString& message);
    void scriptStarted();
    void scriptFinished(bool success);
    void postProofCountsUpdated();

private:
    JobData* m_currentJob;
    DatabaseManager* m_dbManager;
    FileSystemManager* m_fileManager;
    ScriptRunner* m_scriptRunner;
    QSettings* m_settings;

    bool m_isJobSaved;
    bool m_isJobDataLocked;
    bool m_isProofRegenMode;
    bool m_isPostageLocked;

    QString m_originalYear;
    QString m_originalMonth;
    QString m_originalWeek;
    QString m_scriptOutput;

    // For JSON parsing
    QString m_jsonAccumulator;
    bool m_jsonComplete = false;
    bool m_jsonCapturing = false;

    // For script execution output processing
    bool m_hasMissingItems = false;
    QStringList m_missingItems;

    static constexpr size_t NUM_STEPS = 9;
    std::array<double, NUM_STEPS> m_stepWeights;
    std::array<int, NUM_STEPS> m_totalSubtasks;
    std::array<int, NUM_STEPS> m_completedSubtasks;

    void initializeStepWeights();
    bool parsePostProofOutput(const QString& output);
    void generateSyntheticCounts();
    QString getJobNumberForProject(const QString& project);
    QString getPostageForProject(const QString& project);
    void runProofRegenScript(const QString& jobType, const QStringList& files, int version);
    bool confirmOverwrite(const QString& year, const QString& month, const QString& week);
    bool verifyScript(const QString& scriptPath, const QString& defaultPath, QString& resolvedPath);
    bool validateFileOperation(const QString& operation, const QString& sourcePath, const QString& destPath);
};

#endif // JOBCONTROLLER_H
