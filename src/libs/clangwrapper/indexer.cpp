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

#include "indexer.h"
#include "reuse.h"
#include "database.h"

#include <clang-c/Index.h>

#include <qtconcurrent/QtConcurrentTools>

#include <QtCore/QDebug>
#include <QtCore/QVector>
#include <QtCore/QHash>
#include <QtCore/QFuture>
#include <QtCore/QTime>

//#define DEBUG
//#define DEBUG_DIAGNOSTICS

#ifdef DEBUG
    #define BEGIN_PROFILE_SCOPE(ID) { ScopepTimer t(ID);
    #define END_PROFILE_SCOPE }
#else
    #define BEGIN_PROFILE_SCOPE(ID)
    #define END_PROFILE_SCOPE
#endif

namespace Clang {

// Create tags and speclizations in order to make IndexedSymbolInfo satisfy
// the requirements of the database.
struct FileNameKey { typedef QString ValueType; };
template <>
inline QString getKey<FileNameKey>(const IndexedSymbolInfo &info)
{ return info.m_fileName; }

struct SymbolTypeKey { typedef IndexedSymbolInfo::SymbolType ValueType; };
template <>
inline IndexedSymbolInfo::SymbolType getKey<SymbolTypeKey>(const IndexedSymbolInfo &info)
{ return info.m_type; }


// The indexing result, containing the symbols found, reported by the indexer processor.
struct IndexingResult
{
    IndexingResult(const QVector<IndexedSymbolInfo> &info, const QStringList &options)
        : m_symbolsInfo(info)
        , m_compilationOptions(options)
    {}
    QVector<IndexedSymbolInfo> m_symbolsInfo;
    QStringList m_compilationOptions;
};


class IndexerPrivate : public QObject
{
    Q_OBJECT
public:
    IndexerPrivate();

    // This enumeration is used to index a vector. So be careful when changing.
    enum FileType {
        ImplementationFile = 0,
        HeaderFile,
        UnknownFile,
        TotalFileTypes
    };

    struct FileData
    {
        FileData() : m_upToDate(false) {}
        FileData(const QString &fileName,
                 const QStringList &compilationOptions,
                 bool upToDate = false)
            : m_fileName(fileName)
            , m_compilationOptions(compilationOptions)
            , m_upToDate(upToDate)
        {}

        QString m_fileName;
        QStringList m_compilationOptions;
        bool m_upToDate;
    };

public slots:
    void synchronize(int resultIndex);

public:
    void run();
    void cancel(bool wait);

    bool addFile(const QString &fileName, const QStringList &compilationOptions);
    QStringList getAllFiles() const;
    static FileType identifyFileType(const QString &fileName);
    static void populateFileNames(QStringList *all, const QList<FileData> &data);

    QVector<QHash<QString, FileData> > m_files;
    Database<IndexedSymbolInfo, FileNameKey, SymbolTypeKey> m_database;
    QFutureWatcher<IndexingResult> m_indexingWatcher;
};


// The indexer processor, which actually compiles the translation units, visits the AST,
// collects the symbols, and eventually report them to the indexer. It relies only on local data.
class IndexerProcessor : public QObject
{
    Q_OBJECT
public:
    IndexerProcessor(const QHash<QString, IndexerPrivate::FileData> &headers,
                     const QHash<QString, IndexerPrivate::FileData> &impls);

    struct InclusionVisitorData
    {
        InclusionVisitorData(IndexerProcessor *proc, const QStringList &options)
            : m_proc(proc)
            , m_compilationOptions(options)
        {}
        IndexerProcessor *m_proc;
        QStringList m_compilationOptions;
    };
    static void inclusionVisit(CXFile file,
                               CXSourceLocation *,
                               unsigned,
                               CXClientData clientData);

    struct AstVisitorData
    {
        AstVisitorData(IndexerProcessor *proc)
            : m_proc(proc)
        {
            m_symbolsInfo.reserve(10); // Reasonable guess...?
        }
        IndexerProcessor *m_proc;
        QHash<unsigned, QString> m_qualification;
        QVector<IndexedSymbolInfo> m_symbolsInfo;
    };
    static CXChildVisitResult astVisit(CXCursor cursor,
                                       CXCursor parentCursor,
                                       CXClientData clientData);

    void process(QFutureInterface<IndexingResult> &interface);
    void process(QFutureInterface<IndexingResult> &interface, IndexerPrivate::FileData *fileData);

    CXIndex m_clangIndex;
    unsigned m_unitManagementOptions;
    QHash<QString, IndexerPrivate::FileData> m_headers;
    QHash<QString, IndexerPrivate::FileData> m_impls;
};

} // Clang

namespace {

struct ScopepTimer
{
    ScopepTimer(int id = 0) : m_id(id) { m_t.start(); }
    ~ScopepTimer() { qDebug() << "\t#Timer" << m_id << ":" << m_t.elapsed() << "ms"; }
    int m_id;
    QTime m_t;
};

} // Anonymous

using namespace Clang;
using namespace Internal;


IndexerProcessor::IndexerProcessor(const QHash<QString, IndexerPrivate::FileData> &headers,
                                   const QHash<QString, IndexerPrivate::FileData> &impls)
    : m_clangIndex(clang_createIndex(/*excludeDeclsFromPCH*/ 0, /*displayDiagnostics*/ 0))
    , m_unitManagementOptions(CXTranslationUnit_None)
    , m_headers(headers)
    , m_impls(impls)
{}

void IndexerProcessor::inclusionVisit(CXFile file,
                                      CXSourceLocation *,
                                      unsigned,
                                      CXClientData clientData)
{
    const QString &fileName = getQString(clang_getFileName(file));
    IndexerPrivate::FileType fileType = IndexerPrivate::identifyFileType(fileName);
    if (fileType != IndexerPrivate::HeaderFile)
        return;

    // We keep track of headers included in parsed translation units so we can "optimize"
    // further ast visits. Strictly speaking this would not be correct, since such headers
    // might have been affected by content that appears before them. However this
    // significantly improves the indexing and it should be ok to live with that.
    InclusionVisitorData *inclusionData = static_cast<InclusionVisitorData *>(clientData);
    if (inclusionData->m_proc->m_headers.contains(fileName)) {
        inclusionData->m_proc->m_headers[fileName].m_upToDate = true;
    } else {
        inclusionData->m_proc->m_headers.insert(
                    fileName,
                    IndexerPrivate::FileData(fileName,
                                             inclusionData->m_compilationOptions,
                                             true));
    }
}

CXChildVisitResult IndexerProcessor::astVisit(CXCursor cursor,
                                              CXCursor parentCursor,
                                              CXClientData clientData)
{
    AstVisitorData *visitorData = static_cast<AstVisitorData *>(clientData);

    CXCursorKind cursorKind = clang_getCursorKind(cursor);

    // Track name qualification...
    const QString &spelling = getQString(clang_getCursorSpelling(cursor));
    const unsigned parentHash = clang_hashCursor(parentCursor);
    const unsigned currentHash = clang_hashCursor(cursor);
    if (cursorKind == CXCursor_ClassDecl
            || cursorKind == CXCursor_StructDecl
            || cursorKind == CXCursor_Namespace) {
        // @TODO: Treat anonymous...
        QString qualification;
        if (visitorData->m_qualification.contains(parentHash)) {
            qualification = visitorData->m_qualification.value(parentHash);
            qualification.append("::");
        }
        qualification.append(spelling);
        visitorData->m_qualification[currentHash] = qualification;
    } else {
        visitorData->m_qualification[currentHash] = visitorData->m_qualification.value(parentHash);
    }
    QString currentQualification = visitorData->m_qualification.value(currentHash);

    bool isDefinition = false;
    // Identify symbol...
    if (clang_isDeclaration(cursorKind)) {
        CXCursor cursorDefinition = clang_getCursorDefinition(cursor);
        if (!clang_equalCursors(cursorDefinition, clang_getNullCursor())
                && clang_equalCursors(cursor, cursorDefinition)
                && !spelling.trimmed().isEmpty()) {
            isDefinition = true;
            IndexedSymbolInfo symbolInfo;
            getInstantiationLocation(clang_getCursorLocation(cursor),
                                     &symbolInfo.m_fileName,
                                     &symbolInfo.m_line,
                                     &symbolInfo.m_column,
                                     &symbolInfo.m_offset);
            --symbolInfo.m_column;

            // We don't track symbols found in a previous pass through the same header.
            // More details in the inclusion visit.
            if (!symbolInfo.m_fileName.trimmed().isEmpty()
                    && (!visitorData->m_proc->m_headers.contains(symbolInfo.m_fileName)
                        || !visitorData->m_proc->m_headers.value(symbolInfo.m_fileName).m_upToDate)) {
                symbolInfo.m_name = spelling;
                symbolInfo.m_qualification = currentQualification;
                //symbolInfo.m_icon
                if (cursorKind == CXCursor_ClassDecl
                        || cursorKind == CXCursor_StructDecl
                        || cursorKind == CXCursor_UnionDecl) {
                    symbolInfo.m_type = IndexedSymbolInfo::Class;
                } else if (cursorKind == CXCursor_FunctionDecl
                           || cursorKind == CXCursor_FunctionTemplate
                           || cursorKind == CXCursor_CXXMethod) {
                    CXCursorKind parentKind = clang_getCursorKind(parentCursor);
                    if (parentKind == CXCursor_ClassDecl || parentKind == CXCursor_StructDecl)
                        symbolInfo.m_type = IndexedSymbolInfo::Method;
                    else
                        symbolInfo.m_type = IndexedSymbolInfo::Function;
                }
                visitorData->m_symbolsInfo.append(symbolInfo);
            }
        }
    }

    if (isDefinition
            || cursorKind == CXCursor_Namespace
            || cursorKind == CXCursor_LinkageSpec
            || cursorKind == CXCursor_UnexposedStmt) {
        return CXChildVisit_Recurse;
    }

    return CXChildVisit_Continue;
}

void IndexerProcessor::process(QFutureInterface<IndexingResult> &interface)
{
    // This range is actually just an approximation.
    interface.setProgressRange(0, m_impls.count() + 1);

    QHash<QString, IndexerPrivate::FileData>::iterator it = m_impls.begin();
    QHash<QString, IndexerPrivate::FileData>::iterator eit = m_impls.end();
    BEGIN_PROFILE_SCOPE(0);
    for (; it != eit; ++it) {
        process(interface, &it.value());
        interface.setProgressValue(interface.progressValue() + 1);
        if (interface.isCanceled())
            return;
    }
    END_PROFILE_SCOPE;

    it = m_headers.begin();
    eit = m_headers.end();
    for (; it != eit; ++it) {
        process(interface, &it.value());
        if (interface.isCanceled())
            break;
    }
}

void IndexerProcessor::process(QFutureInterface<IndexingResult> &interface,
                               IndexerPrivate::FileData *fileData)
{
    if (fileData->m_upToDate)
        return;

    // @TODO: This logic should be made reusable somewhere...
    QVector<QByteArray> args;
    const int argc = fileData->m_compilationOptions.size();
    for (int m = 0; m < argc; ++m)
        args.append(fileData->m_compilationOptions.at(m).toUtf8());
    const char **argv = new const char*[argc];
    for (int m = 0; m < argc; ++m)
        argv[m] = args.at(m).constData();

    const QByteArray fileName(fileData->m_fileName.toUtf8());

    CXTranslationUnit tu = clang_parseTranslationUnit(m_clangIndex,
                                                      fileName.constData(),
                                                      argv, argc,
                                                      // @TODO: unsaved.files, unsaved.count,
                                                      0, 0,
                                                      m_unitManagementOptions);
    delete[] argv;

    // Mark this is file as up-to-date.
    fileData->m_upToDate = true;

    if (!tu)
        return;

#ifdef DEBUG_DIAGNOSTICS
    unsigned numDiagnostics = clang_getNumDiagnostics(tu);
    for (unsigned i = 0; i < numDiagnostics; ++i) {
        CXDiagnostic diagnostic = clang_getDiagnostic(tu, i);
        CXDiagnosticSeverity severity = clang_getDiagnosticSeverity(diagnostic);
        if (severity == CXDiagnostic_Error || severity == CXDiagnostic_Fatal) {
            qDebug() << "Error:" << fileName << " - severity:" << severity;
            qDebug() << "\t"<< getQString(clang_getDiagnosticSpelling(diagnostic));
        }
        clang_disposeDiagnostic(diagnostic);
    }
#endif

    QScopedPointer<AstVisitorData> visitorData(new AstVisitorData(this));
    CXCursor tuCursor = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(tuCursor, IndexerProcessor::astVisit, visitorData.data());

    // Make some symbols available.
    interface.reportResult(IndexingResult(visitorData->m_symbolsInfo, fileData->m_compilationOptions));

    // Track inclusions.
    QScopedPointer<InclusionVisitorData> inclusionData(
                new InclusionVisitorData(this,
                                         fileData->m_compilationOptions));
    clang_getInclusions(tu, IndexerProcessor::inclusionVisit, inclusionData.data());

    clang_disposeTranslationUnit(tu);
}



IndexerPrivate::IndexerPrivate()
    : m_files(TotalFileTypes)
{
    connect(&m_indexingWatcher, SIGNAL(resultReadyAt(int)), this, SLOT(synchronize(int)));
}

void IndexerPrivate::run()
{
    IndexerProcessor *processor = new IndexerProcessor(m_files.value(HeaderFile),
                                                       m_files.value(ImplementationFile));
    QFuture<IndexingResult> future = QtConcurrent::run(&IndexerProcessor::process, processor);
    m_indexingWatcher.setFuture(future);
    connect(&m_indexingWatcher, SIGNAL(finished()), processor, SLOT(deleteLater()));
}

void IndexerPrivate::cancel(bool wait)
{
    m_indexingWatcher.cancel();
    if (wait)
        m_indexingWatcher.waitForFinished();
}

void IndexerPrivate::synchronize(int resultIndex)
{
    const IndexingResult &result = m_indexingWatcher.resultAt(resultIndex);
    foreach (const IndexedSymbolInfo &info, result.m_symbolsInfo) {
        const QString &fileName = info.m_fileName;
        FileType fileType = identifyFileType(fileName);
        if (m_files.value(fileType).contains(fileName)) {
            m_files[fileType][fileName].m_upToDate = true;
        } else {
            m_files[fileType].insert(fileName, FileData(fileName,
                                                        result.m_compilationOptions,
                                                        true));
        }

        // Make the symbol info available in the database.
        m_database.insert(info);
    }
}

bool IndexerPrivate::addFile(const QString &fileName, const QStringList &compilationOptions)
{
    if (m_indexingWatcher.isRunning() || fileName.trimmed().isEmpty())
        return false;

    FileType fileType = identifyFileType(fileName);
    if (m_files.at(fileType).contains(fileName)) {
        m_files[fileType][fileName].m_upToDate = false;
        m_database.remove(FileNameKey(), fileName);
    } else {
        m_files[fileType].insert(fileName, FileData(fileName, compilationOptions));
    }

    return true;
}

QStringList IndexerPrivate::getAllFiles() const
{
    QStringList all;
    populateFileNames(&all, m_files.at(ImplementationFile).values());
    populateFileNames(&all, m_files.at(HeaderFile).values());
    return all;
}

IndexerPrivate::FileType IndexerPrivate::identifyFileType(const QString &fileName)
{
    // @TODO: Others
    if (fileName.endsWith(".cpp")
            || fileName.endsWith(".cxx")
            || fileName.endsWith(".cc")
            || fileName.endsWith(".c")) {
        return ImplementationFile;
    } else if (fileName.endsWith(".h")
               || fileName.endsWith(".hpp")
               || fileName.endsWith(".hxx")
               || fileName.endsWith(".hh")) {
        return HeaderFile;
    }
    return UnknownFile;
}

void IndexerPrivate::populateFileNames(QStringList *all, const QList<FileData> &data)
{
    foreach (const FileData &fileData, data)
        all->append(fileData.m_fileName);
}

Indexer::Indexer()
    : m_d(new IndexerPrivate)
{}

Indexer::~Indexer()
{}

void Indexer::regenerate()
{
    m_d->run();
}

bool Indexer::isWorking() const
{
    return m_d->m_indexingWatcher.isRunning();
}

QFuture<void> Indexer::workingFuture() const
{
    return m_d->m_indexingWatcher.future();
}

void Indexer::stopWorking(bool waitForFinished)
{
    return m_d->cancel(waitForFinished);
}

bool Indexer::addFile(const QString &fileName, const QStringList &compilationOptions)
{
    return m_d->addFile(fileName, compilationOptions);
}

QStringList Indexer::getAllFiles() const
{
    return m_d->getAllFiles();
}

QList<IndexedSymbolInfo> Indexer::getAllFunctions() const
{
    return m_d->m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Function);
}

QList<IndexedSymbolInfo> Indexer::getAllClasses() const
{
    return m_d->m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Class);
}

QList<IndexedSymbolInfo> Indexer::getAllMethods() const
{
    return m_d->m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Method);
}

QList<IndexedSymbolInfo> Indexer::getFunctionsFromFile(const QString &fileName) const
{
    return m_d->m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Function,
                                  FileNameKey(), fileName);
}

QList<IndexedSymbolInfo> Indexer::getClassesFromFile(const QString &fileName) const
{
    return m_d->m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Class,
                                  FileNameKey(), fileName);
}

QList<IndexedSymbolInfo> Indexer::getMethodsFromFile(const QString &fileName) const
{
    return m_d->m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Method,
                                  FileNameKey(), fileName);
}

QList<IndexedSymbolInfo> Indexer::getAllFromFile(const QString &fileName) const
{
    return m_d->m_database.values(FileNameKey(), fileName);
}

#include "indexer.moc"
