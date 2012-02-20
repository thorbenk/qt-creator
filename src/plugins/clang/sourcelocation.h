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

#ifndef SOURCELOCATION_H
#define SOURCELOCATION_H

#include "clang_global.h"

#include <QtCore/QString>
#include <QtCore/QDebug>

namespace Clang {

class CLANG_EXPORT SourceLocation
{
public:
    SourceLocation();
    SourceLocation(const QString &fileName,
                   unsigned line = 0,
                   unsigned column = 0,
                   unsigned offset = 0);

    bool isNull() const { return m_fileName.isEmpty(); }
    const QString &fileName() const { return m_fileName; }
    unsigned line() const { return m_line; }
    unsigned column() const { return m_column; }
    unsigned offset() const { return m_offset; }

private:
    QString m_fileName;
    unsigned m_line;
    unsigned m_column;
    unsigned m_offset;
};

bool operator==(const SourceLocation &a, const SourceLocation &b);
bool operator!=(const SourceLocation &a, const SourceLocation &b);

QDebug operator<<(QDebug dbg, const SourceLocation &location);

} // Clang

#endif // SOURCELOCATION_H
