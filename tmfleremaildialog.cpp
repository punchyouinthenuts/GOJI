#include "tmfleremaildialog.h"
#include "logger.h"
#include <QDir>
#include <QFileInfo>
#include <QListWidgetItem>
#include <QClipboard>
#include <QApplication>

TMFLEREmailDialog::TMFLEREmailDialog(const QString& jobNumber, QWidget* parent)
    : QDialog(parent),
      m_mainLayout(nullptr),
      m_headerLabel1(nullptr),
      m_headerLabel2(nullptr),
      m_filesLabel(nullptr),
      m_fileList(nullptr),
      m_helpLabel(nullptr),
      m_closeButton(nullptr),
      m_jobNumber(jobNumber)
{
    setWindowTitle("Email Integration - TM FL ER");
    setFixedSize(678, 565);
    setModal(true);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);

    setupUI();
    populateFileList();
    updateCloseButtonState();

    Logger::instance().info("TMFLEREmailDialog created");
}

TMFLEREmailDialog::~TMFLEREmailDialog()
{
    Logger::instance().info("TMFLEREmailDialog destroyed");
}

void TMFLEREmailDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(17);
    m_mainLayout->setContentsMargins(23, 23, 23, 23);

    m_headerLabel1 = new QLabel("DRAG & DROP THE MERGED CSV FILES INTO YOUR E-MAIL", this);
    m_headerLabel1->setFont(QFont("Blender Pro Bold", 16, QFont::Bold));
    m_headerLabel1->setAlignment(Qt::AlignCenter);
    m_headerLabel1->setStyleSheet("color: #2c3e50; margin-bottom: 6px;");
    m_mainLayout->addWidget(m_headerLabel1);

    m_headerLabel2 = new QLabel("FL ER FILES READY FOR ATTACHMENT", this);
    m_headerLabel2->setFont(QFont("Blender Pro Bold", 16, QFont::Bold));
    m_headerLabel2->setAlignment(Qt::AlignCenter);
    m_headerLabel2->setStyleSheet("color: #2c3e50; margin-bottom: 17px;");
    m_mainLayout->addWidget(m_headerLabel2);

    m_filesLabel = new QLabel("FL ER Files:", this);
    m_filesLabel->setFont(QFont("Blender Pro Bold", 14, QFont::Bold));
    m_filesLabel->setStyleSheet("color: #34495e; margin-top: 17px;");
    m_mainLayout->addWidget(m_filesLabel);

    m_fileList = new TMFLEREmailFileListWidget(this);
    m_fileList->setFont(QFont("Blender Pro", 11));
    m_fileList->setStyleSheet(
        "QListWidget {"
        "   border: 2px solid #bdc3c7;"
        "   border-radius: 9px;"
        "   background-color: white;"
        "   selection-background-color: #e3f2fd;"
        "}"
    );
    m_mainLayout->addWidget(m_fileList);

    m_helpLabel = new QLabel("💡 Drag files from the list above directly into your Outlook email", this);
    m_helpLabel->setFont(QFont("Blender Pro", 11, QFont::StyleItalic));
    m_helpLabel->setStyleSheet("color: #666666;");
    m_helpLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(m_helpLabel);

    QHBoxLayout* closeButtonLayout = new QHBoxLayout();
    closeButtonLayout->addStretch();

    m_closeButton = new QPushButton("CLOSE", this);
    m_closeButton->setFont(QFont("Blender Pro Bold", 14, QFont::Bold));
    m_closeButton->setFixedSize(113, 40);
    m_closeButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #6c757d;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 5px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #5a6268; }"
        "QPushButton:pressed { background-color: #4e555b; }"
        "QPushButton:disabled { background-color: #cccccc; color: #666666; }"
    );

    closeButtonLayout->addWidget(m_closeButton);
    closeButtonLayout->addStretch();
    m_mainLayout->addLayout(closeButtonLayout);

    connect(m_fileList, &QListWidget::itemClicked, this, &TMFLEREmailDialog::onFileClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &TMFLEREmailDialog::onCloseClicked);
}

QString TMFLEREmailDialog::getFileDirectory() const
{
    return "C:/Goji/TRACHMAR/FL ER/DATA";
}

void TMFLEREmailDialog::populateFileList()
{
    QDir dir(getFileDirectory());

    if (!dir.exists()) {
        QListWidgetItem* noDirItem = new QListWidgetItem("No DATA directory found");
        noDirItem->setFlags(Qt::NoItemFlags);
        noDirItem->setForeground(QBrush(Qt::gray));
        m_fileList->addItem(noDirItem);
        return;
    }

    QStringList filters;
    if (!m_jobNumber.isEmpty())
        filters << QString("*%1*_MERGED*.csv").arg(m_jobNumber);
    filters << "*_MERGED*.csv";

    dir.setNameFilters(filters);
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

    if (files.isEmpty()) {
        QListWidgetItem* emptyItem = new QListWidgetItem("No FL ER merged files found");
        emptyItem->setFlags(Qt::NoItemFlags);
        emptyItem->setForeground(QBrush(Qt::gray));
        m_fileList->addItem(emptyItem);
        return;
    }

    for (const QFileInfo& fi : files) {
        QListWidgetItem* item = new QListWidgetItem(fi.fileName());
        item->setData(Qt::UserRole, fi.absoluteFilePath());
        item->setToolTip(fi.absoluteFilePath());
        item->setIcon(m_iconProvider.icon(fi));
        m_fileList->addItem(item);
    }
}

void TMFLEREmailDialog::updateCloseButtonState()
{
    if (m_closeButton) {
        m_closeButton->setEnabled(true);
        m_closeButton->setToolTip("Click to close");
    }
}

void TMFLEREmailDialog::onFileClicked()
{
    updateCloseButtonState();
    Logger::instance().info("File clicked in TMFLEREmailDialog");
}

void TMFLEREmailDialog::onCloseClicked()
{
    emit dialogClosed();
    accept();
}

void TMFLEREmailDialog::closeEvent(QCloseEvent* event)
{
    emit dialogClosed();
    event->accept();
}
