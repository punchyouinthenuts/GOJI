#include "archiveutils.h"
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>

static QString psEscape(const QString& s) {
    // Escape for PowerShell single-quoted string: double any single quote
    QString out = s;
    out.replace("'", "''");
    return out;
}

bool isZip(const QString& filePath) {
    QFileInfo fi(filePath);
    return fi.exists() && fi.isFile() && fi.suffix().compare("zip", Qt::CaseInsensitive) == 0;
}

QVector<ZipEntry> listZipEntries(const QString& zipPath, QString* err) {
    QVector<ZipEntry> entries;

    if (!isZip(zipPath)) {
        if (err) *err = "Not a .zip file or file missing.";
        return entries;
    }

    // Use .NET ZipFile to enumerate entries without extracting.
    // Output format: "<FullName>|<Length>|<IsDir>"
    const QString script =
        "$ErrorActionPreference='Stop';"
        "Add-Type -AssemblyName System.IO.Compression.FileSystem;"
        "$z=[IO.Compression.ZipFile]::OpenRead('" + psEscape(zipPath) + "');"
        "foreach($e in $z.Entries){"
        "  $isDir=$e.FullName.EndsWith('/');"
        "  $len= if($isDir){0}else{$e.Length};"
        "  Write-Output ($e.FullName + '|' + $len + '|' + $isDir)"
        "};"
        "$z.Dispose();";

    QProcess p;
    p.start("powershell.exe", {"-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script});
    if (!p.waitForFinished(30000)) {
        if (err) *err = "PowerShell timed out while enumerating ZIP.";
        return entries;
    }

    const QByteArray out = p.readAllStandardOutput();
    const QByteArray outErr = p.readAllStandardError();
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        if (err) *err = QString("PowerShell failed: %1").arg(QString::fromUtf8(outErr));
        return entries;
    }

    const QStringList lines = QString::fromUtf8(out).split('\n', Qt::SkipEmptyParts);
    entries.reserve(lines.size());
    for (const QString& lineRaw : lines) {
        QString line = lineRaw.trimmed();
        const int p1 = line.indexOf('|');
        const int p2 = (p1 >= 0) ? line.indexOf('|', p1 + 1) : -1;
        if (p1 < 0 || p2 < 0) continue;

        ZipEntry e;
        e.pathInArchive = line.left(p1);
        e.size = line.mid(p1 + 1, p2 - p1 - 1).toULongLong();
        const QString isDirStr = line.mid(p2 + 1);
        e.isDir = (isDirStr.compare("True", Qt::CaseInsensitive) == 0);
        entries.push_back(e);
    }

    return entries;
}

bool extractZipToDirectory(const QString& zipPath, const QString& destDir, QString* err) {
    if (!isZip(zipPath)) {
        if (err) *err = "Not a .zip file or file missing.";
        return false;
    }
    if (destDir.isEmpty()) {
        if (err) *err = "Destination directory is empty.";
        return false;
    }

    // Ensure destination exists
    QDir d(destDir);
    if (!d.exists()) {
        if (!d.mkpath(".")) {
            if (err) *err = QString("Could not create destination: %1").arg(destDir);
            return false;
        }
    }

    // Use PowerShell Expand-Archive -Force to extract safely.
    const QString script =
        "$ErrorActionPreference='Stop';"
        "Expand-Archive -LiteralPath '" + psEscape(zipPath) + "' "
        " -DestinationPath '" + psEscape(destDir) + "' -Force;";

    QProcess p;
    p.start("powershell.exe", {"-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script});
    if (!p.waitForFinished(5 * 60 * 1000)) { // up to 5 minutes for large archives
        if (err) *err = "PowerShell timed out while extracting ZIP.";
        return false;
    }

    const QByteArray outErr = p.readAllStandardError();
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        if (err) *err = QString("PowerShell extraction failed: %1").arg(QString::fromUtf8(outErr));
        return false;
    }

    return true;
}
