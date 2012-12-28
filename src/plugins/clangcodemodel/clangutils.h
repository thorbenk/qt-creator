#ifndef CPPTOOLS_CLANGUTILS_H
#define CPPTOOLS_CLANGUTILS_H

#include "clang_global.h"
#include "utils.h"

#include <cpptools/ModelManagerInterface.h>

namespace ClangCodeModel {
namespace Utils {

CLANG_EXPORT ClangCodeModel::UnsavedFiles createUnsavedFiles(CPlusPlus::CppModelManagerInterface::WorkingCopy workingCopy);

CLANG_EXPORT QStringList createClangOptions(const CPlusPlus::CppModelManagerInterface::ProjectPart::Ptr &pPart, const QString &fileName = QString());
CLANG_EXPORT QStringList createClangOptions(const CPlusPlus::CppModelManagerInterface::ProjectPart::Ptr &pPart, bool isObjectiveC);
CLANG_EXPORT QStringList createClangOptions(CPlusPlus::CppModelManagerInterface::ProjectPart::Language lang,
                                            bool isObjC,
                                            CPlusPlus::CppModelManagerInterface::ProjectPart::QtVersion qtVersion,
                                            const QList<QByteArray> &defines,
                                            const QStringList &includePaths,
                                            const QStringList &frameworkPaths);

QStringList clangNonProjectFileOptions();
QString clangOptionForObjC(bool cxxEnabled);

} // namespace Utils
} // namespace Clang

#endif // CPPTOOLS_CLANGUTILS_H
