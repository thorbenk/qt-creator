include(clang_installation.pri)

!isEmpty(LLVM_INSTALL_DIR) {
    INCLUDEPATH += $$LLVM_INCLUDEPATH $$PWD/..
    LIBS *= -l$$qtLibraryName(ClangWrapper)
}
