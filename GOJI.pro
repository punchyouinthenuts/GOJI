QT += core gui widgets sql network

# Ensure Qt6 compatibility
greaterThan(QT_MAJOR_VERSION, 5): QT += core5compat

TARGET = Goji
TEMPLATE = app
CONFIG += c++17

# Make sure Qt features are enabled
CONFIG += qt

DEFINES += APP_VERSION=\\\"0.9.9f\\\"

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    databasemanager.cpp \
    filesystemmanager.cpp \
    jobdata.cpp \
    scriptrunner.cpp \
    jobcontroller.cpp \
    countstabledialog.cpp \
    filelocationsdialog.cpp \
    updatedialog.cpp \
    updatemanager.cpp \
    updatesettingsdialog.cpp

HEADERS += \
    mainwindow.h \
    databasemanager.h \
    filesystemmanager.h \
    jobdata.h \
    scriptrunner.h \
    jobcontroller.h \
    countstabledialog.h \
    filelocationsdialog.h \
    updatedialog.h \
    updatemanager.h \
    updatesettingsdialog.h

FORMS += GOJI.ui
RESOURCES += resources.qrc
RC_ICONS = ShinGoji.ico

# Remove the explicit include paths and let Qt find them automatically
# INCLUDEPATH += $$[QT_INSTALL_HEADERS]
# INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtCore
# INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtGui
# INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtWidgets
# INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtSql
