#include "ailiemaildialog.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLocale>
#include <QMimeData>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTemporaryFile>
#include <QVBoxLayout>
#include <QDrag>
#include <QUrl>

#ifdef Q_OS_WIN
#include <QAxObject>
#endif

namespace
{

class FileDragListWidget : public QListWidget
{
public:
    explicit FileDragListWidget(QWidget *parent = nullptr)
        : QListWidget(parent)
    {
        setDragEnabled(true);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setDefaultDropAction(Qt::CopyAction);
    }

protected:
    QMimeData *mimeData(const QList<QListWidgetItem *> &items) const override
    {
        if (items.isEmpty()) {
            return nullptr;
        }

        QListWidgetItem *item = items.first();
        const QString filePath = item->data(Qt::UserRole).toString();

        if (filePath.isEmpty()) {
            return nullptr;
        }

        QMimeData *mimeData = new QMimeData();
        mimeData->setUrls({ QUrl::fromLocalFile(filePath) });
        return mimeData;
    }
};

static QString normalizeCellText(const QString &value)
{
    QString cleaned = value;
    cleaned.replace(QChar(0x00A0), ' ');
    cleaned.replace('\r', ' ');
    cleaned.replace('\n', ' ');
    return cleaned.simplified();
}

static QString formatCurrencyCell(const QString &value)
{
    QString normalized = normalizeCellText(value);
    normalized.remove('$');
    normalized.remove(',');
    normalized = normalized.trimmed();

    if (normalized.isEmpty()) {
        return QString();
    }

    bool ok = false;
    const double amount = normalized.toDouble(&ok);
    if (!ok) {
        return normalizeCellText(value);
    }

    const QLocale us(QLocale::English, QLocale::UnitedStates);
    return us.toCurrencyString(amount, "$", 2);
}

static QVector<QStringList> normalizeTableData(const QVector<QStringList> &tableData)
{
    QVector<QStringList> normalizedRows;
    normalizedRows.reserve(tableData.size());

    for (const QStringList &row : tableData) {
        QStringList normalizedRow;
        normalizedRow.reserve(row.size());

        for (int col = 0; col < row.size(); ++col) {
            const QString rawValue = row.at(col);
            if (col == 2 || col == 4) {
                normalizedRow.append(formatCurrencyCell(rawValue));
            } else {
                normalizedRow.append(normalizeCellText(rawValue));
            }
        }

        normalizedRows.append(normalizedRow);
    }

    return normalizedRows;
}

static bool copyTableToClipboardUsingWord(const QVector<QStringList> &tableData, QWidget *parent)
{
    Q_UNUSED(parent)
#ifndef Q_OS_WIN
    Q_UNUSED(tableData)
    return false;
#else
    if (tableData.isEmpty() || tableData.first().isEmpty()) {
        return false;
    }

    const int rowCount = tableData.size();
    int columnCount = 0;
    for (const QStringList &row : tableData) {
        if (row.size() > columnCount) {
            columnCount = row.size();
        }
    }

    if (rowCount <= 0 || columnCount <= 0) {
        return false;
    }

    QTemporaryFile tempFile(QDir::tempPath() + "/AILI_EmailTable_XXXXXX.docx");
    tempFile.setAutoRemove(false);

    if (!tempFile.open()) {
        return false;
    }

    const QString tempPath = QDir::toNativeSeparators(tempFile.fileName());
    tempFile.close();

    QAxObject wordApp("Word.Application");
    if (wordApp.isNull()) {
        QFile::remove(tempPath);
        return false;
    }

    wordApp.setProperty("Visible", false);
    wordApp.setProperty("DisplayAlerts", 0);

    QAxObject *documents = wordApp.querySubObject("Documents");
    if (!documents) {
        QFile::remove(tempPath);
        return false;
    }

    QAxObject *document = documents->querySubObject("Add()");
    if (!document) {
        QFile::remove(tempPath);
        return false;
    }

    bool success = true;

    do {
        QAxObject *range = document->querySubObject("Range()");
        if (!range) {
            success = false;
            break;
        }

        QVariantList addTableArgs;
        addTableArgs << range->asVariant() << rowCount << columnCount;

        QAxObject *tables = document->querySubObject("Tables");
        if (!tables) {
            success = false;
            delete range;
            break;
        }

        QAxObject *table = tables->querySubObject("Add(QVariant,int,int)",
                                                  addTableArgs.at(0),
                                                  addTableArgs.at(1).toInt(),
                                                  addTableArgs.at(2).toInt());
        if (!table) {
            success = false;
            delete tables;
            delete range;
            break;
        }

        QAxObject *rows = table->querySubObject("Rows");
        if (rows) {
            rows->setProperty("AllowBreakAcrossPages", false);
            delete rows;
        }

        for (int row = 0; row < rowCount; ++row) {
            const QStringList rowData = tableData.at(row);

            for (int col = 0; col < columnCount; ++col) {
                const QString value = (col < rowData.size()) ? rowData.at(col) : QString();

                QAxObject *cell = table->querySubObject("Cell(int,int)", row + 1, col + 1);
                if (!cell) {
                    continue;
                }

                QAxObject *cellRange = cell->querySubObject("Range");
                if (cellRange) {
                    cellRange->setProperty("Text", value);
                    delete cellRange;
                }

                delete cell;
            }
        }

        QAxObject *borders = table->querySubObject("Borders");
        if (borders) {
            borders->setProperty("Enable", true);
            delete borders;
        }

        QAxObject *tableRange = table->querySubObject("Range");
        if (!tableRange) {
            success = false;
            delete table;
            delete tables;
            delete range;
            break;
        }

        document->dynamicCall("SaveAs2(const QString&)", tempPath);
        tableRange->dynamicCall("Copy()");

        delete tableRange;
        delete table;
        delete tables;
        delete range;
    } while (false);

    document->dynamicCall("Close(bool)", false);
    wordApp.dynamicCall("Quit()");

    delete document;
    delete documents;

    QFile::remove(tempPath);

    if (!success) {
        return false;
    }

    return true;
#endif
}

} // namespace

AILIEmailDialog::AILIEmailDialog(QWidget *parent)
    : QDialog(parent)
    , m_tableHeaderLabel(nullptr)
    , m_fileHeaderLabel(nullptr)
    , m_tableWidget(nullptr)
    , m_copyButton(nullptr)
    , m_closeButton(nullptr)
    , m_fileList(nullptr)
    , m_copyClicked(false)
    , m_fileClicked(false)
{
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
    buildUI();
    updateCloseButtonState();
}

void AILIEmailDialog::setPostageTable(const QVector<QStringList> &tableData)
{
    m_tableData = normalizeTableData(tableData);
    populateTable();
}

void AILIEmailDialog::setInvalidAddressFile(const QString &filePath)
{
    m_invalidFilePath = filePath;

    m_fileList->clear();

    if (m_invalidFilePath.isEmpty()) {
        return;
    }

    QFileInfo info(m_invalidFilePath);
    QListWidgetItem *item = new QListWidgetItem(info.fileName(), m_fileList);
    item->setData(Qt::UserRole, m_invalidFilePath);
    item->setToolTip(m_invalidFilePath);

    const QIcon fileIcon = m_iconProvider.icon(info);
    if (!fileIcon.isNull()) {
        item->setIcon(fileIcon);
    }
}

bool AILIEmailDialog::copyWasClicked() const
{
    return m_copyClicked;
}

bool AILIEmailDialog::fileWasClicked() const
{
    return m_fileClicked;
}

void AILIEmailDialog::handleCopyClicked()
{
    if (!copyTableToClipboardUsingWord(m_tableData, this)) {
        return;
    }

    m_copyClicked = true;
    updateCloseButtonState();
}

void AILIEmailDialog::handleFileClicked()
{
    if (m_fileList->currentItem() == nullptr) {
        return;
    }

    m_fileClicked = true;
    updateCloseButtonState();
}

void AILIEmailDialog::handleCloseClicked()
{
    if (!m_copyClicked || !m_fileClicked) {
        return;
    }

    emit dialogCompleted();
    accept();
}

void AILIEmailDialog::updateCloseButtonState()
{
    m_closeButton->setEnabled(m_copyClicked && m_fileClicked);
}

void AILIEmailDialog::buildUI()
{
    setWindowTitle("Email Integration - AILI");
    setModal(true);
    setFixedSize(760, 520);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(10);

    m_tableHeaderLabel = new QLabel("COPY THE TABLE BELOW AND PASTE INTO E-MAIL", this);
    m_tableHeaderLabel->setAlignment(Qt::AlignCenter);
    m_tableHeaderLabel->setFont(QFont("Blender Pro Bold", 14, QFont::Bold));
    m_tableHeaderLabel->setStyleSheet("color: #2c3e50;");

    QFrame *tableFrame = new QFrame(this);
    tableFrame->setFrameStyle(QFrame::Box);
    tableFrame->setStyleSheet("QFrame { border: 2px solid #bdc3c7; border-radius: 6px; background-color: white; padding: 4px; }");
    QVBoxLayout *tableLayout = new QVBoxLayout(tableFrame);
    tableLayout->setContentsMargins(6, 6, 6, 6);

    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setSelectionMode(QAbstractItemView::NoSelection);
    m_tableWidget->setFocusPolicy(Qt::NoFocus);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->setShowGrid(true);
    m_tableWidget->setWordWrap(false);
    m_tableWidget->horizontalHeader()->setVisible(false);
    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->verticalHeader()->setDefaultSectionSize(24);
    m_tableWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tableWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    tableLayout->addWidget(m_tableWidget);

    m_copyButton = new QPushButton("COPY", this);
    m_copyButton->setFixedSize(84, 30);
    m_copyButton->setStyleSheet(
        "QPushButton { background-color: #3498db; color: white; border: none; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #2980b9; }"
        "QPushButton:pressed { background-color: #21618c; }");

    m_fileHeaderLabel = new QLabel("DRAG THE FILE BELOW INTO THE E-MAIL", this);
    m_fileHeaderLabel->setAlignment(Qt::AlignCenter);
    m_fileHeaderLabel->setFont(QFont("Blender Pro Bold", 14, QFont::Bold));
    m_fileHeaderLabel->setStyleSheet("color: #2c3e50;");

    m_fileList = new FileDragListWidget(this);
    m_fileList->setViewMode(QListView::ListMode);
    m_fileList->setDragDropMode(QAbstractItemView::DragOnly);
    m_fileList->setAlternatingRowColors(false);
    m_fileList->setMinimumHeight(60);
    m_fileList->setMaximumHeight(82);
    m_fileList->setStyleSheet(
        "QListWidget { border: 2px solid #bdc3c7; border-radius: 8px; background-color: white; selection-background-color: #e3f2fd; }");

    m_closeButton = new QPushButton("CLOSE", this);
    m_closeButton->setEnabled(false);
    m_closeButton->setFixedSize(100, 32);
    m_closeButton->setStyleSheet(
        "QPushButton { background-color: #6c757d; color: white; border: none; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #5a6268; }"
        "QPushButton:pressed { background-color: #4e555b; }"
        "QPushButton:disabled { background-color: #cccccc; color: #666666; }");

    QHBoxLayout *copyLayout = new QHBoxLayout();
    copyLayout->addStretch();
    copyLayout->addWidget(m_copyButton);
    copyLayout->addStretch();

    QHBoxLayout *closeLayout = new QHBoxLayout();
    closeLayout->addStretch();
    closeLayout->addWidget(m_closeButton);
    closeLayout->addStretch();

    mainLayout->addWidget(m_tableHeaderLabel);
    mainLayout->addWidget(tableFrame, 0);
    mainLayout->addLayout(copyLayout);
    mainLayout->addWidget(m_fileHeaderLabel);
    mainLayout->addWidget(m_fileList);
    mainLayout->addLayout(closeLayout);

    connect(m_copyButton, &QPushButton::clicked,
            this, &AILIEmailDialog::handleCopyClicked);

    connect(m_fileList, &QListWidget::itemClicked,
            this, [this](QListWidgetItem *) { handleFileClicked(); });

    connect(m_closeButton, &QPushButton::clicked,
            this, &AILIEmailDialog::handleCloseClicked);
}

void AILIEmailDialog::populateTable()
{
    m_tableWidget->clear();
    m_tableWidget->horizontalHeader()->setVisible(false);

    if (m_tableData.isEmpty()) {
        m_tableWidget->setRowCount(0);
        m_tableWidget->setColumnCount(0);
        return;
    }

    int columnCount = 0;
    for (const QStringList &rowData : m_tableData) {
        if (rowData.size() > columnCount) {
            columnCount = rowData.size();
        }
    }

    if (columnCount <= 0) {
        m_tableWidget->setRowCount(0);
        m_tableWidget->setColumnCount(0);
        return;
    }

    m_tableWidget->setColumnCount(columnCount);
    m_tableWidget->setRowCount(m_tableData.size());

    for (int row = 0; row < m_tableData.size(); ++row) {
        const QStringList rowData = m_tableData.at(row);

        for (int col = 0; col < columnCount; ++col) {
            const QString value = (col < rowData.size()) ? rowData.at(col) : QString();
            QTableWidgetItem *item = new QTableWidgetItem(value);

            if (col == 1) {
                item->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
            } else if (col >= 2 && col <= 4) {
                item->setTextAlignment(Qt::AlignVCenter | Qt::AlignRight);
            } else {
                item->setTextAlignment(Qt::AlignCenter);
            }

            m_tableWidget->setItem(row, col, item);
        }
    }

    for (int col = 0; col < columnCount; ++col) {
        m_tableWidget->horizontalHeader()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
    }
    m_tableWidget->horizontalHeader()->setStretchLastSection(false);

    if (columnCount > 1) {
        const int descWidth = qBound(140, m_tableWidget->columnWidth(1), 220);
        m_tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
        m_tableWidget->setColumnWidth(1, descWidth);
    }

    for (int row = 0; row < m_tableWidget->rowCount(); ++row) {
        m_tableWidget->setRowHeight(row, 24);
    }

    const int tableHeight = (m_tableWidget->rowCount() * 24) + 8;
    m_tableWidget->setFixedHeight(tableHeight);
}
