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

#ifndef CLANG_SOURCEMARKER_H
#define CLANG_SOURCEMARKER_H

#include "clangwrapper_global.h"
#include "sourcelocation.h"

namespace Clang {

class QTCREATOR_CLANGWRAPPER_EXPORT SourceMarker
{
public:
    enum Kind {
        Unknown = 0,
        Type = 1,
        Local,
        Field,
        Static,
        VirtualMethod,
        Label
    };

    SourceMarker();
    SourceMarker(const SourceLocation &location,
                 unsigned length,
                 Kind kind);

    bool isValid() const
    { return m_loc.line() != 0; }

    bool isInvalid() const
    { return m_loc.line() == 0; }

    const SourceLocation &location() const
    { return m_loc; }

    unsigned length() const
    { return m_length; }

    Kind kind() const
    { return m_kind; }

    bool lessThan(const SourceMarker &other) const
    {
        if (m_loc.line() != other.m_loc.line())
            return m_loc.line() < other.m_loc.line();
        if (m_loc.column() != other.m_loc.column())
            return m_loc.column() < other.m_loc.column();
        return m_length < other.m_length;
    }

private:
    SourceLocation m_loc;
    unsigned m_length;
    Kind m_kind;
};

QTCREATOR_CLANGWRAPPER_EXPORT inline bool operator<(const SourceMarker &one, const SourceMarker &two)
{ return one.lessThan(two); }

} // namespace Clang

#endif // CLANG_SOURCEMARKER_H
