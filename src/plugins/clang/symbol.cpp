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

#include "symbol.h"

using namespace Clang;

Symbol::Symbol()
    : m_kind(Unknown)
{}

Symbol::Symbol(const QString &name,
               const QString &qualification,
               Kind type,
               const SourceLocation &location)
    : m_name(name)
    , m_qualification(qualification)
    , m_location(location)
    , m_kind(type)
{}

namespace Clang {

QDataStream &operator<<(QDataStream &stream, const Symbol &symbol)
{
    stream << symbol.m_name
           << symbol.m_qualification
           << symbol.m_location.fileName()
           << (quint32)symbol.m_location.line()
           << (quint16)symbol.m_location.column()
           << (quint32)symbol.m_location.offset()
           << (qint8)symbol.m_kind;

    return stream;
}

QDataStream &operator>>(QDataStream &stream, Symbol &symbol)
{
    QString fileName;
    quint32 line;
    quint16 column;
    quint32 offset;
    quint8 kind;
    stream >> symbol.m_name
           >> symbol.m_qualification
           >> fileName
           >> line
           >> column
           >> offset
           >> kind;
    symbol.m_location = SourceLocation(fileName, line, column, offset);
    symbol.m_kind = Symbol::Kind(kind);

    return stream;
}

bool operator==(const Symbol &a, const Symbol &b)
{
    return a.m_name == b.m_name
            && a.m_qualification == b.m_qualification
            && a.m_location == b.m_location
            && a.m_kind == b.m_kind;
}

bool operator!=(const Symbol &a, const Symbol &b)
{
    return !(a == b);
}

} // Clang
