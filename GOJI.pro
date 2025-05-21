QT += core gui widgets sql network concurrent
# Ensure Qt6 compatibility
greaterThan(QT_MAJOR_VERSION, 5): QT += core5compat
TARGET = Goji
TEMPLATE = app
CONFIG += c++17 qt moc

# Define version
DEFINES += APP_VERSION=\\\"0.9.969\\\"

# Include project directories
INCLUDEPATH += $$OUT_PWD/.ui $$PWD
DEPENDPATH += $$OUT_PWD/.ui $$PWD

# Source files grouped by functionality
SOURCES += \
    main.cpp \
    mainwindow.cpp \
    # Core components
    databasemanager.cpp \
    errormanager.cpp \
    logger.cpp \
    validator.cpp \
    # File system components
    basefilesystemmanager.cpp \
    filesystemmanager.cpp \
    fileutils.cpp \
    # UI components
    filelocationsdialog.cpp \
    scriptrunner.cpp \
    updatedialog.cpp \
    updatemanager.cpp \
    updatesettingsdialog.cpp \
    # Configuration components
    configmanager.cpp \
    # TM Weekly PC components
    tmweeklypccontroller.cpp \
    tmweeklypcdbmanager.cpp \
    tmweeklypcfilemanager.cpp

# Header files grouped by functionality
HEADERS += \
    mainwindow.h \
    # Core components
    databasemanager.h \
    errorhandling.h \
    errormanager.h \
    logger.h \
    threadutils.h \
    validator.h \
    # File system components
    basefilesystemmanager.h \
    filesystemmanager.h \
    filesystemmanagerfactory.h \
    fileutils.h \
    # UI components
    excelclipboard.h \
    filelocationsdialog.h \
    scriptrunner.h \
    updatedialog.h \
    updatemanager.h \
    updatesettingsdialog.h \
    # Configuration components
    configmanager.h \
    # TM Weekly PC components
    tmweeklypccontroller.h \
    tmweeklypcdbmanager.h \
    tmweeklypcfilemanager.h

# UI files
FORMS += \
    GOJI.ui

# Resources
RESOURCES += resources.qrc
RC_ICONS = ShinGoji.ico

# Shadow build directories
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

# SQL drivers
QTPLUGIN += qsqlite

# Add QMAKE_CLEAN to remove generated files during clean
QMAKE_CLEAN += $$OUT_PWD/debug/* $$OUT_PWD/release/* $$OUT_PWD/.obj/* $$OUT_PWD/.moc/* $$OUT_PWD/.rcc/* $$OUT_PWD/.ui/*

# Debug flags
CONFIG(debug, debug|release) {
    QMAKE_CXXFLAGS += -Wall -Wextra
    message("Building in debug mode with extended warnings")
}

# Windows deployment configuration
win32 {
    CONFIG += debug_and_release build_all

    # Deploy DLLs for debug builds
    CONFIG(debug, debug|release) {
        message("Setting up debug deployment")
        QMAKE_POST_LINK = $$escape_expand(\\n) $$[QT_INSTALL_BINS]/windeployqt --debug --qmldir $$PWD \"$$OUT_PWD/debug/$${TARGET}.exe\"
    }

    # Deploy DLLs for release builds
    CONFIG(release, debug|release) {
        message("Setting up release deployment")
        QMAKE_POST_LINK = $$escape_expand(\\n) $$[QT_INSTALL_BINS]/windeployqt --release --qmldir $$PWD \"$$OUT_PWD/release/$${TARGET}.exe\"
    }
}
