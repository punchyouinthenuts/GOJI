#include "pathcopydialog.h"

#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

PathCopyDialog::PathCopyDialog(const QString& windowTitle,
                               const QString& path,
                               QWidget* parent)
    : QDialog(parent)
    , m_path(path.trimmed())
    , m_pathLabel(nullptr)
    , m_copyButton(nullptr)
{
    setWindowTitle(windowTitle);
    setModal(true);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    m_pathLabel = new QLabel(m_path.isEmpty() ? QStringLiteral("Path unavailable") : m_path, this);
    m_pathLabel->setFont(QFont("Blender Pro", 10));
    m_pathLabel->setTextFormat(Qt::PlainText);
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pathLabel->setWordWrap(true);
    m_pathLabel->setStyleSheet(
        "QLabel {"
        "   border: 2px solid #bdc3c7;"
        "   border-radius: 8px;"
        "   background-color: white;"
        "   padding: 10px;"
        "   color: #2c3e50;"
        "}"
        );
    mainLayout->addWidget(m_pathLabel);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_copyButton = new QPushButton("COPY", this);
    m_copyButton->setFont(QFont("Blender Pro Bold", 11, QFont::Bold));
    m_copyButton->setFixedSize(90, 34);
    m_copyButton->setStyleSheet(
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
    buttonLayout->addWidget(m_copyButton);

    QPushButton* closeButton = new QPushButton("CLOSE", this);
    closeButton->setFont(QFont("Blender Pro Bold", 11, QFont::Bold));
    closeButton->setFixedSize(100, 34);
    closeButton->setStyleSheet(
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
    buttonLayout->addWidget(closeButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    connect(m_copyButton, &QPushButton::clicked, this, &PathCopyDialog::copyPath);
    connect(closeButton, &QPushButton::clicked, this, &PathCopyDialog::accept);

    resize(680, 180);
}

void PathCopyDialog::copyPath()
{
    if (m_path.isEmpty()) {
        return;
    }

    QApplication::clipboard()->setText(m_path);
    if (m_copyButton) {
        m_copyButton->setText("COPIED");
        QTimer::singleShot(1000, this, [this]() {
            if (m_copyButton) {
                m_copyButton->setText("COPY");
            }
        });
    }
}
