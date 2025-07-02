#include "basetrackercontroller.h"
#include "logger.h"

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

    // Get the row number
    int row = index.row();

    // Get configuration from derived class
    QList<int> visibleColumns = getVisibleColumns();
    QStringList headers = getTrackerHeaders();
    QSqlTableModel* trackerModel = getTrackerModel();

    if (!trackerModel) {
        return "Tracker model not available";
    }

    // Collect data from the selected row
    QStringList rowData;
    for (int i = 0; i < visibleColumns.size(); i++) {
        int sourceCol = visibleColumns[i];
        QString cellData = trackerModel->data(trackerModel->index(row, sourceCol)).toString();

        // Apply custom formatting if implemented by derived class
        cellData = formatCellData(i, cellData);

        rowData.append(cellData);
    }

    // Create Excel file using PowerShell and copy it
    if (createExcelAndCopy(headers, rowData)) {
        outputToTerminal("Copied row to clipboard with Excel formatting", Success);
        return "Row copied to clipboard";
    } else {
        outputToTerminal("Failed to copy row with Excel formatting", Error);
        return "Copy failed";
    }
}

bool BaseTrackerController::createExcelAndCopy(const QStringList& headers, const QStringList& rowData)
{
#ifdef Q_OS_WIN
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString tempFileName = QDir(tempDir).filePath("goji_temp_copy.xlsx");
    QString scriptPath = QDir(tempDir).filePath("goji_excel_script.ps1");

    // Remove existing files
    QFile::remove(tempFileName);
    QFile::remove(scriptPath);

    // Generate the complete PowerShell script
    QString script = generateExcelScript(headers, rowData, tempFileName);

    // Write script to file
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        outputToTerminal("Failed to create PowerShell script file", Error);
        return false;
    }

    QTextStream out(&scriptFile);
    out << script;
    scriptFile.close();

    // Execute PowerShell script
    QProcess process;
    QStringList arguments;
    arguments << "-ExecutionPolicy" << "Bypass" << "-NoProfile" << "-File" << scriptPath;

    process.start("powershell.exe", arguments);
    process.waitForFinished(15000); // 15 second timeout

    QString output = process.readAllStandardOutput();
    QString errorOutput = process.readAllStandardError();

    // Cleanup script file
    QFile::remove(scriptPath);
    QFile::remove(tempFileName);

    // Check if operation was successful
    bool success = output.contains("SUCCESS") && process.exitCode() == 0;

    if (!success) {
        outputToTerminal(QString("Excel copy failed: %1").arg(errorOutput), Error);
    }

    return success;
#else
    outputToTerminal("Excel copy functionality only available on Windows", Warning);
    return false;
#endif
}

QString BaseTrackerController::formatCellData(int columnIndex, const QString& cellData) const
{
    // Default implementation - no special formatting
    // Derived classes can override for specific column formatting
    return cellData;
}

QString BaseTrackerController::generateExcelScript(const QStringList& headers, const QStringList& rowData, const QString& tempFileName) const
{
    QString script = "try {\n";
    script += "  $excel = New-Object -ComObject Excel.Application\n";
    script += "  $excel.Visible = $false\n";
    script += "  $excel.DisplayAlerts = $false\n";
    script += "  $workbook = $excel.Workbooks.Add()\n";
    script += "  $sheet = $workbook.ActiveSheet\n";

    // Add headers with styling
    addHeaderFormatting(script, headers);

    // Add data with formatting
    addDataFormatting(script, rowData);

    // Add borders and column formatting
    addBorderAndColumnFormatting(script, headers.size());

    // Save file and copy to clipboard
    script += QString("  $workbook.SaveAs('%1')\n").arg(QString(tempFileName).replace('/', '\\'));
    script += "  $range.Select()\n";
    script += "  $range.Copy()\n";

    // Keep Excel open briefly so clipboard data persists
    script += "  Start-Sleep -Seconds 1\n";
    script += "  $workbook.Close($false)\n";
    script += "  $excel.Quit()\n";

    // Clean up COM objects
    script += "  [System.Runtime.Interopservices.Marshal]::ReleaseComObject($range) | Out-Null\n";
    script += "  [System.Runtime.Interopservices.Marshal]::ReleaseComObject($sheet) | Out-Null\n";
    script += "  [System.Runtime.Interopservices.Marshal]::ReleaseComObject($workbook) | Out-Null\n";
    script += "  [System.Runtime.Interopservices.Marshal]::ReleaseComObject($excel) | Out-Null\n";
    script += "  [System.GC]::Collect()\n";

    script += "  Write-Output 'SUCCESS'\n";
    script += "} catch {\n";
    script += "  Write-Output \"ERROR: $_\"\n";
    script += "}\n";

    return script;
}

void BaseTrackerController::addHeaderFormatting(QString& script, const QStringList& headers) const
{
    for (int i = 0; i < headers.size(); i++) {
        script += QString("  $sheet.Cells(%1,%2) = '%3'\n").arg(1).arg(i + 1).arg(headers[i]);
        script += QString("  $sheet.Cells(%1,%2).Font.Bold = $true\n").arg(1).arg(i + 1);
        script += QString("  $sheet.Cells(%1,%2).Interior.Color = 14737632\n").arg(1).arg(i + 1);
    }
}

void BaseTrackerController::addDataFormatting(QString& script, const QStringList& rowData) const
{
    for (int i = 0; i < rowData.size(); i++) {
        QString cellValue = rowData[i];
        cellValue.replace("'", "''"); // Escape single quotes
        script += QString("  $sheet.Cells(%1,%2) = '%3'\n").arg(2).arg(i + 1).arg(cellValue);

        // Apply standard formatting for common column types
        if (i == 2) { // Typically POSTAGE column - currency format
            script += QString("  $sheet.Cells(%1,%2).NumberFormat = '$#,##0.00'\n").arg(2).arg(i + 1);
            script += QString("  $sheet.Cells(%1,%2).HorizontalAlignment = -4152\n").arg(2).arg(i + 1);
        } else if (i == 3) { // Typically COUNT column - number format
            script += QString("  $sheet.Cells(%1,%2).NumberFormat = '#,##0'\n").arg(2).arg(i + 1);
            script += QString("  $sheet.Cells(%1,%2).HorizontalAlignment = -4152\n").arg(2).arg(i + 1);
        } else if (i == 4) { // Typically AVG RATE column - decimal format
            script += QString("  $sheet.Cells(%1,%2).NumberFormat = '0.000'\n").arg(2).arg(i + 1);
            script += QString("  $sheet.Cells(%1,%2).HorizontalAlignment = -4152\n").arg(2).arg(i + 1);
        }
    }
}

void BaseTrackerController::addBorderAndColumnFormatting(QString& script, int columnCount) const
{
    // Add borders and formatting
    script += QString("  $range = $sheet.Range('A1:%1%2')\n").arg(QChar('A' + (char)(columnCount - 1))).arg(2);
    script += "  $range.Borders.LineStyle = 1\n";
    script += "  $range.Borders.Weight = 2\n";
    script += "  $range.Borders.Color = 0\n";

    // Set standard column widths - derived classes can override if needed
    script += "  $sheet.Columns.Item(1).ColumnWidth = 8\n";   // JOB
    script += "  $sheet.Columns.Item(2).ColumnWidth = 20\n";  // DESCRIPTION
    script += "  $sheet.Columns.Item(3).ColumnWidth = 10\n";  // POSTAGE
    script += "  $sheet.Columns.Item(4).ColumnWidth = 8\n";   // COUNT
    script += "  $sheet.Columns.Item(5).ColumnWidth = 10\n";  // AVG RATE
    script += "  $sheet.Columns.Item(6).ColumnWidth = 6\n";   // CLASS
    script += "  $sheet.Columns.Item(7).ColumnWidth = 6\n";   // SHAPE
    script += "  $sheet.Columns.Item(8).ColumnWidth = 8\n";   // PERMIT
}
