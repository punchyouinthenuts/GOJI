#include "filelocationsdialog.h"
#include <QClipboard>
#include <QApplication>
#include <QFont>

FileLocationsDialog::FileLocationsDialog(const QString& locationsText, QWidget* parent)
    : QDialog(parent), m_textEdit(new QTextEdit(this)), m_copyButton(new QPushButton("Copy", this)), m_closeButton(new QPushButton("Close", this))
{
    setWindowTitle("Print File Locations");
    resize(600, 300);

    // Set up text edit
    m_textEdit->setText(locationsText);
    m_textEdit->setReadOnly(true);
    m_textEdit->setStyleSheet("QTextEdit { border: 1px solid black; padding: 5px; }");

    // Set up buttons with Blender Pro Bold font
    QFont buttonFont("Blender Pro Bold", 10);
    m_copyButton->setFont(buttonFont);
    m_closeButton->setFont(buttonFont);
    m_copyButton->setFixedWidth(100);
    m_closeButton->setFixedWidth(100);

    // Layout setup
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Top layout for close button (right-aligned)
    QHBoxLayout* topLayout = new QHBoxLayout();
    topLayout->addStretch();
    topLayout->addWidget(m_closeButton);
    mainLayout->addLayout(topLayout);

    // Add text edit
    mainLayout->addWidget(m_textEdit);

    // Bottom layout for copy button (right-aligned)
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    bottomLayout->addWidget(m_copyButton);
    mainLayout->addLayout(bottomLayout);

    // Connect signals
    connect(m_copyButton, &QPushButton::clicked, this, &FileLocationsDialog::onCopyButtonClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &FileLocationsDialog::onCloseButtonClicked);
}

void FileLocationsDialog::onCopyButtonClicked()
{
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(m_textEdit->toPlainText());
}

void FileLocationsDialog::onCloseButtonClicked()
{
    accept();
}
