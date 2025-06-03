#include "naslinkdialog.h"
#include <QFontMetrics>
#include <QScreen>
#include <QGuiApplication>
#include <QTimer>

NASLinkDialog::NASLinkDialog(const QString& windowTitle,
                             const QString& descriptionText,
                             const QString& networkPath,
                             QWidget* parent)
    : QDialog(parent), m_networkPath(networkPath), m_descriptionText(descriptionText)
{
    setWindowTitle(windowTitle);
    setModal(true);

    setupUI();

    // Set focus to copy button for quick keyboard access
    m_copyButton->setFocus();
}

NASLinkDialog::NASLinkDialog(const QString& networkPath, QWidget* parent)
    : NASLinkDialog("File Location", "File located below", networkPath, parent)
{
    // Delegating constructor handles everything
}

void NASLinkDialog::setupUI()
{
    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Description label (customizable text)
    m_descriptionLabel = new QLabel(m_descriptionText, this);
    m_descriptionLabel->setFont(QFont("Blender Pro", 14, QFont::Bold));
    m_descriptionLabel->setAlignment(Qt::AlignCenter);
    m_descriptionLabel->setStyleSheet("color: #333333; margin-bottom: 10px;");
    mainLayout->addWidget(m_descriptionLabel);

    // Separator line
    QFrame* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setStyleSheet("border: 1px solid #cccccc;");
    mainLayout->addWidget(separator);

    // Network path display (read-only line edit for easy selection)
    m_pathLineEdit = new QLineEdit(m_networkPath, this);
    m_pathLineEdit->setReadOnly(true);
    m_pathLineEdit->setFont(QFont("Consolas", 12));
    m_pathLineEdit->setStyleSheet(
        "QLineEdit {"
        "   background-color: #f5f5f5;"
        "   border: 2px solid #cccccc;"
        "   border-radius: 4px;"
        "   padding: 8px;"
        "   selection-background-color: #0078d4;"
        "   color: #000000;"
        "}"
        );

    // Select all text by default for easy copying
    m_pathLineEdit->selectAll();

    mainLayout->addWidget(m_pathLineEdit);

    // Button layout
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);

    // Add stretch to center buttons
    buttonLayout->addStretch();

    // Copy button
    m_copyButton = new QPushButton("COPY", this);
    m_copyButton->setFont(QFont("Blender Pro Bold", 12, QFont::Bold));
    m_copyButton->setFixedSize(100, 35);
    m_copyButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #0078d4;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 4px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: #106ebe;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #005a9e;"
        "}"
        );
    m_copyButton->setDefault(true);
    buttonLayout->addWidget(m_copyButton);

    // Close button
    m_closeButton = new QPushButton("CLOSE", this);
    m_closeButton->setFont(QFont("Blender Pro Bold", 12, QFont::Bold));
    m_closeButton->setFixedSize(100, 35);
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
    buttonLayout->addWidget(m_closeButton);

    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(m_copyButton, &QPushButton::clicked, this, &NASLinkDialog::onCopyClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &NASLinkDialog::onCloseClicked);

    // Set optimal dialog size
    int optimalWidth = calculateOptimalWidth();
    resize(optimalWidth, 180);

    // Center on parent or screen
    if (parentWidget()) {
        move(parentWidget()->geometry().center() - rect().center());
    } else {
        QScreen* screen = QGuiApplication::primaryScreen();
        if (screen) {
            move(screen->geometry().center() - rect().center());
        }
    }
}

void NASLinkDialog::onCopyClicked()
{
    // Copy the network path to clipboard
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(m_networkPath);

    // Provide visual feedback
    m_copyButton->setText("COPIED!");
    m_copyButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #28a745;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 4px;"
        "   font-weight: bold;"
        "}"
        );

    // Reset button after 1.5 seconds
    QTimer::singleShot(1500, this, [this]() {
        m_copyButton->setText("COPY");
        m_copyButton->setStyleSheet(
            "QPushButton {"
            "   background-color: #0078d4;"
            "   color: white;"
            "   border: none;"
            "   border-radius: 4px;"
            "   font-weight: bold;"
            "}"
            "QPushButton:hover {"
            "   background-color: #106ebe;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #005a9e;"
            "}"
            );
    });
}

void NASLinkDialog::onCloseClicked()
{
    accept();
}

int NASLinkDialog::calculateOptimalWidth() const
{
    // Calculate width needed to display the full path without scrolling
    QFontMetrics fontMetrics(QFont("Consolas", 12));
    int textWidth = fontMetrics.horizontalAdvance(m_networkPath);

    // Add padding for line edit margins, borders, and dialog margins
    int totalPadding = 60; // 20px dialog margins + 16px line edit padding + some extra
    int calculatedWidth = textWidth + totalPadding;

    // Set reasonable bounds
    int minWidth = 400;
    int maxWidth = 800;

    // Get screen width to ensure dialog fits
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        int screenWidth = screen->availableGeometry().width();
        maxWidth = qMin(maxWidth, static_cast<int>(screenWidth * 0.8));
    }

    return qBound(minWidth, calculatedWidth, maxWidth);
}
