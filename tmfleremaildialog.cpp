#include "tmfleremaildialog.h"
#include <QFileInfo>
#include <QDesktopServices>
#include <QMimeData>
#include <QDrag>
#include <QApplication>

TMFLEREmailDialog::TMFLEREmailDialog(const QString &jobNumber, QWidget *parent)
    : QDialog(parent), m_jobNumber(jobNumber)
{
    setWindowTitle(QString("Attach Merged CSV — FL ER (%1)").arg(jobNumber));
    setAcceptDrops(true);
    setModal(true);
    resize(600, 400);

    m_mainLayout = new QVBoxLayout(this);

    // Instruction text
    m_instructionLabel = new QLabel("Drag & drop the merged CSV into your email.");
    m_instructionLabel->setWordWrap(true);
    m_instructionLabel->setAlignment(Qt::AlignCenter);
    m_instructionLabel->setStyleSheet("font-size: 14px; margin: 10px;");

    // File list (read-only list of _MERGED files)
    m_fileList = new QListWidget(this);
    m_fileList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fileList->setDragEnabled(true);
    m_fileList->setAcceptDrops(false);
    m_fileList->setDropIndicatorShown(false);
    m_fileList->setDefaultDropAction(Qt::IgnoreAction);
    m_fileList->setStyleSheet("font-family: Consolas; font-size: 12px;");

    populateFileList();

    // Close button
    m_closeButton = new QPushButton("CLOSE", this);
    m_closeButton->setFixedHeight(32);
    m_closeButton->setStyleSheet("font-weight: bold;");

    connect(m_closeButton, &QPushButton::clicked, this, &TMFLEREmailDialog::onCloseClicked);

    // Layout arrangement
    m_mainLayout->addWidget(m_instructionLabel);
    m_mainLayout->addWidget(m_fileList, 1);
    m_mainLayout->addWidget(m_closeButton, 0, Qt::AlignCenter);
}

void TMFLEREmailDialog::populateFileList()
{
    QString dataPath = "C:/Goji/TRACHMAR/FL ER/DATA";
    QDir dir(dataPath);
    QStringList filters;
    filters << "*_MERGED*.csv";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks);

    m_fileList->clear();
    for (const QFileInfo &file : files)
        m_fileList->addItem(file.fileName());
}

void TMFLEREmailDialog::onCloseClicked()
{
    emit dialogClosed();
    accept();
}

// ----- Drag & Drop Support -----
void TMFLEREmailDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void TMFLEREmailDialog::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void TMFLEREmailDialog::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls;
    for (QListWidgetItem *item : m_fileList->selectedItems()) {
        QString filePath = QString("C:/Goji/TRACHMAR/FL ER/DATA/%1").arg(item->text());
        urls.append(QUrl::fromLocalFile(filePath));
    }

    if (!urls.isEmpty()) {
        QMimeData *mimeData = new QMimeData;
        mimeData->setUrls(urls);

        QDrag *drag = new QDrag(this);
        drag->setMimeData(mimeData);
        drag->exec(Qt::CopyAction);
    }

    event->acceptProposedAction();
}
