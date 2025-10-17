#include "tmfleremaildialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDir>
#include <QFileInfoList>
#include <QListWidget>
#include <QMimeData>
#include <QUrl>
#include <QDrag>

TMFLEREmailDialog::TMFLEREmailDialog(const QString &nasPath,
                                     const QString &jobNumber,
                                     QWidget *parent)
    : QDialog(parent),
      m_nasPath(nasPath),
      m_jobNumber(jobNumber)
{
    setWindowTitle(QStringLiteral("Attach Merged CSV — FL ER (%1)").arg(m_jobNumber));
    setModal(true);
    resize(700, 420);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    QLabel *title = new QLabel(
        "<h3 style='margin:0;'>Drag & drop the merged CSV into your email</h3>"
        "<p style='color:#555;'>Only the <b>_MERGED.csv</b> file(s) in the folder below are shown.</p>",
        this);
    title->setWordWrap(true);
    root->addWidget(title);

    QLabel *folderLbl = new QLabel(QString("<b>Folder:</b> %1").arg(m_nasPath), this);
    folderLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(folderLbl);

    m_listWidget = new QListWidget(this);
    m_listWidget->setAlternatingRowColors(true);
    m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listWidget->setDragEnabled(true);
    m_listWidget->setDragDropMode(QAbstractItemView::DragOnly);
    root->addWidget(m_listWidget, 1);

    QPushButton *closeBtn = new QPushButton("CLOSE", this);
    closeBtn->setFixedWidth(120);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);

    populateMergedFiles();
}

void TMFLEREmailDialog::populateMergedFiles()
{
    m_listWidget->clear();
    QDir dir(m_nasPath);
    if (!dir.exists())
        return;

    const QFileInfoList files = dir.entryInfoList(QStringList() << "*_MERGED*.csv",
                                                  QDir::Files | QDir::Readable, QDir::Name);

    for (const QFileInfo &fi : files) {
        QListWidgetItem *item = new QListWidgetItem(fi.fileName());
        item->setToolTip(fi.absoluteFilePath());
        item->setData(Qt::UserRole, fi.absoluteFilePath());
        m_listWidget->addItem(item);
    }

    m_listWidget->setDragEnabled(true);
}
