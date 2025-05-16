QT += core gui widgets sql network concurrent
# Ensure Qt6 compatibility
greaterThan(QT_MAJOR_VERSION, 5): QT += core5compat
TARGET = Goji
TEMPLATE = app
CONFIG += c++17 qt moc

# Define version
DEFINES += APP_VERSION=\\\"0.9.969\\\"

# Include project directories
INCLUDEPATH += . $$PWD
DEPENDPATH += . $$PWD

# Source files
SOURCES += \
    basefilesystemmanager.cpp \
    configmanager.cpp \
    errormanager.cpp \
    fileutils.cpp \
    logger.cpp \
    main.cpp \
    mainwindow.cpp \
    databasemanager.cpp \
    filesystemmanager.cpp \
    scriptrunner.cpp \
    filelocationsdialog.cpp \
    tmweeklypccontroller.cpp \
    tmweeklypcdbmanager.cpp \
    tmweeklypcfilemanager.cpp \
    updatedialog.cpp \
    updatemanager.cpp \
    updatesettingsdialog.cpp \
    validator.cpp

# Header files
HEADERS += \
    basefilesystemmanager.h \
    configmanager.h \
    errorhandling.h \
    errormanager.h \
    excelclipboard.h \
    filesystemmanagerfactory.h \
    fileutils.h \
    logger.h \
    mainwindow.h \
    databasemanager.h \
    filesystemmanager.h \
    scriptrunner.h \
    filelocationsdialog.h \
    threadutils.h \
    tmweeklypccontroller.h \
    tmweeklypcdbmanager.h \
    tmweeklypcfilemanager.h \
    updatedialog.h \
    updatemanager.h \
    updatesettingsdialog.h \
    validator.h

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

# Debug flags
CONFIG(debug, debug|release) {
    QMAKE_CXXFLAGS += -Wall -Wextra
    message("Building in debug mode with extended warnings")
}

# Windows deployment configuration
win32 {
    CONFIG += debug_and_release build_all

    # Important: No explicit LIBS entries that could interfere with Qt's linking
    # LIBS += -LC:/Qt/6.9.0/mingw_64/lib -lQt6Core -lQt6Gui -lQt6Widgets -lQt6Sql -lQt6Network -lQt6Concurrent -lQt6Core5Compat

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
