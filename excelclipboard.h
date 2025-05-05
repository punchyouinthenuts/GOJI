#ifndef EXCELCLIPBOARD_H
#define EXCELCLIPBOARD_H

#include <QClipboard>
#include <QMimeData>
#include <QTableWidget>
#include <QApplication>

class ExcelClipboard
{
public:
    static void copyTableToExcel(QTableWidget* table)
    {
        if (!table || table->rowCount() == 0 || table->columnCount() == 0)
            return;

        // Create Excel-specific HTML with guaranteed cell borders
        QString html = createExcelHtml(table);

        // Create plain text (TSV) backup
        QString plainText = createPlainText(table);

        // Create mime data with all formats
        QMimeData* mimeData = new QMimeData();

        // Add as HTML
        mimeData->setHtml(html);

        // Add as plain text
        mimeData->setText(plainText);

        // Add special Excel format
        QByteArray excelData = html.toUtf8();
        mimeData->setData("text/html", excelData);
        mimeData->setData("application/x-qt-windows-mime;value=\"HTML Format\"", excelData);

        // Set to clipboard
        QClipboard* clipboard = QApplication::clipboard();
        clipboard->setMimeData(mimeData);
    }

private:
    static QString createExcelHtml(QTableWidget* table)
    {
        // Create Excel-specific HTML with Office namespaces for best compatibility
        QString html = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n";
        html += "<html xmlns:o=\"urn:schemas-microsoft-com:office:office\" "
                "xmlns:x=\"urn:schemas-microsoft-com:office:excel\" "
                "xmlns=\"http://www.w3.org/TR/REC-html40\">\n";
        html += "<head>\n";
        html += "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n";
        html += "<meta name=\"ProgId\" content=\"Excel.Sheet\">\n";

        // XML processing instructions for Excel
        html += "<!--[if gte mso 9]>\n";
        html += "<xml>\n";
        html += "<x:ExcelWorkbook>\n";
        html += "<x:ExcelWorksheets>\n";
        html += "<x:ExcelWorksheet>\n";
        html += "<x:Name>Sheet</x:Name>\n";
        html += "<x:WorksheetOptions>\n";
        html += "<x:DisplayGridlines/>\n";
        html += "</x:WorksheetOptions>\n";
        html += "</x:ExcelWorksheet>\n";
        html += "</x:ExcelWorksheets>\n";
        html += "</x:ExcelWorkbook>\n";
        html += "</xml>\n";
        html += "<![endif]-->\n";

        // Define styles specifically for Excel
        html += "<style>\n";
        html += "table {border-collapse: collapse; mso-table-lspace:0pt; mso-table-rspace:0pt;}\n";
        html += "td, th {border: 1.0pt solid windowtext; padding: 4pt;}\n"; // Explicit Excel-friendly borders
        html += "th {background-color: #e0e0e0; font-weight: bold;}\n";
        html += "tr:nth-child(odd) {background-color: #f8f8f8;}\n";
        html += ".number {mso-number-format:\"General\";}\n";
        html += ".currency {mso-number-format:\"$#,##0.00\";}\n";
        html += ".text {mso-number-format:\"@\";}\n";
        html += ".right {text-align: right;}\n";
        html += ".left {text-align: left;}\n";
        html += ".center {text-align: center;}\n";
        html += "</style>\n";
        html += "</head>\n<body>\n";

        // Begin table with explicit borders
        html += "<table border=1 cellspacing=0 cellpadding=0 style=\"border-collapse:collapse; border:1.0pt solid windowtext;\">\n";

        // Add header row
        html += "<tr>\n";
        for (int col = 0; col < table->columnCount(); ++col) {
            QTableWidgetItem* item = table->horizontalHeaderItem(col);
            QString headerText = item ? item->text() : "";
            html += "<th style=\"border:1.0pt solid windowtext; background-color:#e0e0e0; font-weight:bold;\">"
                    + headerText + "</th>\n";
        }
        html += "</tr>\n";

        // Add data rows
        for (int row = 0; row < table->rowCount(); ++row) {
            // Background for alternating rows
            QString bgStyle = (row % 2 == 0) ?
                                  "background-color:#ffffff;" : "background-color:#f8f8f8;";

            html += "<tr>\n";
            for (int col = 0; col < table->columnCount(); ++col) {
                QTableWidgetItem* item = table->item(row, col);
                QString cellText = item ? item->text() : "";

                // Determine CSS class and format based on content type and alignment
                QString cellClass = "text"; // Default to text format
                QString alignClass = "left"; // Default alignment

                if (item) {
                    int alignment = item->textAlignment();
                    if (alignment & Qt::AlignRight)
                        alignClass = "right";
                    else if (alignment & Qt::AlignCenter)
                        alignClass = "center";

                    // Check for number/currency columns (assumes columns 3-5 are numbers, 6 is currency)
                    if (col >= 3 && col <= 5)
                        cellClass = "number";
                    else if (col == 6)
                        cellClass = "currency";
                }

                // Create cell with explicit styling for better Excel compatibility
                html += QString("<td class=\"%1 %2\" style=\"border:1.0pt solid windowtext; %3\">%4</td>\n")
                            .arg(cellClass, alignClass, bgStyle, cellText);
            }
            html += "</tr>\n";
        }

        html += "</table>\n</body>\n</html>";
        return html;
    }

    static QString createPlainText(QTableWidget* table)
    {
        QString text;

        // Header row
        for (int col = 0; col < table->columnCount(); ++col) {
            QTableWidgetItem* item = table->horizontalHeaderItem(col);
            text += item ? item->text() : "";
            if (col < table->columnCount() - 1)
                text += "\t";
        }
        text += "\n";

        // Data rows
        for (int row = 0; row < table->rowCount(); ++row) {
            for (int col = 0; col < table->columnCount(); ++col) {
                QTableWidgetItem* item = table->item(row, col);
                text += item ? item->text() : "";
                if (col < table->columnCount() - 1)
                    text += "\t";
            }
            text += "\n";
        }

        return text;
    }
};

#endif // EXCELCLIPBOARD_H
