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

#include "reuse.h"

namespace Clang {
namespace Internal {

QString getQString(const CXString &cxString, bool disposeCXString)
{
    QString s = QString::fromUtf8(clang_getCString(cxString));
    if (disposeCXString)
        clang_disposeString(cxString);
    return s;
}

namespace {

SourceLocation getLocation(const CXSourceLocation &loc,
                           void (*clangFunction)(CXSourceLocation,
                                                 CXFile *,
                                                 unsigned *,
                                                 unsigned *,
                                                 unsigned *))
{
    CXFile file;
    unsigned line, column, offset;
    (*clangFunction)(loc, &file, &line, &column, &offset);
    return SourceLocation(getQString(clang_getFileName(file)), line, column, offset);
}

} // Anonymous

SourceLocation getInstantiationLocation(const CXSourceLocation &loc)
{
    return getLocation(loc, &clang_getInstantiationLocation);
}

SourceLocation getSpellingLocation(const CXSourceLocation &loc)
{
    return getLocation(loc, &clang_getSpellingLocation);
}

} // Internal
} // Clang

