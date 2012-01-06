#ifndef CPPTOOLS_CLANGUTILS_H
#define CPPTOOLS_CLANGUTILS_H

#include "cpptools_global.h"

#include <clangwrapper/typedefs.h>

#include <cplusplus/ModelManagerInterface.h>

namespace CppTools {
namespace ClangUtils {

CPPTOOLS_EXPORT Clang::UnsavedFiles createUnsavedFiles(const CPlusPlus::CppModelManagerInterface::WorkingCopy &workingCopy);

} // namespace ClangUtils
} // namespace CppTools

#endif // CPPTOOLS_CLANGUTILS_H
