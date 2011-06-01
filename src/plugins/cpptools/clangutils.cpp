#include "clangutils.h"

#include <QFile>

using namespace Clang;

CPPTOOLS_EXPORT ClangWrapper::UnsavedFiles CppTools::ClangUtils::createUnsavedFiles(const CPlusPlus::CppModelManagerInterface::WorkingCopy &workingCopy)
{
    // TODO: change the modelmanager to hold one working copy, and amend it every time we ask for one.
    // TODO: Reason: the UnsavedFile needs a QByteArray.

    QList<ClangWrapper::UnsavedFile> result;
    QHashIterator<QString, QPair<QString, unsigned> > wcIter = workingCopy.iterator();
    while (wcIter.hasNext()) {
        wcIter.next();
        if (QFile(wcIter.key()).exists())
            result.append(ClangWrapper::UnsavedFile(wcIter.key(), wcIter.value().first.toUtf8()));
    }

    return result;
}
