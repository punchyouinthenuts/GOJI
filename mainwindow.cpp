#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "rac.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Create the RAC widget and make it the central widget
    RAC *racForm = new RAC(this);
    setCentralWidget(racForm);

    // Set a proper window title
    setWindowTitle("RAC Weekly Report Application");
}

MainWindow::~MainWindow()
{
    delete ui;
}
