#include "tmmaemaildialog.h"

#include "tmfleremailfilelistwidget.h"

#include <QAbstractItemView>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

TMMAEmailDialog::TMMAEmailDialog(const QStringList& filePaths, QWidget* parent)
    : QDialog(parent)
    , m_headingLabel(nullptr)
    , m_fileList(nullptr)
    , m_helpLabel(nullptr)
    , m_closeButton(nullptr)
{
    for (const QString& filePath : filePaths) {
        m_filePaths.append(QFileInfo(filePath).absoluteFilePath());
    }
    setWindowTitle(QStringLiteral("Email Attachment - TM MA"));
    setFixedSize(678, 420);
    setModal(true);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(17);
    mainLayout->setContentsMargins(23, 23, 23, 23);

    m_headingLabel = new QLabel(QStringLiteral("DRAG & DROP FILE INTO E-MAIL"), this);
    m_headingLabel->setFont(QFont(QStringLiteral("Blender Pro Bold"), 16, QFont::Bold));
    m_headingLabel->setAlignment(Qt::AlignCenter);
    m_headingLabel->setStyleSheet(QStringLiteral("color: #2c3e50; margin-bottom: 17px;"));
    mainLayout->addWidget(m_headingLabel);

    m_fileList = new TMFLEREmailFileListWidget(this);
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileList->setFont(QFont(QStringLiteral("Blender Pro"), 11));
    m_fileList->setStyleSheet(QStringLiteral(
        "QListWidget { border: 2px solid #bdc3c7; border-radius: 9px; "
        "background-color: white; selection-background-color: #e3f2fd; }"));

    QFileIconProvider iconProvider;
    for (const QString& filePath : m_filePaths) {
        const QFileInfo fileInfo(filePath);
        auto* item = new QListWidgetItem(fileInfo.fileName());
        item->setData(Qt::UserRole, filePath);
        item->setToolTip(filePath);
        item->setIcon(iconProvider.icon(fileInfo));
        m_fileList->addItem(item);
    }
    if (m_fileList->count() > 0) {
        m_fileList->setCurrentRow(0);
    }
    mainLayout->addWidget(m_fileList);

    m_helpLabel = new QLabel(
        QStringLiteral("Drag either file above directly into your Outlook email. Both files will remain in DATA."),
        this);
    m_helpLabel->setFont(QFont(QStringLiteral("Blender Pro"), 11, QFont::StyleItalic));
    m_helpLabel->setAlignment(Qt::AlignCenter);
    m_helpLabel->setStyleSheet(QStringLiteral("color: #666666;"));
    mainLayout->addWidget(m_helpLabel);

    auto* closeLayout = new QHBoxLayout();
    closeLayout->addStretch();
    m_closeButton = new QPushButton(QStringLiteral("CLOSE"), this);
    m_closeButton->setFont(QFont(QStringLiteral("Blender Pro Bold"), 14, QFont::Bold));
    m_closeButton->setFixedSize(113, 40);
    m_closeButton->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #6c757d; color: white; border: none; "
        "border-radius: 5px; font-weight: bold; }"
        "QPushButton:hover { background-color: #5a6268; }"
        "QPushButton:pressed { background-color: #4e555b; }"));
    closeLayout->addWidget(m_closeButton);
    closeLayout->addStretch();
    mainLayout->addLayout(closeLayout);

    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
}
