# qmake: include("QFontIcon/QFontIcon.pri") in your .pro (CMake: see CMakeLists.txt)
QT += widgets

HEADERS += \
    $$PWD/qfonticon.h

SOURCES += \
    $$PWD/qfonticon.cpp

INCLUDEPATH += $$PWD
