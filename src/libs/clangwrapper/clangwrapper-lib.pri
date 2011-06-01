include(clang_installation.pri)

dll {
    DEFINES += QTCREATOR_CLANGWRAPPER_LIB
} else {
    DEFINES += QTCREATOR_CLANGWRAPPER_STATIC_LIB
}

INCLUDEPATH += $$PWD/..

SOURCES += \
    $$PWD/clangwrapper.cpp \
    $$PWD/sourcemarker.cpp

HEADERS += \
    $$PWD/clangwrapper.h \
    $$PWD/sourcemarker.h \
    clangwrapper_global.h



