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

#if 0
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
#endif

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
}

} // Anonymous namespace

class Clang::ClangWrapper::PrivateData
{
public:
    PrivateData()
        : m_unit(0)
    {
        initClang();

        const int excludeDeclsFromPCH = 1;
        const int displayDiagnostics = 1;
        m_index = clang_createIndex(excludeDeclsFromPCH, displayDiagnostics);
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

        if (isEditable)
            m_unit = clang_parseTranslationUnit(m_index, fn.constData(), argv, argc, unsaved.files, unsaved.count, clang_defaultEditingTranslationUnitOptions());
        else
            m_unit = clang_createTranslationUnitFromSourceFile(m_index, fn.constData(), argc, argv, unsaved.count, unsaved.files);

#ifdef DEBUG_TIMING
        qDebug() << "=== Parsing of" << m_fileName << "with args" << m_options << "->" << (m_unit?"SUCCESS":"FAIL") << "in" << t.elapsed() << "ms";
#endif // DEBUG_TIMING

        checkDiagnostics();
        delete[] argv;
        return m_unit != 0;
    }

    bool parseFromPCH()
    {
        Q_ASSERT(!m_unit);

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

            Diagnostic::Severity severity = static_cast<Diagnostic::Severity>(clang_getDiagnosticSeverity(diag));
            CXSourceLocation loc = clang_getDiagnosticLocation(diag);
            QString fileName;
            unsigned line = 0, column = 0;
            getInstantiationLocation(loc, &fileName, &line, &column);
            const QString spelling = toQString(clang_getDiagnosticSpelling(diag));

            unsigned length = 1;
            if (clang_getDiagnosticNumRanges(diag) > 0) {
                CXSourceRange r = clang_getDiagnosticRange(diag, 0);
                unsigned begin = 0;
                getInstantiationLocation(clang_getRangeStart(r), 0, 0, 0, &begin);
                getInstantiationLocation(clang_getRangeEnd(r), 0, 0, 0, &length);
                length -= begin;
            }

            Diagnostic d(severity, fileName, line, column, length, spelling);
#if 0
            qDebug() << d.severityAsString() << fileName << line << column << length << spelling;
#endif
            m_diagnostics.append(d);

            clang_disposeDiagnostic(diag);
        }
    }

public:
    QString m_fileName;
    QStringList m_options;
    CXIndex m_index;
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

ClangWrapper::ClangWrapper()
    : m_d(new PrivateData)
{
}

ClangWrapper::~ClangWrapper()
{
    delete m_d;
    m_d = 0;
}

QString ClangWrapper::fileName() const
{
    return m_d->m_fileName;
}

void ClangWrapper::setFileName(const QString &fileName)
{
    m_d->m_fileName = fileName;
    m_d->invalidateTranslationUnit();
}

QStringList ClangWrapper::options() const
{
    return m_d->m_options;
}

void ClangWrapper::setOptions(const QStringList &options) const
{
    m_d->m_options = options;
    m_d->invalidateTranslationUnit();
}

bool ClangWrapper::reparse(const UnsavedFiles &unsavedFiles)
{
    if (!m_d->m_unit)
        return m_d->parseFromFile(unsavedFiles);

    unsigned opts = clang_defaultReparseOptions(m_d->m_unit);
    UnsavedFileData unsaved(unsavedFiles);
    if (clang_reparseTranslationUnit(m_d->m_unit, unsaved.count, unsaved.files, opts) == 0) {
        // success:
        m_d->checkDiagnostics();
        return true;
    } else {
        // failure:
        m_d->checkDiagnostics();
        m_d->invalidateTranslationUnit();
        return false;
    }
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
    CXSourceLocation endLocation = clang_getLocation(tu, file, lastLine + 1, 1);
    CXSourceRange range = clang_getRange(startLocation, endLocation);
    clang_tokenize(tu, range, &tokens, &tokenCount);
#if DEBUG_TIMING
    qDebug() << tokenCount << "tokens"
             << "for range" << firstLine << "to" << lastLine
             << "in" << t.elapsed() << "ms.";
#endif // DEBUG_TIMING
    for (unsigned i = 0; i < tokenCount; ++i)
        if (CXToken_Identifier == clang_getTokenKind(tokens[i]))
            identifierTokens.append(tokens[i]);
#if DEBUG_TIMING
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
        case CXCursor_TemplateRef:
        case CXCursor_TypeRef:
            add(result, clang_getTokenExtent(tu, idToken), SourceMarker::Type);
            break;

        case CXCursor_MemberRefExpr:
            // either: member_field
            //     or: member_field.member_field
            //### So treat this different
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

        default:
            break;
        }
    }
#if DEBUG_TIMING
    qDebug() << "identified ranges in" << t.elapsed()<< "ms.";
#endif // DEBUG_TIMING

    delete[] idCursors;

    return result;
}

QList<CodeCompletionResult> ClangWrapper::codeCompleteAt(unsigned line, unsigned column, const UnsavedFiles &unsavedFiles)
{
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
        m_d->checkDiagnostics();
    }

    return completions;
}

QString ClangWrapper::precompile(const QString &headerFileName, const QStringList &options)
{
    initClang();

    QString outFileName = headerFileName + ".out";
    if (QFile(outFileName).exists()) {
        return outFileName;
    }

#ifdef DEBUG_TIMING
    qDebug() << "** Precompiling" << outFileName << "from" << headerFileName << "...";
    qDebug() << "** Options:" << options;
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
    return m_d->m_diagnostics;
}

bool ClangWrapper::objcEnabled() const
{
    static const QString objcOption = QLatin1String("-ObjC++");
    return m_d->m_options.contains(objcOption);
}
