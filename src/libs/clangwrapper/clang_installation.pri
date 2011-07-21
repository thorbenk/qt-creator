win32 {
INCLUDEPATH += C:/SCM/LLVM-project/install/include
LIBS += -lC:/SCM/LLVM-project/install/lib/libclang
} else {
LLVM_CONFIG=llvm-config

INCLUDEPATH += $$system($$LLVM_CONFIG --includedir)
LLVM_LIBDIR = $$system($$LLVM_CONFIG --libdir)
LIBS += -L$$LLVM_LIBDIR -llibclang

macx:QMAKE_POST_LINK=install_name_tool -change @executable_path/../lib/liblibclang.3.0.dylib \'$$LLVM_LIBDIR/liblibclang.3.0.dylib\' \
    \'$$IDE_LIBRARY_PATH/libClangWrapper_debug.dylib\'
}
