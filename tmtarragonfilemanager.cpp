#include "tmtarragonfilemanager.h"
#include "logger.h"
#include <QDir>
#include <QStandardPaths>

// Static constants
const QString TMTarragonFileManager::BASE_PATH = "C:/Goji/TRACHMAR/TARRAGON HOMES";
const QString TMTarragonFileManager::INPUT_SUBDIR = "INPUT";
const QString TMTarragonFileManager::OUTPUT_SUBDIR = "OUTPUT";
const QString TMTarragonFileManager::ARCHIVE_SUBDIR = "ARCHIVE";
const QString TMTarragonFileManager::SCRIPTS_PATH = "C:/Goji/scripts/TRACHMAR/TARRAGON HOMES";

TMTarragonFileManager::TMTarragonFileManager(QSettings* settings)
    : m_settings(settings)
{
    // Ensure base directories exist
    ensureDirectoriesExist();
}

TMTarragonFileManager::~TMTarragonFileManager()
{
    // Note: We don't delete m_settings as it's owned by the caller
}

QString TMTarragonFileManager::getBasePath() const
{
    return BASE_PATH;
}

bool TMTarragonFileManager::createBaseDirectory()
{
    QDir dir;
    bool success = dir.mkpath(BASE_PATH);

    if (success) {
        Logger::instance().info("Created TM Tarragon base directory: " + BASE_PATH);
    } else {
        Logger::instance().error("Failed to create TM Tarragon base directory: " + BASE_PATH);
    }

    return success;
}

QString TMTarragonFileManager::getInputPath() const
{
    return QDir(BASE_PATH).filePath(INPUT_SUBDIR);
}

QString TMTarragonFileManager::getOutputPath() const
{
    return QDir(BASE_PATH).filePath(OUTPUT_SUBDIR);
}

QString TMTarragonFileManager::getArchivePath() const
{
    return QDir(BASE_PATH).filePath(ARCHIVE_SUBDIR);
}

QString TMTarragonFileManager::getScriptsPath() const
{
    return SCRIPTS_PATH;
}

QString TMTarragonFileManager::getScriptPath(const QString& scriptName) const
{
    QString fileName;

    if (scriptName.toLower() == "initial" || scriptName == "01INITIAL") {
        fileName = "01INITIAL.py";
    } else if (scriptName.toLower() == "finalstep" || scriptName == "02FINALSTEP") {
        fileName = "02FINALSTEP.py";
    } else {
        // Default case - assume the scriptName is the filename
        fileName = scriptName;
        if (!fileName.endsWith(".py")) {
            fileName += ".py";
        }
    }

    return QDir(SCRIPTS_PATH).filePath(fileName);
}

bool TMTarragonFileManager::ensureDirectoriesExist()
{
    QStringList directories = {
        BASE_PATH,
        getInputPath(),
        getOutputPath(),
        getArchivePath(),
        SCRIPTS_PATH
    };

    bool allSuccess = true;
    QDir dir;

    for (const QString& dirPath : directories) {
        if (!dir.exists(dirPath)) {
            if (dir.mkpath(dirPath)) {
                Logger::instance().info("Created directory: " + dirPath);
            } else {
                Logger::instance().error("Failed to create directory: " + dirPath);
                allSuccess = false;
            }
        }
    }

    return allSuccess;
}
