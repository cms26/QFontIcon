#-------------------------------------------------
#
# Project created by QtCreator 2017-01-03T13:37:08
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = QFontIcon
TEMPLATE = app


SOURCES += main.cpp

# Icons from the font files (the CMake build compiles them in instead)
DEFINES += QFONTICON_EXAMPLE_USE_FONTS


include("QFontIcon/QFontIcon.pri")

RESOURCES += \
    resource.qrc
