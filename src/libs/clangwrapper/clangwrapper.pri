include(clang_installation.pri)
INCLUDEPATH += $$LLVM_INCLUDEPATH $$PWD/..
LIBS *= -l$$qtLibraryName(ClangWrapper)
