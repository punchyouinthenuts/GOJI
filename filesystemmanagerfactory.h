#ifndef FILESYSTEMMANAGERFACTORY_H
#define FILESYSTEMMANAGERFACTORY_H

#include "basefilesystemmanager.h"
#include "tmweeklypcfilemanager.h"
#include <QSettings>
#include <QString>

/**
 * @brief Tab types in the application
 */
enum class TabType {
    TMWeeklyPC,
    TMWeeklyPacket,
    TMTerm
    // Add more tab types as needed
};

/**
 * @brief Factory class for creating file system managers
 *
 * This class provides static methods to create file system managers
 * for different tabs/modules in the application.
 */
class FileSystemManagerFactory
{
public:
    /**
     * @brief Create a file system manager for a specific tab
     * @param type Tab type
     * @param settings Application settings
     * @return Pointer to a file system manager, or nullptr if type is unsupported
     */
    static BaseFileSystemManager* createFileManager(TabType type, QSettings* settings)
    {
        switch (type) {
        case TabType::TMWeeklyPC:
            return new TMWeeklyPCFileManager(settings);

            // Add more cases as new tab types are implemented
            // case TabType::TMWeeklyPacket:
            //     return new TMWeeklyPacketFileManager(settings);
            // case TabType::TMTerm:
            //     return new TMTermFileManager(settings);

        default:
            return nullptr;
        }
    }

    /**
     * @brief Create a file system manager based on tab name
     * @param tabName Name of the tab
     * @param settings Application settings
     * @return Pointer to a file system manager, or nullptr if tab name is unsupported
     */
    static BaseFileSystemManager* createFileManager(const QString& tabName, QSettings* settings)
    {
        if (tabName == "TM WEEKLY PC") {
            return new TMWeeklyPCFileManager(settings);
        }
        // Add more conditions as new tab types are implemented
        // else if (tabName == "TM WEEKLY PACK/IDO") {
        //     return new TMWeeklyPacketFileManager(settings);
        // }
        // else if (tabName == "TM TERM") {
        //     return new TMTermFileManager(settings);
        // }

        return nullptr;
    }
};

#endif // FILESYSTEMMANAGERFACTORY_H
