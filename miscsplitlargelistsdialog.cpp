#include "miscsplitlargelistsdialog.h"

#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

MiscSplitLargeListsDialog::MiscSplitLargeListsDialog(QWidget* parent)
    : QDialog(parent)
    , m_openButton(nullptr)
    , m_selectedFileNameLabel(nullptr)
    , m_downloadsRadio(nullptr)
    , m_inputRadio(nullptr)
    , m_otherRadio(nullptr)
    , m_outputPathLabel(nullptr)
    , m_halfButton(nullptr)
    , m_thirdButton(nullptr)
    , m_quarterButton(nullptr)
    , m_previewLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_primaryButton(nullptr)
    , m_recordCount(0)
    , m_outputDestination(OutputDestination::Downloads)
    , m_otherOutputDirectory(QStringLiteral("C:/Users/JCox/Downloads"))
    , m_running(false)
{
    setWindowTitle("Split Large Lists");
    setModal(true);
    setFixedSize(760, 560);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);

    setupUi();
    updatePrimaryButtonText();
    updateOutputPathLabel();
    updatePreviewText();
    setStatusMessage("Click OPEN to load a file.", TerminalSeverity::Info);
    updateControlStates();
}

void MiscSplitLargeListsDialog::setStatusMessage(const QString& message, TerminalSeverity severity)
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

void MiscSplitLargeListsDialog::setRunning(bool running)
{
    m_running = running;
    updateControlStates();
}

void MiscSplitLargeListsDialog::setLoadedFileInfo(const QString& filePath,
                                                  const QString& baseName,
                                                  qint64 recordCount)
{
    m_loadedFilePath = filePath.trimmed();
    m_baseOutputName = baseName.trimmed();
    m_recordCount = recordCount < 0 ? 0 : recordCount;

    if (m_selectedFileNameLabel) {
        if (m_loadedFilePath.isEmpty()) {
            m_selectedFileNameLabel->setText("No file loaded.");
        } else {
            m_selectedFileNameLabel->setText(QFileInfo(m_loadedFilePath).fileName());
        }
    }

    clearSplitSelection();
    updateOutputPathLabel();
    updatePreviewText();
    updatePrimaryButtonText();
    updateControlStates();
}

void MiscSplitLargeListsDialog::closeEvent(QCloseEvent* event)
{
    if (m_running) {
        event->ignore();
        return;
    }

    QDialog::closeEvent(event);
}

void MiscSplitLargeListsDialog::onOpenClicked()
{
    if (m_running) {
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "Select File",
        "C:/Users/JCox/Downloads",
        "Data Files (*.csv *.xls *.xlsx)");

    if (filePath.isEmpty()) {
        setStatusMessage("No file selected.", TerminalSeverity::Warning);
        emit terminalMessageRequested("Split Large Lists: no file selected.", TerminalSeverity::Warning);
        return;
    }

    setStatusMessage("Loading record count...", TerminalSeverity::Info);
    emit terminalMessageRequested(
        QString("Split Large Lists: selected file %1").arg(QFileInfo(filePath).fileName()),
        TerminalSeverity::Info);
    emit loadRequested(filePath);
}

void MiscSplitLargeListsDialog::onDestinationSelectionChanged()
{
    if (m_running) {
        return;
    }

    if (sender() == m_downloadsRadio && m_downloadsRadio && m_downloadsRadio->isChecked()) {
        m_outputDestination = OutputDestination::Downloads;
        updateOutputPathLabel();
        return;
    }

    if (sender() == m_inputRadio && m_inputRadio && m_inputRadio->isChecked()) {
        m_outputDestination = OutputDestination::Input;
        updateOutputPathLabel();
        return;
    }

    if (sender() == m_otherRadio && m_otherRadio && m_otherRadio->isChecked()) {
        const QString initialDir = m_otherOutputDirectory.trimmed().isEmpty()
            ? QStringLiteral("C:/Users/JCox/Downloads")
            : m_otherOutputDirectory;
        const QString selectedDir = QFileDialog::getExistingDirectory(
            this,
            "Select Output Directory",
            initialDir,
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

        if (selectedDir.trimmed().isEmpty()) {
            QSignalBlocker blockOther(m_otherRadio);
            QSignalBlocker blockDownloads(m_downloadsRadio);
            m_otherRadio->setChecked(false);
            m_downloadsRadio->setChecked(true);
            m_outputDestination = OutputDestination::Downloads;
            setStatusMessage("Output destination reverted to Downloads.",
                             TerminalSeverity::Warning);
            emit terminalMessageRequested(
                "Split Large Lists: output folder selection canceled, using Downloads.",
                TerminalSeverity::Warning);
        } else {
            m_otherOutputDirectory = selectedDir;
            m_outputDestination = OutputDestination::Other;
        }

        updateOutputPathLabel();
    }
}

void MiscSplitLargeListsDialog::onSplitButtonClicked()
{
    if (m_running) {
        return;
    }

    QPushButton* clickedButton = qobject_cast<QPushButton*>(sender());
    if (!clickedButton) {
        return;
    }

    const bool checked = clickedButton->isChecked();
    if (checked) {
        if (clickedButton != m_halfButton && m_halfButton) {
            QSignalBlocker blocker(m_halfButton);
            m_halfButton->setChecked(false);
        }
        if (clickedButton != m_thirdButton && m_thirdButton) {
            QSignalBlocker blocker(m_thirdButton);
            m_thirdButton->setChecked(false);
        }
        if (clickedButton != m_quarterButton && m_quarterButton) {
            QSignalBlocker blocker(m_quarterButton);
            m_quarterButton->setChecked(false);
        }
    }

    updatePrimaryButtonText();
    updatePreviewText();
}

void MiscSplitLargeListsDialog::onPrimaryClicked()
{
    if (m_running) {
        return;
    }

    const int parts = selectedParts();
    if (parts <= 0) {
        accept();
        return;
    }

    if (m_loadedFilePath.trimmed().isEmpty()) {
        setStatusMessage("No file loaded.", TerminalSeverity::Error);
        emit terminalMessageRequested("Split Large Lists run blocked: no file loaded.",
                                      TerminalSeverity::Error);
        return;
    }

    emit runRequested(m_loadedFilePath,
                      parts,
                      effectiveOutputDirectory(),
                      m_baseOutputName);
}

void MiscSplitLargeListsDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QLabel* headerLabel = new QLabel("LOAD FILE, CHOOSE SPLIT, AND RUN", this);
    headerLabel->setFont(QFont("Blender Pro Bold", 14, QFont::Bold));
    headerLabel->setAlignment(Qt::AlignCenter);
    headerLabel->setStyleSheet("color: #2c3e50;");
    mainLayout->addWidget(headerLabel);

    QHBoxLayout* openLayout = new QHBoxLayout();
    openLayout->addStretch();

    m_openButton = new QPushButton("OPEN", this);
    m_openButton->setFont(QFont("Blender Pro Bold", 12, QFont::Bold));
    m_openButton->setFixedSize(130, 36);
    m_openButton->setStyleSheet(
        "QPushButton { background-color: #0078d4; color: white; border: none; border-radius: 4px; }"
        "QPushButton:hover { background-color: #106ebe; }"
        "QPushButton:pressed { background-color: #005a9e; }"
        "QPushButton:disabled { background-color: #9aa0a6; color: #eeeeee; }");
    connect(m_openButton, &QPushButton::clicked, this, &MiscSplitLargeListsDialog::onOpenClicked);

    openLayout->addWidget(m_openButton);
    openLayout->addStretch();
    mainLayout->addLayout(openLayout);

    QLabel* fileTitleLabel = new QLabel("Selected File:", this);
    fileTitleLabel->setFont(QFont("Blender Pro Bold", 11, QFont::Bold));
    fileTitleLabel->setStyleSheet("color: #34495e;");
    mainLayout->addWidget(fileTitleLabel);

    m_selectedFileNameLabel = new QLabel("No file loaded.", this);
    m_selectedFileNameLabel->setFont(QFont("Consolas", 10));
    m_selectedFileNameLabel->setStyleSheet(
        "QLabel { border: 1px solid #bdc3c7; border-radius: 6px; background-color: #ffffff; "
        "padding: 8px; color: #2c3e50; }");
    m_selectedFileNameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mainLayout->addWidget(m_selectedFileNameLabel);

    QLabel* outputTitleLabel = new QLabel("Output Destination:", this);
    outputTitleLabel->setFont(QFont("Blender Pro Bold", 11, QFont::Bold));
    outputTitleLabel->setStyleSheet("color: #34495e;");
    mainLayout->addWidget(outputTitleLabel);

    QHBoxLayout* destinationLayout = new QHBoxLayout();
    destinationLayout->setSpacing(12);
    m_downloadsRadio = new QRadioButton("DOWNLOADS", this);
    m_inputRadio = new QRadioButton("INPUT", this);
    m_otherRadio = new QRadioButton("OTHER", this);
    m_downloadsRadio->setChecked(true);
    m_downloadsRadio->setFont(QFont("Blender Pro Bold", 10, QFont::Bold));
    m_inputRadio->setFont(QFont("Blender Pro Bold", 10, QFont::Bold));
    m_otherRadio->setFont(QFont("Blender Pro Bold", 10, QFont::Bold));
    connect(m_downloadsRadio, &QRadioButton::clicked,
            this, &MiscSplitLargeListsDialog::onDestinationSelectionChanged);
    connect(m_inputRadio, &QRadioButton::clicked,
            this, &MiscSplitLargeListsDialog::onDestinationSelectionChanged);
    connect(m_otherRadio, &QRadioButton::clicked,
            this, &MiscSplitLargeListsDialog::onDestinationSelectionChanged);
    destinationLayout->addWidget(m_downloadsRadio);
    destinationLayout->addWidget(m_inputRadio);
    destinationLayout->addWidget(m_otherRadio);
    destinationLayout->addStretch();
    mainLayout->addLayout(destinationLayout);

    m_outputPathLabel = new QLabel(this);
    m_outputPathLabel->setFont(QFont("Consolas", 10));
    m_outputPathLabel->setStyleSheet(
        "QLabel { border: 1px solid #bdc3c7; border-radius: 6px; background-color: #ffffff; "
        "padding: 8px; color: #2c3e50; }");
    m_outputPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mainLayout->addWidget(m_outputPathLabel);

    QLabel* splitTitleLabel = new QLabel("Split Choice:", this);
    splitTitleLabel->setFont(QFont("Blender Pro Bold", 11, QFont::Bold));
    splitTitleLabel->setStyleSheet("color: #34495e;");
    mainLayout->addWidget(splitTitleLabel);

    QHBoxLayout* splitButtonsLayout = new QHBoxLayout();
    splitButtonsLayout->addStretch();
    m_halfButton = new QPushButton(QString::fromUtf8("\u00bd"), this);
    m_thirdButton = new QPushButton(QString::fromUtf8("\u2153"), this);
    m_quarterButton = new QPushButton(QString::fromUtf8("\u00bc"), this);
    const QList<QPushButton*> splitButtons = {m_halfButton, m_thirdButton, m_quarterButton};
    for (QPushButton* button : splitButtons) {
        button->setCheckable(true);
        button->setFixedSize(68, 42);
        button->setFont(QFont("Segoe UI Symbol", 16, QFont::Bold));
        button->setStyleSheet(
            "QPushButton { background-color: #ffffff; color: #2c3e50; border: 1px solid #bdc3c7; border-radius: 4px; }"
            "QPushButton:hover { background-color: #f1f4f7; }"
            "QPushButton:checked { background-color: #0078d4; color: white; border: 1px solid #005a9e; }"
            "QPushButton:disabled { background-color: #f5f5f5; color: #9aa0a6; border: 1px solid #d9d9d9; }");
        connect(button, &QPushButton::clicked, this, &MiscSplitLargeListsDialog::onSplitButtonClicked);
        splitButtonsLayout->addWidget(button);
    }
    splitButtonsLayout->addStretch();
    mainLayout->addLayout(splitButtonsLayout);

    QLabel* previewTitle = new QLabel("Preview:", this);
    previewTitle->setFont(QFont("Blender Pro Bold", 11, QFont::Bold));
    previewTitle->setStyleSheet("color: #34495e;");
    mainLayout->addWidget(previewTitle);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setFont(QFont("Consolas", 10));
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setStyleSheet(
        "QLabel { border: 1px solid #bdc3c7; border-radius: 6px; background-color: #ffffff; "
        "padding: 8px; color: #2c3e50; min-height: 92px; }");
    mainLayout->addWidget(m_previewLabel);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_statusLabel->setFont(QFont("Blender Pro", 10));
    mainLayout->addWidget(m_statusLabel);

    QHBoxLayout* primaryLayout = new QHBoxLayout();
    primaryLayout->addStretch();
    m_primaryButton = new QPushButton(this);
    m_primaryButton->setFont(QFont("Blender Pro Bold", 12, QFont::Bold));
    m_primaryButton->setFixedSize(140, 38);
    m_primaryButton->setStyleSheet(
        "QPushButton { background-color: #6c757d; color: white; border: none; border-radius: 4px; }"
        "QPushButton:hover { background-color: #5a6268; }"
        "QPushButton:pressed { background-color: #4e555b; }"
        "QPushButton:disabled { background-color: #b2b6ba; color: #eeeeee; }");
    connect(m_primaryButton, &QPushButton::clicked, this, &MiscSplitLargeListsDialog::onPrimaryClicked);
    primaryLayout->addWidget(m_primaryButton);
    primaryLayout->addStretch();
    mainLayout->addLayout(primaryLayout);
}

void MiscSplitLargeListsDialog::updateControlStates()
{
    const bool hasLoadedFile = !m_loadedFilePath.trimmed().isEmpty();

    if (m_openButton) {
        m_openButton->setEnabled(!m_running);
    }
    if (m_downloadsRadio) {
        m_downloadsRadio->setEnabled(!m_running);
    }
    if (m_inputRadio) {
        m_inputRadio->setEnabled(!m_running);
    }
    if (m_otherRadio) {
        m_otherRadio->setEnabled(!m_running);
    }
    if (m_halfButton) {
        m_halfButton->setEnabled(!m_running && hasLoadedFile);
    }
    if (m_thirdButton) {
        m_thirdButton->setEnabled(!m_running && hasLoadedFile);
    }
    if (m_quarterButton) {
        m_quarterButton->setEnabled(!m_running && hasLoadedFile);
    }
    if (m_primaryButton) {
        m_primaryButton->setEnabled(!m_running);
    }
}

void MiscSplitLargeListsDialog::updatePrimaryButtonText()
{
    if (!m_primaryButton) {
        return;
    }

    if (selectedParts() > 0) {
        m_primaryButton->setText("RUN");
    } else {
        m_primaryButton->setText("EXIT");
    }
}

void MiscSplitLargeListsDialog::updateOutputPathLabel()
{
    if (!m_outputPathLabel) {
        return;
    }

    m_outputPathLabel->setText(QDir::toNativeSeparators(effectiveOutputDirectory()));
}

void MiscSplitLargeListsDialog::updatePreviewText()
{
    if (!m_previewLabel) {
        return;
    }

    if (m_loadedFilePath.trimmed().isEmpty()) {
        m_previewLabel->setText("Load a file to preview split counts.");
        return;
    }

    const int parts = selectedParts();
    if (parts <= 0) {
        m_previewLabel->setText("Select a split option to preview.");
        return;
    }

    const qint64 baseSize = m_recordCount / parts;
    const int remainder = static_cast<int>(m_recordCount % parts);
    QStringList lines;
    for (int i = 0; i < parts; ++i) {
        const qint64 size = baseSize + (i < remainder ? 1 : 0);
        lines << QString("Part %1: %2 records")
                     .arg(i + 1, 2, 10, QLatin1Char('0'))
                     .arg(formatRecordCount(size));
    }
    m_previewLabel->setText(lines.join('\n'));
}

QString MiscSplitLargeListsDialog::effectiveOutputDirectory() const
{
    if (m_outputDestination == OutputDestination::Input) {
        const QString trimmedPath = m_loadedFilePath.trimmed();
        if (!trimmedPath.isEmpty()) {
            return QFileInfo(trimmedPath).absolutePath();
        }
    }

    if (m_outputDestination == OutputDestination::Other) {
        const QString trimmed = m_otherOutputDirectory.trimmed();
        if (!trimmed.isEmpty()) {
            return trimmed;
        }
    }

    return QStringLiteral("C:/Users/JCox/Downloads");
}

int MiscSplitLargeListsDialog::selectedParts() const
{
    if (m_halfButton && m_halfButton->isChecked()) {
        return 2;
    }
    if (m_thirdButton && m_thirdButton->isChecked()) {
        return 3;
    }
    if (m_quarterButton && m_quarterButton->isChecked()) {
        return 4;
    }
    return 0;
}

void MiscSplitLargeListsDialog::clearSplitSelection()
{
    if (m_halfButton) {
        QSignalBlocker blocker(m_halfButton);
        m_halfButton->setChecked(false);
    }
    if (m_thirdButton) {
        QSignalBlocker blocker(m_thirdButton);
        m_thirdButton->setChecked(false);
    }
    if (m_quarterButton) {
        QSignalBlocker blocker(m_quarterButton);
        m_quarterButton->setChecked(false);
    }
}

QString MiscSplitLargeListsDialog::statusColorForSeverity(TerminalSeverity severity)
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

QString MiscSplitLargeListsDialog::formatRecordCount(qint64 value)
{
    const QLocale locale(QLocale::English, QLocale::UnitedStates);
    return locale.toString(value);
}
