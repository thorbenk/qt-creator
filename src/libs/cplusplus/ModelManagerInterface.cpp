/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at info@qt.nokia.com.
**
**************************************************************************/

#include "ModelManagerInterface.h"

using namespace CPlusPlus;

QStringList CppModelManagerInterface::ProjectPart::createClangOptions() const
{
    QStringList pchs;
    bool cpp0x, objc;
    if (clangPCH && !clangPCH->fileName().isEmpty()) {
        pchs << clangPCH->fileName();
        cpp0x = clangPCH->cpp0x();
        objc = clangPCH->objc();
    } else {
        cpp0x = cpp0xEnabled();
        objc = objcEnabled();
    }

    return createClangOptions(cpp0x,
                              objc,
                              pchs,
                              defines.split('\n'),
                              includePaths,
                              frameworkPaths);
}

QStringList CppModelManagerInterface::ProjectPart::createClangOptions(bool useCpp0x,
                                                                      bool useObjc,
                                                                      const QStringList &precompiledHeaders,
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

#ifdef Q_OS_WIN
    result << QLatin1String("-fms-extensions")
           << QLatin1String("-fdelayed-template-parsing");
#endif

    foreach (const QString &pch, precompiledHeaders) {
        if (result.contains(pch))
            continue;
        if (QFile(pch).exists())
            result << QLatin1String("-include-pch") << pch;
    }
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
#ifdef Q_OS_MAC
            if (!inc.contains("i686-apple-darwin"))
#endif // Q_OS_MAC
                result << ("-I" + inc);

#if 0
    qDebug() << "--- m_args:";
    foreach (const QString &arg, result)
        qDebug() << "\t" << qPrintable(arg);
    qDebug() << "---";
#endif

    return result;
}



static CppModelManagerInterface *g_instance = 0;

CppModelManagerInterface::CppModelManagerInterface(QObject *parent)
    : QObject(parent)
{
    Q_ASSERT(! g_instance);
    g_instance = this;
}

CppModelManagerInterface::~CppModelManagerInterface()
{
    Q_ASSERT(g_instance == this);
    g_instance = 0;
}

CppModelManagerInterface *CppModelManagerInterface::instance()
{
    return g_instance;
}

