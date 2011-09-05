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

    INCLUDEPATH += $$system($$LLVM_CONFIG --includedir)
    LLVM_LIBDIR = $$system($$LLVM_CONFIG --libdir)

    LIBS += -L$$LLVM_LIBDIR \
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

    exists ($${LLVM_LIBDIR}/libclang.*) {
        message("LLVM was build with autotools")
        CLANG_LIB = clang
    } else {
        exists ($${LLVM_LIBDIR}/liblibclang.*) {
            message("LLVM was build with CMake")
            CLANG_LIB = libclang
        } else {
            error("Cannot find Clang shared library!")
        }
    }

    LIBS += -l$${CLANG_LIB}

    macx:QMAKE_POST_LINK=install_name_tool -change @rpath/lib$${CLANG_LIB}.dylib \'$$LLVM_LIBDIR/lib$${CLANG_LIB}.dylib\' -change @executable_path/../lib/lib$${CLANG_LIB}.3.0.dylib \'$$LLVM_LIBDIR/lib$${CLANG_LIB}.dylib\' \'$$IDE_LIBRARY_PATH/libClangWrapper_debug.dylib\'

    DEFINES += __STDC_LIMIT_MACROS __STDC_CONSTANT_MACROS
}

