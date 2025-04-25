QT += core gui widgets sql

# For Qt 6 compatibility
greaterThan(QT_MAJOR_VERSION, 5): QT += core5compat

TARGET = Goji
TEMPLATE = app
CONFIG += c++17

DEFINES += APP_VERSION=\\\"0.9.9c\\\"

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    databasemanager.cpp \
    filesystemmanager.cpp \
    jobdata.cpp \
    scriptrunner.cpp \
    jobcontroller.cpp \
    countstabledialog.cpp \
    filelocationsdialog.cpp

HEADERS += \
    mainwindow.h \
    databasemanager.h \
    filesystemmanager.h \
    jobdata.h \
    scriptrunner.h \
    jobcontroller.h \
    countstabledialog.h \
    filelocationsdialog.h

FORMS += GOJI.ui

RESOURCES += resources.qrc

RC_ICONS = ShinGoji.ico

# Ensure Qt headers are found
INCLUDEPATH += $$[QT_INSTALL_HEADERS]
