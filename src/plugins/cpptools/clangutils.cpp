#include "clangutils.h"

#include <editormanager/editormanager.h>
#include <editormanager/ieditor.h>
#include <coreplugin/icore.h>

#include <QFile>
#include <QSet>
#include <QString>

using namespace Clang;
using namespace Core;

CPPTOOLS_EXPORT ClangWrapper::UnsavedFiles CppTools::ClangUtils::createUnsavedFiles(const CPlusPlus::CppModelManagerInterface::WorkingCopy &workingCopy)
{
    // TODO: change the modelmanager to hold one working copy, and amend it every time we ask for one.
    // TODO: Reason: the UnsavedFile needs a QByteArray.

    ICore *core = ICore::instance(); // FIXME
    QSet<QString> openFiles;
    foreach (IEditor *editor, core->editorManager()->openedEditors())
        openFiles.insert(editor->file()->fileName());

    QList<ClangWrapper::UnsavedFile> result;
    QHashIterator<QString, QPair<QString, unsigned> > wcIter = workingCopy.iterator();
    while (wcIter.hasNext()) {
        wcIter.next();
        const QString &fileName = wcIter.key();
        if (openFiles.contains(fileName) && QFile(fileName).exists())
            result.append(ClangWrapper::UnsavedFile(fileName, wcIter.value().first.toUtf8()));
    }

    return result;
}
