#include "ailiemaildialog.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTemporaryFile>
#include <QVBoxLayout>
#include <QFont>
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
        if (items.isEmpty())
            return nullptr;

        QListWidgetItem *item = items.first();
        const QString filePath = item->data(Qt::UserRole).toString();

        if (filePath.isEmpty())
            return nullptr;

        QMimeData *mimeData = new QMimeData();
        mimeData->setUrls({ QUrl::fromLocalFile(filePath) });
        return mimeData;
    }
};

static bool copyTableToClipboardUsingWord(const QVector<QStringList> &tableData, QWidget *parent)
{
#ifndef Q_OS_WIN
    Q_UNUSED(tableData)
    QMessageBox::warning(parent,
                         "AILI",
                         "Word table copy is only supported on Windows.");
    return false;
#else
    if (tableData.isEmpty() || tableData.first().isEmpty())
    {
        QMessageBox::warning(parent,
                             "AILI",
                             "No table data is available to copy.");
        return false;
    }

    const int rowCount = tableData.size();
    const int columnCount = tableData.first().size();

    if (rowCount <= 0 || columnCount <= 0)
    {
        QMessageBox::warning(parent,
                             "AILI",
                             "The table data is invalid.");
        return false;
    }

    QTemporaryFile tempFile(QDir::tempPath() + "/AILI_EmailTable_XXXXXX.docx");
    tempFile.setAutoRemove(false);

    if (!tempFile.open())
    {
        QMessageBox::warning(parent,
                             "AILI",
                             "Unable to create a temporary Word document.");
        return false;
    }

    const QString tempPath = QDir::toNativeSeparators(tempFile.fileName());
    tempFile.close();

    QAxObject wordApp("Word.Application");
    if (wordApp.isNull())
    {
        QFile::remove(tempPath);
        QMessageBox::warning(parent,
                             "AILI",
                             "Microsoft Word could not be started.");
        return false;
    }

    wordApp.setProperty("Visible", false);
    wordApp.setProperty("DisplayAlerts", 0);

    QAxObject *documents = wordApp.querySubObject("Documents");
    if (!documents)
    {
        QFile::remove(tempPath);
        QMessageBox::warning(parent,
                             "AILI",
                             "Unable to access Word documents.");
        return false;
    }

    QAxObject *document = documents->querySubObject("Add()");
    if (!document)
    {
        QFile::remove(tempPath);
        QMessageBox::warning(parent,
                             "AILI",
                             "Unable to create a Word document.");
        wordApp.dynamicCall("Quit()");
        return false;
    }

    bool success = true;

    do
    {
        QAxObject *range = document->querySubObject("Range()");
        if (!range)
        {
            success = false;
            break;
        }

        QVariantList addTableArgs;
        addTableArgs << range->asVariant() << rowCount << columnCount;

        QAxObject *tables = document->querySubObject("Tables");
        if (!tables)
        {
            success = false;
            delete range;
            break;
        }

        QAxObject *table = tables->querySubObject("Add(QVariant,int,int)",
                                                  addTableArgs.at(0),
                                                  addTableArgs.at(1).toInt(),
                                                  addTableArgs.at(2).toInt());
        if (!table)
        {
            success = false;
            delete tables;
            delete range;
            break;
        }

        QAxObject *rows = table->querySubObject("Rows");
        if (rows)
        {
            rows->setProperty("AllowBreakAcrossPages", false);
            delete rows;
        }

        for (int row = 0; row < rowCount; ++row)
        {
            const QStringList rowData = tableData.at(row);

            for (int col = 0; col < columnCount; ++col)
            {
                const QString value = (col < rowData.size()) ? rowData.at(col) : QString();

                QAxObject *cell = table->querySubObject("Cell(int,int)", row + 1, col + 1);
                if (!cell)
                    continue;

                QAxObject *cellRange = cell->querySubObject("Range");
                if (cellRange)
                {
                    cellRange->setProperty("Text", value);

                    if (row == 0)
                    {
                        QAxObject *font = cellRange->querySubObject("Font");
                        if (font)
                        {
                            font->setProperty("Bold", true);
                            delete font;
                        }
                    }

                    delete cellRange;
                }

                delete cell;
            }
        }

        QAxObject *borders = table->querySubObject("Borders");
        if (borders)
        {
            borders->setProperty("Enable", true);
            delete borders;
        }

        QAxObject *tableRange = table->querySubObject("Range");
        if (!tableRange)
        {
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
    }
    while (false);

    document->dynamicCall("Close(bool)", false);
    wordApp.dynamicCall("Quit()");

    delete document;
    delete documents;

    QFile::remove(tempPath);

    if (!success)
    {
        QMessageBox::warning(parent,
                             "AILI",
                             "Failed to create the Word table for clipboard copy.");
        return false;
    }

    return true;
#endif
}

} // namespace

AILIEmailDialog::AILIEmailDialog(QWidget *parent)
    : QDialog(parent),
    m_tableHeaderLabel(nullptr),
    m_fileHeaderLabel(nullptr),
    m_tableWidget(nullptr),
    m_copyButton(nullptr),
    m_closeButton(nullptr),
    m_fileList(nullptr),
    m_copyClicked(false),
    m_fileClicked(false)
{
    buildUI();
    updateCloseButtonState();
}

void AILIEmailDialog::setPostageTable(const QVector<QStringList> &tableData)
{
    m_tableData = tableData;
    populateTable();
}

void AILIEmailDialog::setInvalidAddressFile(const QString &filePath)
{
    m_invalidFilePath = filePath;

    m_fileList->clear();

    if (m_invalidFilePath.isEmpty())
        return;

    QFileInfo info(m_invalidFilePath);
    QListWidgetItem *item = new QListWidgetItem(info.fileName(), m_fileList);
    item->setData(Qt::UserRole, m_invalidFilePath);
    item->setToolTip(m_invalidFilePath);
    m_fileList->addItem(item);
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
    if (!copyTableToClipboardUsingWord(m_tableData, this))
        return;

    m_copyClicked = true;
    updateCloseButtonState();
}

void AILIEmailDialog::handleFileClicked()
{
    if (m_fileList->currentItem() == nullptr)
        return;

    m_fileClicked = true;
    updateCloseButtonState();
}

void AILIEmailDialog::handleCloseClicked()
{
    if (!m_copyClicked || !m_fileClicked)
        return;

    emit dialogCompleted();
    accept();
}

void AILIEmailDialog::updateCloseButtonState()
{
    m_closeButton->setEnabled(m_copyClicked && m_fileClicked);
}

void AILIEmailDialog::buildUI()
{
    setWindowTitle("AILI Email Preparation");
    setModal(true);
    resize(950, 820);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    QFont headerFont;
    headerFont.setPointSize(11);
    headerFont.setBold(true);

    m_tableHeaderLabel = new QLabel("COPY THE TABLE BELOW AND PASTE IT INTO THE E-MAIL", this);
    m_tableHeaderLabel->setAlignment(Qt::AlignCenter);
    m_tableHeaderLabel->setFont(headerFont);

    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setSelectionMode(QAbstractItemView::NoSelection);
    m_tableWidget->setFocusPolicy(Qt::NoFocus);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableWidget->verticalHeader()->setVisible(false);

    m_copyButton = new QPushButton("COPY", this);

    m_fileHeaderLabel = new QLabel("DRAG THE FILE BELOW INTO THE E-MAIL", this);
    m_fileHeaderLabel->setAlignment(Qt::AlignCenter);
    m_fileHeaderLabel->setFont(headerFont);

    m_fileList = new FileDragListWidget(this);
    m_fileList->setViewMode(QListView::ListMode);
    m_fileList->setDragDropMode(QAbstractItemView::DragOnly);
    m_fileList->setAlternatingRowColors(false);
    m_fileList->setMinimumHeight(120);

    m_closeButton = new QPushButton("CLOSE", this);
    m_closeButton->setEnabled(false);
    m_closeButton->setFixedWidth(120);

    QHBoxLayout *copyLayout = new QHBoxLayout();
    copyLayout->addStretch();
    copyLayout->addWidget(m_copyButton);
    copyLayout->addStretch();

    QHBoxLayout *closeLayout = new QHBoxLayout();
    closeLayout->addStretch();
    closeLayout->addWidget(m_closeButton);
    closeLayout->addStretch();

    mainLayout->addWidget(m_tableHeaderLabel);
    mainLayout->addWidget(m_tableWidget, 1);
    mainLayout->addLayout(copyLayout);
    mainLayout->addSpacing(8);
    mainLayout->addWidget(m_fileHeaderLabel);
    mainLayout->addWidget(m_fileList);
    mainLayout->addSpacing(8);
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

    if (m_tableData.isEmpty())
    {
        m_tableWidget->setRowCount(0);
        m_tableWidget->setColumnCount(0);
        return;
    }

    const QStringList headers = m_tableData.first();
    const int columnCount = headers.size();
    const int dataRowCount = qMax(0, m_tableData.size() - 1);

    m_tableWidget->setColumnCount(columnCount);
    m_tableWidget->setHorizontalHeaderLabels(headers);
    m_tableWidget->setRowCount(dataRowCount);

    for (int row = 1; row < m_tableData.size(); ++row)
    {
        const QStringList rowData = m_tableData.at(row);

        for (int col = 0; col < columnCount; ++col)
        {
            const QString value = (col < rowData.size()) ? rowData.at(col) : QString();

            QTableWidgetItem *item = new QTableWidgetItem(value);
            item->setTextAlignment(Qt::AlignCenter);
            m_tableWidget->setItem(row - 1, col, item);
        }
    }

    m_tableWidget->resizeRowsToContents();
}
