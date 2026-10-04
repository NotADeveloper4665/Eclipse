QT += core network testlib
CONFIG += console c++17 testcase
CONFIG -= app_bundle
TARGET = passthrough-test
INCLUDEPATH += ../app
SOURCES += passthrough_test.cpp ../app/backend/devicepassthrough.cpp ../app/backend/mediapassthrough.cpp
HEADERS += ../app/backend/devicepassthrough.h ../app/backend/mediapassthrough.h
