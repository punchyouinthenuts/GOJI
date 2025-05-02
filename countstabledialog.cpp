#include "countstabledialog.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
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
    resize(800, 500);
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
    m_copyCountsButton->setStyleSheet("padding: 5px 15px; margin: 5px 0; border-radius: 4px; background-color: #f0f0f0;");
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

    // Close button with enhanced appearance
    QPushButton* closeButton = new QPushButton(tr("Close"), this);
    closeButton->setStyleSheet("padding: 5px 15px; margin: 5px 0; border-radius: 4px; background-color: #f0f0f0;");
    closeButton->setMinimumWidth(150);
    closeButton->setCursor(Qt::PointingHandCursor);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    QHBoxLayout* closeButtonLayout = new QHBoxLayout();
    closeButtonLayout->addStretch();
    closeButtonLayout->addWidget(closeButton);
    layout->addLayout(closeButtonLayout);

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

    // Build a map of project types to job numbers
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

    // Set up the counts table
    m_countsTable->setRowCount(counts.size());

    QLocale locale(QLocale::English, QLocale::UnitedStates);
    int row = 0;

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

        QTableWidgetItem* projectItem = new QTableWidgetItem(count["project"].toString());
        projectItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_countsTable->setItem(row, 2, projectItem);

        QTableWidgetItem* prCountItem = new QTableWidgetItem(locale.toString(count["pr_count"].toInt()));
        prCountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_countsTable->setItem(row, 3, prCountItem);

        QTableWidgetItem* cancCountItem = new QTableWidgetItem(locale.toString(count["canc_count"].toInt()));
        cancCountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_countsTable->setItem(row, 4, cancCountItem);

        QTableWidgetItem* usCountItem = new QTableWidgetItem(locale.toString(count["us_count"].toInt()));
        usCountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_countsTable->setItem(row, 5, usCountItem);

        // Format postage as currency
        bool ok;
        double postageValue = count["postage"].toString().toDouble(&ok);
        QString formattedPostage = ok ? locale.toCurrencyString(postageValue, "$", 2) : "$0.00";
        QTableWidgetItem* postageItem = new QTableWidgetItem(formattedPostage);
        postageItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
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

        QTableWidgetItem* inputItem = new QTableWidgetItem(locale.toString(comp["input_count"].toInt()));
        inputItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_comparisonTable->setItem(row, 1, inputItem);

        QTableWidgetItem* outputItem = new QTableWidgetItem(locale.toString(comp["output_count"].toInt()));
        outputItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_comparisonTable->setItem(row, 2, outputItem);

        // Highlight differences in red
        int difference = comp["difference"].toInt();
        QTableWidgetItem* diffItem = new QTableWidgetItem(locale.toString(difference));
        diffItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        if (difference != 0) {
            diffItem->setForeground(Qt::red);
            diffItem->setFont(QFont("Blender Pro", 10, QFont::Bold));
        }

        m_comparisonTable->setItem(row, 3, diffItem);
        row++;
    }
}

void CountsTableDialog::onCopyCountsButtonClicked()
{
    // Create Excel-compatible HTML with properly formatted table
    QString html = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n";
    html += "<html>\n<head>\n<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n";
    html += "<style type=\"text/css\">\n";
    html += "table { border-collapse: collapse; }\n";
    html += "th { font-weight: bold; text-align: center; padding: 4px; border: 1px solid black; background-color: #e0e0e0; }\n";
    html += "td { padding: 4px; border: 1px solid black; }\n";
    html += "tr:nth-child(odd) { background-color: #f9f9f9; }\n";
    html += "</style>\n</head>\n<body>\n";
    html += "<table border='1' cellspacing='0' cellpadding='3'>\n";

    // Add header row
    html += "<tr>";
    for (int col = 0; col < m_countsTable->columnCount(); ++col) {
        html += "<th>" + m_countsTable->horizontalHeaderItem(col)->text() + "</th>";
    }
    html += "</tr>\n";

    // Add data rows
    for (int row = 0; row < m_countsTable->rowCount(); ++row) {
        html += "<tr>";
        for (int col = 0; col < m_countsTable->columnCount(); ++col) {
            QTableWidgetItem* item = m_countsTable->item(row, col);
            QString cellText = item ? item->text() : "";
            QString alignment;

            // Set alignment based on column
            if (col == 0 || col == 1) {
                alignment = " style=\"text-align: center;\"";
            } else if (col == 2) {
                alignment = " style=\"text-align: left;\"";
            } else {
                alignment = " style=\"text-align: right;\"";
            }

            html += "<td" + alignment + ">" + cellText + "</td>";
        }
        html += "</tr>\n";
    }
    html += "</table>\n</body>\n</html>";

    // Set both HTML and plain text versions for maximum compatibility
    QMimeData* mimeData = new QMimeData();
    mimeData->setHtml(html);

    // Create plain text version for applications that don't support HTML
    QString plainText;
    for (int col = 0; col < m_countsTable->columnCount(); ++col) {
        plainText += m_countsTable->horizontalHeaderItem(col)->text() + "\t";
    }
    plainText.chop(1); // Remove trailing tab
    plainText += "\n";

    for (int row = 0; row < m_countsTable->rowCount(); ++row) {
        for (int col = 0; col < m_countsTable->columnCount(); ++col) {
            QTableWidgetItem* item = m_countsTable->item(row, col);
            plainText += (item ? item->text() : "") + "\t";
        }
        plainText.chop(1); // Remove trailing tab
        plainText += "\n";
    }
    mimeData->setText(plainText);

    // Create application/x-qt-windows-mime format for Excel
    QByteArray excelData;
    // Adding Excel-specific metadata
    excelData.append(html.toUtf8());
    mimeData->setData("application/x-qt-windows-mime;value=\"HTML Format\"", excelData);

    // Set clipboard content
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setMimeData(mimeData);

    // Create and run button color animation
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
