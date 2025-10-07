#include "tmfleremaildialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMimeData>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QDebug>  // for qWarning()

// ---------- DragListWidget: enables dragging file URLs out ----------
class DragListWidget : public QListWidget
{
public:
    explicit DragListWidget(QWidget *parent = nullptr) : QListWidget(parent)
    {
        setSelectionMode(QAbstractItemView::ExtendedSelection);
        setDragEnabled(true);
        setDefaultDropAction(Qt::IgnoreAction);
        setDragDropMode(QAbstractItemView::DragOnly);
    }

protected:
    QMimeData* mimeData(const QList<QListWidgetItem *> &items) const override
    {
        if (items.isEmpty()) return nullptr;

        auto *mime = new QMimeData();
        QList<QUrl> urls;
        QStringList textPaths;

        for (auto *it : items) {
            const QString path = it->data(Qt::UserRole).toString();
            if (!path.isEmpty()) {
                urls << QUrl::fromLocalFile(path);
                textPaths << path;
            }
        }

        if (!urls.isEmpty()) {
            mime->setUrls(urls);
            mime->setText(textPaths.join('\n'));
        }
        return mime;
    }
};

// ---------- TMFLEREmailDialog ----------

TMFLEREmailDialog::TMFLEREmailDialog(const QString &nasPath,
                                     const QString &jobNumber,
                                     QWidget *parent)
    : QDialog(parent)
    , m_nasPath(nasPath)
    , m_jobNumber(jobNumber)
{
    setWindowTitle(QStringLiteral("Attach Merged CSVs — FL ER (%1)").arg(m_jobNumber));
    setModal(true);
    resize(720, 420);

    // Close button (acts like Cancel / reject())
    setWindowFlags(windowFlags() | Qt::WindowCloseButtonHint);

    auto *root = new QVBoxLayout(this);

    // Layout padding
    root->setContentsMargins(12, 12, 12, 12);

    // Path display
    auto *pathRow = new QHBoxLayout();
    auto *pathLbl = new QLabel(QStringLiteral("<b>Folder:</b> %1").arg(m_nasPath), this);
    pathLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathRow->addWidget(pathLbl);
    pathRow->addStretch(1);
    root->addLayout(pathRow);

    // Instruction
    auto *hint = new QLabel(
        "Select one or more <b>_MERGED</b> CSV files below and <b>drag them into your email</b> (e.g., Outlook).\n"
        "When finished, click <b>Continue</b> to resume final processing.",
        this);
    hint->setWordWrap(true);
    root->addWidget(hint);

    // File list (drag-out)
    m_list = new DragListWidget(this);
    m_list->setAlternatingRowColors(true);
    m_list->setMinimumHeight(260);
    root->addWidget(m_list, 1);

    // Buttons
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto *okBtn = buttons->button(QDialogButtonBox::Ok);
    okBtn->setText("Continue");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    // Populate list
    refreshFileList();
}

void TMFLEREmailDialog::refreshFileList()
{
    m_list->clear();

    QDir dir(m_nasPath);
    if (!dir.exists())
        return;

    // Show *_MERGED*.csv
    const QStringList filters{ "*_MERGED*.csv" };
    const QFileInfoList entries = dir.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Name);

    for (const QFileInfo &fi : entries) {
        auto *item = new QListWidgetItem(fi.fileName());
        item->setToolTip(fi.absoluteFilePath());
        item->setData(Qt::UserRole, fi.absoluteFilePath());
        m_list->addItem(item);
    }

    if (m_list->count() == 0) {
        // Helpful for debugging: log when expected MERGED CSVs are missing
        qWarning() << "No _MERGED CSV files found in" << m_nasPath;
    }
}
