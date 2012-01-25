#ifndef CPPTOOLS_CLANGUTILS_H
#define CPPTOOLS_CLANGUTILS_H

#include "cpptools_global.h"

#include <clangwrapper/utils.h>

#include <cplusplus/ModelManagerInterface.h>

namespace CppTools {
namespace ClangUtils {

CPPTOOLS_EXPORT Clang::UnsavedFiles createUnsavedFiles(const CPlusPlus::CppModelManagerInterface::WorkingCopy &workingCopy);

CPPTOOLS_EXPORT QStringList createClangOptions(const CPlusPlus::CppModelManagerInterface::ProjectPart::Ptr &pPart);
CPPTOOLS_EXPORT QStringList createClangOptions(bool useCpp0x,
                                               bool useObjc,
                                               CPlusPlus::CppModelManagerInterface::ProjectPart::QtVersion qtVersion,
                                               const QStringList &precompiledHeaders,
                                               const QList<QByteArray> &defines,
                                               const QStringList &includePaths,
                                               const QStringList &frameworkPaths);

} // namespace ClangUtils
} // namespace CppTools

#endif // CPPTOOLS_CLANGUTILS_H
