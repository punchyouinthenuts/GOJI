#include "miscrenameheadersdialog.h"

#include <QAbstractItemView>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QKeyEvent>

namespace {
QString statusColorForSeverity(TerminalSeverity severity)
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

class RenameHeadersTableWidget : public QTableWidget
{
public:
    explicit RenameHeadersTableWidget(QWidget* parent = nullptr)
        : QTableWidget(parent)
    {
    }

protected:
    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override
    {
        Q_UNUSED(modifiers)

        const int totalRows = rowCount();
        if (totalRows <= 0) {
            return QTableWidget::moveCursor(cursorAction, modifiers);
        }

        const QModelIndex current = currentIndex();
        int row = current.isValid() ? current.row() : 0;
        if (row < 0) {
            row = 0;
        }

        if (cursorAction == MoveNext) {
            row = qMin(row + 1, totalRows - 1);
            return model()->index(row, 1, rootIndex());
        }

        if (cursorAction == MovePrevious) {
            row = qMax(row - 1, 0);
            return model()->index(row, 1, rootIndex());
        }

        if (current.isValid() && current.column() != 1) {
            return model()->index(current.row(), 1, rootIndex());
        }

        return QTableWidget::moveCursor(cursorAction, modifiers);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (!event) {
            QTableWidget::keyPressEvent(event);
            return;
        }

        const int totalRows = rowCount();
        const QModelIndex current = currentIndex();
        int row = current.isValid() ? current.row() : 0;

        if (event->key() == Qt::Key_Tab) {
            if (totalRows > 0) {
                const int nextRow = qMin(row + 1, totalRows - 1);
                setCurrentCell(nextRow, 1);
            }
            event->accept();
            return;
        }

        if (event->key() == Qt::Key_Backtab) {
            if (totalRows > 0) {
                const int prevRow = qMax(row - 1, 0);
                setCurrentCell(prevRow, 1);
            }
            event->accept();
            return;
        }

        if (current.isValid() && current.column() != 1) {
            setCurrentCell(current.row(), 1);
        }

        QTableWidget::keyPressEvent(event);
    }
};
}

MiscRenameHeadersDialog::MiscRenameHeadersDialog(QWidget* parent)
    : QDialog(parent)
    , m_openButton(nullptr)
    , m_filePathLabel(nullptr)
    , m_headersTable(nullptr)
    , m_statusLabel(nullptr)
    , m_primaryButton(nullptr)
    , m_running(false)
{
    setWindowTitle("Rename Headers");
    setModal(true);
    setFixedSize(760, 560);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);

    setupUi();
    resetToCloseState();
    setStatusMessage("Click OPEN to load a file.", TerminalSeverity::Info);
    updateControlStates();
}

void MiscRenameHeadersDialog::setStatusMessage(const QString& message, TerminalSeverity severity)
{
    if (!m_statusLabel) {
        return;
    }

    const QString color = statusColorForSeverity(severity);
    m_statusLabel->setStyleSheet(
        QString("QLabel { border: 1px solid #bdc3c7; border-radius: 6px; background-color: #f8f9fa; "
                "padding: 8px; color: %1; }")
            .arg(color));
    m_statusLabel->setText(message);
}

void MiscRenameHeadersDialog::setRunning(bool running)
{
    m_running = running;
    updateControlStates();
}

void MiscRenameHeadersDialog::setLoadedFileHeaders(const QString& filePath, const QStringList& headers)
{
    m_loadedFilePath = filePath;
    if (m_filePathLabel) {
        m_filePathLabel->setText(filePath);
    }

    if (m_headersTable) {
        m_headersTable->setRowCount(0);
        m_headersTable->setRowCount(headers.size());

        for (int i = 0; i < headers.size(); ++i) {
            QTableWidgetItem* oldHeaderItem = new QTableWidgetItem(headers.at(i));
            oldHeaderItem->setFlags(Qt::ItemIsEnabled);
            m_headersTable->setItem(i, 0, oldHeaderItem);

            QTableWidgetItem* newHeaderItem = new QTableWidgetItem(QString());
            m_headersTable->setItem(i, 1, newHeaderItem);
        }

        if (headers.size() > 0) {
            m_headersTable->setCurrentCell(0, 1);
        }

        m_headersTable->resizeRowsToContents();
    }

    updatePrimaryButtonText();
    updateControlStates();
}

bool MiscRenameHeadersDialog::hasLoadedFile() const
{
    return !m_loadedFilePath.trimmed().isEmpty();
}

QString MiscRenameHeadersDialog::loadedFilePath() const
{
    return m_loadedFilePath;
}

QMap<int, QString> MiscRenameHeadersDialog::enteredHeaderChanges() const
{
    QMap<int, QString> changes;
    if (!m_headersTable || !hasLoadedFile()) {
        return changes;
    }

    for (int row = 0; row < m_headersTable->rowCount(); ++row) {
        QTableWidgetItem* item = m_headersTable->item(row, 1);
        if (!item) {
            continue;
        }

        const QString replacement = item->text().trimmed();
        if (replacement.isEmpty()) {
            continue;
        }

        changes.insert(row, replacement);
    }

    return changes;
}

void MiscRenameHeadersDialog::closeEvent(QCloseEvent* event)
{
    if (m_running) {
        event->ignore();
        return;
    }

    QDialog::closeEvent(event);
}

void MiscRenameHeadersDialog::onOpenClicked()
{
    if (m_running) {
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "Select File",
        "C:/Users/JCox/Downloads",
        "Data Files (*.csv *.txt *.xls *.xlsx)");

    if (filePath.isEmpty()) {
        setStatusMessage("No file selected.", TerminalSeverity::Warning);
        emit terminalMessageRequested("Rename Headers: no file selected.", TerminalSeverity::Warning);
        return;
    }

    setStatusMessage("Loading headers...", TerminalSeverity::Info);
    emit terminalMessageRequested(
        QString("Rename Headers: selected file %1").arg(QFileInfo(filePath).fileName()),
        TerminalSeverity::Info);
    emit loadHeadersRequested(filePath);
}

void MiscRenameHeadersDialog::onPrimaryClicked()
{
    if (m_running) {
        return;
    }

    if (!hasLoadedFile()) {
        accept();
        return;
    }

    emit saveRequested();
}

void MiscRenameHeadersDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QLabel* headerLabel = new QLabel("LOAD A FILE, EDIT REPLACEMENT HEADERS, THEN SAVE", this);
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
    connect(m_openButton, &QPushButton::clicked,
            this, &MiscRenameHeadersDialog::onOpenClicked);

    openLayout->addWidget(m_openButton);
    openLayout->addStretch();
    mainLayout->addLayout(openLayout);

    QLabel* pathTitleLabel = new QLabel("Selected File:", this);
    pathTitleLabel->setFont(QFont("Blender Pro Bold", 11, QFont::Bold));
    pathTitleLabel->setStyleSheet("color: #34495e;");
    mainLayout->addWidget(pathTitleLabel);

    m_filePathLabel = new QLabel("No file loaded.", this);
    m_filePathLabel->setWordWrap(true);
    m_filePathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_filePathLabel->setFont(QFont("Consolas", 10));
    m_filePathLabel->setStyleSheet(
        "QLabel { border: 1px solid #bdc3c7; border-radius: 6px; background-color: #ffffff; "
        "padding: 8px; color: #2c3e50; }");
    mainLayout->addWidget(m_filePathLabel);

    m_headersTable = new RenameHeadersTableWidget(this);
    m_headersTable->setColumnCount(2);
    m_headersTable->setHorizontalHeaderLabels(QStringList() << "Current Header" << "Replacement Header");
    m_headersTable->horizontalHeader()->setStretchLastSection(true);
    m_headersTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_headersTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_headersTable->verticalHeader()->setVisible(false);
    m_headersTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_headersTable->setEditTriggers(QAbstractItemView::AllEditTriggers);
    m_headersTable->setFont(QFont("Blender Pro", 10));
    m_headersTable->setStyleSheet(
        "QTableWidget { border: 2px solid #bdc3c7; border-radius: 8px; background-color: white; }"
        "QHeaderView::section { background-color: #ecf0f1; color: #2c3e50; font-weight: bold; padding: 4px; "
        "border: 1px solid #d0d7de; }");
    mainLayout->addWidget(m_headersTable);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_statusLabel->setFont(QFont("Blender Pro", 10));
    mainLayout->addWidget(m_statusLabel);

    QHBoxLayout* primaryLayout = new QHBoxLayout();
    primaryLayout->addStretch();

    m_primaryButton = new QPushButton(this);
    m_primaryButton->setFont(QFont("Blender Pro Bold", 12, QFont::Bold));
    m_primaryButton->setFixedSize(160, 38);
    m_primaryButton->setStyleSheet(
        "QPushButton { background-color: #6c757d; color: white; border: none; border-radius: 4px; }"
        "QPushButton:hover { background-color: #5a6268; }"
        "QPushButton:pressed { background-color: #4e555b; }"
        "QPushButton:disabled { background-color: #b2b6ba; color: #eeeeee; }");
    connect(m_primaryButton, &QPushButton::clicked,
            this, &MiscRenameHeadersDialog::onPrimaryClicked);

    primaryLayout->addWidget(m_primaryButton);
    primaryLayout->addStretch();
    mainLayout->addLayout(primaryLayout);
}

void MiscRenameHeadersDialog::resetToCloseState()
{
    m_loadedFilePath.clear();

    if (m_filePathLabel) {
        m_filePathLabel->setText("No file loaded.");
    }

    if (m_headersTable) {
        m_headersTable->setRowCount(0);
    }

    updatePrimaryButtonText();
}

void MiscRenameHeadersDialog::updateControlStates()
{
    if (m_openButton) {
        m_openButton->setEnabled(!m_running);
    }
    if (m_primaryButton) {
        m_primaryButton->setEnabled(!m_running);
    }
    if (m_headersTable) {
        m_headersTable->setEnabled(!m_running);
    }
}

void MiscRenameHeadersDialog::updatePrimaryButtonText()
{
    if (!m_primaryButton) {
        return;
    }

    if (hasLoadedFile()) {
        m_primaryButton->setText("SAVE && CLOSE");
    } else {
        m_primaryButton->setText("CLOSE");
    }
}
