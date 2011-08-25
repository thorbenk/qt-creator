# @TODO...
win32 {
    LLVM = $$(LLVM_INSTALL_DIR)
    INCLUDEPATH += $$LLVM/include
    LIBS += -L$$LLVM/lib \
        -llibclang \
        -lclangLex \
        -lclangBasic \
        -lclangCodeGen \
        -lclangAnalysis \
        -lclangRewrite \
        -lclangSema \
        -lclangDriver \
        -lclangAST \
        -lclangParse \
        -lLLVMCore \
        -lLLVMSupport

    LIBS += -ladvapi32 -lshell32
} else {
    LLVM_CONFIG=llvm-config

    LLVM_BUILD_MODE = $$system($$LLVM_CONFIG --build-mode)

    INCLUDEPATH += $$system($$LLVM_CONFIG --includedir)
    LLVM_LIBDIR = $$system($$LLVM_CONFIG --libdir)

    contains(LLVM_BUILD_MODE, ".*\\bDebug\\b.*") {
        message("LLVM was build in debug mode")
        LIBS += -L$$LLVM_LIBDIR \
            -lclang \
            -lclangLex \
            -lclangBasic \
            -lclangCodeGen \
            -lclangAnalysis \
            -lclangRewrite \
            -lclangSema \
            -lclangDriver \
            -lclangAST \
            -lclangParse \
            -lLLVMCore \
            -lLLVMSupport

        macx:QMAKE_POST_LINK=install_name_tool -change @rpath/libclang.dylib \'$$LLVM_LIBDIR/libclang.dylib\' \'$$IDE_LIBRARY_PATH/libClangWrapper_debug.dylib\'
    } else {
        message("LLVM was build in release mode")
        LIBS += -L$$LLVM_LIBDIR \
            -llibclang \
            -llibclangLex \
            -llibclangBasic \
            -llibclangCodeGen \
            -llibclangAnalysis \
            -llibclangRewrite \
            -llibclangSema \
            -llibclangDriver \
            -llibclangAST \
            -llibclangParse \
            -llibLLVMCore \
            -llibLLVMSupport

        macx:QMAKE_POST_LINK=install_name_tool -change @executable_path/../lib/liblibclang.3.0.dylib \'$$LLVM_LIBDIR/liblibclang.3.0.dylib\' \'$$IDE_LIBRARY_PATH/libClangWrapper_debug.dylib\'
    }

    DEFINES += __STDC_LIMIT_MACROS __STDC_CONSTANT_MACROS
}

