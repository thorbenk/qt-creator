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

#ifndef INDEXEDSYMBOLINFO_H
#define INDEXEDSYMBOLINFO_H

#include "sourcelocation.h"

#include <QtCore/QString>
#include <QtCore/QDataStream>

namespace Clang {

class Symbol
{
public:
    enum Kind {
        Enum,
        Class,
        Method,       // A member-function.
        Function,     // A free-function (global or within a namespace).
        Declaration,
        Constructor,
        Destructor,
        Unknown
    };

    Symbol();
    Symbol(const QString &name,
           const QString &qualification,
           Kind type,
           const SourceLocation &location);

    QString m_name;
    QString m_qualification;
    SourceLocation m_location;
    Kind m_kind;
};

QDataStream &operator<<(QDataStream &stream, const Symbol &symbol);
QDataStream &operator>>(QDataStream &stream, Symbol &symbol);

bool operator==(const Symbol &a, const Symbol &b);
bool operator!=(const Symbol &a, const Symbol &b);

} // Clang

#endif // INDEXEDSYMBOLINFO_H
