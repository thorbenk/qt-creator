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
    $$PWD/token.cpp \
    $$PWD/reuse.cpp \
    $$PWD/indexer.cpp \
    $$PWD/indexedsymbolinfo.cpp \
    $$PWD/sourcelocation.cpp \
    $$PWD/unit.cpp \
    $$PWD/liveunitsmanager.cpp \
    $$PWD/codenavigator.cpp \
    $$PWD/pchinfo.cpp

HEADERS += \
    clangwrapper_global.h \
    $$PWD/clangwrapper.h \
    $$PWD/sourcemarker.h \
    $$PWD/rawlexer.h \
    $$PWD/keywords.h \
    $$PWD/token.h \
    $$PWD/constants.h \
    $$PWD/reuse.h \
    $$PWD/indexer.h \
    $$PWD/indexedsymbolinfo.h \
    $$PWD/database.h \
    $$PWD/cxraii.h \
    $$PWD/sourcelocation.h \
    $$PWD/unit.h \
    $$PWD/typedefs.h \
    $$PWD/liveunitsmanager.h \
    $$PWD/codenavigator.h \
    $$PWD/pchinfo.h
