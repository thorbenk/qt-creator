include(clang_installation.pri)

!isEmpty(LLVM_INSTALL_DIR) {
    message("Building with Clang from $$LLVM_INSTALL_DIR")

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
        $$PWD/clangcompleter.cpp \
        $$PWD/sourcemarker.cpp \
        $$PWD/rawlexer.cpp \
        $$PWD/keywords.cpp \
        $$PWD/token.cpp \
        $$PWD/indexer.cpp \
        $$PWD/symbol.cpp \
        $$PWD/sourcelocation.cpp \
        $$PWD/unit.cpp \
        $$PWD/utils.cpp \
        $$PWD/utils_p.cpp \
        $$PWD/liveunitsmanager.cpp \
        $$PWD/codenavigator.cpp \
        $$PWD/unitsetup.cpp \
        $$PWD/index.cpp \
        $$PWD/includetracker.cpp \
        $$PWD/dependencygraph.cpp \
        $$PWD/semanticmarker.cpp \
        $$PWD/diagnostic.cpp \
        $$PWD/unsavedfiledata.cpp

    HEADERS += \
        $$PWD/clangwrapper_global.h \
        $$PWD/clangcompleter.h \
        $$PWD/sourcemarker.h \
        $$PWD/rawlexer.h \
        $$PWD/keywords.h \
        $$PWD/token.h \
        $$PWD/constants.h \
        $$PWD/indexer.h \
        $$PWD/symbol.h \
        $$PWD/cxraii.h \
        $$PWD/sourcelocation.h \
        $$PWD/unit.h \
        $$PWD/utils.h \
        $$PWD/utils_p.h \
        $$PWD/liveunitsmanager.h \
        $$PWD/codenavigator.h \
        $$PWD/unitsetup.h \
        $$PWD/index.h \
        $$PWD/includetracker.h \
        $$PWD/dependencygraph.h \
        $$PWD/semanticmarker.h \
        $$PWD/diagnostic.h \
        $$PWD/unsavedfiledata.h
}
