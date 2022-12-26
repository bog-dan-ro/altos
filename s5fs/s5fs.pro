QT =
CONFIG += c++17 console
CONFIG -= app_bundle

CONFIG += link_pkgconfig
PKGCONFIG += fuse3

SOURCES += \
        main.cpp \
        s52k.cpp

HEADERS += \
    s52k.h
