#ifndef TMTARRAGONFILEMANAGER_H
#define TMTARRAGONFILEMANAGER_H

#include <QString>
#include <QSettings>
#include <QDir>

class TMTarragonFileManager
{
public:
    explicit TMTarragonFileManager(QSettings* settings);
    ~TMTarragonFileManager();

    // Base path operations
    QString getBasePath() const;
    bool createBaseDirectory();

    // Specific directory paths
    QString getInputPath() const;
    QString getOutputPath() const;
    QString getArchivePath() const;
    QString getScriptsPath() const;

    // Script paths
    QString getScriptPath(const QString& scriptName) const;

    // Directory operations
    bool ensureDirectoriesExist();

    // Settings access
    QSettings* getSettings() const { return m_settings; }

private:
    QSettings* m_settings;

    // Path constants
    static const QString BASE_PATH;
    static const QString INPUT_SUBDIR;
    static const QString OUTPUT_SUBDIR;
    static const QString ARCHIVE_SUBDIR;
    static const QString SCRIPTS_PATH;
};

#endif // TMTARRAGONFILEMANAGER_H
