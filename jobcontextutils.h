#ifndef JOBCONTEXTUTILS_H
#define JOBCONTEXTUTILS_H

#include <QString>
#include <QSet>

/**
 * @brief Utilities for job context management across GOJI tabs
 * 
 * This namespace provides centralized logic for determining which tabs
 * support job persistence and which are valid job-supporting tabs.
 */
namespace JobContextUtils {

/**
 * @brief Set of all valid job-supporting tab object names
 * 
 * Contains the objectName() values of all tabs that support job operations.
 * This includes both tabs that persist jobs (FOURHANDS, TMWEEKLYPC, etc.)
 * and tabs that support job operations but don't persist (TMWEEKLYPIDO).
 */
static const QSet<QString> validJobTabs = {
    "TMWEEKLYPC",
    "TMWEEKLYPIDO",
    "TMTERM",
    "FOURHANDS",
    "TMTARRAGON",
    "TMFLER",
    "TMHEALTHY",
    "TMBROKEN",
    "TMFARMWORKERS"
};

/**
 * @brief Check if a tab object name represents a valid job tab
 * @param objectName The QWidget::objectName() of the tab to check
 * @return true if the tab supports job operations
 */
inline bool isValidJobTab(const QString& objectName)
{
    return validJobTabs.contains(objectName);
}

/**
 * @brief Check if a tab supports job persistence (save/load)
 * @param objectName The QWidget::objectName() of the tab to check
 * @return true if the tab can save and load jobs from database
 * 
 * Note: Some tabs like TMWEEKLYPIDO are valid job tabs but don't
 * persist jobs to the database.
 */
inline bool supportsJobPersistence(const QString& objectName)
{
    // TMWEEKLYPIDO supports job operations but doesn't persist jobs
    // TMFARMWORKERS doesn't yet support persistence (future implementation)
    return isValidJobTab(objectName) && 
           objectName != "TMWEEKLYPIDO" &&
           objectName != "TMFARMWORKERS";
}

} // namespace JobContextUtils

#endif // JOBCONTEXTUTILS_H
