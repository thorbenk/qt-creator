#include "clangutils.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/icore.h>

#include <QFile>
#include <QSet>
#include <QString>

using namespace Clang;
using namespace Core;

CLANG_EXPORT UnsavedFiles Clang::Utils::createUnsavedFiles(const CPlusPlus::CppModelManagerInterface::WorkingCopy &workingCopy)
{
    // TODO: change the modelmanager to hold one working copy, and amend it every time we ask for one.
    // TODO: Reason: the UnsavedFile needs a QByteArray.

    ICore *core = ICore::instance(); // FIXME
    QSet<QString> openFiles;
    foreach (IEditor *editor, core->editorManager()->openedEditors())
        openFiles.insert(editor->document()->fileName());

    UnsavedFiles result;
    QHashIterator<QString, QPair<QString, unsigned> > wcIter = workingCopy.iterator();
    while (wcIter.hasNext()) {
        wcIter.next();
        const QString &fileName = wcIter.key();
        if (openFiles.contains(fileName) && QFile(fileName).exists())
            result.insert(fileName, wcIter.value().first.toUtf8());
    }

    return result;
}

QStringList Clang::Utils::createClangOptions(const CPlusPlus::CppModelManagerInterface::ProjectPart::Ptr &pPart)
{
    Q_ASSERT(pPart);

    return createClangOptions(pPart->cpp0xEnabled(),
                              pPart->objcEnabled(),
                              pPart->qtVersion,
                              pPart->defines.split('\n'),
                              pPart->includePaths,
                              pPart->frameworkPaths);
}

QStringList Clang::Utils::createClangOptions(bool useCpp0x,
                                                     bool useObjc,
                                                     CPlusPlus::CppModelManagerInterface::ProjectPart::QtVersion qtVersion,
                                                     const QList<QByteArray> &defines,
                                                     const QStringList &includePaths,
                                                     const QStringList &frameworkPaths)
{
    QStringList result;

    if (useCpp0x)
        result << QLatin1String("-std=c++0x");

    if (useObjc)
        result << QLatin1String("-xobjective-c++");
    else
        result << QLatin1String("-xc++");

    static const QString injectedHeader(Core::ICore::instance()->resourcePath() + QLatin1String("/cplusplus/qt%1-qobjectdefs-injected.h"));
    if (qtVersion == CPlusPlus::CppModelManagerInterface::ProjectPart::Qt4)
        result << QLatin1String("-include") << injectedHeader.arg(QLatin1Char('4'));
    if (qtVersion == CPlusPlus::CppModelManagerInterface::ProjectPart::Qt5)
        result << QLatin1String("-include") << injectedHeader.arg(QLatin1Char('5'));

#ifdef _MSC_VER
    result << QLatin1String("-fms-extensions")
           << QLatin1String("-fdelayed-template-parsing");
#endif

    result << QLatin1String("-nobuiltininc");

    foreach (QByteArray def, defines) {
        if (def.isEmpty())
            continue;

        //### FIXME: the next 3 check shouldn't be needed: we probably don't want to get the compiler-defined defines in.
        if (!def.startsWith("#define "))
            continue;
        if (def.startsWith("#define _"))
            continue;
        if (def.startsWith("#define OBJC_NEW_PROPERTIES"))
            continue;

        QByteArray str = def.mid(8);
        int spaceIdx = str.indexOf(' ');
        QString arg;
        if (spaceIdx != -1) {
            arg = QLatin1String("-D" + str.left(spaceIdx) + "=" + str.mid(spaceIdx + 1));
        } else {
            arg = QLatin1String("-D" + str);
        }
        arg = arg.replace("\\\"", "\"");
        arg = arg.replace("\"", "");
        if (!result.contains(arg))
            result.append(arg);
    }
    foreach (const QString &frameworkPath, frameworkPaths)
        result.append(QLatin1String("-F") + frameworkPath);
    foreach (const QString &inc, includePaths)
        if (!inc.isEmpty())
            result << ("-I" + inc);

#if 0
    qDebug() << "--- m_args:";
    foreach (const QString &arg, result)
        qDebug() << "\t" << qPrintable(arg);
    qDebug() << "---";
#endif

    return result;
}
