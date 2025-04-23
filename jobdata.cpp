#include "jobdata.h"

JobData::JobData()
{
    // Initialize with default values
    reset();
}

bool JobData::isValid() const
{
    return !year.isEmpty() && !month.isEmpty() && !week.isEmpty() && !hasEmptyJobNumbers();
}

bool JobData::hasEmptyJobNumbers() const
{
    return cbcJobNumber.isEmpty() || excJobNumber.isEmpty() ||
           inactiveJobNumber.isEmpty() || ncwoJobNumber.isEmpty() ||
           prepifJobNumber.isEmpty();
}

QString JobData::getJobNumberForJobType(const QString& jobType) const
{
    if (jobType == "CBC") return cbcJobNumber;
    if (jobType == "EXC") return excJobNumber;
    if (jobType == "INACTIVE") return inactiveJobNumber;
    if (jobType == "NCWO") return ncwoJobNumber;
    if (jobType == "PREPIF") return prepifJobNumber;
    return QString();
}

void JobData::updateStepsFromFlags()
{
    step0_complete = isOpenIZComplete ? 1 : 0;
    step1_complete = isRunInitialComplete ? 1 : 0;
    step2_complete = isRunPreProofComplete ? 1 : 0;
    step3_complete = isRunPreProofComplete ? 1 : 0; // Same as step2
    step4_complete = isOpenProofFilesComplete ? 1 : 0;
    step5_complete = isRunPostProofComplete ? 1 : 0;
    // step6_complete is set externally (proof approval)
    step7_complete = isOpenPrintFilesComplete ? 1 : 0;
    step8_complete = isRunPostPrintComplete ? 1 : 0;
}

void JobData::updateFlagsFromSteps()
{
    isOpenIZComplete = step0_complete == 1;
    isRunInitialComplete = step1_complete == 1;
    isRunPreProofComplete = step2_complete == 1 && step3_complete == 1;
    isOpenProofFilesComplete = step4_complete == 1;
    isRunPostProofComplete = step5_complete == 1;
    isOpenPrintFilesComplete = step7_complete == 1;
    isRunPostPrintComplete = step8_complete == 1;
}

void JobData::reset()
{
    year = "";
    month = "";
    week = "";

    cbcJobNumber = "";
    excJobNumber = "";
    inactiveJobNumber = "";
    ncwoJobNumber = "";
    prepifJobNumber = "";

    cbc2Postage = "";
    cbc3Postage = "";
    excPostage = "";
    inactivePOPostage = "";
    inactivePUPostage = "";
    ncwo1APostage = "";
    ncwo2APostage = "";
    ncwo1APPostage = "";
    ncwo2APPostage = "";
    prepifPostage = "";

    isOpenIZComplete = false;
    isRunInitialComplete = false;
    isRunPreProofComplete = false;
    isOpenProofFilesComplete = false;
    isRunPostProofComplete = false;
    isOpenPrintFilesComplete = false;
    isRunPostPrintComplete = false;

    step0_complete = 0;
    step1_complete = 0;
    step2_complete = 0;
    step3_complete = 0;
    step4_complete = 0;
    step5_complete = 0;
    step6_complete = 0;
    step7_complete = 0;
    step8_complete = 0;
}
