#ifndef ARCHIVEUTILS_H
#define ARCHIVEUTILS_H

#include <QString>
#include <QVector>

/**
 * @brief Lightweight description of an entry inside a ZIP archive.
 */
struct ZipEntry {
    QString pathInArchive;  // e.g. "reports/summary.xlsx"
    quint64 size = 0;       // 0 for directories
    bool isDir = false;
};

/**
 * @brief True if filePath has a .zip (case-insensitive) extension.
 */
bool isZip(const QString& filePath);

/**
 * @brief Enumerate entries inside a ZIP using PowerShell/.NET (no extraction).
 * @param zipPath Full path to the .zip file
 * @param err Optional error string (set on failure)
 * @return List of entries (empty on failure)
 *
 * Note: Requires PowerShell (present on modern Windows).
 */
QVector<ZipEntry> listZipEntries(const QString& zipPath, QString* err = nullptr);

/**
 * @brief Extract a ZIP to an existing destination directory (recursively).
 * @param zipPath Full path to the .zip file
 * @param destDir Destination directory (created beforehand if needed)
 * @param err Optional error string (set on failure)
 * @return true on success, false on failure
 *
 * Implementation calls PowerShell Expand-Archive -Force.
 */
bool extractZipToDirectory(const QString& zipPath, const QString& destDir, QString* err = nullptr);

#endif // ARCHIVEUTILS_H
