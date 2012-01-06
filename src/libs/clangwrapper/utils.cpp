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

#include "unit.h"
#include "utils.h"
#include "utils_p.h"

#include <clang-c/Index.h>

#include <QMutex>
#include <QMutexLocker>

namespace Clang {

QPair<bool, QStringList> precompile(const QString &headerFileName,
                                    const QStringList &options,
                                    const QString &outFileName)
{
    bool ok = false;

    Internal::Unit unit(headerFileName);
    unit.setCompilationOptions(options);

    unsigned parseOpts = clang_defaultEditingTranslationUnitOptions();
    parseOpts |= CXTranslationUnit_Incomplete;
    parseOpts &= ~CXTranslationUnit_CacheCompletionResults;
    unit.setManagementOptions(parseOpts);

    unit.parse();
    if (unit.isLoaded())
        ok = CXSaveError_None == unit.save(outFileName);

    return qMakePair(ok, Internal::formattedDiagnostics(unit));
}

namespace {
static bool clangInitialised = false;
static QMutex initialisationMutex;
}

void initializeClang()
{
    if (clangInitialised)
        return;

    QMutexLocker locker(&initialisationMutex);
    if (clangInitialised)
        return;

    clang_toggleCrashRecovery(1);
    clang_enableStackTraces();
    clangInitialised = true;

    qRegisterMetaType<Clang::Diagnostic>();
    qRegisterMetaType<QList<Clang::Diagnostic> >();
}

} // namespace Clang

