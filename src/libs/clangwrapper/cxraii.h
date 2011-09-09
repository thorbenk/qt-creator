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

#ifndef CXRAII_H
#define CXRAII_H

#include <clang-c/Index.h>

// Simple RAII types for their CX correspondings

namespace Clang {
namespace Internal {

template <class CXType_T>
struct ScopedCXType
{
protected:
    typedef void (*DisposeFunction)(CXType_T);

    ScopedCXType(const CXType_T cx, DisposeFunction f)
        : m_cx(cx) , m_disposeFunction(f)
    {}

public:
    ~ScopedCXType()
    {
        if (m_cx)
            m_disposeFunction(m_cx);
    }

    operator CXType_T() const { return m_cx; }
    bool operator!() const { return !m_cx; }

private:
    CXType_T m_cx;
    DisposeFunction m_disposeFunction;
};

struct ScopedCXIndex : ScopedCXType<CXIndex>
{
    ScopedCXIndex(const CXIndex &index)
        : ScopedCXType<CXIndex>(index, &clang_disposeIndex)
    {}
};

struct ScopedCXTranslationUnit : ScopedCXType<CXTranslationUnit>
{
    ScopedCXTranslationUnit(const CXTranslationUnit &unit)
        : ScopedCXType<CXTranslationUnit>(unit, &clang_disposeTranslationUnit)
    {}
};

struct ScopedCXDiagnostic : ScopedCXType<CXDiagnostic>
{
    ScopedCXDiagnostic(const CXDiagnostic &diagnostic)
        : ScopedCXType<CXDiagnostic>(diagnostic, &clang_disposeDiagnostic)
    {}
};

} // Internal
} // Clang

#endif // CXRAII_H
