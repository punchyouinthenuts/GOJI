#include "misccombinedatadialog.h"

#include <QCloseEvent>
#include <QAbstractItemView>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

MiscCombineDataDialog::MiscCombineDataDialog(QWidget* parent)
    : QDialog(parent)
    , m_selectFilesButton(nullptr)
    , m_selectedFilesList(nullptr)
    , m_combineButton(nullptr)
    , m_statusLabel(nullptr)
    , m_closeButton(nullptr)
    , m_running(false)
{
    setWindowTitle("Combine Data Files");
    setModal(true);
    setFixedSize(700, 520);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);

    setupUi();
    rebuildFileList();
    updateControlStates();
}

void MiscCombineDataDialog::setStatusMessage(const QString& message, TerminalSeverity severity)
{
    if (!m_statusLabel) {
        return;
    }

    QString color = "#2c3e50";
    switch (severity) {
    case TerminalSeverity::Success:
        color = "#1f7a3a";
        break;
    case TerminalSeverity::Warning:
        color = "#a35d00";
        break;
    case TerminalSeverity::Error:
        color = "#b32020";
        break;
    case TerminalSeverity::Info:
    default:
        color = "#2c3e50";
        break;
    }

    m_statusLabel->setStyleSheet(
        QString("QLabel { border: 1px solid #bdc3c7; border-radius: 6px; background-color: #f8f9fa; "
                "padding: 8px; color: %1; }")
            .arg(color));
    m_statusLabel->setText(message);
}

void MiscCombineDataDialog::setRunning(bool running)
{
    m_running = running;
    updateControlStates();
}

void MiscCombineDataDialog::setSelectedFiles(const QStringList& files)
{
    m_selectedFiles = files;
    rebuildFileList();
    updateControlStates();
}

QStringList MiscCombineDataDialog::selectedFiles() const
{
    return m_selectedFiles;
}

void MiscCombineDataDialog::closeEvent(QCloseEvent* event)
{
    if (m_running) {
        event->ignore();
        return;
    }

    QDialog::closeEvent(event);
}

void MiscCombineDataDialog::onSelectFilesClicked()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        "Select Data Files",
        "C:/Users/JCox/Downloads",
        "Data Files (*.csv *.xls *.xlsx)");

    if (files.isEmpty()) {
        setStatusMessage("No files selected.", TerminalSeverity::Warning);
        return;
    }

    setSelectedFiles(files);
    setStatusMessage(QString("Selected %1 file(s).")
                         .arg(files.size()),
                     TerminalSeverity::Info);
}

void MiscCombineDataDialog::onCombineClicked()
{
    if (m_running) {
        return;
    }

    if (m_selectedFiles.isEmpty()) {
        setStatusMessage("Select at least one file before combining.", TerminalSeverity::Warning);
        return;
    }

    emit combineRequested(m_selectedFiles);
}

void MiscCombineDataDialog::onCloseClicked()
{
    if (!m_running) {
        accept();
    }
}

void MiscCombineDataDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QLabel* headerLabel = new QLabel("SELECT CSV/XLS/XLSX FILES TO COMBINE", this);
    headerLabel->setFont(QFont("Blender Pro Bold", 14, QFont::Bold));
    headerLabel->setAlignment(Qt::AlignCenter);
    headerLabel->setStyleSheet("color: #2c3e50;");
    mainLayout->addWidget(headerLabel);

    QLabel* outputLabel = new QLabel("Output: C:\\Users\\JCox\\Downloads\\COMBINED.csv", this);
    outputLabel->setFont(QFont("Consolas", 10));
    outputLabel->setAlignment(Qt::AlignCenter);
    outputLabel->setStyleSheet(
        "QLabel { border: 1px solid #bdc3c7; border-radius: 6px; background-color: #ffffff; "
        "padding: 8px; color: #2c3e50; }");
    outputLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mainLayout->addWidget(outputLabel);

    QHBoxLayout* selectButtonLayout = new QHBoxLayout();
    selectButtonLayout->addStretch();

    m_selectFilesButton = new QPushButton("SELECT FILES", this);
    m_selectFilesButton->setFont(QFont("Blender Pro Bold", 12, QFont::Bold));
    m_selectFilesButton->setFixedSize(160, 36);
    m_selectFilesButton->setStyleSheet(
        "QPushButton { background-color: #0078d4; color: white; border: none; border-radius: 4px; }"
        "QPushButton:hover { background-color: #106ebe; }"
        "QPushButton:pressed { background-color: #005a9e; }"
        "QPushButton:disabled { background-color: #9aa0a6; color: #eeeeee; }");
    connect(m_selectFilesButton, &QPushButton::clicked,
            this, &MiscCombineDataDialog::onSelectFilesClicked);

    selectButtonLayout->addWidget(m_selectFilesButton);
    selectButtonLayout->addStretch();
    mainLayout->addLayout(selectButtonLayout);

    QLabel* listTitle = new QLabel("Selected Files (order is processing order):", this);
    listTitle->setFont(QFont("Blender Pro Bold", 11, QFont::Bold));
    listTitle->setStyleSheet("color: #34495e;");
    mainLayout->addWidget(listTitle);

    m_selectedFilesList = new QListWidget(this);
    m_selectedFilesList->setSelectionMode(QAbstractItemView::NoSelection);
    m_selectedFilesList->setFont(QFont("Blender Pro", 10));
    m_selectedFilesList->setStyleSheet(
        "QListWidget { border: 2px solid #bdc3c7; border-radius: 8px; background-color: white; }"
        "QListWidget::item { padding: 6px; border-bottom: 1px solid #ecf0f1; }");
    mainLayout->addWidget(m_selectedFilesList);

    QHBoxLayout* combineButtonLayout = new QHBoxLayout();
    combineButtonLayout->addStretch();

    m_combineButton = new QPushButton("COMBINE", this);
    m_combineButton->setFont(QFont("Blender Pro Bold", 12, QFont::Bold));
    m_combineButton->setFixedSize(140, 38);
    m_combineButton->setStyleSheet(
        "QPushButton { background-color: #198754; color: white; border: none; border-radius: 4px; }"
        "QPushButton:hover { background-color: #157347; }"
        "QPushButton:pressed { background-color: #146c43; }"
        "QPushButton:disabled { background-color: #9aa0a6; color: #eeeeee; }");
    connect(m_combineButton, &QPushButton::clicked,
            this, &MiscCombineDataDialog::onCombineClicked);

    combineButtonLayout->addWidget(m_combineButton);
    combineButtonLayout->addStretch();
    mainLayout->addLayout(combineButtonLayout);

    m_statusLabel = new QLabel("Select files, then click COMBINE.", this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_statusLabel->setFont(QFont("Blender Pro", 10));
    mainLayout->addWidget(m_statusLabel);
    setStatusMessage("Select files, then click COMBINE.", TerminalSeverity::Info);

    QHBoxLayout* closeLayout = new QHBoxLayout();
    closeLayout->addStretch();

    m_closeButton = new QPushButton("CLOSE", this);
    m_closeButton->setFont(QFont("Blender Pro Bold", 12, QFont::Bold));
    m_closeButton->setFixedSize(110, 36);
    m_closeButton->setStyleSheet(
        "QPushButton { background-color: #6c757d; color: white; border: none; border-radius: 4px; }"
        "QPushButton:hover { background-color: #5a6268; }"
        "QPushButton:pressed { background-color: #4e555b; }"
        "QPushButton:disabled { background-color: #b2b6ba; color: #eeeeee; }");
    connect(m_closeButton, &QPushButton::clicked,
            this, &MiscCombineDataDialog::onCloseClicked);

    closeLayout->addWidget(m_closeButton);
    closeLayout->addStretch();
    mainLayout->addLayout(closeLayout);
}

void MiscCombineDataDialog::rebuildFileList()
{
    if (!m_selectedFilesList) {
        return;
    }

    m_selectedFilesList->clear();

    if (m_selectedFiles.isEmpty()) {
        m_selectedFilesList->addItem("No files selected.");
        return;
    }

    for (int i = 0; i < m_selectedFiles.size(); ++i) {
        const QString& path = m_selectedFiles.at(i);
        const QFileInfo fileInfo(path);
        m_selectedFilesList->addItem(QString("%1. %2")
                                         .arg(i + 1)
                                         .arg(fileInfo.fileName()));
    }
}

void MiscCombineDataDialog::updateControlStates()
{
    const bool hasFiles = !m_selectedFiles.isEmpty();

    if (m_selectFilesButton) {
        m_selectFilesButton->setEnabled(!m_running);
    }
    if (m_combineButton) {
        m_combineButton->setEnabled(!m_running && hasFiles);
    }
    if (m_closeButton) {
        m_closeButton->setEnabled(!m_running);
    }
}
