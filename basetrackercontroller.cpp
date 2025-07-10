#include "basetrackercontroller.h"
#include "logger.h"
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>
#include <QProcess>
#include <QString>

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
        cellData = formatCellData(i, cellData);
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
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString tempFileName = QDir(tempDir).filePath("goji_temp_copy.docx");
    QString scriptPath = QDir(tempDir).filePath("goji_word_script.ps1");

    // Remove existing files
    QFile::remove(tempFileName);
    QFile::remove(scriptPath);

    // Generate PowerShell script for Word
    QString script = "try {\n";
    script += "  $word = New-Object -ComObject Word.Application\n";
    script += "  $word.Visible = $false\n";
    script += "  $doc = $word.Documents.Add()\n";
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
    script += QString("  $doc.SaveAs('%1')\n").arg(escapedPath);
    script += "  $table.Range.Select()\n";
    script += "  $word.Selection.Copy()\n";

    // Delay for clipboard flush
    script += "  Start-Sleep -Seconds 2\n";
    script += "  $doc.Close($false)\n";
    script += "  $word.Quit()\n";

    // Release COM
    script += "  [System.Runtime.Interopservices.Marshal]::ReleaseComObject($table) | Out-Null\n";
    script += "  [System.Runtime.Interopservices.Marshal]::ReleaseComObject($range) | Out-Null\n";
    script += "  [System.Runtime.Interopservices.Marshal]::ReleaseComObject($doc) | Out-Null\n";
    script += "  [System.Runtime.Interopservices.Marshal]::ReleaseComObject($word) | Out-Null\n";
    script += "  [System.GC]::Collect()\n";

    script += "  Write-Output 'SUCCESS'\n";
    script += "} catch {\n";
    script += "  Write-Output \"ERROR: $_\"\n";
    script += "}\n";

    // Write and execute script
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        outputToTerminal("Failed to create PowerShell script file", Error);
        return false;
    }
    QTextStream out(&scriptFile);
    out << script;
    scriptFile.close();

    QProcess process;
    QStringList arguments;
    arguments << "-ExecutionPolicy" << "Bypass" << "-NoProfile" << "-File" << scriptPath;
    process.start("powershell.exe", arguments);
    process.waitForFinished(20000);  // Longer timeout for Word

    QString output = process.readAllStandardOutput();
    QString errorOutput = process.readAllStandardError();

    QFile::remove(scriptPath);
    QFile::remove(tempFileName);

    bool success = output.contains("SUCCESS") && process.exitCode() == 0;
    if (!success) {
        outputToTerminal(QString("Word copy failed: %1").arg(errorOutput), Error);
    }
    return success;
#else
    outputToTerminal("Word copy functionality only available on Windows", Warning);
    return false;
#endif
}

QString BaseTrackerController::formatCellData(int columnIndex, const QString& cellData) const
{
    // Default implementation - no special formatting
    // Derived classes can override for specific column formatting
    return cellData;
}
