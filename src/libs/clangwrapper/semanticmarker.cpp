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

#include "semanticmarker.h"
#include "unit.h"
#include "utils_p.h"

using namespace Clang;
using namespace Clang::Internal;

SemanticMarker::SemanticMarker()
{
}

SemanticMarker::~SemanticMarker()
{
}

QString SemanticMarker::fileName() const
{
    if (!m_unit)
        return QString();

    return m_unit->fileName();
}

void SemanticMarker::setFileName(const QString &fileName)
{
    if (this->fileName() == fileName)
        return;

    QStringList oldOptions;
    if (m_unit)
        oldOptions = m_unit->compilationOptions();
    m_unit.reset(new Unit(fileName));
    if (!oldOptions.isEmpty())
        m_unit->setCompilationOptions(oldOptions);

    unsigned clangOpts = clang_defaultEditingTranslationUnitOptions();
    clangOpts |= CXTranslationUnit_Incomplete;
    clangOpts &= ~CXTranslationUnit_CacheCompletionResults;
    m_unit->setManagementOptions(clangOpts);
}

void SemanticMarker::setCompilationOptions(const QStringList &options)
{
    Q_ASSERT(m_unit);

    if (m_unit->compilationOptions() == options)
        return;

    m_unit->setCompilationOptions(options);
}

void SemanticMarker::reparse(const UnsavedFiles &unsavedFiles)
{
    Q_ASSERT(m_unit);

    m_unit->setUnsavedFiles(unsavedFiles);
    if (m_unit->isLoaded())
        m_unit->reparse();
    else
        m_unit->parse();
}

QList<Diagnostic> SemanticMarker::diagnostics() const
{
    QList<Diagnostic> diagnostics;
    if (!m_unit || !m_unit->isLoaded())
        return diagnostics;

    const unsigned diagCount = m_unit->getNumDiagnostics();
    for (unsigned i = 0; i < diagCount; ++i) {
        CXDiagnostic diag = m_unit->getDiagnostic(i);
        Diagnostic::Severity severity = static_cast<Diagnostic::Severity>(clang_getDiagnosticSeverity(diag));
        CXSourceLocation cxLocation = clang_getDiagnosticLocation(diag);
        const QString spelling = Internal::getQString(clang_getDiagnosticSpelling(diag));

        const unsigned rangeCount = clang_getDiagnosticNumRanges(diag);
        if (rangeCount > 0) {
            for (unsigned i = 0; i < rangeCount; ++i) {
                CXSourceRange r = clang_getDiagnosticRange(diag, 0);
                const SourceLocation &spellBegin = Internal::getSpellingLocation(clang_getRangeStart(r));
                const SourceLocation &spellEnd = Internal::getSpellingLocation(clang_getRangeEnd(r));
                unsigned length = spellEnd.offset() - spellBegin.offset();

                Diagnostic d(severity, spellBegin, length, spelling);
                diagnostics.append(d);
            }
        } else {
            const SourceLocation &location = Internal::getExpansionLocation(cxLocation);
            Diagnostic d(severity, location, 0, spelling);
            diagnostics.append(d);
        }

        clang_disposeDiagnostic(diag);
    }

    return diagnostics;
}

namespace {
static void add(QList<SourceMarker> &markers,
                const CXSourceRange &extent,
                SourceMarker::Kind kind)
{
    CXSourceLocation start = clang_getRangeStart(extent);
    CXSourceLocation end = clang_getRangeEnd(extent);
    const SourceLocation &location = Internal::getExpansionLocation(start);
    const SourceLocation &locationEnd = Internal::getExpansionLocation(end);

    if (location.offset() < locationEnd.offset()) {
        const unsigned length = locationEnd.offset() - location.offset();
        markers.append(SourceMarker(location, length, kind));
    }
}
} // Anonymous namespace

QList<SourceMarker> SemanticMarker::sourceMarkersInRange(unsigned firstLine,
                                                         unsigned lastLine)
{
    Q_ASSERT(m_unit);

    QList<SourceMarker> result;

    if (firstLine > lastLine || !m_unit->isLoaded())
        return result;

    IdentifierTokens idTokens(*m_unit, firstLine, lastLine);

    for (unsigned i = 0; i < idTokens.count(); ++i) {
        const CXCursor &cursor = idTokens.cursor(i);
        const CXSourceRange &tokenExtent = idTokens.extent(i);

        switch (clang_getCursorKind(cursor)) {
        case CXCursor_ClassDecl:
        case CXCursor_EnumDecl:
        case CXCursor_EnumConstantDecl:
        case CXCursor_Namespace:
        case CXCursor_NamespaceRef:
        case CXCursor_NamespaceAlias:
        case CXCursor_StructDecl:
        case CXCursor_TemplateRef:
        case CXCursor_TypeRef:
        case CXCursor_TypedefDecl:
            add(result, tokenExtent, SourceMarker::Type);
            break;

        case CXCursor_DeclRefExpr: {
            CXCursor referenced = clang_getCursorReferenced(cursor);
            if (clang_getCursorKind(referenced) == CXCursor_EnumConstantDecl)
                add(result, tokenExtent, SourceMarker::Type);
        } break;

        case CXCursor_MemberRefExpr:
        case CXCursor_MemberRef:
            add(result, tokenExtent, SourceMarker::Field);
            break;

        case CXCursor_Destructor:
        case CXCursor_CXXMethod: {
            if (clang_CXXMethod_isVirtual(cursor))
                add(result,
                    tokenExtent,
                    SourceMarker::VirtualMethod);
        } break;

        case CXCursor_LabelRef:
        case CXCursor_LabelStmt:
            add(result, tokenExtent, SourceMarker::Label);
            break;

        default:
            break;
        }
    }

    return result;
}
