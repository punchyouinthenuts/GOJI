#include "miscdarkreportdialog.h"

#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QAbstractItemView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStringList>

namespace {
const QString kRuntimeDarkReportScriptPath =
    QStringLiteral("C:/Goji/scripts/THE DARK REPORT/PROCESS DATA FILE.py");
constexpr double kDomesticRate = 1.270;
constexpr double kInternationalRate = 3.400;

QString buildClipboardHtmlTable(const QTableWidget* table)
{
    if (!table || table->rowCount() <= 0 || table->columnCount() <= 0) {
        return QString();
    }

    QString html;
    html += "<table border=\"1\" cellspacing=\"0\" cellpadding=\"4\" "
            "style=\"border-collapse:collapse; font-family:Arial; font-size:11pt;\">";

    html += "<tr style=\"background-color:#dce6f1; font-weight:bold;\">";
    for (int col = 0; col < table->columnCount(); ++col) {
        const QTableWidgetItem* header = table->horizontalHeaderItem(col);
        const QString headerText = header ? header->text() : QString();
        html += QString("<th>%1</th>").arg(headerText.toHtmlEscaped());
    }
    html += "</tr>";

    for (int row = 0; row < table->rowCount(); ++row) {
        html += "<tr>";
        for (int col = 0; col < table->columnCount(); ++col) {
            const QTableWidgetItem* cell = table->item(row, col);
            const QString cellText = cell ? cell->text() : QString();
            int alignment = cell ? cell->textAlignment() : (Qt::AlignLeft | Qt::AlignVCenter);

            QString alignAttr = "left";
            if (alignment & Qt::AlignRight) {
                alignAttr = "right";
            } else if (alignment & Qt::AlignHCenter) {
                alignAttr = "center";
            }

            html += QString("<td align=\"%1\">%2</td>")
                        .arg(alignAttr, cellText.toHtmlEscaped());
        }
        html += "</tr>";
    }

    html += "</table>";
    return html;
}

QByteArray buildCfHtmlPayload(const QString& htmlFragment)
{
    const QByteArray fragmentUtf8 = htmlFragment.toUtf8();
    const QString bodyPrefix =
        "<html><body>\r\n"
        "<!--StartFragment-->\r\n";
    const QString bodySuffix =
        "\r\n<!--EndFragment-->\r\n"
        "</body></html>";

    const QByteArray bodyPrefixUtf8 = bodyPrefix.toUtf8();
    const QByteArray bodySuffixUtf8 = bodySuffix.toUtf8();

    const QString headerTemplate =
        "Version:0.9\r\n"
        "StartHTML:XXXXXXXXX\r\n"
        "EndHTML:XXXXXXXXX\r\n"
        "StartFragment:XXXXXXXXX\r\n"
        "EndFragment:XXXXXXXXX\r\n";
    const int headerLen = headerTemplate.toUtf8().size();

    const int startHtml = headerLen;
    const int startFragment = headerLen + bodyPrefixUtf8.size();
    const int endFragment = startFragment + fragmentUtf8.size();
    const int endHtml = endFragment + bodySuffixUtf8.size();

    const QString header =
        QString("Version:0.9\r\n")
        + QString("StartHTML:%1\r\n").arg(startHtml, 9, 10, QChar('0'))
        + QString("EndHTML:%1\r\n").arg(endHtml, 9, 10, QChar('0'))
        + QString("StartFragment:%1\r\n").arg(startFragment, 9, 10, QChar('0'))
        + QString("EndFragment:%1\r\n").arg(endFragment, 9, 10, QChar('0'));

    QByteArray payload;
    payload.append(header.toUtf8());
    payload.append(bodyPrefixUtf8);
    payload.append(fragmentUtf8);
    payload.append(bodySuffixUtf8);
    return payload;
}
}

MiscDarkReportDialog::MiscDarkReportDialog(QWidget* parent)
    : QDialog(parent)
    , m_chooseFileButton(nullptr)
    , m_selectedFileLabel(nullptr)
    , m_jobNumberLabel(nullptr)
    , m_jobNumberEdit(nullptr)
    , m_processButton(nullptr)
    , m_resultsTable(nullptr)
    , m_copyButton(nullptr)
    , m_closeButton(nullptr)
    , m_statusLabel(nullptr)
    , m_running(false)
    , m_hasResults(false)
{
    setWindowTitle("THE DARK REPORT");
    setModal(true);
    setFixedSize(1080, 620);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);

    setupUi();
    resetTable();
    updateControlStates();
    setStatusMessage("Choose a file, enter a 5-digit job number, then click PROCESS.",
                     TerminalSeverity::Info);
}

void MiscDarkReportDialog::setStatusMessage(const QString& message, TerminalSeverity severity)
{
    if (!m_statusLabel) {
        return;
    }

    m_statusLabel->setStyleSheet(
        QString("QLabel { border: 1px solid #bdc3c7; border-radius: 6px; background-color: #f8f9fa; "
                "padding: 8px; color: %1; }")
            .arg(statusColorForSeverity(severity)));
    m_statusLabel->setText(message);
}

void MiscDarkReportDialog::onChooseFileClicked()
{
    if (m_running) {
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "Select THE DARK REPORT File",
        "C:/Users/JCox/Downloads",
        "Data Files (*.csv *.xls *.xlsx)");

    if (filePath.trimmed().isEmpty()) {
        setStatusMessage("No file selected.", TerminalSeverity::Warning);
        emit terminalMessageRequested("THE DARK REPORT: no file selected.", TerminalSeverity::Warning);
        return;
    }

    m_selectedFilePath = filePath;
    if (m_selectedFileLabel) {
        m_selectedFileLabel->setText(QFileInfo(filePath).fileName());
    }

    setStatusMessage("File selected. Enter job number and click PROCESS.", TerminalSeverity::Info);
    emit terminalMessageRequested(
        QString("THE DARK REPORT: selected file %1").arg(QFileInfo(filePath).fileName()),
        TerminalSeverity::Info);
    updateControlStates();
}

void MiscDarkReportDialog::onProcessClicked()
{
    if (m_running) {
        return;
    }

    const QString jobNumber = m_jobNumberEdit ? m_jobNumberEdit->text().trimmed() : QString();
    if (!QRegularExpression(QStringLiteral("^\\d{5}$")).match(jobNumber).hasMatch()) {
        setStatusMessage("Job number must be exactly five digits.", TerminalSeverity::Error);
        emit terminalMessageRequested(
            "THE DARK REPORT: process blocked by invalid job number.",
            TerminalSeverity::Error);
        return;
    }

    if (m_selectedFilePath.trimmed().isEmpty()) {
        setStatusMessage("Choose an input file before processing.", TerminalSeverity::Error);
        emit terminalMessageRequested("THE DARK REPORT: process blocked by missing file.",
                                      TerminalSeverity::Error);
        return;
    }

    m_running = true;
    updateControlStates();
    setStatusMessage("Processing file...", TerminalSeverity::Info);

    QString errorMessage;
    QString outputFilePath;
    int domesticCount = 0;
    int internationalCount = 0;
    int totalCount = 0;

    const bool ok = runProcessorScript(m_selectedFilePath,
                                       jobNumber,
                                       &errorMessage,
                                       &domesticCount,
                                       &internationalCount,
                                       &totalCount,
                                       &outputFilePath);

    m_running = false;
    if (!ok) {
        m_hasResults = false;
        resetTable();
        setStatusMessage(errorMessage, TerminalSeverity::Error);
        emit terminalMessageRequested(QString("THE DARK REPORT: %1").arg(errorMessage),
                                      TerminalSeverity::Error);
        updateControlStates();
        return;
    }

    if (totalCount != (domesticCount + internationalCount)) {
        totalCount = domesticCount + internationalCount;
    }

    populateResultsTable(jobNumber, domesticCount, internationalCount);
    m_hasResults = true;
    updateControlStates();
    setStatusMessage(
        QString("PROCESS COMPLETE. Output saved: %1")
            .arg(QFileInfo(outputFilePath).fileName()),
        TerminalSeverity::Success);
    emit terminalMessageRequested(
        QString("THE DARK REPORT: processed successfully (%1 total pieces).").arg(totalCount),
        TerminalSeverity::Success);
}

void MiscDarkReportDialog::onCopyClicked()
{
    if (!m_hasResults) {
        setStatusMessage("No table data to copy. Run PROCESS first.", TerminalSeverity::Warning);
        return;
    }

    const QString plainText = buildClipboardText();
    const QString htmlTable = buildClipboardHtmlTable(m_resultsTable);
    if (plainText.trimmed().isEmpty() || htmlTable.trimmed().isEmpty()) {
        setStatusMessage("Unable to prepare table clipboard content.", TerminalSeverity::Error);
        emit terminalMessageRequested("THE DARK REPORT: failed to build clipboard table content.",
                                      TerminalSeverity::Error);
        return;
    }

    QMimeData* mime = new QMimeData();
    mime->setHtml(htmlTable);
    mime->setData("text/html", buildCfHtmlPayload(htmlTable));
    mime->setText(plainText);
    QApplication::clipboard()->setMimeData(mime);

    setStatusMessage("Copied formatted table data to clipboard.", TerminalSeverity::Success);
    emit terminalMessageRequested("THE DARK REPORT: copied formatted table data to clipboard.",
                                  TerminalSeverity::Success);
}

void MiscDarkReportDialog::onCloseClicked()
{
    if (!m_running) {
        accept();
    }
}

void MiscDarkReportDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(18, 18, 18, 16);

    QHBoxLayout* chooseLayout = new QHBoxLayout();
    chooseLayout->addStretch();
    m_chooseFileButton = new QPushButton("CHOOSE FILE", this);
    m_chooseFileButton->setFont(QFont("Blender Pro Bold", 12, QFont::Bold));
    m_chooseFileButton->setFixedSize(190, 38);
    m_chooseFileButton->setStyleSheet(
        "QPushButton { background-color: #0078d4; color: white; border: none; border-radius: 4px; }"
        "QPushButton:hover { background-color: #106ebe; }"
        "QPushButton:pressed { background-color: #005a9e; }"
        "QPushButton:disabled { background-color: #9aa0a6; color: #eeeeee; }");
    connect(m_chooseFileButton, &QPushButton::clicked,
            this, &MiscDarkReportDialog::onChooseFileClicked);
    chooseLayout->addWidget(m_chooseFileButton);
    chooseLayout->addStretch();
    mainLayout->addLayout(chooseLayout);

    m_selectedFileLabel = new QLabel("No file selected.", this);
    m_selectedFileLabel->setAlignment(Qt::AlignCenter);
    m_selectedFileLabel->setFont(QFont("Consolas", 10));
    m_selectedFileLabel->setStyleSheet(
        "QLabel { border: 1px solid #bdc3c7; border-radius: 6px; background-color: #ffffff; "
        "padding: 6px; color: #2c3e50; }");
    m_selectedFileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mainLayout->addWidget(m_selectedFileLabel);

    m_jobNumberLabel = new QLabel("JOB NUMBER", this);
    m_jobNumberLabel->setAlignment(Qt::AlignCenter);
    m_jobNumberLabel->setFont(QFont("Blender Pro", 14, QFont::Bold));
    m_jobNumberLabel->setStyleSheet("color: #2c3e50;");
    mainLayout->addWidget(m_jobNumberLabel);

    QHBoxLayout* jobLayout = new QHBoxLayout();
    jobLayout->addStretch();
    m_jobNumberEdit = new QLineEdit(this);
    m_jobNumberEdit->setFixedSize(130, 30);
    m_jobNumberEdit->setMaxLength(5);
    m_jobNumberEdit->setAlignment(Qt::AlignCenter);
    m_jobNumberEdit->setFont(QFont("Blender Pro", 14, QFont::Bold));
    m_jobNumberEdit->setValidator(
        new QRegularExpressionValidator(QRegularExpression("^\\d{0,5}$"), m_jobNumberEdit));
    m_jobNumberEdit->setStyleSheet(
        "QLineEdit { border: 1px solid #bdc3c7; border-radius: 4px; background-color: white; color: #2c3e50; padding: 2px 4px; }"
        "QLineEdit:focus { border: 1px solid #0078d4; }");
    jobLayout->addWidget(m_jobNumberEdit);
    jobLayout->addStretch();
    mainLayout->addLayout(jobLayout);

    QHBoxLayout* processLayout = new QHBoxLayout();
    processLayout->addStretch();
    m_processButton = new QPushButton("PROCESS", this);
    m_processButton->setFont(QFont("Blender Pro Bold", 12, QFont::Bold));
    m_processButton->setFixedSize(150, 38);
    m_processButton->setStyleSheet(
        "QPushButton { background-color: #198754; color: white; border: none; border-radius: 4px; }"
        "QPushButton:hover { background-color: #157347; }"
        "QPushButton:pressed { background-color: #146c43; }"
        "QPushButton:disabled { background-color: #9aa0a6; color: #eeeeee; }");
    connect(m_processButton, &QPushButton::clicked, this, &MiscDarkReportDialog::onProcessClicked);
    processLayout->addWidget(m_processButton);
    processLayout->addStretch();
    mainLayout->addLayout(processLayout);

    m_resultsTable = new QTableWidget(3, 8, this);
    m_resultsTable->setHorizontalHeaderLabels(
        QStringList() << "JOB" << "DESCRIPTION" << "POSTAGE" << "COUNT"
                      << "AVG RATE" << "CLASS" << "SHAPE" << "PERMIT");
    m_resultsTable->verticalHeader()->setVisible(false);
    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_resultsTable->setFocusPolicy(Qt::NoFocus);
    m_resultsTable->setShowGrid(true);
    m_resultsTable->setAlternatingRowColors(false);
    m_resultsTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_resultsTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_resultsTable->setStyleSheet(
        "QTableWidget { border: 2px solid #bdc3c7; border-radius: 8px; background-color: white; gridline-color: #bdc3c7; }"
        "QHeaderView::section { background-color: #eceff3; color: #2c3e50; border: 1px solid #bdc3c7; font: bold 11pt 'Blender Pro'; padding: 6px; }");
    m_resultsTable->setFont(QFont("Blender Pro", 11));
    m_resultsTable->horizontalHeader()->setStretchLastSection(false);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Fixed);
    m_resultsTable->setColumnWidth(0, 92);
    m_resultsTable->setColumnWidth(1, 278);
    m_resultsTable->setColumnWidth(2, 118);
    m_resultsTable->setColumnWidth(3, 96);
    m_resultsTable->setColumnWidth(4, 108);
    m_resultsTable->setColumnWidth(5, 96);
    m_resultsTable->setColumnWidth(6, 84);
    m_resultsTable->setColumnWidth(7, 98);
    for (int row = 0; row < 3; ++row) {
        m_resultsTable->setRowHeight(row, 34);
    }
    m_resultsTable->setFixedHeight(34 * 3 + m_resultsTable->horizontalHeader()->height() + 8);
    mainLayout->addWidget(m_resultsTable);

    QHBoxLayout* copyLayout = new QHBoxLayout();
    copyLayout->addStretch();
    m_copyButton = new QPushButton("COPY", this);
    m_copyButton->setFont(QFont("Blender Pro Bold", 12, QFont::Bold));
    m_copyButton->setFixedSize(120, 36);
    m_copyButton->setStyleSheet(
        "QPushButton { background-color: #0d6efd; color: white; border: none; border-radius: 4px; }"
        "QPushButton:hover { background-color: #0b5ed7; }"
        "QPushButton:pressed { background-color: #0a58ca; }"
        "QPushButton:disabled { background-color: #9aa0a6; color: #eeeeee; }");
    connect(m_copyButton, &QPushButton::clicked, this, &MiscDarkReportDialog::onCopyClicked);
    copyLayout->addWidget(m_copyButton);
    copyLayout->addStretch();
    mainLayout->addLayout(copyLayout);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_statusLabel->setFont(QFont("Blender Pro", 10));
    mainLayout->addWidget(m_statusLabel);

    QHBoxLayout* closeLayout = new QHBoxLayout();
    closeLayout->addStretch();
    m_closeButton = new QPushButton("CLOSE", this);
    m_closeButton->setFont(QFont("Blender Pro Bold", 12, QFont::Bold));
    m_closeButton->setFixedSize(120, 36);
    m_closeButton->setStyleSheet(
        "QPushButton { background-color: #6c757d; color: white; border: none; border-radius: 4px; }"
        "QPushButton:hover { background-color: #5a6268; }"
        "QPushButton:pressed { background-color: #4e555b; }"
        "QPushButton:disabled { background-color: #b2b6ba; color: #eeeeee; }");
    connect(m_closeButton, &QPushButton::clicked, this, &MiscDarkReportDialog::onCloseClicked);
    closeLayout->addWidget(m_closeButton);
    closeLayout->addStretch();
    mainLayout->addLayout(closeLayout);
}

void MiscDarkReportDialog::updateControlStates()
{
    if (m_chooseFileButton) {
        m_chooseFileButton->setEnabled(!m_running);
    }

    if (m_jobNumberEdit) {
        m_jobNumberEdit->setEnabled(!m_running);
    }

    if (m_processButton) {
        m_processButton->setEnabled(!m_running && !m_selectedFilePath.trimmed().isEmpty());
    }

    if (m_copyButton) {
        m_copyButton->setEnabled(!m_running && m_hasResults);
    }

    if (m_closeButton) {
        m_closeButton->setEnabled(!m_running);
    }
}

void MiscDarkReportDialog::resetTable()
{
    if (!m_resultsTable) {
        return;
    }

    for (int row = 0; row < m_resultsTable->rowCount(); ++row) {
        for (int col = 0; col < m_resultsTable->columnCount(); ++col) {
            setCell(row, col, QString(), Qt::AlignLeft | Qt::AlignVCenter, false);
        }
    }
}

void MiscDarkReportDialog::populateResultsTable(const QString& jobNumber,
                                                int domesticCount,
                                                int internationalCount)
{
    if (!m_resultsTable) {
        return;
    }

    const double domesticPostage = domesticCount * kDomesticRate;
    const double internationalPostage = internationalCount * kInternationalRate;
    const double totalPostage = domesticPostage + internationalPostage;
    const int totalCount = domesticCount + internationalCount;

    setCell(0, 0, jobNumber, Qt::AlignCenter, false);
    setCell(0, 1, "THE DARK REPORT", Qt::AlignLeft | Qt::AlignVCenter, false);
    setCell(0, 2, formatCurrency(domesticPostage), Qt::AlignRight | Qt::AlignVCenter, false);
    setCell(0, 3, QString::number(domesticCount), Qt::AlignRight | Qt::AlignVCenter, false);
    setCell(0, 4, "1.270", Qt::AlignCenter, false);
    setCell(0, 5, "FC NM", Qt::AlignCenter, false);
    setCell(0, 6, "LTR", Qt::AlignCenter, false);
    setCell(0, 7, "STAMP", Qt::AlignCenter, false);

    setCell(1, 0, QString(), Qt::AlignCenter, false);
    setCell(1, 1, QString(), Qt::AlignLeft | Qt::AlignVCenter, false);
    setCell(1, 2, formatCurrency(internationalPostage), Qt::AlignRight | Qt::AlignVCenter, false);
    setCell(1, 3, QString::number(internationalCount), Qt::AlignRight | Qt::AlignVCenter, false);
    setCell(1, 4, "3.400", Qt::AlignCenter, false);
    setCell(1, 5, "FC INTL", Qt::AlignCenter, false);
    setCell(1, 6, "LTR", Qt::AlignCenter, false);
    setCell(1, 7, "METER", Qt::AlignCenter, false);

    setCell(2, 0, QString(), Qt::AlignCenter, false);
    setCell(2, 1, QString(), Qt::AlignLeft | Qt::AlignVCenter, false);
    setCell(2, 2, formatCurrency(totalPostage), Qt::AlignRight | Qt::AlignVCenter, false);
    setCell(2, 3, QString::number(totalCount), Qt::AlignRight | Qt::AlignVCenter, false);
    setCell(2, 4, QString(), Qt::AlignCenter, false);
    setCell(2, 5, QString(), Qt::AlignCenter, false);
    setCell(2, 6, QString(), Qt::AlignCenter, false);
    setCell(2, 7, QString(), Qt::AlignCenter, false);
}

bool MiscDarkReportDialog::runProcessorScript(const QString& filePath,
                                              const QString& jobNumber,
                                              QString* errorMessage,
                                              int* domesticCount,
                                              int* internationalCount,
                                              int* totalCount,
                                              QString* outputFilePath)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (domesticCount) {
        *domesticCount = 0;
    }
    if (internationalCount) {
        *internationalCount = 0;
    }
    if (totalCount) {
        *totalCount = 0;
    }
    if (outputFilePath) {
        outputFilePath->clear();
    }

    const QFileInfo scriptInfo(kRuntimeDarkReportScriptPath);
    if (!scriptInfo.exists()) {
        if (errorMessage) {
            *errorMessage = QString("Script not found: %1")
                                .arg(QDir::toNativeSeparators(kRuntimeDarkReportScriptPath));
        }
        return false;
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    QStringList arguments;
    arguments << kRuntimeDarkReportScriptPath
              << "--input-file" << filePath
              << "--job-number" << jobNumber
              << "--json";

    process.start("python", arguments, QIODevice::ReadOnly);
    if (!process.waitForStarted(5000)) {
        if (errorMessage) {
            *errorMessage = "Failed to start Python process.";
        }
        return false;
    }

    if (!process.waitForFinished(120000)) {
        process.kill();
        process.waitForFinished(2000);
        if (errorMessage) {
            *errorMessage = "Processing timed out.";
        }
        return false;
    }

    const QString stdoutText = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    const QString stderrText = QString::fromUtf8(process.readAllStandardError()).trimmed();

    QJsonObject payload;
    bool parsed = false;
    const QStringList lines = stdoutText.split('\n', Qt::SkipEmptyParts);
    for (int i = lines.size() - 1; i >= 0; --i) {
        const QString candidate = lines.at(i).trimmed();
        if (!candidate.startsWith('{') || !candidate.endsWith('}')) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(candidate.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            payload = document.object();
            parsed = true;
            break;
        }
    }

    if (!parsed) {
        if (errorMessage) {
            *errorMessage = stdoutText.isEmpty()
                ? QString("Processing failed. %1").arg(stderrText.isEmpty() ? "No output returned." : stderrText)
                : QString("Processing returned invalid output: %1").arg(stdoutText);
        }
        return false;
    }

    if (!payload.value("ok").toBool(false)) {
        if (errorMessage) {
            const QString error = payload.value("error").toString().trimmed();
            *errorMessage = error.isEmpty() ? "Processing failed." : error;
        }
        return false;
    }

    if (domesticCount) {
        *domesticCount = payload.value("domestic_count").toInt(0);
    }
    if (internationalCount) {
        *internationalCount = payload.value("international_count").toInt(0);
    }
    if (totalCount) {
        *totalCount = payload.value("total_count").toInt(0);
    }
    if (outputFilePath) {
        *outputFilePath = payload.value("output_file").toString().trimmed();
    }

    return true;
}

QString MiscDarkReportDialog::statusColorForSeverity(TerminalSeverity severity)
{
    switch (severity) {
    case TerminalSeverity::Success:
        return "#1f7a3a";
    case TerminalSeverity::Warning:
        return "#a35d00";
    case TerminalSeverity::Error:
        return "#b32020";
    case TerminalSeverity::Info:
    default:
        return "#2c3e50";
    }
}

QString MiscDarkReportDialog::formatCurrency(double value)
{
    return QString("$%1").arg(value, 0, 'f', 2);
}

void MiscDarkReportDialog::setCell(int row,
                                   int column,
                                   const QString& text,
                                   Qt::Alignment alignment,
                                   bool bold)
{
    if (!m_resultsTable) {
        return;
    }

    QTableWidgetItem* item = m_resultsTable->item(row, column);
    if (!item) {
        item = new QTableWidgetItem();
        m_resultsTable->setItem(row, column, item);
    }

    item->setText(text);
    item->setTextAlignment(alignment);
    QFont font("Blender Pro", 11);
    font.setBold(bold);
    item->setFont(font);
}

QString MiscDarkReportDialog::buildClipboardText() const
{
    if (!m_resultsTable) {
        return QString();
    }

    QStringList lines;

    for (int row = 0; row < m_resultsTable->rowCount(); ++row) {
        QStringList cells;
        cells.reserve(m_resultsTable->columnCount());
        for (int col = 0; col < m_resultsTable->columnCount(); ++col) {
            const QTableWidgetItem* cell = m_resultsTable->item(row, col);
            cells << (cell ? cell->text() : QString());
        }
        lines << cells.join('\t');
    }

    return lines.join('\n');
}
