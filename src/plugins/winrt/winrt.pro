include(../../qtcreatorplugin.pri)

HEADERS += \
    winrtplugin.h \
    winrtrunconfiguration.h \
    winrtruncontrol.h \
    winrtrunfactories.h \
    winrtrunconfigurationwidget.h \
    winrtrunconfigurationmodel.h \
    winrtutils.h \
    winrtselectappdialog.h \
    winrtdebugsupport.h \
    packagemanager.h

SOURCES += \
    winrtplugin.cpp \
    winrtrunconfiguration.cpp \
    winrtruncontrol.cpp \
    winrtrunfactories.cpp \
    winrtrunconfigurationwidget.cpp \
    winrtrunconfigurationmodel.cpp \
    winrtselectappdialog.cpp \
    winrtdebugsupport.cpp \
    packagemanager.cpp \
    winrtutils.cpp

# #fixme: Remove once Qt Creator introduces C++11
*g++*:QMAKE_CXXFLAGS +=-std=c++0x

DEFINES += WINRT_LIBRARY

win32-msvc2012:LIBS += -lruntimeobject -ladvapi32

FORMS += \
    winrtrunconfigurationwidget.ui \
    winrtselectappdialog.ui
