#include "validator.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextDocument>
#include <QtGlobal>
#include <cfloat>
#include <climits>

// Initialize static regex patterns
const QRegularExpression Validator::emailRegex(
    "^[^@\\s]+@[^@\\s]+\\.[^@\\s]{2,}$"
    );

const QRegularExpression Validator::invalidFileCharsRegex(
    "[\\\\/:*?\"<>|]"
    );

const QRegularExpression Validator::currencyCleanupRegex(
    "[^0-9\\.,+-]"
    );

bool Validator::isNotEmpty(const QString& value, bool allowWhitespace)
{
    if (allowWhitespace) {
        return !value.isEmpty();
    } else {
        return !value.trimmed().isEmpty();
    }
}

bool Validator::hasMinLength(const QString& value, int minLength)
{
    if (minLength < 0) {
        return false; // Invalid parameter
    }
    return value.length() >= minLength;
}

bool Validator::hasMaxLength(const QString& value, int maxLength)
{
    if (maxLength < 0) {
        return false; // Invalid parameter
    }
    return value.length() <= maxLength;
}

bool Validator::matchesPattern(const QString& value, const QRegularExpression& regex)
{
    QRegularExpressionMatch match = regex.match(value);
    return match.hasMatch();
}

bool Validator::matchesPattern(const QString& value, const QString& pattern)
{
    QRegularExpression regex(pattern);
    return matchesPattern(value, regex);
}

bool Validator::isValidInteger(const QString& value, int min, int max)
{
    if (min > max) {
        return false; // Invalid range
    }

    bool ok;
    int intValue = value.toInt(&ok);
    return ok && intValue >= min && intValue <= max;
}

bool Validator::isValidDouble(const QString& value, double min, double max)
{
    if (min > max) {
        return false; // Invalid range
    }

    bool ok;
    double doubleValue = value.toDouble(&ok);
    return ok && doubleValue >= min && doubleValue <= max;
}

bool Validator::isValidCurrency(const QString& value, const QLocale& locale, bool allowNegative)
{
    // Try locale-aware parsing first
    bool ok;
    double amount = locale.toDouble(value, &ok);

    if (!ok) {
        // If direct parsing fails, try cleaning the string
        QString cleaned = value;
        cleaned.remove(currencyCleanupRegex);

        // Replace locale decimal point if needed
        if (locale.decimalPoint() != '.') {
            cleaned.replace(locale.decimalPoint(), QChar('.'));
        }

        amount = cleaned.toDouble(&ok);
        if (!ok) {
            return false;
        }
    }

    // Check if negative values are allowed
    return allowNegative || amount >= 0.0;
}

bool Validator::isValidFilePath(const QString& value, bool mustExist,
                                bool mustBeReadable, bool mustBeWritable)
{
    if (value.isEmpty()) {
        return false;
    }

    // Platform-specific checks (stricter on Windows)
#ifdef Q_OS_WIN
    if (value.contains(invalidFileCharsRegex)) {
        return false;
    }
#endif

    if (!mustExist) {
        // Check if parent directory exists and is writable for new files
        QFileInfo parentInfo(QFileInfo(value).dir().path());
        return parentInfo.exists() && parentInfo.isDir() && parentInfo.isWritable();
    }

    QFileInfo fileInfo(value);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    if (mustBeReadable && !fileInfo.isReadable()) {
        return false;
    }

    if (mustBeWritable && !fileInfo.isWritable()) {
        return false;
    }

    return true;
}

bool Validator::isValidDirectoryPath(const QString& value, bool mustExist,
                                     bool mustBeReadable, bool mustBeWritable)
{
    if (value.isEmpty()) {
        return false;
    }

    // Platform-specific checks (stricter on Windows)
#ifdef Q_OS_WIN
    if (value.contains(invalidFileCharsRegex)) {
        return false;
    }
#endif

    if (!mustExist) {
        // Check if parent directory exists and is writable
        QFileInfo parentInfo(QFileInfo(value).dir().path());
        return parentInfo.exists() && parentInfo.isDir() && parentInfo.isWritable();
    }

    QFileInfo dirInfo(value);
    if (!dirInfo.exists() || !dirInfo.isDir()) {
        return false;
    }

    if (mustBeReadable && !dirInfo.isReadable()) {
        return false;
    }

    if (mustBeWritable && !dirInfo.isWritable()) {
        return false;
    }

    return true;
}

bool Validator::isValidUrl(const QString& value, const QStringList& schemes)
{
    QUrl url(value);
    if (!url.isValid()) {
        return false;
    }

    // Check scheme if specified
    if (!schemes.isEmpty() && !schemes.contains(url.scheme(), Qt::CaseInsensitive)) {
        return false;
    }

    // Minimal URL validation: must have a scheme and host
    return !url.scheme().isEmpty() && !url.host().isEmpty();
}

bool Validator::isValidEmail(const QString& value)
{
    return emailRegex.match(value).hasMatch();
}

bool Validator::isValidDate(const QString& value, const QString& format)
{
    QDateTime dateTime = QDateTime::fromString(value, format);
    return dateTime.isValid();
}

bool Validator::isValidTime(const QString& value, const QString& format)
{
    QTime time = QTime::fromString(value, format);
    return time.isValid();
}

bool Validator::isValidDateTime(const QString& value, const QString& format)
{
    QDateTime dateTime = QDateTime::fromString(value, format);
    return dateTime.isValid();
}

QString Validator::formatAsCurrency(const QString& value, const QLocale& locale,
                                    const QString& symbol, int decimals)
{
    bool ok;
    double amount = locale.toDouble(value, &ok);

    if (!ok) {
        return value; // Return the original if we can't parse it
    }

    return locale.toCurrencyString(amount, symbol, decimals);
}

QString Validator::sanitizeForFilePath(const QString& value)
{
    // Replace characters that are invalid in file names
    QString sanitized = value;
    sanitized.replace(invalidFileCharsRegex, "_");

    // Trim leading/trailing whitespace
    sanitized = sanitized.trimmed();

    // Ensure the string is not empty
    if (sanitized.isEmpty()) {
        sanitized = "unnamed";
    }

    return sanitized;
}

QString Validator::sanitizeForDatabase(const QString& value, const QString& dbType)
{
    QString sanitized = value;

    // Different escaping for different databases
    if (dbType.compare("mysql", Qt::CaseInsensitive) == 0) {
        // MySQL escaping
        sanitized.replace("\\", "\\\\");
        sanitized.replace("'", "\\'");
        sanitized.replace("\"", "\\\"");
        sanitized.replace("\n", "\\n");
        sanitized.replace("\r", "\\r");
        sanitized.replace("\t", "\\t");
        sanitized.replace("\0", "\\0");
    }
    else if (dbType.compare("postgresql", Qt::CaseInsensitive) == 0) {
        // PostgreSQL escaping (single quotes are doubled)
        sanitized.replace("'", "''");
    }
    else {
        // Generic escaping (just single quotes)
        sanitized.replace("'", "''");
    }

    return sanitized;
}

QString Validator::sanitizeForHtml(const QString& value)
{
    // Use Qt's built-in HTML escaping
    return value.toHtmlEscaped();
}

QString Validator::sanitizeForJson(const QString& value)
{
    // Use QJsonValue to properly escape the string for JSON
    QJsonObject tempObject;
    tempObject["value"] = value;
    QJsonDocument doc(tempObject);

    // Extract just the value part without using regex
    QString jsonStr = QString::fromUtf8(doc.toJson());
    int startPos = jsonStr.indexOf("\"value\":\"") + 9;
    int endPos = jsonStr.lastIndexOf("\"");

    if (startPos > 9 && endPos > startPos) {
        return jsonStr.mid(startPos, endPos - startPos);
    }

    // Fallback: manually escape key JSON characters
    QString sanitized = value;
    sanitized.replace(QLatin1String("\\"), QLatin1String("\\\\"));
    sanitized.replace(QLatin1String("\""), QLatin1String("\\\""));
    sanitized.replace(QLatin1String("\n"), QLatin1String("\\n"));
    sanitized.replace(QLatin1String("\r"), QLatin1String("\\r"));
    sanitized.replace(QLatin1String("\t"), QLatin1String("\\t"));
    sanitized.replace(QLatin1String("\b"), QLatin1String("\\b"));
    sanitized.replace(QLatin1String("\f"), QLatin1String("\\f"));

    return sanitized;
}
