#include "configmanager.h"
#include "logger.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QStandardPaths>

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
    QMutexLocker locker(&m_mutex);

    // Clean up existing settings if any
    if (m_settings) {
        m_settings->sync();
        delete m_settings;
        m_settings = nullptr;
    }

    // Create settings object
    if (!configFilePath.isEmpty()) {
        m_settings = new QSettings(configFilePath, QSettings::IniFormat);
    } else {
        m_settings = new QSettings(organization, application);
    }

    m_initialized = true;

    // Set default values for paths
    QMap<QString, QVariant> defaultPaths;
    defaultPaths["BasePath"] = "C:/Goji/RAC";
    defaultPaths["ScriptsPath"] = "C:/Goji/Scripts/RAC/WEEKLIES";
    defaultPaths["IZPath"] = "${BasePath}/WEEKLY/INPUTZIP";
    defaultPaths["ProofPath"] = "${BasePath}/${JobType}/JOB/PROOF";
    defaultPaths["PrintPath"] = "${BasePath}/${JobType}/JOB/PRINT";
    defaultPaths["DatabasePath"] = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Goji/SQL";
    defaultPaths["LogPath"] = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    defaultPaths["PreProofScript"] = "${ScriptsPath}/02RUNSECOND.bat";
    defaultPaths["PostProofScript"] = "${ScriptsPath}/04POSTPROOF.py";
    defaultPaths["PostPrintScript"] = "${ScriptsPath}/05POSTPRINT.ps1";

    // Apply defaults
    setDefaults(defaultPaths);

    LOG_INFO(QString("ConfigManager initialized for %1/%2").arg(organization, application));
}

QVariant ConfigManager::getValue(const QString& key, const QVariant& defaultValue) const
{
    QMutexLocker locker(&m_mutex);

    if (!m_initialized || !m_settings) {
        LOG_WARNING(QString("Attempted to get value '%1' before initialization").arg(key));
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

void ConfigManager::setValue(const QString& key, const QVariant& value)
{
    QMutexLocker locker(&m_mutex);

    if (!m_initialized || !m_settings) {
        LOG_WARNING(QString("Attempted to set value '%1' before initialization").arg(key));
        return;
    }

    QVariant oldValue = getValue(key);
    if (oldValue != value) {
        m_settings->setValue(key, value);
        emit valueChanged(key, value);
    }
}

bool ConfigManager::containsKey(const QString& key) const
{
    QMutexLocker locker(&m_mutex);

    if (!m_initialized || !m_settings) {
        return false;
    }

    return m_settings->contains(key) || m_defaults.contains(key);
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

QString ConfigManager::getBasePath() const
{
    return getString("BasePath", "C:/Goji/RAC");
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
                LOG_WARNING(QString("Failed to create directory: %1").arg(path));
            }
        }
    }

    return path;
}

void ConfigManager::setDefaults(const QMap<QString, QVariant>& defaults)
{
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

void ConfigManager::save()
{
    QMutexLocker locker(&m_mutex);

    if (m_initialized && m_settings) {
        m_settings->sync();
    }
}

void ConfigManager::reload()
{
    QMutexLocker locker(&m_mutex);

    if (m_initialized && m_settings) {
        m_settings->sync();
    }
}

QSettings* ConfigManager::getSettings() const
{
    return m_settings;
}

QString ConfigManager::resolvePath(const QString& path) const
{
    QString result = path;

    // Replace ${BasePath} with actual base path
    result.replace("${BasePath}", getBasePath());

    // Replace other variables
    QRegularExpression variableRegex(QStringLiteral("\\$\\{([^}]+)\\}"));
    QRegularExpressionMatchIterator it = variableRegex.globalMatch(result);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString variableName = match.captured(1);
        QString variableValue = getString(variableName, "");

        if (!variableValue.isEmpty()) {
            result.replace("${" + variableName + "}", variableValue);
        }
    }

    return result;
}
