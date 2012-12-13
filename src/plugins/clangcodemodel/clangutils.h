#ifndef CPPTOOLS_CLANGUTILS_H
#define CPPTOOLS_CLANGUTILS_H

#include "clang_global.h"
#include "utils.h"

#include <cpptools/ModelManagerInterface.h>

namespace ClangCodeModel {
namespace Utils {

CLANG_EXPORT ClangCodeModel::UnsavedFiles createUnsavedFiles(CPlusPlus::CppModelManagerInterface::WorkingCopy workingCopy);

CLANG_EXPORT QStringList createClangOptions(const CPlusPlus::CppModelManagerInterface::ProjectPart::Ptr &pPart);
CLANG_EXPORT QStringList createClangOptions(CPlusPlus::CppModelManagerInterface::ProjectPart::Language language,
                                            CPlusPlus::CppModelManagerInterface::ProjectPart::QtVersion qtVersion,
                                            const QList<QByteArray> &defines,
                                            const QStringList &includePaths,
                                            const QStringList &frameworkPaths);
CLANG_EXPORT QString getClangOptionForObjC(CPlusPlus::CppModelManagerInterface::ProjectPart::Language language);

} // namespace Utils
} // namespace Clang

#endif // CPPTOOLS_CLANGUTILS_H
