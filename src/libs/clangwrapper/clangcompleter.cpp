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

#include "clangcompleter.h"
#include "sourcemarker.h"
#include "unsavedfiledata.h"
#include "utils_p.h"

#include <QDebug>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTime>

#include <clang-c/Index.h>

namespace {

static inline QString toString(CXCompletionChunkKind kind)
{
    switch (kind) {
    case CXCompletionChunk_Optional:
        return QLatin1String("Optional");
    case CXCompletionChunk_TypedText:
        return QLatin1String("TypedText");
    case CXCompletionChunk_Text:
        return QLatin1String("Text");
    case CXCompletionChunk_Placeholder:
        return QLatin1String("Placeholder");
    case CXCompletionChunk_Informative:
        return QLatin1String("Informative");
    case CXCompletionChunk_CurrentParameter:
        return QLatin1String("CurrentParameter");
    case CXCompletionChunk_LeftParen:
        return QLatin1String("LeftParen");
    case CXCompletionChunk_RightParen:
        return QLatin1String("RightParen");
    case CXCompletionChunk_LeftBracket:
        return QLatin1String("LeftBracket");
    case CXCompletionChunk_RightBracket:
        return QLatin1String("RightBracket");
    case CXCompletionChunk_LeftBrace:
        return QLatin1String("LeftBrace");
    case CXCompletionChunk_RightBrace:
        return QLatin1String("RightBrace");
    case CXCompletionChunk_LeftAngle:
        return QLatin1String("LeftAngle");
    case CXCompletionChunk_RightAngle:
        return QLatin1String("RightAngle");
    case CXCompletionChunk_Comma:
        return QLatin1String("Comma");
    case CXCompletionChunk_ResultType:
        return QLatin1String("ResultType");
    case CXCompletionChunk_Colon:
        return QLatin1String("Colon");
    case CXCompletionChunk_SemiColon:
        return QLatin1String("SemiColon");
    case CXCompletionChunk_Equal:
        return QLatin1String("Equal");
    case CXCompletionChunk_HorizontalSpace:
        return QLatin1String("HorizontalSpace");
    case CXCompletionChunk_VerticalSpace:
        return QLatin1String("VerticalSpace");
    default:
        return QLatin1String("<UNKNOWN>");
    }
}

} // Anonymous namespace

class Clang::ClangCompleter::PrivateData
{
public:
    PrivateData()
        : m_unit(0)
    {
        const int excludeDeclsFromPCH = 1;
        const int displayDiagnostics = 1;
        m_index = clang_createIndex(excludeDeclsFromPCH, displayDiagnostics);

        m_editingOpts = clang_defaultEditingTranslationUnitOptions();
    }

    ~PrivateData()
    {
        invalidateTranslationUnit();
        if (m_index) {
            clang_disposeIndex(m_index);
            m_index = 0;
        }
    }

    void invalidateTranslationUnit()
    {
        Q_ASSERT(m_index);

        if (m_unit) {
            clang_disposeTranslationUnit(m_unit);
            m_unit = 0;
        }
    }

    bool parseFromFile(const UnsavedFiles &unsavedFiles)
    {
        Q_ASSERT(!m_unit);

        if (m_fileName.isEmpty())
            return false;

        m_args.clear();
        const int argc = m_options.size();
        const char **argv = new const char*[argc];
        for (int i = 0; i < argc; ++i) {
            const QByteArray arg = m_options.at(i).toUtf8();
            m_args.append(arg);
            argv[i] = arg.constData();
        }

        const QByteArray fn(m_fileName.toUtf8());
        Clang::Internal::UnsavedFileData unsaved(unsavedFiles);

        m_unit = clang_parseTranslationUnit(m_index, fn.constData(), argv, argc, unsaved.files(), unsaved.count(), m_editingOpts);

        delete[] argv;
        return m_unit != 0;
    }

public:
    QString m_fileName;
    QStringList m_options;
    CXIndex m_index;
    unsigned m_editingOpts;
    CXTranslationUnit m_unit;

private:
    QList<QByteArray> m_args;
};

using namespace Clang;
using namespace Clang::Internal;

CodeCompletionResult::CodeCompletionResult()
    : m_priority(0)
    , m_completionKind(Other)
    , m_availability(Available)
    , m_hasParameters(false)
{}

CodeCompletionResult::CodeCompletionResult(unsigned priority)
    : m_priority(priority)
    , m_completionKind(Other)
    , m_availability(Available)
    , m_hasParameters(false)
{
}

ClangCompleter::ClangCompleter()
    : m_d(new PrivateData)
    , m_mutex(QMutex::Recursive)
{
}

ClangCompleter::~ClangCompleter()
{
    Q_ASSERT(m_d);

    delete m_d;
    m_d = 0;
}

QString ClangCompleter::fileName() const
{
    Q_ASSERT(m_d);

    return m_d->m_fileName;
}

void ClangCompleter::setFileName(const QString &fileName)
{
    Q_ASSERT(m_d);
    if (m_d->m_fileName != fileName) {
        m_d->m_fileName = fileName;
        m_d->invalidateTranslationUnit();
    }
}

QStringList ClangCompleter::options() const
{
    Q_ASSERT(m_d);

    return m_d->m_options;
}

void ClangCompleter::setOptions(const QStringList &options) const
{
    Q_ASSERT(m_d);

    if (m_d->m_options != options) {
        m_d->m_options = options;
        m_d->invalidateTranslationUnit();
    }
}

bool ClangCompleter::reparse(const UnsavedFiles &unsavedFiles)
{
    Q_ASSERT(m_d);

    if (!m_d->m_unit)
        return m_d->parseFromFile(unsavedFiles);

    UnsavedFileData unsaved(unsavedFiles);

    unsigned opts = clang_defaultReparseOptions(m_d->m_unit);
    if (clang_reparseTranslationUnit(m_d->m_unit, unsaved.count(), unsaved.files(), opts) == 0)
        return true;

    m_d->invalidateTranslationUnit();
    return false;
}

QList<CodeCompletionResult> ClangCompleter::codeCompleteAt(unsigned line, unsigned column, const UnsavedFiles &unsavedFiles)
{
    Q_ASSERT(m_d);

    QList<CodeCompletionResult> completions;

    if (!m_d->m_unit) {
        QTime t;
        t.start();
        m_d->parseFromFile(unsavedFiles);
    }

    if (!m_d->m_unit)
        return completions;

    const QByteArray fn(m_d->m_fileName.toUtf8());
    UnsavedFileData unsaved(unsavedFiles);
    unsigned opts = clang_defaultCodeCompleteOptions();
    CXCodeCompleteResults *results = clang_codeCompleteAt(m_d->m_unit, fn.constData(), line, column, unsaved.files(), unsaved.count(), opts);
    if (results) {
        for (unsigned i = 0; i < results->NumResults; ++i) {
            CXCompletionString complStr = results->Results[i].CompletionString;
            unsigned priority = clang_getCompletionPriority(complStr);

            CodeCompletionResult ccr(priority);

            bool previousChunkWasLParen = false;
            unsigned chunckCount = clang_getNumCompletionChunks(complStr);
            for (unsigned j = 0; j < chunckCount; ++j) {
                CXCompletionChunkKind chunkKind = clang_getCompletionChunkKind(complStr, j);
                const QString chunkText =
                        Internal::getQString(clang_getCompletionChunkText(complStr, j), false);

                if (chunkKind == CXCompletionChunk_TypedText)
                    ccr.setText(chunkText);

                if (chunkKind == CXCompletionChunk_RightParen && previousChunkWasLParen)
                    ccr.setHasParameters(false);

                if (chunkKind == CXCompletionChunk_LeftParen) {
                    previousChunkWasLParen = true;
                    ccr.setHasParameters(true);
                } else {
                    previousChunkWasLParen = false;
                }

                if (!chunkText.isEmpty()) {
                    if (ccr.hint().size() > 0 && ccr.hint().at(ccr.hint().size() - 1).isLetterOrNumber())
                        ccr.setHint(ccr.hint() + QLatin1Char(' '));

                    ccr.setHint(ccr.hint() + chunkText);
                }
            }

            switch (results->Results[i].CursorKind) {
            case CXCursor_Constructor:
                ccr.setCompletionKind(CodeCompletionResult::ConstructorCompletionKind);
                break;

            case CXCursor_Destructor:
                ccr.setCompletionKind(CodeCompletionResult::DestructorCompletionKind);
                break;

            case CXCursor_CXXMethod:
            case CXCursor_ConversionFunction:
            case CXCursor_FunctionDecl:
            case CXCursor_FunctionTemplate:
            case CXCursor_MemberRef:
            case CXCursor_MemberRefExpr:
                ccr.setCompletionKind(CodeCompletionResult::FunctionCompletionKind);
                break;

            case CXCursor_FieldDecl:
            case CXCursor_VarDecl:
                ccr.setCompletionKind(CodeCompletionResult::VariableCompletionKind);
                break;

            case CXCursor_Namespace:
            case CXCursor_NamespaceAlias:
            case CXCursor_NamespaceRef:
                ccr.setCompletionKind(CodeCompletionResult::NamespaceCompletionKind);
                break;

            case CXCursor_StructDecl:
            case CXCursor_UnionDecl:
            case CXCursor_ClassDecl:
            case CXCursor_TypeRef:
            case CXCursor_TemplateRef:
            case CXCursor_TypedefDecl:
            case CXCursor_ClassTemplate:
            case CXCursor_ClassTemplatePartialSpecialization:
            // TODO: objc cursors
                ccr.setCompletionKind(CodeCompletionResult::ClassCompletionKind);
                break;

            case CXCursor_EnumConstantDecl:
                ccr.setCompletionKind(CodeCompletionResult::EnumeratorCompletionKind);
                break;

            case CXCursor_EnumDecl:
                ccr.setCompletionKind(CodeCompletionResult::EnumCompletionKind);
                break;

            case CXCursor_PreprocessingDirective:
            case CXCursor_MacroDefinition:
            case CXCursor_MacroExpansion:
            case CXCursor_InclusionDirective:
                ccr.setCompletionKind(CodeCompletionResult::PreProcessorCompletionKind);
                break;

            default:
                break;
            }

            switch (clang_getCompletionAvailability(complStr)) {
            case CXAvailability_Deprecated:
                ccr.setAvailability(CodeCompletionResult::Deprecated);
                break;

            case CXAvailability_NotAvailable:
                ccr.setAvailability(CodeCompletionResult::NotAvailable);
                break;

            case CXAvailability_NotAccessible:
                ccr.setAvailability(CodeCompletionResult::NotAccessible);
                break;

            default:
                break;
            }

            completions.append(ccr);
        }

        clang_disposeCodeCompleteResults(results);
    }

    return completions;
}

bool ClangCompleter::objcEnabled() const
{
    Q_ASSERT(m_d);

    static const QString objcOption = QLatin1String("-ObjC++");

    return m_d->m_options.contains(objcOption);
}
