#include "mainwindow.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Set global stylesheet for disabled widgets
    a.setStyleSheet(R"(
    QPushButton:disabled, QToolButton:disabled {
        background-color: #d3d3d3; /* Light grey background */
        color: #a9a9a9; /* Dark grey text */
        border: 1px solid #a9a9a9; /* Dark grey border */
    }
    QComboBox:disabled {
        background-color: #d3d3d3; /* Light grey background */
        color: #696969; /* Darker grey text */
        border: 1px solid #a9a9a9; /* Dark grey border */
    }
    )");

    QCoreApplication::setApplicationName("Goji");

    MainWindow w;
    w.setWindowIcon(QIcon(":/resources/icons/ShinGoji.ico"));
    w.show();

    return a.exec();
}
