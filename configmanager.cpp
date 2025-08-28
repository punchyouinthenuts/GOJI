#include "configmanager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDebug>

// Static instance
ConfigManager& ConfigManager::instance()
{
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager()
    : QObject(nullptr),
    m_settings(nullptr),
    m_initialized(false)
{
    qDebug() << "ConfigManager constructor called";
}

ConfigManager::~ConfigManager()
{
    if (m_settings) {
        m_settings->sync();
        delete m_settings;
    }
}

void ConfigManager::initialize(const QString& organization, const QString& application,
                               const QString& configFilePath)
{
    try {
        QMutexLocker locker(&m_mutex);
        qDebug() << "ConfigManager::initialize - Creating settings";

        // Clean up existing settings if any
        if (m_settings) {
            m_settings->sync();
            delete m_settings;
            m_settings = nullptr;
        }

        // Set application information
        QCoreApplication::setApplicationName(application);
        QCoreApplication::setOrganizationName(organization);

        // Create settings object
        if (!configFilePath.isEmpty()) {
            m_settings = new QSettings(configFilePath, QSettings::IniFormat);
            qDebug() << "ConfigManager::initialize - Using config file path:" << configFilePath;
        } else {
            m_settings = new QSettings(organization, application);
            qDebug() << "ConfigManager::initialize - Using organization/application settings";
        }

        qDebug() << "ConfigManager::initialize - Settings path:" << m_settings->fileName();

        // Ensure the settings directory exists
        QFileInfo fileInfo(m_settings->fileName());
        QDir dir = fileInfo.dir();
        if (!dir.exists()) {
            qDebug() << "ConfigManager::initialize - Creating settings directory";
            if (!dir.mkpath(".")) {
                qDebug() << "ConfigManager::initialize - Failed to create settings directory";
                return;
            }
        }

        m_initialized = true;

        // Set default values for shared paths
        QMap<QString, QVariant> defaultPaths;
        defaultPaths["DatabasePath"] = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Goji/SQL";
        defaultPaths["LogPath"] = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";

        // RAC paths
        defaultPaths["RAC/BasePath"] = "C:/Goji/RAC";
        defaultPaths["RAC/ScriptsPath"] = "C:/Goji/Scripts/RAC/WEEKLIES";
        defaultPaths["RAC/IZPath"] = "${RAC/BasePath}/WEEKLY/INPUTZIP";
        defaultPaths["RAC/ProofPath"] = "${RAC/BasePath}/${JobType}/JOB/PROOF";
        defaultPaths["RAC/PrintPath"] = "${RAC/BasePath}/${JobType}/JOB/PRINT";
        defaultPaths["RAC/PreProofScript"] = "${RAC/ScriptsPath}/02RUNSECOND.bat";
        defaultPaths["RAC/PostProofScript"] = "${RAC/ScriptsPath}/04POSTPROOF.py";
        defaultPaths["RAC/PostPrintScript"] = "${RAC/ScriptsPath}/05POSTPRINT.ps1";

        // TM WEEKLY PC paths
        defaultPaths["TM/BasePath"] = "C:/Goji/TRACHMAR";
        defaultPaths["TM/ScriptsPath"] = "C:/Goji/Scripts/TRACHMAR/WEEKLY PC";
        defaultPaths["TM/InputPath"] = "${TM/BasePath}/WEEKLY PC/JOB/INPUT";
        defaultPaths["TM/OutputPath"] = "${TM/BasePath}/WEEKLY PC/JOB/OUTPUT";
        defaultPaths["TM/ProofPath"] = "${TM/BasePath}/WEEKLY PC/JOB/PROOF";
        defaultPaths["TM/PrintPath"] = "${TM/BasePath}/WEEKLY PC/JOB/PRINT";
        defaultPaths["TM/ArtPath"] = "${TM/BasePath}/WEEKLY PC/ART";
        defaultPaths["TM/InitialScript"] = "${TM/ScriptsPath}/01INITIAL.py";
        defaultPaths["TM/ProofDataScript"] = "${TM/ScriptsPath}/02PROOFDATA.py";
        defaultPaths["TM/WeeklyMergedScript"] = "${TM/ScriptsPath}/03WEEKLYMERGED.py";
        defaultPaths["TM/PostPrintScript"] = "${TM/ScriptsPath}/04POSTPRINT.py";

        // Apply defaults
        setDefaults(defaultPaths);

        qDebug() << "ConfigManager::initialize - Successfully initialized";
    }
    catch (const std::exception& e) {
        qDebug() << "ConfigManager::initialize - Exception:" << e.what();
    }
    catch (...) {
        qDebug() << "ConfigManager::initialize - Unknown exception";
    }
}

QVariant ConfigManager::getValue(const QString& key, const QVariant& defaultValue) const
{
    try {
        QMutexLocker locker(&m_mutex);

        if (!m_initialized || !m_settings) {
            qDebug() << "ConfigManager::getValue - Warning: Attempted to get value before initialization:" << key;
            return defaultValue;
        }

        if (m_settings->contains(key)) {
            return m_settings->value(key);
        } else if (m_defaults.contains(key)) {
            return m_defaults[key];
        } else {
            return defaultValue;
        }
    }
    catch (const std::exception& e) {
        qDebug() << "ConfigManager::getValue - Exception:" << e.what();
        return defaultValue;
    }
    catch (...) {
        qDebug() << "ConfigManager::getValue - Unknown exception";
        return defaultValue;
    }
}

void ConfigManager::setValue(const QString& key, const QVariant& value)
{
    try {
        QMutexLocker locker(&m_mutex);

        if (!m_initialized || !m_settings) {
            qDebug() << "ConfigManager::setValue - Warning: Attempted to set value before initialization:" << key;
            return;
        }

        QVariant oldValue = getValue(key);
        if (oldValue != value) {
            m_settings->setValue(key, value);
            emit valueChanged(key, value);
        }
    }
    catch (const std::exception& e) {
        qDebug() << "ConfigManager::setValue - Exception:" << e.what();
    }
    catch (...) {
        qDebug() << "ConfigManager::setValue - Unknown exception";
    }
}

bool ConfigManager::containsKey(const QString& key) const
{
    try {
        QMutexLocker locker(&m_mutex);

        if (!m_initialized || !m_settings) {
            return false;
        }

        return m_settings->contains(key) || m_defaults.contains(key);
    }
    catch (...) {
        qDebug() << "ConfigManager::containsKey - Exception occurred";
        return false;
    }
}

QString ConfigManager::getString(const QString& key, const QString& defaultValue) const
{
    return getValue(key, defaultValue).toString();
}

int ConfigManager::getInt(const QString& key, int defaultValue) const
{
    return getValue(key, defaultValue).toInt();
}

double ConfigManager::getDouble(const QString& key, double defaultValue) const
{
    return getValue(key, defaultValue).toDouble();
}

bool ConfigManager::getBool(const QString& key, bool defaultValue) const
{
    return getValue(key, defaultValue).toBool();
}

QStringList ConfigManager::getStringList(const QString& key, const QStringList& defaultValue) const
{
    QVariant value = getValue(key);
    if (value.isValid()) {
        return value.toStringList();
    }
    return defaultValue;
}

QString ConfigManager::getBasePath(const QString& module) const
{
    if (module.isEmpty()) {
        return getString("BasePath", "C:/Goji");
    } else {
        return getString(module + "/BasePath", "C:/Goji/" + module);
    }
}

QString ConfigManager::getPath(const QString& key, const QString& defaultPath, bool createIfMissing) const
{
    QString path = getString(key, defaultPath);

    // Resolve any variable substitutions
    path = resolvePath(path);

    // Create directory if requested and it doesn't exist
    if (createIfMissing) {
        QDir dir(path);
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                qDebug() << "ConfigManager::getPath - Failed to create directory:" << path;
                return path;
            }
        }
    }

    return path;
}

void ConfigManager::setDefaults(const QMap<QString, QVariant>& defaults)
{
    try {
        QMutexLocker locker(&m_mutex);

        // Add all defaults to our map
        for (auto it = defaults.constBegin(); it != defaults.constEnd(); ++it) {
            m_defaults[it.key()] = it.value();

            // If the key doesn't exist in settings, initialize it with the default
            if (!m_settings->contains(it.key())) {
                m_settings->setValue(it.key(), it.value());
            }
        }
    }
    catch (const std::exception& e) {
        qDebug() << "ConfigManager::setDefaults - Exception:" << e.what();
    }
    catch (...) {
        qDebug() << "ConfigManager::setDefaults - Unknown exception";
    }
}

void ConfigManager::save()
{
    try {
        QMutexLocker locker(&m_mutex);

        if (m_initialized && m_settings) {
            m_settings->sync();
        }
    }
    catch (...) {
        qDebug() << "ConfigManager::save - Exception occurred";
    }
}

void ConfigManager::reload()
{
    try {
        QMutexLocker locker(&m_mutex);

        if (m_initialized && m_settings) {
            m_settings->sync();
        }
    }
    catch (...) {
        qDebug() << "ConfigManager::reload - Exception occurred";
    }
}

QSettings* ConfigManager::getSettings() const
{
    if (!m_initialized) {
        qDebug() << "ConfigManager::getSettings - Warning: Calling getSettings() before initialize()";
        return nullptr;
    }

    return m_settings;
}

QString ConfigManager::resolvePath(const QString& path) const
{
    try {
        QString result = path;

        // Static regex object - compiled once and reused
        static const QRegularExpression variableRegex(QStringLiteral("\\$\\{([^}]+)\\}"));

        // Keep track of variables we've already processed to avoid infinite recursion
        QSet<QString> processedVars;
        int maxIterations = 10; // Safety limit

        // Perform multiple passes to handle nested variables
        for (int i = 0; i < maxIterations && result.contains("${"); i++) {
            QRegularExpressionMatchIterator it = variableRegex.globalMatch(result);
            if (!it.hasNext()) {
                break;
            }
            while (it.hasNext()) {
                QRegularExpressionMatch match = it.next();
                QString variableName = match.captured(1);

                // Skip if already processed to avoid infinite recursion
                if (processedVars.contains(variableName)) {
                    continue;
                }

                QString variableValue = getString(variableName, "");
                if (!variableValue.isEmpty()) {
                    result.replace("${" + variableName + "}", variableValue);
                    processedVars.insert(variableName);
                }
            }
        }
        return result;
    }
    catch (...) {
        qDebug() << "ConfigManager::resolvePath - Exception occurred";
        return path;
    }
}
