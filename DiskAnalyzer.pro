QT       += core gui charts concurrent

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = DiskAnalyzer
TEMPLATE = app


DEFINES += QT_DEPRECATED_WARNINGS



CONFIG += c++17

SOURCES += \
        fileitem.cpp \
        main.cpp \
        mainwindow.cpp \
        scanner.cpp



HEADERS += \
        fileitem.h \
        mainwindow.h \
        scanner.h



FORMS += \
        mainwindow.ui

# Для работы с большими файлами
win32 {
    DEFINES += _LARGEFILE_SOURCE _FILE_OFFSET_BITS=64
}
