QT += core gui widgets sql network concurrent

# Ensure Qt6 compatibility
greaterThan(QT_MAJOR_VERSION, 5): QT += core5compat

TARGET = Goji
TEMPLATE = app
CONFIG += c++17 qt moc

DEFINES += APP_VERSION=\\\"0.9.968\\\"

# Explicit Qt paths
INCLUDEPATH += C:/Qt/6.9.0/mingw_64/include
DEPENDPATH += C:/Qt/6.9.0/mingw_64/include
LIBS += -LC:/Qt/6.9.0/mingw_64/lib -lQt6Core -lQt6Gui -lQt6Widgets -lQt6Sql -lQt6Network -lQt6Concurrent -lQt6Core5Compat

# Include project directories
INCLUDEPATH += . $$PWD
DEPENDPATH += . $$PWD

SOURCES += \
    configmanager.cpp \
    errormanager.cpp \
    fileutils.cpp \
    logger.cpp \
    main.cpp \
    mainwindow.cpp \
    databasemanager.cpp \
    filesystemmanager.cpp \
    jobdata.cpp \
    pdffilehelper.cpp \
    pdfrepairintegration.cpp \
    scriptrunner.cpp \
    jobcontroller.cpp \
    countstabledialog.cpp \
    filelocationsdialog.cpp \
    updatedialog.cpp \
    updatemanager.cpp \
    updatesettingsdialog.cpp \
    validator.cpp

HEADERS += \
    configmanager.h \
    errorhandling.h \
    errormanager.h \
    excelclipboard.h \
    fileutils.h \
    logger.h \
    mainwindow.h \
    databasemanager.h \
    filesystemmanager.h \
    jobdata.h \
    pdffilehelper.h \
    pdfrepairintegration.h \
    scriptrunner.h \
    jobcontroller.h \
    countstabledialog.h \
    filelocationsdialog.h \
    threadutils.h \
    updatedialog.h \
    updatemanager.h \
    updatesettingsdialog.h \
    validator.h

FORMS += \
    GOJI.ui

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
    QMAKE_CXXFLAGS += -Wall -Wextra -x c++
    message("Building in debug mode with extended warnings")
}
