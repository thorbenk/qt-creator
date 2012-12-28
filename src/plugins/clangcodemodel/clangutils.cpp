#include "clangutils.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/icore.h>

#include <QFile>
#include <QSet>
#include <QString>

using namespace ClangCodeModel;
using namespace Core;

UnsavedFiles ClangCodeModel::Utils::createUnsavedFiles(CPlusPlus::CppModelManagerInterface::WorkingCopy workingCopy)
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

/**
 * @brief Creates list of command-line arguments required for correct parsing
 * @param pPart - null if file isn't part of any project
 * @param fileName - path to file, source isn't ObjC if name is empty
 */
QStringList ClangCodeModel::Utils::createClangOptions(const CPlusPlus::CppModelManagerInterface::ProjectPart::Ptr &pPart, const QString &fileName)
{
    if (pPart.isNull())
        return clangNonProjectFileOptions();

    const bool isObjC = fileName.isEmpty() ? false : pPart->objcSourceFiles.contains(fileName);

    return createClangOptions(pPart, isObjC);
}

/**
 * @brief Creates list of command-line arguments required for correct parsing
 * @param pPart - null if file isn't part of any project
 * @param isObjectiveC - file is ObjectiveC or ObjectiveC++
 */
QStringList ClangCodeModel::Utils::createClangOptions(const CPlusPlus::CppModelManagerInterface::ProjectPart::Ptr &pPart, bool isObjectiveC)
{
    if (pPart.isNull())
        return clangNonProjectFileOptions();

    return createClangOptions(pPart->language,
                              isObjectiveC,
                              pPart->qtVersion,
                              pPart->defines.split('\n'),
                              pPart->includePaths,
                              pPart->frameworkPaths);
}


QStringList ClangCodeModel::Utils::createClangOptions(CPlusPlus::CppModelManagerInterface::ProjectPart::Language lang,
                                                      bool isObjC,
                                                      CPlusPlus::CppModelManagerInterface::ProjectPart::QtVersion qtVersion,
                                                      const QList<QByteArray> &defines,
                                                      const QStringList &includePaths,
                                                      const QStringList &frameworkPaths)
{
    QStringList result;

    switch (lang) {
    case CPlusPlus::CppModelManagerInterface::ProjectPart::C89:
        result << QLatin1String("-std=gnu89");
        if (isObjC)
            result << ClangCodeModel::Utils::clangOptionForObjC(false);
        break;
    case CPlusPlus::CppModelManagerInterface::ProjectPart::C99:
        result << QLatin1String("-std=gnu99");
        if (isObjC)
            result << ClangCodeModel::Utils::clangOptionForObjC(false);
        break;
    case CPlusPlus::CppModelManagerInterface::ProjectPart::CXX:
        result << QLatin1String("-std=gnu++98");
        if (isObjC)
            result << ClangCodeModel::Utils::clangOptionForObjC(true);
        break;
    case CPlusPlus::CppModelManagerInterface::ProjectPart::CXX11:
        result << QLatin1String("-std=c++11");
        if (isObjC)
            result << ClangCodeModel::Utils::clangOptionForObjC(true);
        break;
    default:
        break;
    }

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

QStringList ClangCodeModel::Utils::clangNonProjectFileOptions()
{
    return QStringList() << QLatin1String("-std=c++11");
}

QString ClangCodeModel::Utils::clangOptionForObjC(bool cxxEnabled)
{
    if (cxxEnabled)
        return QLatin1String("-ObjC++");
    else
        return QLatin1String("-ObjC");
}
