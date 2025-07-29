#ifndef FILESYSTEMMANAGERFACTORY_H
#define FILESYSTEMMANAGERFACTORY_H

#include "basefilesystemmanager.h"
#include "tmweeklypcfilemanager.h"
#include "tmtermfilemanager.h"
// NOTE: Factory pattern deprecated per ADR-001 in favor of direct instantiation
// with ConfigManager integration. This header retained for legacy compatibility
// but new file managers should use direct instantiation pattern.
// TMHealthy, TMFLER, and TMTarragon use direct instantiation and are not
// included in this factory interface.
#include <QSettings>
#include <QString>

/**
 * @brief Tab types in the application
 */
enum class TabType {
    TMWeeklyPC,
    TMWeeklyPacket,  // No file manager exists - PIDO uses different pattern
    TMTerm
    // TMHealthyBeginnings, TMFLER, TMTarragon intentionally excluded
    // Add more tab types as needed
};

/**
 * @brief Factory class for creating file system managers
 *
 * This class provides static methods to create file system managers
 * for different tabs/modules in the application.
 * 
 * DEPRECATED: Use direct instantiation with ConfigManager::instance().getSettings()
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

        case TabType::TMTerm:
            return new TMTermFileManager(settings);

            // Add more cases as new tab types are implemented
            // case TabType::TMWeeklyPacket:
            //     return new TMWeeklyPacketFileManager(settings);

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
        else if (tabName == "TM TERM") {
            return new TMTermFileManager(settings);
        }
        // Add more conditions as new tab types are implemented
        // else if (tabName == "TM WEEKLY PACK/IDO") {
        //     return new TMWeeklyPacketFileManager(settings);
        // }

        return nullptr;
    }
};

#endif // FILESYSTEMMANAGERFACTORY_H
