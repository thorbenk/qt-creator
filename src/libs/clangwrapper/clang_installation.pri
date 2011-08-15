win32 {
    INCLUDEPATH += C:/SCM/LLVM-project/install/include
    LIBS += -lC:/SCM/LLVM-project/install/lib/libclang
} else {
    LLVM_CONFIG=llvm-config

    LLVM_BUILD_MODE = $$system($$LLVM_CONFIG --build-mode)

    INCLUDEPATH += $$system($$LLVM_CONFIG --includedir)
    LLVM_LIBDIR = $$system($$LLVM_CONFIG --libdir)

    contains(LLVM_BUILD_MODE, ".*\\bDebug\\b.*") {
        message("LLVM was build in debug mode")
        LIBS += -L$$LLVM_LIBDIR -lclang
        macx:QMAKE_POST_LINK=install_name_tool -change @rpath/libclang.dylib \'$$LLVM_LIBDIR/libclang.dylib\' \'$$IDE_LIBRARY_PATH/libClangWrapper_debug.dylib\'
    } else {
        message("LLVM was build in release mode")
        LIBS += -L$$LLVM_LIBDIR -llibclang
        macx:QMAKE_POST_LINK=install_name_tool -change @executable_path/../lib/liblibclang.3.0.dylib \'$$LLVM_LIBDIR/liblibclang.3.0.dylib\' \'$$IDE_LIBRARY_PATH/libClangWrapper_debug.dylib\'
    }
}

