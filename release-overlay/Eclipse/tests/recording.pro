QT += core
CONFIG += console c++17 link_pkgconfig
CONFIG -= app_bundle
PKGCONFIG += libavformat libavcodec libavutil opus
DEFINES += HAVE_FFMPEG
INCLUDEPATH += ../moonlight-common-c/moonlight-common-c/src
SOURCES += recording_smoke.cpp \
    ../app/streaming/recording/streamrecorder.cpp
HEADERS += ../app/streaming/recording/streamrecorder.h
TARGET = recording-smoke
