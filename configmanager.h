#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QMutex>
#include <memory>

/**
 * @brief Singleton class for managing application configuration
 *
 * This class provides a centralized way to manage application settings
 * and configuration, with support for:
 * - Default values
 * - Multiple configuration sections
 * - Config file loading/saving
 * - Dynamic updates
 */
class ConfigManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the ConfigManager instance
     */
    static ConfigManager& instance();

    /**
     * @brief Initialize with default settings file
     * @param organization Organization name for QSettings
     * @param application Application name for QSettings
     * @param configFilePath Optional path to config file
     */
    void initialize(const QString& organization, const QString& application,
                    const QString& configFilePath = QString());

    /**
     * @brief Get a configuration value
     * @param key The setting key
     * @param defaultValue The default value if key not found
     * @return The setting value or defaultValue if not found
     */
    QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant()) const;

    /**
     * @brief Set a configuration value
     * @param key The setting key
     * @param value The value to set
     */
    void setValue(const QString& key, const QVariant& value);

    /**
     * @brief Check if a key exists in the configuration
     * @param key The key to check
     * @return True if the key exists
     */
    bool containsKey(const QString& key) const;

    /**
     * @brief Get a string value
     * @param key The setting key
     * @param defaultValue The default value if key not found
     * @return The string value or defaultValue if not found
     */
    QString getString(const QString& key, const QString& defaultValue = QString()) const;

    /**
     * @brief Get an integer value
     * @param key The setting key
     * @param defaultValue The default value if key not found
     * @return The integer value or defaultValue if not found
     */
    int getInt(const QString& key, int defaultValue = 0) const;

    /**
     * @brief Get a double value
     * @param key The setting key
     * @param defaultValue The default value if key not found
     * @return The double value or defaultValue if not found
     */
    double getDouble(const QString& key, double defaultValue = 0.0) const;

    /**
     * @brief Get a boolean value
     * @param key The setting key
     * @param defaultValue The default value if key not found
     * @return The boolean value or defaultValue if not found
     */
    bool getBool(const QString& key, bool defaultValue = false) const;

    /**
     * @brief Get a string list value
     * @param key The setting key
     * @param defaultValue The default value if key not found
     * @return The string list or defaultValue if not found
     */
    QStringList getStringList(const QString& key, const QStringList& defaultValue = QStringList()) const;

    /**
     * @brief Get the base path for the application or a specific module
     * @param module The module name (e.g., "RAC", "TM") or empty for global
     * @return The base path
     */
    QString getBasePath(const QString& module = QString()) const;

    /**
     * @brief Get a file path from configuration with dynamic substitution
     * @param key The key for the path
     * @param defaultPath Default path if key not found
     * @param createIfMissing Create the directory if it doesn't exist
     * @return The resolved file path
     */
    QString getPath(const QString& key, const QString& defaultPath = QString(), bool createIfMissing = false) const;

    /**
     * @brief Set default values for settings
     * @param defaults Map of default key-value pairs
     */
    void setDefaults(const QMap<QString, QVariant>& defaults);

    /**
     * @brief Save all settings to the configuration file
     */
    void save();

    /**
     * @brief Reload settings from the configuration file
     */
    void reload();

    /**
     * @brief Get direct access to the QSettings object (use with caution)
     * @return Pointer to the QSettings object
     */
    QSettings* getSettings() const;

signals:
    /**
     * @brief Signal emitted when a configuration value changes
     * @param key The key that was changed
     * @param value The new value
     */
    void valueChanged(const QString& key, const QVariant& value);

private:
    // Private constructor for singleton
    ConfigManager();
    ~ConfigManager();

    // Disable copy
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // Internal data
    QSettings* m_settings;
    QMap<QString, QVariant> m_defaults;
    bool m_initialized;

    // Thread safety
    mutable QMutex m_mutex;

    // Helper methods
    QString resolvePath(const QString& path) const;
};

#endif // CONFIGMANAGER_H
