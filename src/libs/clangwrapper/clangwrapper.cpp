#include "clangwrapper.h"
#include "sourcemarker.h"

#include <QDebug>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTime>
#include <QVarLengthArray>

#include <clang-c/Index.h>

#undef DEBUG_TIMING
#undef NEVER_REPARSE_ALWAYS_PARSE

namespace {
static inline QString toQString(const CXString &str)
{
    return QString::fromUtf8(clang_getCString(str));
}

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

static void getInstantiationLocation(const CXSourceLocation &loc,
                                     QString *fileName = 0,
                                     unsigned *line = 0,
                                     unsigned *column = 0,
                                     unsigned *offset = 0)
{
    CXFile file;
    clang_getInstantiationLocation(loc, &file, line, column, offset);
    if (fileName) {
        CXString fn = clang_getFileName(file);
        *fileName = toQString(fn);
        clang_disposeString(fn);
    }
}

static void getSpellingLocation(const CXSourceLocation &loc,
                                QString *fileName = 0,
                                unsigned *line = 0,
                                unsigned *column = 0,
                                unsigned *offset = 0)
{
    CXFile *file = 0;
    clang_getSpellingLocation(loc, file, line, column, offset);
    if (fileName) {
        CXString fn = clang_getFileName(file);
        *fileName = toQString(fn);
        clang_disposeString(fn);
    }
}

struct UnsavedFileData
{
    typedef Clang::ClangWrapper::UnsavedFile UnsavedFile;

    UnsavedFileData(const QList<UnsavedFile> &unsavedFiles)
        : count(unsavedFiles.size())
        , files(0)
    {
        if (count) {
            files = new CXUnsavedFile[count];
            for (unsigned i = 0; i < count; ++i) {
                const UnsavedFile &unsavedFile = unsavedFiles.at(i);
                const QByteArray fileName = unsavedFile.fileName.toUtf8();
                files[i].Contents = unsavedFile.contents.constData();
                files[i].Length = unsavedFile.contents.size();
                files[i].Filename = fileName.constData();
                buffers.append(unsavedFile.contents);
                buffers.append(fileName);
            }
        }
    }

    ~UnsavedFileData()
    {
        delete[] files;
    }

    unsigned count;
    CXUnsavedFile *files;
    QList<QByteArray> buffers;
};

static bool clangInitialised = false;
static QMutex initialisationMutex;

static void initClang()
{
    if (clangInitialised)
        return;

    QMutexLocker locker(&initialisationMutex);
    if (clangInitialised)
        return;

    clang_toggleCrashRecovery(1);
    clang_enableStackTraces();
    clangInitialised = true;

    qRegisterMetaType<Clang::Diagnostic>();
    qRegisterMetaType<QList<Clang::Diagnostic> >();
}

} // Anonymous namespace

class Clang::ClangWrapper::PrivateData
{
public:
    PrivateData(bool useForCodeCompletion)
        : m_unit(0)
    {
        initClang();

        const int excludeDeclsFromPCH = 1;
        const int displayDiagnostics = 1;
        m_index = clang_createIndex(excludeDeclsFromPCH, displayDiagnostics);

        m_editingOpts = clang_defaultEditingTranslationUnitOptions();
        if (!useForCodeCompletion)
            m_editingOpts &= ~CXTranslationUnit_CacheCompletionResults;
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

    bool parseFromFile(const UnsavedFiles &unsavedFiles, bool isEditable = true)
    {
        Q_ASSERT(!m_unit);

        m_diagnostics.clear();

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
        UnsavedFileData unsaved(unsavedFiles);

#ifdef DEBUG_TIMING
        QTime t;t.start();
#endif // DEBUG_TIMING

        if (isEditable) {
            unsigned opts = m_editingOpts;
//            opts = opts & ~CXTranslationUnit_CXXPrecompiledPreamble;
            m_unit = clang_parseTranslationUnit(m_index, fn.constData(), argv, argc, unsaved.files, unsaved.count, opts);
        } else {
            m_unit = clang_createTranslationUnitFromSourceFile(m_index, fn.constData(), argc, argv, unsaved.count, unsaved.files);
        }

#ifdef DEBUG_TIMING
        qDebug() << "=== Parsing"
                 << (isEditable ? "(editable)" : "(not editable")
                 << "->" << (m_unit ? "successful" : "failed")
                 << "in" << t.elapsed() << "ms";
        qDebug() << "Command-line:";
        qDebug("clang -fsyntax-only %s %c", m_fileName.toUtf8().data(), (m_options.isEmpty() ? ' ' : '\\'));
        for (int i = 1; i <= m_options.size(); ++i)
            qDebug("  %s%s", m_options[i - 1].toUtf8().data(), (i == m_options.size() ? "" : " \\"));
#endif // DEBUG_TIMING

        checkDiagnostics();
        delete[] argv;
        return m_unit != 0;
    }

    bool parseFromPCH()
    {
        Q_ASSERT(!m_unit);

        m_diagnostics.clear();

        if (m_fileName.isEmpty())
            return false;

        m_unit = clang_createTranslationUnit(m_index,
                                             m_fileName.toUtf8().constData());
        checkDiagnostics();
        return m_unit != 0;
    }

    bool save(const QString &fileName) const
    {
        if (!m_unit)
            return false;

        const QByteArray fn(fileName.toUtf8());
        return 0 == clang_saveTranslationUnit(m_unit, fn.constData(), clang_defaultSaveOptions(m_unit));
    }

    void checkDiagnostics()
    {
        if (!m_unit)
            return;

        const unsigned diagCount = clang_getNumDiagnostics(m_unit);
#ifdef DEBUG_TIMING
        qDebug() <<"..." << diagCount << "diagnostics.";
#endif // DEBUG_TIMING
        for (unsigned i = 0; i < diagCount; ++i) {
            CXDiagnostic diag = clang_getDiagnostic(m_unit, i);
#ifdef DEBUG_TIMING
            unsigned opt = CXDiagnostic_DisplaySourceLocation
                    | CXDiagnostic_DisplayColumn
                    | CXDiagnostic_DisplaySourceRanges
                    | CXDiagnostic_DisplayOption
                    | CXDiagnostic_DisplayCategoryId
                    | CXDiagnostic_DisplayCategoryName
                    ;
            qDebug() << toQString(clang_formatDiagnostic(diag, opt));
#endif

            Diagnostic::Severity severity = static_cast<Diagnostic::Severity>(clang_getDiagnosticSeverity(diag));
            CXSourceLocation loc = clang_getDiagnosticLocation(diag);
            QString fileName;
            unsigned line = 0, column = 0;
            getInstantiationLocation(loc, &fileName, &line, &column);
            const QString spelling = toQString(clang_getDiagnosticSpelling(diag));

            const unsigned rangeCount = clang_getDiagnosticNumRanges(diag);
            if (rangeCount > 0) {
                for (unsigned i = 0; i < rangeCount; ++i) {
                    CXSourceRange r = clang_getDiagnosticRange(diag, 0);
                    unsigned begin = 0;
                    unsigned length = 0;
                    getSpellingLocation(clang_getRangeStart(r), 0, 0, 0, &begin);
                    getSpellingLocation(clang_getRangeEnd(r), 0, 0, 0, &length);
                    length -= begin;

                    Diagnostic d(severity, fileName, line, column, length, spelling);
#ifdef DEBUG_TIMING
                    qDebug() << d.severityAsString() << fileName << line << column << length << spelling;
#endif
                    m_diagnostics.append(d);
                }
            } else {
                Diagnostic d(severity, fileName, line, column, 0, spelling);
#ifdef DEBUG_TIMING
                qDebug() << d.severityAsString() << fileName << line << column << 0 << spelling;
#endif
                m_diagnostics.append(d);
            }

            clang_disposeDiagnostic(diag);
        }
    }

public:
    QString m_fileName;
    QStringList m_options;
    CXIndex m_index;
    unsigned m_editingOpts;
    CXTranslationUnit m_unit;
    QList<Diagnostic> m_diagnostics;

private:
    QList<QByteArray> m_args;
};

using namespace Clang;

CodeCompletionResult::CodeCompletionResult(unsigned priority)
    : _priority(priority)
{
}

const QString &Diagnostic::severityAsString() const
{
    static QStringList strs = QStringList() << "ignored"
                                            << "note"
                                            << "warning"
                                            << "error"
                                            << "fatal" ;
    return strs.at(m_severity);
}

ClangWrapper::UnsavedFile::UnsavedFile(const QString &fileName,
                                       const QByteArray &contents)
    : fileName(fileName)
    , contents(contents)
{
}

ClangWrapper::ClangWrapper(bool useForCodeCompletion)
    : m_d(new PrivateData(useForCodeCompletion))
    , m_mutex(QMutex::Recursive)
{
}

ClangWrapper::~ClangWrapper()
{
    Q_ASSERT(m_d);

    delete m_d;
    m_d = 0;
}

QString ClangWrapper::fileName() const
{
    Q_ASSERT(m_d);

    return m_d->m_fileName;
}

void ClangWrapper::setFileName(const QString &fileName)
{
    Q_ASSERT(m_d);
    if (m_d->m_fileName != fileName) {
        m_d->m_fileName = fileName;
        m_d->invalidateTranslationUnit();
    }
}

QStringList ClangWrapper::options() const
{
    Q_ASSERT(m_d);

    return m_d->m_options;
}

void ClangWrapper::setOptions(const QStringList &options) const
{
    Q_ASSERT(m_d);

    if (m_d->m_options != options) {
        m_d->m_options = options;
        m_d->invalidateTranslationUnit();
    }
}

bool ClangWrapper::reparse(const UnsavedFiles &unsavedFiles)
{
    Q_ASSERT(m_d);

    m_d->m_diagnostics.clear();

#ifdef NEVER_REPARSE_ALWAYS_PARSE
    if (m_d->m_unit) {
        clang_disposeTranslationUnit(m_d->m_unit);
        m_d->m_unit = 0;
    }

    return m_d->parseFromFile(unsavedFiles);
#else // !NEVER_REPARSE_ALWAYS_PARSE
    if (!m_d->m_unit)
        return m_d->parseFromFile(unsavedFiles);

    UnsavedFileData unsaved(unsavedFiles);

#ifdef DEBUG_TIMING
    QTime t; t.start();
    qDebug() << "Re-parsring with" << unsaved.count << "unsaved files...";
    qDebug() << "Command-line:";
    qDebug("clang -fsyntax-only %s %c", m_d->m_fileName.toUtf8().data(), (m_d->m_options.isEmpty() ? ' ' : '\\'));
    for (int i = 1; i <= m_d->m_options.size(); ++i)
        qDebug("  %s%s", m_d->m_options[i - 1].toUtf8().data(), (i == m_d->m_options.size() ? "" : " \\"));
#endif // DEBUG_TIMING

    unsigned opts = clang_defaultReparseOptions(m_d->m_unit);
    if (clang_reparseTranslationUnit(m_d->m_unit, unsaved.count, unsaved.files, opts) == 0) {
        // success:
#ifdef DEBUG_TIMING
        qDebug() << "-> reparsing successful in" << t.elapsed() << "ms.";
#endif // DEBUG_TIMING

        m_d->checkDiagnostics();
        return true;
    } else {
        // failure:
#ifdef DEBUG_TIMING
        qDebug() << "-> reparsing failed in" << t.elapsed() << "ms.";
#endif // DEBUG_TIMING

        m_d->checkDiagnostics();
        m_d->invalidateTranslationUnit();
        return false;
    }
#endif // NEVER_REPARSE_ALWAYS_PARSE
}

namespace {
static void add(QList<SourceMarker> &markers,
                const CXSourceRange &extent,
                SourceMarker::Kind kind)
{
    unsigned line = 0, column = 0, startOffset = 0, endOffset = 0;
    CXSourceLocation start = clang_getRangeStart(extent);
    CXSourceLocation end = clang_getRangeEnd(extent);

    CXFile file;
    clang_getInstantiationLocation(start, &file, &line, &column, &startOffset);
    clang_getInstantiationLocation(end, &file, 0, 0, &endOffset);

    if (startOffset < endOffset) {
        const unsigned length = endOffset - startOffset;
        markers.append(SourceMarker(line, column, length, kind));
    }
}

} // Anonymous namespace

QList<SourceMarker> ClangWrapper::sourceMarkersInRange(unsigned firstLine,
                                                       unsigned lastLine)
{
    Q_ASSERT(m_d);

    QList<SourceMarker> result;

    if (firstLine > lastLine || !m_d->m_unit)
        return result;

    QVarLengthArray<CXToken> identifierTokens;

    QTime t;t.start();

    // Retrieve all identifier tokens:
    CXToken *tokens = 0;
    unsigned tokenCount = 0;
    CXTranslationUnit tu = m_d->m_unit;
    CXFile file = clang_getFile(tu, m_d->m_fileName.toUtf8().constData());
    CXSourceLocation startLocation = clang_getLocation(tu, file, firstLine, 1);
    CXSourceLocation endLocation = clang_getLocation(tu, file, lastLine, 1);
    CXSourceRange range = clang_getRange(startLocation, endLocation);
    clang_tokenize(tu, range, &tokens, &tokenCount);
#ifdef DEBUG_TIMING
    qDebug() << tokenCount << "tokens"
             << "for range" << firstLine << "to" << lastLine
             << "in" << t.elapsed() << "ms.";
#endif // DEBUG_TIMING
    for (unsigned i = 0; i < tokenCount; ++i)
        if (CXToken_Identifier == clang_getTokenKind(tokens[i]))
            identifierTokens.append(tokens[i]);
#ifdef DEBUG_TIMING
    qDebug()<<identifierTokens.size()<<"identifier tokens";
#endif // DEBUG_TIMING
    if (identifierTokens.isEmpty())
        return result;

    // Get the cursors for the tokens:
    CXCursor *idCursors = new CXCursor[identifierTokens.size()];
    clang_annotateTokens(tu,
                         identifierTokens.data(),
                         identifierTokens.size(),
                         idCursors);

    // Create the markers using the cursor to check the types:
    for (int i = 0; i < identifierTokens.size(); ++i) {
        CXCursor &c = idCursors[i];
        const CXToken &idToken = identifierTokens[i];

        switch (clang_getCursorKind(c)) {
        case CXCursor_ClassDecl:
        case CXCursor_EnumDecl:
        case CXCursor_EnumConstantDecl:
        case CXCursor_Namespace:
        case CXCursor_NamespaceRef:
        case CXCursor_NamespaceAlias:
        case CXCursor_StructDecl:
        case CXCursor_TemplateRef:
        case CXCursor_TypeRef:
            add(result, clang_getTokenExtent(tu, idToken), SourceMarker::Type);
            break;

        case CXCursor_DeclRefExpr: {
            CXCursor referenced = clang_getCursorReferenced(c);
            if (clang_getCursorKind(referenced) == CXCursor_EnumConstantDecl)
                add(result, clang_getTokenExtent(tu, idToken), SourceMarker::Type);
        } break;

        case CXCursor_MemberRefExpr:
        case CXCursor_MemberRef:
            add(result, clang_getTokenExtent(tu, idToken), SourceMarker::Field);
            break;

        case CXCursor_Destructor:
        case CXCursor_CXXMethod:
            if (clang_CXXMethod_isVirtual(c))
                add(result,
                    clang_getTokenExtent(tu, idToken),
                    SourceMarker::VirtualMethod);
            break;

        case CXCursor_LabelRef:
        case CXCursor_LabelStmt:
            add(result, clang_getTokenExtent(tu, idToken), SourceMarker::Label);
            break;

        default:
            break;
        }
    }
#ifdef DEBUG_TIMING
    qDebug() << "identified ranges in" << t.elapsed()<< "ms.";
#endif // DEBUG_TIMING

    delete[] idCursors;

    return result;
}

QList<CodeCompletionResult> ClangWrapper::codeCompleteAt(unsigned line, unsigned column, const UnsavedFiles &unsavedFiles)
{
    Q_ASSERT(m_d);

    m_d->m_diagnostics.clear();

    QList<CodeCompletionResult> completions;

    if (!m_d->m_unit) {
        QTime t;
        t.start();
        m_d->parseFromFile(unsavedFiles);
#ifdef DEBUG_TIMING
        qDebug() << "Initial parse done in" << t.elapsed() << "ms.";
#endif // DEBUG_TIMING
    }

    if (!m_d->m_unit)
        return completions;

#ifdef DEBUG_TIMING
    qDebug() << "codeCompleteAt line" << line << "column" << column;
#endif // DEBUG_TIMING

    const QByteArray fn(m_d->m_fileName.toUtf8());
    UnsavedFileData unsaved(unsavedFiles);
    unsigned opts = clang_defaultCodeCompleteOptions();
    CXCodeCompleteResults *results = clang_codeCompleteAt(m_d->m_unit, fn.constData(), line, column, unsaved.files, unsaved.count, opts);
    if (results) {
        for (unsigned i = 0; i < results->NumResults; ++i) {
            CXCompletionString complStr = results->Results[i].CompletionString;
            unsigned priority = clang_getCompletionPriority(complStr);

            CodeCompletionResult ccr(priority);
            CXCursorKind kind = results->Results[i].CursorKind;
            ccr.isFunctionCompletion = (kind == CXCursor_FunctionDecl || kind == CXCursor_CXXMethod);

#if 0
            CXCursorKind kind = results->Results[i].CursorKind;
            qDebug() << "--- kind:"
                     << toQString(clang_getCursorKindSpelling(kind))
                     << "(" << priority << ")";
#endif

            unsigned chunckCount = clang_getNumCompletionChunks(complStr);
            for (unsigned j = 0; j < chunckCount; ++j) {
                CXCompletionChunkKind chunkKind = clang_getCompletionChunkKind(complStr, j);
                const QString chunkText = toQString(clang_getCompletionChunkText(complStr, j));

                if (chunkKind == CXCompletionChunk_TypedText)
                    ccr.text = chunkText;

                if (!chunkText.isEmpty()) {
                    if (ccr.hint.size() > 0 && ccr.hint.at(ccr.hint.size() - 1).isLetterOrNumber())
                        ccr.hint += QLatin1Char(' ');

                    ccr.hint += chunkText;
                }

#if 0
                qDebug() << "    chunk" << j << ": kind: " << chunkKind << toString(chunkKind);
                qDebug() << "              text:" << chunkText;
#endif
            }

            completions.append(ccr);
        }

        clang_disposeCodeCompleteResults(results);
    } else {
//        m_d->checkDiagnostics();
    }

    return completions;
}

QString ClangWrapper::precompile(const QString &headerFileName, const QStringList &options)
{
    initClang();

    QString outFileName = headerFileName + QLatin1String(".out");
    if (QFile(outFileName).exists()) {
        return outFileName;
    }

#ifdef DEBUG_TIMING
    qDebug() << "** Precompiling" << outFileName << "from" << headerFileName << "...";
#endif // DEBUG_TIMING

    ClangWrapper wrapper;
    wrapper.setFileName(headerFileName);
    wrapper.setOptions(options);
    if (wrapper.reparse(UnsavedFiles())) {
        wrapper.m_d->save(outFileName);

#ifdef DEBUG_TIMING
        qDebug() << "** Precompiling done.";
#endif // DEBUG_TIMING
    } else {
#ifdef DEBUG_TIMING
        qWarning() << "** Precompiling failed!";
#endif // DEBUG_TIMING
        outFileName = QString::null;
    }

    return outFileName;
}

QList<Diagnostic> ClangWrapper::diagnostics() const
{
    Q_ASSERT(m_d);

    return m_d->m_diagnostics;
}

bool ClangWrapper::objcEnabled() const
{
    Q_ASSERT(m_d);

    static const QString objcOption = QLatin1String("-ObjC++");

    return m_d->m_options.contains(objcOption);
}
