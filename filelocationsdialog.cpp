#include "filelocationsdialog.h"
#include <QClipboard>
#include <QApplication>
#include <QFont>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QTextEdit>

FileLocationsDialog::FileLocationsDialog(const QString& locationsText, ButtonType buttonType, QWidget* parent)
    : QDialog(parent), m_textEdit(new QTextEdit(this))
{
    setWindowTitle(buttonType == CopyCloseButtons ? "Print File Locations" : "Missing Files");
    resize(600, 300);

    // Set up text edit
    m_textEdit->setText(locationsText);
    m_textEdit->setReadOnly(true);
    m_textEdit->setStyleSheet("QTextEdit { border: 1px solid black; padding: 5px; }");

    // Set up buttons with Blender Pro Bold font
    QFont buttonFont("Blender Pro Bold", 10);

    // Layout setup
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Add text edit
    mainLayout->addWidget(m_textEdit);

    // Bottom layout for buttons
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch(); // Add spacing before buttons

    if (buttonType == CopyCloseButtons) {
        m_copyButton = new QPushButton("Copy", this);
        m_closeButton = new QPushButton("Close", this);
        m_copyButton->setFont(buttonFont);
        m_closeButton->setFont(buttonFont);
        m_copyButton->setFixedWidth(100);
        m_closeButton->setFixedWidth(100);
        bottomLayout->addWidget(m_copyButton);
        bottomLayout->addSpacing(20); // Physical separation
        bottomLayout->addWidget(m_closeButton);
        connect(m_copyButton, &QPushButton::clicked, this, &FileLocationsDialog::onCopyButtonClicked);
        connect(m_closeButton, &QPushButton::clicked, this, &FileLocationsDialog::onCloseButtonClicked);
    } else if (buttonType == YesNoButtons) {
        m_yesButton = new QPushButton("Yes", this);
        m_noButton = new QPushButton("No", this);
        m_yesButton->setFont(buttonFont);
        m_noButton->setFont(buttonFont);
        m_yesButton->setFixedWidth(100);
        m_noButton->setFixedWidth(100);
        bottomLayout->addWidget(m_yesButton);
        bottomLayout->addSpacing(20); // Physical separation
        bottomLayout->addWidget(m_noButton);
        connect(m_yesButton, &QPushButton::clicked, this, &FileLocationsDialog::accept);
        connect(m_noButton, &QPushButton::clicked, this, &FileLocationsDialog::reject);
    } else if (buttonType == OkButton) {
        m_okButton = new QPushButton("OK", this);
        m_okButton->setFont(buttonFont);
        m_okButton->setFixedWidth(100);
        bottomLayout->addWidget(m_okButton);
        connect(m_okButton, &QPushButton::clicked, this, &FileLocationsDialog::accept);
    }

    bottomLayout->addStretch(); // Add spacing after buttons
    mainLayout->addLayout(bottomLayout);
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
