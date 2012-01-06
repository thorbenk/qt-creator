/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
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
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#ifndef CLANG_DIAGNOSTIC_H
#define CLANG_DIAGNOSTIC_H

#include "clangwrapper_global.h"
#include "sourcelocation.h"

namespace Clang {

class QTCREATOR_CLANGWRAPPER_EXPORT Diagnostic
{
public:
    enum Severity {
        Unknown = -1,
        Ignored = 0,
        Note = 1,
        Warning = 2,
        Error = 3,
        Fatal = 4
    };

public:
    Diagnostic();
    Diagnostic(Severity severity, const SourceLocation &location, unsigned length, const QString &spelling);

    Severity severity() const
    { return m_severity; }

    const QString &severityAsString() const;

    const SourceLocation &location() const
    { return m_loc; }

    unsigned length() const
    { return m_length; }

    const QString &spelling() const
    { return m_spelling; }

private:
    Severity m_severity;
    SourceLocation m_loc;
    unsigned m_length;
    QString m_spelling;
};

} // namespace Clang

#endif // CLANG_DIAGNOSTIC_H
