#include "tmweeklypcpostprintdialog.h"

#include "tmhealthyemailfilelistwidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidgetItem>
#include <QFileInfo>
#include <QFont>
#include <QApplication>
#include <QClipboard>
#include <QTimer>

TMWeeklyPCPostPrintDialog::TMWeeklyPCPostPrintDialog(const QString& outputPath, const QStringList& filePaths, QWidget* parent)
    : QDialog(parent)
    , m_outputPath(outputPath.trimmed())
    , m_headerLabel(nullptr)
    , m_outputPathLabel(nullptr)
    , m_copyPathButton(nullptr)
    , m_fileList(nullptr)
    , m_closeButton(nullptr)
{
    setWindowTitle("TM WEEKLY PC Post Print Files");
    setFixedSize(680, 520);
    setModal(true);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);

    setupUI();
    populateFileList(filePaths);
}

void TMWeeklyPCPostPrintDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    m_headerLabel = new QLabel("POST-PRINT FILES ARE READY. DRAG FILE(S) INTO YOUR TARGET WINDOW.", this);
    m_headerLabel->setFont(QFont("Blender Pro Bold", 14, QFont::Bold));
    m_headerLabel->setAlignment(Qt::AlignCenter);
    m_headerLabel->setWordWrap(true);
    m_headerLabel->setStyleSheet("color: #2c3e50;");
    mainLayout->addWidget(m_headerLabel);

    QLabel* outputPathTitle = new QLabel("Output Path:", this);
    outputPathTitle->setFont(QFont("Blender Pro Bold", 12, QFont::Bold));
    outputPathTitle->setStyleSheet("color: #34495e;");
    mainLayout->addWidget(outputPathTitle);

    m_outputPathLabel = new QLabel(this);
    m_outputPathLabel->setFont(QFont("Blender Pro", 10));
    m_outputPathLabel->setText(m_outputPath.isEmpty() ? "Path unavailable" : m_outputPath);
    m_outputPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_outputPathLabel->setWordWrap(true);
    m_outputPathLabel->setStyleSheet(
        "QLabel {"
        "   border: 2px solid #bdc3c7;"
        "   border-radius: 8px;"
        "   background-color: white;"
        "   padding: 10px;"
        "   color: #2c3e50;"
        "}"
        );
    mainLayout->addWidget(m_outputPathLabel);

    QHBoxLayout* copyLayout = new QHBoxLayout();
    copyLayout->addStretch();
    if (!m_copyPathButton) {
        m_copyPathButton = new QPushButton("COPY", this);
        m_copyPathButton->setFont(QFont("Blender Pro Bold", 11, QFont::Bold));
        m_copyPathButton->setFixedSize(90, 34);
        m_copyPathButton->setStyleSheet(
            "QPushButton {"
            "   background-color: #198754;"
            "   color: white;"
            "   border: none;"
            "   border-radius: 4px;"
            "   font-weight: bold;"
            "}"
            "QPushButton:hover {"
            "   background-color: #157347;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #146c43;"
            "}"
            );
        connect(m_copyPathButton, &QPushButton::clicked, this, &TMWeeklyPCPostPrintDialog::onCopyPathClicked);
    }
    copyLayout->addWidget(m_copyPathButton);
    copyLayout->addStretch();
    mainLayout->addLayout(copyLayout);

    QLabel* filesTitle = new QLabel("Current Run Files (drag out):", this);
    filesTitle->setFont(QFont("Blender Pro Bold", 12, QFont::Bold));
    filesTitle->setStyleSheet("color: #34495e;");
    mainLayout->addWidget(filesTitle);

    m_fileList = new TMHealthyEmailFileListWidget(this);
    m_fileList->setFont(QFont("Blender Pro", 10));
    m_fileList->setStyleSheet(
        "QListWidget {"
        "   border: 2px solid #bdc3c7;"
        "   border-radius: 8px;"
        "   background-color: white;"
        "   selection-background-color: #e3f2fd;"
        "}"
        );
    mainLayout->addWidget(m_fileList);

    QHBoxLayout* closeLayout = new QHBoxLayout();
    closeLayout->addStretch();

    m_closeButton = new QPushButton("CLOSE", this);
    m_closeButton->setFont(QFont("Blender Pro Bold", 12, QFont::Bold));
    m_closeButton->setFixedSize(110, 36);
    m_closeButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #6c757d;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 4px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: #5a6268;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #4e555b;"
        "}"
        );
    closeLayout->addWidget(m_closeButton);
    closeLayout->addStretch();
    mainLayout->addLayout(closeLayout);

    connect(m_closeButton, &QPushButton::clicked, this, &TMWeeklyPCPostPrintDialog::onCloseClicked);
}

void TMWeeklyPCPostPrintDialog::onCopyPathClicked()
{
    if (!m_outputPathLabel) {
        return;
    }

    const QString outputPath = m_outputPathLabel->text().trimmed();
    if (outputPath.isEmpty() || outputPath == "Path unavailable") {
        return;
    }

    QApplication::clipboard()->setText(outputPath);
    if (m_copyPathButton) {
        m_copyPathButton->setText("COPIED");
        QTimer::singleShot(1000, this, [this]() {
            if (m_copyPathButton) {
                m_copyPathButton->setText("COPY");
            }
        });
    }
}

void TMWeeklyPCPostPrintDialog::populateFileList(const QStringList& filePaths)
{
    m_fileList->clear();

    for (int i = 0; i < filePaths.size(); ++i) {
        const QString filePath = filePaths.at(i).trimmed();
        if (filePath.isEmpty()) {
            continue;
        }

        const QFileInfo fileInfo(filePath);
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            continue;
        }

        QListWidgetItem* item = new QListWidgetItem(fileInfo.fileName());
        item->setData(Qt::UserRole, fileInfo.absoluteFilePath());

        const QIcon icon = m_iconProvider.icon(fileInfo);
        if (!icon.isNull()) {
            item->setIcon(icon);
        }

        m_fileList->addItem(item);
    }
}

void TMWeeklyPCPostPrintDialog::onCloseClicked()
{
    accept();
}
