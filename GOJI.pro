QT += core gui widgets sql network

# Ensure Qt6 compatibility
greaterThan(QT_MAJOR_VERSION, 5): QT += core5compat

TARGET = Goji
TEMPLATE = app
CONFIG += c++17

# Make sure Qt features are enabled
CONFIG += qt

DEFINES += APP_VERSION=\\\"0.9.92\\\"

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

# Explicitly set output directories for Release and Debug builds
CONFIG(release, debug|release) {
    DESTDIR = $$PWD/build/Release
    OBJECTS_DIR = $$PWD/build/Release/.obj
    MOC_DIR = $$PWD/build/Release/.moc
    RCC_DIR = $$PWD/build/Release/.rcc
    UI_DIR = $$PWD/build/Release/.ui
}
CONFIG(debug, debug|release) {
    DESTDIR = $$PWD/build/Debug
    OBJECTS_DIR = $$PWD/build/Debug/.obj
    MOC_DIR = $$PWD/build/Debug/.moc
    RCC_DIR = $$PWD/build/Debug/.rcc
    UI_DIR = $$PWD/build/Debug/.ui
}
