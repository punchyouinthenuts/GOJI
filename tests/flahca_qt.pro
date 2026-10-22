QT += core gui widgets sql network concurrent axcontainer core5compat
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = flahca_qt_check
GOJI_ROOT = $$clean_path($$PWD/..)
INCLUDEPATH += $$GOJI_ROOT $$OUT_PWD/ui
UI_DIR = $$OUT_PWD/ui
MOC_DIR = $$OUT_PWD/moc
OBJECTS_DIR = $$OUT_PWD/obj
RCC_DIR = $$OUT_PWD/rcc
FORMS += $$GOJI_ROOT/GOJI.ui
RESOURCES += $$GOJI_ROOT/resources.qrc
SOURCES += $$PWD/test_flahca_qt.cpp
MODULES = tmflahcacontroller tmflahcadbmanager tmflahcafilemanager \
    tmflahcaemaildialog tmflahcaemailfilelistwidget basetrackercontroller \
    basefilesystemmanager databasemanager scriptrunner dropwindow \
    dropbindinghelper archiveutils configmanager fileutils logger \
    terminaloutputhelper monthcomboboxhelper yearcomboboxhelper \
    scriptrunnerbindinghelper naslinkdialog
for(module, MODULES) {
    SOURCES += $$GOJI_ROOT/$${module}.cpp
    HEADERS += $$GOJI_ROOT/$${module}.h
}
