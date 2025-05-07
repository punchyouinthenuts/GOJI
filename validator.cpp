#include "validator.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <cfloat>
#include <climits>

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
    return value.length() >= minLength;
}

bool Validator::hasMaxLength(const QString& value, int maxLength)
{
    return value.length() <= maxLength;
}

bool Validator::matchesPattern(const QString& value, const QString& pattern)
{
    static const QRegularExpression regex(pattern);
    QRegularExpressionMatch match = regex.match(value);
    return match.hasMatch();
}

bool Validator::isValidInteger(const QString& value, int min, int max)
{
    bool ok;
    int intValue = value.toInt(&ok);
    return ok && intValue >= min && intValue <= max;
}

bool Validator::isValidDouble(const QString& value, double min, double max)
{
    bool ok;
    double doubleValue = value.toDouble(&ok);
    return ok && doubleValue >= min && doubleValue <= max;
}

bool Validator::isValidCurrency(const QString& value, const QLocale& locale)
{
    static const QRegularExpression regex("[^0-9\\.,+-]");
    // Remove currency symbols, spaces, and grouping separators
    QString cleaned = value;
    cleaned.remove(regex);

    // Replace locale decimal point if needed
    if (locale.decimalPoint() != '.') {
        cleaned.replace(locale.decimalPoint(), QChar('.')); // Use QChar for compatibility
    }

    // Now check if it's a valid double
    bool ok;
    double amount = cleaned.toDouble(&ok);
    return ok && amount >= 0.0; // Currencies are typically non-negative
}

bool Validator::isValidFilePath(const QString& value, bool mustExist,
                                bool mustBeReadable, bool mustBeWritable)
{
    static const QRegularExpression invalidChars("[\\\\/:*?\"<>|]");
    // Check if path is empty or contains invalid characters
    if (value.isEmpty() || value.contains(invalidChars)) {
        return false;
    }

    if (!mustExist) {
        return true; // No need to check existence
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
    // Basic email validation using a regex pattern
    static const QRegularExpression emailRegex(
        "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$"
        );

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
    static const QRegularExpression regex("[^0-9\\.,+-]");
    bool ok;
    double amount = value.toDouble(&ok);

    if (!ok) {
        // Try to clean up the string first
        QString cleaned = value;
        cleaned.remove(regex);

        // Replace locale decimal point if needed
        if (locale.decimalPoint() != '.') {
            cleaned.replace(locale.decimalPoint(), QChar('.')); // Use QChar for compatibility
        }

        amount = cleaned.toDouble(&ok);
        if (!ok) {
            return value; // Return the original if we can't parse it
        }
    }

    return locale.toCurrencyString(amount, symbol, decimals);
}

QString Validator::sanitizeForFilePath(const QString& value)
{
    static const QRegularExpression invalidChars("[\\\\/:*?\"<>|]");
    // Replace characters that are invalid in file names
    QString sanitized = value;
    sanitized.replace(invalidChars, "_");

    // Trim leading/trailing whitespace
    sanitized = sanitized.trimmed();

    // Ensure the string is not empty
    if (sanitized.isEmpty()) {
        sanitized = "unnamed";
    }

    return sanitized;
}

QString Validator::sanitizeForDatabase(const QString& value)
{
    // Simple SQL injection prevention: escape single quotes
    QString sanitized = value;
    sanitized.replace("'", "''");

    return sanitized;
}

QString Validator::sanitizeForHtml(const QString& value)
{
    QString sanitized = value;

    // Replace special HTML characters
    sanitized.replace("&", "&amp;");
    sanitized.replace("<", "&lt;");
    sanitized.replace(">", "&gt;");
    sanitized.replace("\"", "&quot;");
    sanitized.replace("'", "&#39;");

    return sanitized;
}

QString Validator::sanitizeForJson(const QString& value)
{
    static const QRegularExpression regex("\"value\":\"(.*?)\"");
    // Use QJsonValue to properly escape the string for JSON
    QJsonObject tempObject;
    tempObject["value"] = value;
    QJsonDocument doc(tempObject);
    QString jsonStr = QString::fromUtf8(doc.toJson());

    // Extract just the value part
    QRegularExpressionMatch match = regex.match(jsonStr);

    if (match.hasMatch()) {
        return match.captured(1);
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
