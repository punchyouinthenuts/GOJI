#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <QString>
#include <QStringList>
#include <QDir>
#include <QRegularExpression>
#include <QUrl>
#include <QLocale>
#include <QHash>
#include <cfloat>
#include <climits>

/**
 * @brief Static utility class for input validation
 *
 * This class provides methods for validating different types of
 * user input, file paths, numbers, and other common data types.
 */
class Validator
{
public:
    /**
     * @brief Validate if a string is not empty
     * @param value The string to validate
     * @param allowWhitespace Whether whitespace-only strings are valid
     * @return True if validation passes
     */
    static bool isNotEmpty(const QString& value, bool allowWhitespace = false);

    /**
     * @brief Validate if a string has a minimum length
     * @param value The string to validate
     * @param minLength The minimum required length (must be non-negative)
     * @return True if validation passes
     */
    static bool hasMinLength(const QString& value, int minLength);

    /**
     * @brief Validate if a string has a maximum length
     * @param value The string to validate
     * @param maxLength The maximum allowed length (must be non-negative)
     * @return True if validation passes
     */
    static bool hasMaxLength(const QString& value, int maxLength);

    /**
     * @brief Validate if a string matches a regular expression pattern
     * @param value The string to validate
     * @param regex The compiled regular expression to match against
     * @return True if validation passes
     */
    static bool matchesPattern(const QString& value, const QRegularExpression& regex);

    /**
     * @brief Validate if a string matches a regular expression pattern string
     * @param value The string to validate
     * @param pattern The regular expression pattern string
     * @return True if validation passes
     * @note This overload compiles the pattern each time. For performance-critical code,
     *       consider pre-compiling the pattern and using the QRegularExpression overload.
     */
    static bool matchesPattern(const QString& value, const QString& pattern);

    /**
     * @brief Validate if a string is a valid integer
     * @param value The string to validate
     * @param min The minimum allowed value (optional)
     * @param max The maximum allowed value (optional)
     * @return True if validation passes
     */
    static bool isValidInteger(const QString& value, int min = INT_MIN, int max = INT_MAX);

    /**
     * @brief Validate if a string is a valid floating-point number
     * @param value The string to validate
     * @param min The minimum allowed value (optional)
     * @param max The maximum allowed value (optional)
     * @return True if validation passes
     */
    static bool isValidDouble(const QString& value, double min = -DBL_MAX, double max = DBL_MAX);

    /**
     * @brief Validate if a string is a valid currency amount
     * @param value The string to validate
     * @param locale The locale for currency format (optional)
     * @param allowNegative Whether negative values are allowed (default: true)
     * @return True if validation passes
     */
    static bool isValidCurrency(const QString& value, const QLocale& locale = QLocale::system(),
                                bool allowNegative = true);

    /**
     * @brief Validate if a string is a valid file path
     * @param value The string to validate
     * @param mustExist Whether the file must exist
     * @param mustBeReadable Whether the file must be readable
     * @param mustBeWritable Whether the file must be writable
     * @return True if validation passes
     * @note This function works cross-platform but has additional checks for invalid
     *       characters on Windows systems.
     */
    static bool isValidFilePath(const QString& value, bool mustExist = true,
                                bool mustBeReadable = false, bool mustBeWritable = false);

    /**
     * @brief Validate if a string is a valid directory path
     * @param value The string to validate
     * @param mustExist Whether the directory must exist
     * @param mustBeReadable Whether the directory must be readable
     * @param mustBeWritable Whether the directory must be writable
     * @return True if validation passes
     * @note This function works cross-platform but has additional checks for invalid
     *       characters on Windows systems.
     */
    static bool isValidDirectoryPath(const QString& value, bool mustExist = true,
                                     bool mustBeReadable = false, bool mustBeWritable = false);

    /**
     * @brief Validate if a string is a valid URL
     * @param value The string to validate
     * @param schemes List of allowed schemes (e.g., "http", "https")
     *                Default is {"http", "https"}. If empty, any valid scheme is allowed.
     * @return True if validation passes
     */
    static bool isValidUrl(const QString& value, const QStringList& schemes = {"http", "https"});

    /**
     * @brief Validate if a string is a valid email address
     * @param value The string to validate
     * @return True if validation passes
     * @note This uses a simplified validation that may not catch all edge cases.
     *       For mission-critical email validation, consider using a specialized library
     *       or verify with a confirmation email.
     */
    static bool isValidEmail(const QString& value);

    /**
     * @brief Validate if a string is a valid date
     * @param value The string to validate
     * @param format The expected date format (e.g., "yyyy-MM-dd")
     * @return True if validation passes
     */
    static bool isValidDate(const QString& value, const QString& format = "yyyy-MM-dd");

    /**
     * @brief Validate if a string is a valid time
     * @param value The string to validate
     * @param format The expected time format (e.g., "hh:mm:ss")
     * @return True if validation passes
     */
    static bool isValidTime(const QString& value, const QString& format = "hh:mm:ss");

    /**
     * @brief Validate if a string is a valid date and time
     * @param value The string to validate
     * @param format The expected date and time format (e.g., "yyyy-MM-dd hh:mm:ss")
     * @return True if validation passes
     */
    static bool isValidDateTime(const QString& value, const QString& format = "yyyy-MM-dd hh:mm:ss");

    /**
     * @brief Format a string as currency
     * @param value The string to format
     * @param locale The locale for currency format (optional)
     * @param symbol The currency symbol (optional)
     * @param decimals The number of decimal places (optional)
     * @return The formatted currency string
     */
    static QString formatAsCurrency(const QString& value, const QLocale& locale = QLocale::system(),
                                    const QString& symbol = "$", int decimals = 2);

    /**
     * @brief Sanitize a string for safe use in file paths
     * @param value The string to sanitize
     * @return The sanitized string
     * @note This follows Windows file naming conventions which are more restrictive.
     *       It will produce valid filenames for all platforms.
     */
    static QString sanitizeForFilePath(const QString& value);

    /**
     * @brief Sanitize a string for safe use in database queries
     * @param value The string to sanitize
     * @param dbType Optional database type to use specific escaping rules
     * @return The sanitized string
     * @note This is a basic escape function and not a replacement for prepared statements.
     *       ALWAYS use prepared statements when possible for database queries.
     */
    static QString sanitizeForDatabase(const QString& value, const QString& dbType = "generic");

    /**
     * @brief Sanitize a string for safe use in HTML
     * @param value The string to sanitize
     * @return The sanitized string with HTML entities
     */
    static QString sanitizeForHtml(const QString& value);

    /**
     * @brief Sanitize a string for safe use in JSON
     * @param value The string to sanitize
     * @return The sanitized string
     */
    static QString sanitizeForJson(const QString& value);

private:
    // Private static regex patterns for thread-safety
    static const QRegularExpression emailRegex;
    static const QRegularExpression invalidFileCharsRegex;
    static const QRegularExpression currencyCleanupRegex;
};

#endif // VALIDATOR_H
