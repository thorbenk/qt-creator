include(clang_installation.pri)

LIBS += $$LLVM_LIBS
INCLUDEPATH += $$LLVM_INCLUDEPATH
DEFINES += $$LLVM_DEFINES

macx: QMAKE_LFLAGS += -Wl,-rpath,\'$$LLVM_LIBDIR\'


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
    $$PWD/symbol.cpp \
    $$PWD/sourcelocation.cpp \
    $$PWD/unit.cpp \
    $$PWD/liveunitsmanager.cpp \
    $$PWD/codenavigator.cpp \
    $$PWD/pchinfo.cpp \
    $$PWD/unitsetup.cpp \
    $$PWD/index.cpp \
    $$PWD/includetracker.cpp \
    $$PWD/dependencygraph.cpp

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
    $$PWD/symbol.h \
    $$PWD/cxraii.h \
    $$PWD/sourcelocation.h \
    $$PWD/unit.h \
    $$PWD/typedefs.h \
    $$PWD/liveunitsmanager.h \
    $$PWD/codenavigator.h \
    $$PWD/pchinfo.h \
    $$PWD/unitsetup.h \
    $$PWD/index.h \
    $$PWD/includetracker.h \
    $$PWD/dependencygraph.h
