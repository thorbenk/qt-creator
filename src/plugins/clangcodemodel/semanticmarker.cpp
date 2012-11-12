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

using namespace ClangCodeModel;
using namespace ClangCodeModel::Internal;

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
    clangOpts |= CXTranslationUnit_DetailedPreprocessingRecord;
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

/**
 * @brief Selects correct highlighting for cursor that is reference
 * @return SourceMarker::Unknown if cannot select highlighting
 */
static SourceMarker::Kind getKindByReferencedCursor(const CXCursor &cursor)
{
    const CXCursor referenced = clang_getCursorReferenced(cursor);
    switch (clang_getCursorKind(referenced)) {
    case CXCursor_EnumConstantDecl:
        return SourceMarker::Enumeration;

    case CXCursor_FieldDecl:
        return SourceMarker::Field;

    case CXCursor_FunctionDecl:
    case CXCursor_FunctionTemplate:
    case CXCursor_Constructor:
        return SourceMarker::Function;

    case CXCursor_VarDecl:
    case CXCursor_ParmDecl:
        return SourceMarker::Local;

    case CXCursor_CXXMethod:
        if (clang_CXXMethod_isVirtual(referenced))
            return SourceMarker::VirtualMethod;
        else
            return SourceMarker::Function;

    default:
        break;
    }
    return SourceMarker::Unknown;
}

} // Anonymous namespace

/**
 * @brief SemanticMarker::sourceMarkersInRange
 * @param firstLine - first line where to generate highlighting markers
 * @param lastLine - last line where to generate highlighting markers
 * @note Problematic places:
 *    - range based for from C++ 2011 requires at least child visiting
 *    - function call expression with implicit type convertion requires child
 *      visiting, example:
 *          glVertexPointer(3, GL_DOUBLE, 0, m_verticies.front()));
 *      where m_verticies is "std::vector<vec3>" and vec3 has (const double*)
 *      convertion operator. "front" cannot be highlighted even with visitor,
 *      when comparing visited cursor and token by CXSourceRange.
 *    - some compound statements have type DeclStmt or CompoundStmt which
 *      refers to top-level construction
 *    - global variables highlighted as locals
 *    - template members of template classes/functions always highlighted
 *      as members, even if they are functions - there no way to differ.
 *    - invalid code will be not highlighted or underscored
 */
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
        const CXCursorKind cursorKind = clang_getCursorKind(cursor);
        if (clang_isInvalid(cursorKind))
            continue;

        const CXSourceRange &tokenExtent = idTokens.extent(i);

        switch (cursorKind) {
        case CXCursor_EnumConstantDecl:
            add(result, tokenExtent, SourceMarker::Enumeration);
            break;

        case CXCursor_ClassDecl:
        case CXCursor_ClassTemplate:
        case CXCursor_ClassTemplatePartialSpecialization:
        case CXCursor_EnumDecl:
        case CXCursor_Namespace:
        case CXCursor_NamespaceRef:
        case CXCursor_NamespaceAlias:
        case CXCursor_StructDecl:
        case CXCursor_TemplateRef:
        case CXCursor_TypeRef:
        case CXCursor_TypedefDecl:
        case CXCursor_Constructor:
        case CXCursor_TemplateTypeParameter:
        case CXCursor_UnexposedDecl: /* friend class MyClass; */
            add(result, tokenExtent, SourceMarker::Type);
            break;

        case CXCursor_ParmDecl:
        case CXCursor_VariableRef: /* available since clang 3.1 */
        case CXCursor_VarDecl:
            add(result, tokenExtent, SourceMarker::Local);
            break;

        case CXCursor_MemberRefExpr:
        case CXCursor_MemberRef:
        case CXCursor_DeclRefExpr:
        case CXCursor_CallExpr: {
            SourceMarker::Kind kind = getKindByReferencedCursor(cursor);
            if (kind == SourceMarker::Unknown && cursorKind == CXCursor_MemberRefExpr) {
                /* template class member in template function */
                kind = SourceMarker::Field;
            }
            if (kind != SourceMarker::Unknown)
                add(result, tokenExtent, kind);
        } break;

        case CXCursor_FieldDecl:
            add(result, tokenExtent, SourceMarker::Field);
            break;

        case CXCursor_Destructor:
        case CXCursor_CXXMethod: {
            if (clang_CXXMethod_isVirtual(cursor))
                add(result, tokenExtent, SourceMarker::VirtualMethod);
            else
                add(result, tokenExtent, SourceMarker::Function);
        } break;

        case CXCursor_CXXOverrideAttr:
        case CXCursor_CXXFinalAttr:
            add(result, tokenExtent, SourceMarker::PseudoKeyword);
            break;

        case CXCursor_FunctionDecl:
        case CXCursor_FunctionTemplate:
        case CXCursor_OverloadedDeclRef:
            add(result, tokenExtent, SourceMarker::Function);
            break;

        case CXCursor_MacroDefinition:
        case CXCursor_MacroExpansion:
            add(result, tokenExtent, SourceMarker::Macro);
            break;

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
