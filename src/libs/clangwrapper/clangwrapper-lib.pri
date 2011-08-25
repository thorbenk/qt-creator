include(clang_installation.pri)

dll {
    DEFINES += QTCREATOR_CLANGWRAPPER_LIB
} else {
    DEFINES += QTCREATOR_CLANGWRAPPER_STATIC_LIB
}

INCLUDEPATH += $$PWD/..

SOURCES += \
    $$PWD/clangwrapper.cpp \
    $$PWD/sourcemarker.cpp \
    $$PWD/rawlexer.cpp \
    $$PWD/keywords.cpp \
    $$PWD/token.cpp

HEADERS += \
    clangwrapper_global.h \
    $$PWD/clangwrapper.h \
    $$PWD/sourcemarker.h \
    $$PWD/rawlexer.h \
    $$PWD/keywords.h \
    $$PWD/token.h \
    $$PWD/constants.h
