#ifndef JOBDATA_H
#define JOBDATA_H

#include <QString>
#include <QMap>

class JobData
{
public:
    JobData();

    // Core job identification
    QString year;
    QString month;
    QString week;

    // Job numbers for different types
    QString cbcJobNumber;
    QString excJobNumber;
    QString inactiveJobNumber;
    QString ncwoJobNumber;
    QString prepifJobNumber;

    // Postage values
    QString cbc2Postage;
    QString cbc3Postage;
    QString excPostage;
    QString inactivePOPostage;
    QString inactivePUPostage;
    QString ncwo1APostage;
    QString ncwo2APostage;
    QString ncwo1APPostage;
    QString ncwo2APPostage;
    QString prepifPostage;

    // Workflow progress
    bool isOpenIZComplete;
    bool isRunInitialComplete;
    bool isRunPreProofComplete;
    bool isOpenProofFilesComplete;
    bool isRunPostProofComplete;
    bool isOpenPrintFilesComplete;
    bool isRunPostPrintComplete;

    // Step completion for database storage
    int step0_complete;
    int step1_complete;
    int step2_complete;
    int step3_complete;
    int step4_complete;
    int step5_complete;
    int step6_complete;
    int step7_complete;
    int step8_complete;

    // Helper methods
    bool isValid() const;
    bool hasEmptyJobNumbers() const;
    QString getJobNumberForJobType(const QString& jobType) const;
    void updateStepsFromFlags();
    void updateFlagsFromSteps();
    void reset();
};

#endif // JOBDATA_H
