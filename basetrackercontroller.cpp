#include "basetrackercontroller.h"
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>
#include <QProcess>
#include <QString>
#include <QTemporaryDir>
#include <QElapsedTimer>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

BaseTrackerController::BaseTrackerController(QObject *parent)
    : QObject(parent)
{
}

QString BaseTrackerController::copyFormattedRow()
{
    QTableView* tracker = getTrackerWidget();
    if (!tracker) {
        return "Table view not available";
    }

    QModelIndex index = tracker->currentIndex();
    if (!index.isValid()) {
        return "No row selected";
    }

    int row = index.row();
    QList<int> visibleColumns = getVisibleColumns();
    QStringList headers = getTrackerHeaders();
    QSqlTableModel* trackerModel = getTrackerModel();

    if (!trackerModel) {
        return "Tracker model not available";
    }

    QStringList rowData;
    for (int i = 0; i < visibleColumns.size(); i++) {
        int sourceCol = visibleColumns[i];
        QString cellData = trackerModel->data(trackerModel->index(row, sourceCol)).toString();
        cellData = formatCellDataForCopy(i, cellData);
        rowData.append(cellData);
    }

    if (createExcelAndCopy(headers, rowData)) {
        outputToTerminal("Copied row to clipboard with Word formatting", Success);
        return "Row copied to clipboard";
    } else {
        outputToTerminal("Failed to copy row with Word formatting", Error);
        return "Copy failed";
    }
}

bool BaseTrackerController::createExcelAndCopy(const QStringList& headers, const QStringList& rowData)
{
#ifdef Q_OS_WIN
    QTemporaryDir tempDir(QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                              .filePath("goji-word-copy-XXXXXX"));
    if (!tempDir.isValid()) {
        outputToTerminal("Word copy failed: Cannot create temporary directory: " + tempDir.errorString(), Error);
        return false;
    }
    const QString tempFileName = tempDir.filePath("row.docx");
    const QString scriptPath = tempDir.filePath("copy.ps1");

    // Generate PowerShell script for Word
    QString script = "$ErrorActionPreference = 'Stop'\n";
    script += "[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)\n";
    script += "$word = $doc = $range = $table = $null\n";
    script += "$failed = $false\n";
    script += "$doNotSaveChanges = 0\n";
    script += "$stage = 'starting Word'\n";
    script += "try {\n";
    script += "  $word = New-Object -ComObject Word.Application\n";
    // Capture the exact automation instance before any document operations can hang.
    script += "  [Console]::Out.WriteLine('GOJI_WORD_WINDOW=' + $word.Hwnd)\n";
    script += "  $word.Visible = $false\n";
    script += "  $word.DisplayAlerts = 0\n";
    script += "  $stage = 'creating the table'\n";
    script += "  $doc = $word.Documents.Add()\n";
    // Word can report Hwnd = 0 until its first document exists.
    script += "  [Console]::Out.WriteLine('GOJI_WORD_WINDOW=' + $word.Hwnd)\n";
    script += "  $range = $doc.Range()\n";

    // Add table: rows=2 (header + data), columns=headers.size()
    script += QString("  $table = $doc.Tables.Add($range, 2, %1)\n").arg(headers.size());
    script += "  $table.Style = 'Table Grid'\n";  // Applies basic grid borders

    // Set headers
    for (int i = 0; i < headers.size(); i++) {
        QString headerValue = headers[i];
        headerValue = headerValue.replace(QString("'"), QString("''"));  // Explicit escape
        script += QString("  $table.Cell(1,%1).Range.Text = '%2'\n").arg(i+1).arg(headerValue);
        script += QString("  $table.Cell(1,%1).Range.Bold = $true\n").arg(i+1);
        script += QString("  $table.Cell(1,%1).Range.Shading.BackgroundPatternColor = 14737632\n").arg(i+1);  // Gray
    }

    // Set data with formatting
    for (int i = 0; i < rowData.size(); i++) {
        QString cellValue = rowData[i];
        cellValue = cellValue.replace(QString("'"), QString("''"));  // Explicit escape
        script += QString("  $table.Cell(2,%1).Range.Text = '%2'\n").arg(i+1).arg(cellValue);

        // Align and format numbers
        if (i == 2 || i == 3 || i == 4) {  // Postage/Count/AVG right-align
            script += QString("  $table.Cell(2,%1).Range.ParagraphFormat.Alignment = 2\n").arg(i+1);  // wdAlignParagraphRight
        }
    }

    // Apply borders explicitly
    script += "  $table.Borders.Enable = $true\n";

    // Save, select table, copy
    QString escapedPath = tempFileName;
    escapedPath = escapedPath.replace(QString("/"), QString("\\"));  // Explicit escape
    escapedPath.replace("'", "''");
    script += "  $stage = 'saving the temporary document'\n";
    script += QString("  $doc.SaveAs('%1')\n").arg(escapedPath);
    script += "  $stage = 'copying the table to the clipboard'\n";
    script += "  $table.Range.Select()\n";
    script += "  $word.Selection.Copy()\n";

    // Delay for clipboard flush
    script += "  Start-Sleep -Seconds 2\n";
    script += "} catch {\n";
    script += "  $failed = $true\n";
    script += "  [Console]::Error.WriteLine(\"Error while ${stage}: $($_.Exception.Message)\")\n";
    script += "} finally {\n";
    // Independent cleanup attempts: a failed Close must not prevent Quit or COM release.
    script += "  try { if ($null -ne $doc) { $doc.Close(0) } } catch {\n";
    script += "    $failed = $true; [Console]::Error.WriteLine(\"Closing Word document failed: $($_.Exception.Message)\")\n";
    script += "  }\n";
    script += "  try { if ($null -ne $word) { $word.Quit([ref]$doNotSaveChanges) } } catch {\n";
    script += "    $failed = $true; [Console]::Error.WriteLine(\"Quitting Word failed: $($_.Exception.Message)\")\n";
    script += "  }\n";
    script += "  foreach ($name in @('table', 'range', 'doc', 'word')) {\n";
    script += "    try {\n";
    script += "      $comObject = Get-Variable -Name $name -ValueOnly\n";
    script += "      if ($null -ne $comObject -and [System.Runtime.Interopservices.Marshal]::IsComObject($comObject)) {\n";
    script += "        [void][System.Runtime.Interopservices.Marshal]::FinalReleaseComObject($comObject)\n";
    script += "      }\n";
    script += "    } catch {\n";
    script += "      $failed = $true; [Console]::Error.WriteLine(\"Releasing $name failed: $($_.Exception.Message)\")\n";
    script += "    }\n";
    script += "  }\n";
    script += "  $comObject = $word = $doc = $range = $table = $null\n";
    script += "  [System.GC]::Collect()\n";
    script += "  [System.GC]::WaitForPendingFinalizers()\n";
    script += "}\n";
    script += "if ($failed) { exit 1 }\n";
    script += "Write-Output 'SUCCESS'\n";

    // Write and execute script
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        outputToTerminal("Word copy failed: Cannot create PowerShell script: " + scriptFile.errorString(), Error);
        return false;
    }
    // Windows PowerShell 5.1 needs a BOM to read non-ASCII headers, values and paths as UTF-8.
    const QByteArray scriptBytes = QByteArray::fromHex("efbbbf") + script.toUtf8();
    if (scriptFile.write(scriptBytes) != scriptBytes.size() || !scriptFile.flush()) {
        outputToTerminal("Word copy failed: Cannot write PowerShell script: " + scriptFile.errorString(), Error);
        return false;
    }
    scriptFile.close();

    QProcess process;
    QByteArray outputBytes;
    HANDLE wordProcess = nullptr;
    FILETIME copyStartTime;
    GetSystemTimeAsFileTime(&copyStartTime);
    auto readOutput = [&]() {
        outputBytes += process.readAllStandardOutput();
        if (wordProcess) return;
        QList<QByteArray> lines = outputBytes.split('\n');
        lines.removeLast(); // Only parse complete lines, never a partially received window handle.
        for (const QByteArray& line : lines) {
            if (!line.startsWith("GOJI_WORD_WINDOW=")) continue;
            const HWND window = reinterpret_cast<HWND>(line.mid(17).trimmed().toULongLong());
            DWORD pid = 0;
            GetWindowThreadProcessId(window, &pid);
            HANDLE candidate = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
            if (!candidate) continue;
            FILETIME created, exited, kernel, user;
            wchar_t path[MAX_PATH];
            DWORD pathSize = MAX_PATH;
            // Never terminate a Word instance that was already open before this copy.
            if (GetProcessTimes(candidate, &created, &exited, &kernel, &user)
                && CompareFileTime(&created, &copyStartTime) >= 0
                && QueryFullProcessImageNameW(candidate, 0, path, &pathSize)
                && QString::fromWCharArray(path, pathSize).endsWith("\\WINWORD.EXE", Qt::CaseInsensitive)) {
                wordProcess = candidate;
                break;
            }
            CloseHandle(candidate);
        }
    };
    connect(&process, &QProcess::readyReadStandardOutput, &process, readOutput);
    QStringList arguments;
    arguments << "-ExecutionPolicy" << "Bypass" << "-NoProfile" << "-NonInteractive" << "-STA" << "-File" << scriptPath;
    QElapsedTimer timer;
    timer.start();
    process.start("powershell.exe", arguments);
    const bool started = process.waitForStarted(20000);
    const bool finished = started && process.waitForFinished(qMax(0, 20000 - int(timer.elapsed())));
    readOutput();
    QStringList errors;
    if (!started) {
        errors << "Could not start PowerShell: " + process.errorString();
    } else if (!finished) {
        errors << "Timed out after 20 seconds while copying with Word.";
        if (!wordProcess) errors << "No new Word instance could be identified for automatic cleanup.";
        // A hung COM call cannot reach the script's finally block. Stop only its identified
        // Word instance, then stop PowerShell before removing this attempt's temporary files.
    }
    const bool success = finished && process.exitStatus() == QProcess::NormalExit
                         && process.exitCode() == 0
                         && QString::fromUtf8(outputBytes).replace("\r\n", "\n").split('\n').contains("SUCCESS");
    if (!success && wordProcess && WaitForSingleObject(wordProcess, 0) == WAIT_TIMEOUT) {
        if (!TerminateProcess(wordProcess, 1)) {
            errors << QString("Could not stop the Word copy instance (Windows error %1).").arg(GetLastError());
        } else if (WaitForSingleObject(wordProcess, 1000) != WAIT_OBJECT_0) {
            errors << "The Word copy instance has not finished stopping.";
        }
    }
    if (process.state() != QProcess::NotRunning) {
        process.kill();
        process.waitForFinished(1000);
    }
    readOutput();
    if (wordProcess) CloseHandle(wordProcess);
    const QString errorOutput = QString::fromUtf8(process.readAllStandardError()).trimmed();
    if (!success) {
        if (!errorOutput.isEmpty()) errors << errorOutput;
        // Include stdout too: script/host errors may be written to either stream.
        for (const QString& line : QString::fromUtf8(outputBytes).split('\n')) {
            if (!line.startsWith("GOJI_WORD_WINDOW=") && !line.trimmed().isEmpty()) errors << line.trimmed();
        }
        if (errors.isEmpty()) {
            errors << QString("PowerShell %1 (exit code %2).").arg(
                process.exitStatus() == QProcess::CrashExit ? "crashed" : "returned no success confirmation")
                .arg(process.exitCode());
        }
        outputToTerminal("Word copy failed: " + errors.join("\n"), Error);
    }
    if (!tempDir.remove()) {
        outputToTerminal("Could not remove Word copy temporary files: " + tempDir.path(), Warning);
    }
    return success;
#else
    outputToTerminal("Word copy functionality only available on Windows", Warning);
    return false;
#endif
}

QString BaseTrackerController::formatCellData(int /*columnIndex*/, const QString& cellData) const
{
    // Default implementation - no special formatting
    // Derived classes can override for specific column formatting
    return cellData;
}

QString BaseTrackerController::formatCellDataForCopy(int columnIndex, const QString& cellData) const
{
    // Default implementation for copy operations
    // COUNT columns should be returned as plain integers without thousand separators
    // Based on standard header order: {"JOB", "DESCRIPTION", "POSTAGE", "COUNT", "AVG RATE", "CLASS", "SHAPE", "PERMIT"}
    // columnIndex 2 = POSTAGE, columnIndex 3 = COUNT
    
    if (columnIndex == 3) { // COUNT column position in visible columns list
        // Remove commas and return as plain integer string
        QString cleanData = cellData;
        cleanData.remove(',');
        bool ok;
        qlonglong val = cleanData.toLongLong(&ok);
        if (ok) {
            return QString::number(val);
        }
    }
    
    // For all other columns, use regular display formatting
    return formatCellData(columnIndex, cellData);
}
