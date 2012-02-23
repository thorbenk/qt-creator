TEMPLATE = lib
TARGET = Clang

include(../../qtcreatorplugin.pri)
include(../../plugins/coreplugin/coreplugin.pri)
include(../../libs/utils/utils.pri)
include(../../plugins/cpptools/cpptools.pri)
include(../../plugins/texteditor/texteditor.pri)
include(clang_installation.pri)

message("Building with Clang from $$LLVM_INSTALL_DIR")

LIBS += $$LLVM_LIBS
INCLUDEPATH += $$LLVM_INCLUDEPATH
DEFINES += $$LLVM_DEFINES
DEFINES += CLANG_LIBRARY

macx: QMAKE_LFLAGS += -Wl,-rpath,\'$$LLVM_LIBDIR\'

contains(DEFINES, CLANG_COMPLETION) {
    HEADERS += clangcompletion.h clangcompleter.h
    SOURCES += clangcompletion.cpp clangcompleter.cpp
}

contains(DEFINES, CLANG_HIGHLIGHTING) {
    HEADERS += cppcreatemarkers.h clanghighlightingsupport.h
    SOURCES += cppcreatemarkers.cpp clanghighlightingsupport.cpp
}

HEADERS += clangutils.h

SOURCES += clangutils.cpp

SOURCES += \
    $$PWD/clangplugin.cpp \
    $$PWD/sourcemarker.cpp \
    $$PWD/token.cpp \
    $$PWD/symbol.cpp \
    $$PWD/sourcelocation.cpp \
    $$PWD/unit.cpp \
    $$PWD/utils.cpp \
    $$PWD/utils_p.cpp \
    $$PWD/liveunitsmanager.cpp \
    $$PWD/index.cpp \
    $$PWD/semanticmarker.cpp \
    $$PWD/diagnostic.cpp \
    $$PWD/unsavedfiledata.cpp

HEADERS += \
    $$PWD/clangplugin.h \
    $$PWD/clang_global.h \
    $$PWD/sourcemarker.h \
    $$PWD/token.h \
    $$PWD/constants.h \
    $$PWD/symbol.h \
    $$PWD/cxraii.h \
    $$PWD/sourcelocation.h \
    $$PWD/unit.h \
    $$PWD/utils.h \
    $$PWD/utils_p.h \
    $$PWD/liveunitsmanager.h \
    $$PWD/index.h \
    $$PWD/semanticmarker.h \
    $$PWD/diagnostic.h \
    $$PWD/unsavedfiledata.h

contains(DEFINES, CLANG_LEXER) {
    HEADERS += \
        $$PWD/rawlexer.h \
        $$PWD/keywords.h \
        $$PWD/includetracker.h \
        $$PWD/dependencygraph.h \
        $$PWD/indexer.h \
        $$PWD/codenavigator.h \
        $$PWD/unitsetup.h
    SOURCES += \
        $$PWD/rawlexer.cpp \
        $$PWD/keywords.cpp \
        $$PWD/includetracker.cpp \
        $$PWD/dependencygraph.cpp \
        $$PWD/indexer.cpp \
        $$PWD/codenavigator.cpp \
        $$PWD/unitsetup.cpp
}
