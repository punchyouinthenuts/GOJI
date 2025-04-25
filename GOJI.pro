QT += core gui widgets sql
TARGET = Goji
TEMPLATE = app
CONFIG += c++17
DEFINES += APP_VERSION=\\\"0.9.9a\\\"

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
INCLUDEPATH += $$PWD
