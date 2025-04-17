QT += core gui widgets sql
TARGET = Goji
TEMPLATE = app
CONFIG += c++17
DEFINES += APP_VERSION=\\\"0.9.7\\\"
SOURCES += main.cpp goji.cpp
HEADERS += goji.h
FORMS += GOJI.ui
RESOURCES += resources.qrc
RC_ICONS = ShinGoji.ico
