#include "countstabledialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QClipboard>
#include <QApplication>
#include <QLocale>
#include <QDebug>
#include <QHeaderView>
#include <QMessageBox>
#include <QMimeData>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsColorizeEffect>

CountsTableDialog::CountsTableDialog(DatabaseManager* dbManager, QWidget* parent)
    : QDialog(parent), m_dbManager(dbManager)
{
    setupUI();
    loadData();
    setWindowTitle(tr("Post-Proof Counts"));
    resize(900, 600);
}

void CountsTableDialog::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    // Counts table setup
    QLabel* countsLabel = new QLabel(tr("Counts Table"), this);
    countsLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    layout->addWidget(countsLabel);

    m_countsTable = new QTableWidget(this);
    m_countsTable->setColumnCount(7);
    m_countsTable->setHorizontalHeaderLabels({
        tr("Job Number"), tr("Week"), tr("Project"),
        tr("PR Count"), tr("CANC Count"), tr("US Count"), tr("Postage")
    });

    // Apply consistent style to the table and its cells
    m_countsTable->setStyleSheet(
        "QTableWidget { border: 1px solid black; gridline-color: black; }"
        "QTableWidget::item { border: 1px solid black; padding: 5px; }"
        "QHeaderView::section { background-color: #e0e0e0; border: 1px solid black; padding: 5px; }"
        );

    // Adjust table properties
    m_countsTable->setAlternatingRowColors(true);
    m_countsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_countsTable->verticalHeader()->setVisible(false);
    m_countsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_countsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_countsTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    layout->addWidget(m_countsTable);

    // Copy button with enhanced appearance
    m_copyCountsButton = new QPushButton(tr("Copy Counts"), this);
    m_copyCountsButton->setStyleSheet("padding: 8px 20px; margin: 5px 0; border-radius: 4px; background-color: #f0f0f0; font-weight: bold;");
    m_copyCountsButton->setMinimumWidth(150);
    m_copyCountsButton->setCursor(Qt::PointingHandCursor);
    connect(m_copyCountsButton, &QPushButton::clicked, this, &CountsTableDialog::onCopyCountsButtonClicked);

    // Create the color effect for animation
    m_colorEffect = new QGraphicsColorizeEffect(this);
    m_colorEffect->setColor(Qt::green);
    m_colorEffect->setStrength(0.0);
    m_copyCountsButton->setGraphicsEffect(m_colorEffect);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_copyCountsButton);
    layout->addLayout(buttonLayout);

    // Comparison table setup
    QLabel* comparisonLabel = new QLabel(tr("Comparison Table"), this);
    comparisonLabel->setStyleSheet("font-weight: bold; font-size: 14px; margin-top: 10px;");
    layout->addWidget(comparisonLabel);

    m_comparisonTable = new QTableWidget(this);
    m_comparisonTable->setColumnCount(4);
    m_comparisonTable->setHorizontalHeaderLabels({
        tr("Group"), tr("Input Count"), tr("Output Count"), tr("Difference")
    });

    // Apply consistent style to the comparison table
    m_comparisonTable->setStyleSheet(
        "QTableWidget { border: 1px solid black; gridline-color: black; }"
        "QTableWidget::item { border: 1px solid black; padding: 5px; }"
        "QHeaderView::section { background-color: #e0e0e0; border: 1px solid black; padding: 5px; }"
        );

    // Adjust comparison table properties
    m_comparisonTable->setAlternatingRowColors(true);
    m_comparisonTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_comparisonTable->verticalHeader()->setVisible(false);
    m_comparisonTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_comparisonTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_comparisonTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    layout->addWidget(m_comparisonTable);

    setLayout(layout);
}

void CountsTableDialog::loadData()
{
    // Load counts data
    QList<QMap<QString, QVariant>> counts = m_dbManager->getPostProofCounts();

    if (counts.isEmpty()) {
        QMessageBox::warning(this, tr("No Data"), tr("No post-proof counts data available."));
        return;
    }

    // Set up the counts table
    m_countsTable->setRowCount(counts.size());

    QLocale locale(QLocale::English, QLocale::UnitedStates);
    int row = 0;

    // Build job number mapping to ensure consistency
    QMap<QString, QString> projectToJobNumber;

    // First pass to collect project prefix to job number mappings
    for (const QMap<QString, QVariant>& count : counts) {
        QString project = count["project"].toString();
        QString jobNumber = count["job_number"].toString();

        if (project.startsWith("CBC")) {
            projectToJobNumber["CBC"] = jobNumber;
        } else if (project.startsWith("EXC")) {
            projectToJobNumber["EXC"] = jobNumber;
        } else if (project.startsWith("INACTIVE")) {
            projectToJobNumber["INACTIVE"] = jobNumber;
        } else if (project.startsWith("NCWO")) {
            projectToJobNumber["NCWO"] = jobNumber;
        } else if (project.startsWith("PREPIF")) {
            projectToJobNumber["PREPIF"] = jobNumber;
        }
    }

    for (const QMap<QString, QVariant>& count : counts) {
        QString project = count["project"].toString();
        QString jobNumber;

        // Determine the correct job number based on project
        if (project.startsWith("CBC")) {
            jobNumber = projectToJobNumber["CBC"];
        } else if (project.startsWith("EXC")) {
            jobNumber = projectToJobNumber["EXC"];
        } else if (project.startsWith("INACTIVE")) {
            jobNumber = projectToJobNumber["INACTIVE"];
        } else if (project.startsWith("NCWO")) {
            jobNumber = projectToJobNumber["NCWO"];
        } else if (project.startsWith("PREPIF")) {
            jobNumber = projectToJobNumber["PREPIF"];
        } else {
            jobNumber = count["job_number"].toString();
        }

        // Set the table items
        QTableWidgetItem* jobNumberItem = new QTableWidgetItem(jobNumber);
        jobNumberItem->setTextAlignment(Qt::AlignCenter);
        m_countsTable->setItem(row, 0, jobNumberItem);

        QTableWidgetItem* weekItem = new QTableWidgetItem(count["week"].toString());
        weekItem->setTextAlignment(Qt::AlignCenter);
        m_countsTable->setItem(row, 1, weekItem);

        QTableWidgetItem* projectItem = new QTableWidgetItem(project);
        projectItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_countsTable->setItem(row, 2, projectItem);

        int prCount = count["pr_count"].toInt();
        QTableWidgetItem* prCountItem = new QTableWidgetItem(locale.toString(prCount));
        prCountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        prCountItem->setData(Qt::UserRole, prCount); // Store raw value for sorting
        m_countsTable->setItem(row, 3, prCountItem);

        int cancCount = count["canc_count"].toInt();
        QTableWidgetItem* cancCountItem = new QTableWidgetItem(locale.toString(cancCount));
        cancCountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        cancCountItem->setData(Qt::UserRole, cancCount); // Store raw value for sorting
        m_countsTable->setItem(row, 4, cancCountItem);

        int usCount = count["us_count"].toInt();
        QTableWidgetItem* usCountItem = new QTableWidgetItem(locale.toString(usCount));
        usCountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        usCountItem->setData(Qt::UserRole, usCount); // Store raw value for sorting
        m_countsTable->setItem(row, 5, usCountItem);

        // Format postage as currency
        bool ok;
        double postageValue = count["postage"].toString().toDouble(&ok);
        QString formattedPostage = ok ? locale.toCurrencyString(postageValue, "$", 2) : "$0.00";
        QTableWidgetItem* postageItem = new QTableWidgetItem(formattedPostage);
        postageItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        postageItem->setData(Qt::UserRole, postageValue); // Store raw value for sorting
        m_countsTable->setItem(row, 6, postageItem);

        row++;
    }

    // Load comparison data
    QList<QMap<QString, QVariant>> comparison = m_dbManager->getCountComparison();
    m_comparisonTable->setRowCount(comparison.size());

    row = 0;
    for (const QMap<QString, QVariant>& comp : comparison) {
        QTableWidgetItem* groupItem = new QTableWidgetItem(comp["group_name"].toString());
        groupItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_comparisonTable->setItem(row, 0, groupItem);

        int inputCount = comp["input_count"].toInt();
        QTableWidgetItem* inputItem = new QTableWidgetItem(locale.toString(inputCount));
        inputItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        inputItem->setData(Qt::UserRole, inputCount); // Store raw value for sorting
        m_comparisonTable->setItem(row, 1, inputItem);

        int outputCount = comp["output_count"].toInt();
        QTableWidgetItem* outputItem = new QTableWidgetItem(locale.toString(outputCount));
        outputItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        outputItem->setData(Qt::UserRole, outputCount); // Store raw value for sorting
        m_comparisonTable->setItem(row, 2, outputItem);

        // Highlight differences in red
        int difference = comp["difference"].toInt();
        QTableWidgetItem* diffItem = new QTableWidgetItem(locale.toString(difference));
        diffItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        diffItem->setData(Qt::UserRole, difference); // Store raw value for sorting

        if (difference != 0) {
            diffItem->setForeground(Qt::red);
            diffItem->setFont(QFont("Arial", 10, QFont::Bold));
        }

        m_comparisonTable->setItem(row, 3, diffItem);
        row++;
    }
}

void CountsTableDialog::onCopyCountsButtonClicked()
{
    // Create Excel-specific HTML with guaranteed cell borders
    QString html = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n";
    html += "<html xmlns:o=\"urn:schemas-microsoft-com:office:office\" "
            "xmlns:x=\"urn:schemas-microsoft-com:office:excel\" "
            "xmlns=\"http://www.w3.org/TR/REC-html40\">\n";
    html += "<head>\n";
    html += "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n";
    html += "<meta name=\"ProgId\" content=\"Excel.Sheet\">\n";

    // XML processing instructions specifically for Excel
    html += "<!--[if gte mso 9]>\n";
    html += "<xml>\n";
    html += "<x:ExcelWorkbook>\n";
    html += "<x:ExcelWorksheets>\n";
    html += "<x:ExcelWorksheet>\n";
    html += "<x:Name>Count Data</x:Name>\n";
    html += "<x:WorksheetOptions>\n";
    html += "<x:DisplayGridlines/>\n";
    html += "</x:WorksheetOptions>\n";
    html += "</x:ExcelWorksheet>\n";
    html += "</x:ExcelWorksheets>\n";
    html += "</x:ExcelWorkbook>\n";
    html += "</xml>\n";
    html += "<![endif]-->\n";

    // Define styles specifically for Excel with explicit borders
    html += "<style>\n";
    html += "table {border-collapse: collapse; mso-table-lspace:0pt; mso-table-rspace:0pt; border:1pt solid black;}\n";
    html += "th {border:1pt solid black; background-color:#e0e0e0; font-weight:bold; text-align:center; padding:4pt;}\n";
    html += "td {border:1pt solid black; padding:4pt;}\n";
    html += "tr:nth-child(odd) {background-color:#f8f8f8;}\n";
    html += "tr:nth-child(even) {background-color:#ffffff;}\n";
    html += ".number {mso-number-format:\"General\"; text-align:right;}\n";
    html += ".currency {mso-number-format:\"$#,##0.00\"; text-align:right;}\n";
    html += ".text {mso-number-format:\"@\"; text-align:left;}\n";
    html += ".center {text-align:center;}\n";
    html += "</style>\n";
    html += "</head>\n<body>\n";

    // Begin table with EXPLICIT border=1 attribute for better Excel compatibility
    html += "<table border=1 cellspacing=0 cellpadding=0>\n";

    // Add header row
    html += "<tr>\n";
    for (int col = 0; col < m_countsTable->columnCount(); ++col) {
        QTableWidgetItem* item = m_countsTable->horizontalHeaderItem(col);
        if (item) {
            html += "<th>" + item->text() + "</th>\n";
        }
    }
    html += "</tr>\n";

    // Add data rows
    for (int row = 0; row < m_countsTable->rowCount(); ++row) {
        html += "<tr>\n";
        for (int col = 0; col < m_countsTable->columnCount(); ++col) {
            QTableWidgetItem* item = m_countsTable->item(row, col);
            QString cellText = item ? item->text() : "";

            // Determine cell class based on column type
            QString cellClass;
            if (col == 0 || col == 1) {
                cellClass = "center"; // Job Number and Week are centered
            } else if (col == 2) {
                cellClass = "text"; // Project is left-aligned text
            } else if (col >= 3 && col <= 5) {
                cellClass = "number"; // Count columns are right-aligned numbers
            } else if (col == 6) {
                cellClass = "currency"; // Postage is currency format
            } else {
                cellClass = "text"; // Default to text
            }

            html += QString("<td class=\"%1\">%2</td>\n").arg(cellClass, cellText);
        }
        html += "</tr>\n";
    }

    html += "</table>\n";

    // Add a separator and then the comparison table
    html += "<br/><br/>\n";

    // Add comparison table with the same Excel-friendly formatting
    html += "<table border=1 cellspacing=0 cellpadding=0>\n";

    // Add header row for comparison table
    html += "<tr>\n";
    for (int col = 0; col < m_comparisonTable->columnCount(); ++col) {
        QTableWidgetItem* item = m_comparisonTable->horizontalHeaderItem(col);
        if (item) {
            html += "<th>" + item->text() + "</th>\n";
        }
    }
    html += "</tr>\n";

    // Add data rows for comparison table
    for (int row = 0; row < m_comparisonTable->rowCount(); ++row) {
        html += "<tr>\n";
        for (int col = 0; col < m_comparisonTable->columnCount(); ++col) {
            QTableWidgetItem* item = m_comparisonTable->item(row, col);
            QString cellText = item ? item->text() : "";

            // Determine cell class based on column
            QString cellClass;
            if (col == 0) {
                cellClass = "text"; // Group name is text
            } else {
                cellClass = "number"; // Count columns are numeric
            }

            // Add special formatting for difference column if not zero
            if (col == 3 && item && item->text() != "0") {
                html += QString("<td class=\"%1\" style=\"color:red; font-weight:bold;\">%2</td>\n")
                .arg(cellClass, cellText);
            } else {
                html += QString("<td class=\"%1\">%2</td>\n").arg(cellClass, cellText);
            }
        }
        html += "</tr>\n";
    }

    html += "</table>\n</body>\n</html>";

    // Create plain text (TSV) backup for applications that don't support HTML
    QString plainText;

    // Header for first table
    for (int col = 0; col < m_countsTable->columnCount(); ++col) {
        QTableWidgetItem* item = m_countsTable->horizontalHeaderItem(col);
        plainText += item ? item->text() : "";
        plainText += (col < m_countsTable->columnCount() - 1) ? "\t" : "\n";
    }

    // Data rows for first table
    for (int row = 0; row < m_countsTable->rowCount(); ++row) {
        for (int col = 0; col < m_countsTable->columnCount(); ++col) {
            QTableWidgetItem* item = m_countsTable->item(row, col);
            plainText += item ? item->text() : "";
            plainText += (col < m_countsTable->columnCount() - 1) ? "\t" : "\n";
        }
    }

    // Separator
    plainText += "\n\n";

    // Header for comparison table
    for (int col = 0; col < m_comparisonTable->columnCount(); ++col) {
        QTableWidgetItem* item = m_comparisonTable->horizontalHeaderItem(col);
        plainText += item ? item->text() : "";
        plainText += (col < m_comparisonTable->columnCount() - 1) ? "\t" : "\n";
    }

    // Data rows for comparison table
    for (int row = 0; row < m_comparisonTable->rowCount(); ++row) {
        for (int col = 0; col < m_comparisonTable->columnCount(); ++col) {
            QTableWidgetItem* item = m_comparisonTable->item(row, col);
            plainText += item ? item->text() : "";
            plainText += (col < m_comparisonTable->columnCount() - 1) ? "\t" : "\n";
        }
    }

    // Create mime data with all formats for maximum compatibility
    QMimeData* mimeData = new QMimeData();

    // Add as HTML
    mimeData->setHtml(html);

    // Add as plain text
    mimeData->setText(plainText);

    // Add special Excel-specific format for guaranteed borders
    QByteArray excelData = html.toUtf8();
    mimeData->setData("text/html", excelData);
    mimeData->setData("application/x-qt-windows-mime;value=\"HTML Format\"", excelData);

    // Set to clipboard
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setMimeData(mimeData);

    // Visual feedback animation
    QPropertyAnimation* animation = new QPropertyAnimation(m_colorEffect, "strength");
    animation->setDuration(1500);  // 1.5 seconds
    animation->setStartValue(0.0);
    animation->setKeyValueAt(0.3, 0.8);  // Peak at 0.8 strength at 30% of the animation
    animation->setEndValue(0.0);
    animation->start(QPropertyAnimation::DeleteWhenStopped);

    // Change button text temporarily
    QString originalText = m_copyCountsButton->text();
    m_copyCountsButton->setText(tr("Copied!"));

    // Reset button text after animation finishes
    QTimer::singleShot(1500, this, [this, originalText]() {
        m_copyCountsButton->setText(originalText);
    });
}
