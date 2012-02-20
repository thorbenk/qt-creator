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

#ifndef CODENAVIGATOR_H
#define CODENAVIGATOR_H

#include "clang_global.h"
#include "sourcelocation.h"

#include <clang-c/Index.h>

#include <QtCore/QScopedPointer>

namespace Clang {

class Indexer;

namespace Internal {
class UnitSetup;
}

class CLANG_EXPORT CodeNavigator
{
public:
    CodeNavigator();
    ~CodeNavigator();

    void setup(const QString &fileName, Indexer *indexer);

    SourceLocation followItem(unsigned line, unsigned column) const;
    SourceLocation switchDeclarationDefinition(unsigned line, unsigned column) const;

private:
    SourceLocation findDefinition(const CXCursor &cursor,
                                  CXCursorKind cursorKind) const;
    SourceLocation findInclude(const CXCursor &cursor) const;
    CXCursor getCursor(unsigned line, unsigned column) const;

    QScopedPointer<Internal::UnitSetup> m_setup;
};

} // Clang

#endif // CODENAVIGATOR_H
