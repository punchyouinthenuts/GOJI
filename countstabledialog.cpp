#include "countstabledialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QClipboard>
#include <QApplication>
#include <QLocale>
#include <QDebug>

CountsTableDialog::CountsTableDialog(DatabaseManager* dbManager, QWidget* parent)
    : QDialog(parent), m_dbManager(dbManager)
{
    setupUI();
    loadData();
    setWindowTitle(tr("Post-Proof Counts and Comparison"));
    resize(600, 400);
}

void CountsTableDialog::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    // Counts table setup
    QPushButton* copyCountsButton = new QPushButton(tr("Copy Counts"), this);
    connect(copyCountsButton, &QPushButton::clicked, this, &CountsTableDialog::onCopyCountsButtonClicked);
    layout->addWidget(copyCountsButton);

    m_countsTable = new QTableWidget(this);
    m_countsTable->setColumnCount(7);
    m_countsTable->setHorizontalHeaderLabels({
        tr("Job Number"), tr("Week"), tr("Project"),
        tr("PR Count"), tr("CANC Count"), tr("US Count"), tr("Postage")
    });
    m_countsTable->setStyleSheet("QTableWidget { border: 1px solid black; } QTableWidget::item { border: 1px solid black; }");
    layout->addWidget(m_countsTable);

    // Comparison table setup
    QPushButton* copyComparisonButton = new QPushButton(tr("Copy Comparison"), this);
    connect(copyComparisonButton, &QPushButton::clicked, this, &CountsTableDialog::onCopyComparisonButtonClicked);
    layout->addWidget(copyComparisonButton);

    m_comparisonTable = new QTableWidget(this);
    m_comparisonTable->setColumnCount(4);
    m_comparisonTable->setHorizontalHeaderLabels({
        tr("Group"), tr("Input Count"), tr("Output Count"), tr("Difference")
    });
    m_comparisonTable->setStyleSheet("QTableWidget { border: 1px solid black; } QTableWidget::item { border: 1px solid black; }");
    layout->addWidget(m_comparisonTable);

    setLayout(layout);
}

void CountsTableDialog::loadData()
{
    // Load counts data
    QList<QMap<QString, QVariant>> counts = m_dbManager->getPostProofCounts();
    m_countsTable->setRowCount(counts.size());

    QLocale locale(QLocale::English, QLocale::UnitedStates);
    int row = 0;
    for (const QMap<QString, QVariant>& count : counts) {
        m_countsTable->setItem(row, 0, new QTableWidgetItem(count["job_number"].toString()));
        m_countsTable->setItem(row, 1, new QTableWidgetItem(count["week"].toString()));
        m_countsTable->setItem(row, 2, new QTableWidgetItem(count["project"].toString()));
        m_countsTable->setItem(row, 3, new QTableWidgetItem(count["pr_count"].toString()));
        m_countsTable->setItem(row, 4, new QTableWidgetItem(count["canc_count"].toString()));
        m_countsTable->setItem(row, 5, new QTableWidgetItem(count["us_count"].toString()));

        // Format postage as currency
        bool ok;
        double postageValue = count["postage"].toString().toDouble(&ok);
        QString formattedPostage = ok ? locale.toCurrencyString(postageValue, "$", 2) : "$0.00";
        m_countsTable->setItem(row, 6, new QTableWidgetItem(formattedPostage));

        row++;
    }

    // Load comparison data
    QList<QMap<QString, QVariant>> comparison = m_dbManager->getCountComparison();
    m_comparisonTable->setRowCount(comparison.size());

    row = 0;
    for (const QMap<QString, QVariant>& comp : comparison) {
        m_comparisonTable->setItem(row, 0, new QTableWidgetItem(comp["group_name"].toString()));
        m_comparisonTable->setItem(row, 1, new QTableWidgetItem(comp["input_count"].toString()));
        m_comparisonTable->setItem(row, 2, new QTableWidgetItem(comp["output_count"].toString()));
        m_comparisonTable->setItem(row, 3, new QTableWidgetItem(comp["difference"].toString()));

        row++;
    }
}

void CountsTableDialog::onCopyCountsButtonClicked()
{
    QString html = "<table border='1'>";
    html += "<tr>";
    for (int col = 0; col < m_countsTable->columnCount(); ++col) {
        html += "<th>" + m_countsTable->horizontalHeaderItem(col)->text() + "</th>";
    }
    html += "</tr>";

    for (int row = 0; row < m_countsTable->rowCount(); ++row) {
        html += "<tr>";
        for (int col = 0; col < m_countsTable->columnCount(); ++col) {
            QTableWidgetItem* item = m_countsTable->item(row, col);
            html += "<td>" + (item ? item->text() : "") + "</td>";
        }
        html += "</tr>";
    }
    html += "</table>";

    QApplication::clipboard()->setText(html, QClipboard::Clipboard);
}

void CountsTableDialog::onCopyComparisonButtonClicked()
{
    QString html = "<table border='1'>";
    html += "<tr>";
    for (int col = 0; col < m_comparisonTable->columnCount(); ++col) {
        html += "<th>" + m_comparisonTable->horizontalHeaderItem(col)->text() + "</th>";
    }
    html += "</tr>";

    for (int row = 0; row < m_comparisonTable->rowCount(); ++row) {
        html += "<tr>";
        for (int col = 0; col < m_comparisonTable->columnCount(); ++col) {
            QTableWidgetItem* item = m_comparisonTable->item(row, col);
            html += "<td>" + (item ? item->text() : "") + "</td>";
        }
        html += "</tr>";
    }
    html += "</table>";

    QApplication::clipboard()->setText(html, QClipboard::Clipboard);
}
