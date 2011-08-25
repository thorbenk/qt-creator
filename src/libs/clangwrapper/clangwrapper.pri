win32 {
INCLUDEPATH += $$(LLVM_INSTALL_DIR)/include
} else {
INCLUDEPATH += $$system(llvm-config --includedir)
}
INCLUDEPATH += $$PWD/..
LIBS *= -l$$qtLibraryName(ClangWrapper)
