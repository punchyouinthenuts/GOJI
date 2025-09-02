QT += core gui widgets sql network concurrent axcontainer
# Ensure Qt6 compatibility
greaterThan(QT_MAJOR_VERSION, 5): QT += core5compat

TARGET = Goji
TEMPLATE = app
CONFIG += c++17 qt

# Define version
DEFINES += APP_VERSION=\\\"1.1.044\\\"

# Ensure MOC, UIC, and RCC use UTF-8
QMAKE_MOC_OPTIONS += -DUNICODE
CODECFORSRC = UTF-8
CODECFORTR = UTF-8

# Force clean builds by ensuring proper dependency tracking
CONFIG += depend_includepath
CONFIG += object_parallel_to_source

# Enhanced build directories - use absolute paths to avoid conflicts
BUILD_DIR = $$PWD/build-$$TARGET
OBJECTS_DIR = $$BUILD_DIR/obj
MOC_DIR = $$BUILD_DIR/moc
RCC_DIR = $$BUILD_DIR/rcc
UI_DIR = $$BUILD_DIR/ui
DESTDIR = $$BUILD_DIR/bin

# Include project directories with explicit paths
INCLUDEPATH += $$PWD
INCLUDEPATH += $$UI_DIR
DEPENDPATH += $$PWD
DEPENDPATH += $$UI_DIR

# Source files - grouped by functionality and alphabetically sorted
SOURCES += \
    archiveutils.cpp \
    basetrackercontroller.cpp \
    dropwindow.cpp \
    main.cpp \
    mainwindow.cpp \
    basefilesystemmanager.cpp \
    configmanager.cpp \
    databasemanager.cpp \
    errormanager.cpp \
    filelocationsdialog.cpp \
    filesystemmanager.cpp \
    fileutils.cpp \
    logger.cpp \
    naslinkdialog.cpp \
    scriptrunner.cpp \
    tmflercontroller.cpp \
    tmflerdbmanager.cpp \
    tmflerfilemanager.cpp \
    tmhealthycontroller.cpp \
    tmhealthydbmanager.cpp \
    tmhealthyfilemanager.cpp \
    tmhealthyemaildialog.cpp \
    tmhealthyemailfilelistwidget.cpp \
    tmhealthynetworkdialog.cpp \
    tmtarragoncontroller.cpp \
    tmtarragondbmanager.cpp \
    tmtarragonfilemanager.cpp \
    tmtermcontroller.cpp \
    tmtermdbmanager.cpp \
    tmtermemaildialog.cpp \
    tmtermfilemanager.cpp \
    tmweeklypccontroller.cpp \
    tmweeklypcdbmanager.cpp \
    tmweeklypcfilemanager.cpp \
    tmweeklypcfilemanagerdialog.cpp \
    tmweeklypidocontroller.cpp \
    tmweeklypidozipfilesdialog.cpp \
    updatedialog.cpp \
    updatemanager.cpp \
    updatesettingsdialog.cpp \
    validator.cpp

# Header files - grouped by functionality and alphabetically sorted
HEADERS += \
    archiveutils.h \
    basetrackercontroller.h \
    dropwindow.h \
    mainwindow.h \
    basefilesystemmanager.h \
    configmanager.h \
    databasemanager.h \
    errorhandling.h \
    errormanager.h \
    excelclipboard.h \
    filelocationsdialog.h \
    filesystemmanager.h \
    filesystemmanagerfactory.h \
    fileutils.h \
    logger.h \
    naslinkdialog.h \
    scriptrunner.h \
    threadutils.h \
    tmflercontroller.h \
    tmflerdbmanager.h \
    tmflerfilemanager.h \
    tmhealthycontroller.h \
    tmhealthydbmanager.h \
    tmhealthyfilemanager.h \
    tmhealthyemaildialog.h \
    tmhealthyemailfilelistwidget.h \
    tmhealthynetworkdialog.h \
    tmtarragoncontroller.h \
    tmtarragondbmanager.h \
    tmtarragonfilemanager.h \
    tmtermcontroller.h \
    tmtermdbmanager.h \
    tmtermemaildialog.h \
    tmtermfilemanager.h \
    tmweeklypccontroller.h \
    tmweeklypcdbmanager.h \
    tmweeklypcfilemanager.h \
 tmweeklypcfilemanagerdialog.h \
    tmweeklypidocontroller.h \
    tmweeklypidozipfilesdialog.h \
    updatedialog.h \
    updatemanager.h \
    updatesettingsdialog.h \
    validator.h

# UI files
FORMS += GOJI.ui

# Resources
RESOURCES += resources.qrc
RC_ICONS = icons/ShinGoji.ico

# Ensure proper MOC compilation
CONFIG += moc
QMAKE_MOC = $$[QT_INSTALL_BINS]/moc$$QMAKE_EXT_EXE

# Force dependency tracking for all files
CONFIG += create_prl

# Clean up rules - enhanced cleanup
QMAKE_CLEAN += $$BUILD_DIR/*
QMAKE_CLEAN += Makefile*
QMAKE_CLEAN += .qmake.stash
QMAKE_CLEAN += *.tmp

# Additional distclean targets
QMAKE_DISTCLEAN += $$TARGET.pro.user*
QMAKE_DISTCLEAN += .qmake.stash

# Debug configuration
CONFIG(debug, debug|release) {
    QMAKE_CXXFLAGS += -Wall -Wextra
    DEFINES += QT_QML_DEBUG
    TARGET = $${TARGET}_debug
    message("Building in debug mode with extended warnings")
}

# Release configuration
CONFIG(release, debug|release) {
    DEFINES += QT_NO_DEBUG_OUTPUT
    QMAKE_CXXFLAGS += -O2
    TARGET = $${TARGET}_release
    message("Building in release mode")
}

# Windows specific configuration
win32 {
    # Use proper Windows threading
    CONFIG += windeployqt
    
    # Deployment configuration
    CONFIG(debug, debug|release) {
        DEPLOY_COMMAND = $$[QT_INSTALL_BINS]/windeployqt --debug --compiler-runtime
    }
    CONFIG(release, debug|release) {
        DEPLOY_COMMAND = $$[QT_INSTALL_BINS]/windeployqt --release --compiler-runtime
    }
    
    # Post-build deployment (optional)
    # QMAKE_POST_LINK = $$DEPLOY_COMMAND $$shell_quote($$DESTDIR/$$TARGET$$TARGET_EXT)
}

# Ensure SQL drivers are available
QTPLUGIN += qsqlite

# Verbose makefile for debugging build issues (uncomment if needed)
# CONFIG += debug_and_release
# CONFIG += build_all

# Force qmake to recognize all dependencies
CONFIG += force_debug_info

# Message for successful qmake run
message("Project configured successfully. Build directories:")
message("  Objects: $$OBJECTS_DIR")
message("  MOC: $$MOC_DIR")
message("  UI: $$UI_DIR")
message("  RCC: $$RCC_DIR")
message("  Destination: $$DESTDIR")

DISTFILES += \
    resources/tmterm/default.html \
    resources/tmterm/instructions.html \
    resources/tmhealthy/default.html \
    resources/tmhealthy/instructions.html

# --- BROKEN APPOINTMENTS ---
SOURCES += \
    tmbrokencontroller.cpp \
    tmbrokendbmanager.cpp \
    tmbrokenfilemanager.cpp \
    tmbrokennetworkdialog.cpp \
    tmbrokenemaildialog.cpp \
    tmbrokenemailfilelistwidget.cpp

HEADERS += \
    tmbrokencontroller.h \
    tmbrokendbmanager.h \
    tmbrokenfilemanager.h \
    tmbrokennetworkdialog.h \
    tmbrokenemaildialog.h \
    tmbrokenemailfilelistwidget.h
