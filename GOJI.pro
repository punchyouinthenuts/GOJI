QT += core gui widgets sql network

# Ensure Qt6 compatibility
greaterThan(QT_MAJOR_VERSION, 5): QT += core5compat

TARGET = Goji
TEMPLATE = app
CONFIG += c++17

# Make sure Qt features are enabled
CONFIG += qt

DEFINES += APP_VERSION=\\\"0.9.95\\\"

INCLUDEPATH += $$shadowed($$PWD)

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
    logging.h \
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

# Use default shadow build directories
CONFIG(release, debug|release) {
    OBJECTS_DIR = $$OUT_PWD/.obj
    MOC_DIR = $$OUT_PWD/.moc
    RCC_DIR = $$OUT_PWD/.rcc
    UI_DIR = $$OUT_PWD/.ui
}
CONFIG(debug, debug|release) {
    OBJECTS_DIR = $$OUT_PWD/.obj
    MOC_DIR = $$OUT_PWD/.moc
    RCC_DIR = $$OUT_PWD/.rcc
    UI_DIR = $$OUT_PWD/.ui
}

# Ensure SQL drivers are included
QTPLUGIN += qsqlite
