#ifndef CPPTOOLS_CLANGUTILS_H
#define CPPTOOLS_CLANGUTILS_H

#include "utils.h"

#include <cpptools/ModelManagerInterface.h>

namespace ClangCodeModel {
namespace Utils {

ClangCodeModel::Internal::UnsavedFiles createUnsavedFiles(CPlusPlus::CppModelManagerInterface::WorkingCopy workingCopy);

QStringList createClangOptions(const CPlusPlus::CppModelManagerInterface::ProjectPart::Ptr &pPart, const QString &fileName = QString());
QStringList createClangOptions(const CPlusPlus::CppModelManagerInterface::ProjectPart::Ptr &pPart, bool isObjectiveC, bool isHeader);
QStringList createClangOptions(CPlusPlus::CppModelManagerInterface::ProjectPart::Language lang,
                               bool isObjC,
                               bool isHeader,
                               CPlusPlus::CppModelManagerInterface::ProjectPart::QtVersion qtVersion,
                               const QList<QByteArray> &defines,
                               const QStringList &includePaths,
                               const QStringList &frameworkPaths);
QStringList clangNonProjectFileOptions();
QStringList clangLanguageOption(bool cxxEnabled, bool isHeader, bool isObjC);
QStringList createPCHInclusionOptions(const QStringList &pchFiles);
QStringList createPCHInclusionOptions(const QString &pchFile);

} // namespace Utils
} // namespace Clang

#endif // CPPTOOLS_CLANGUTILS_H
