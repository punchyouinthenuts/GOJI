QT       += core gui widgets sql

TARGET = Goji

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

SOURCES += \
    main.cpp \
    goji.cpp \

HEADERS += \
    goji.h \

FORMS += \
    GOJI.ui

RESOURCES += \
    resources.qrc

RC_ICONS = ShinGoji.ico
