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
#include "index.h"
#include "reuse.h"
#include "cxraii.h"
#include "sourcelocation.h"
#include "liveunitsmanager.h"
#include "dependencygraph.h"

#include <clang-c/Index.h>

#include <qtconcurrent/QtConcurrentTools>

#include <utils/fileutils.h>

#include <QtCore/QDebug>
#include <QtCore/QVector>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QFuture>
#include <QtCore/QTime>
#include <QtCore/QtConcurrentMap>
#include <QtCore/QThread>
#include <QtCore/QDateTime>
#include <QStringBuilder>

//#define DEBUG
//#define DEBUG_DIAGNOSTICS

#ifdef DEBUG
    #define BEGIN_PROFILE_SCOPE(ID) { ScopepTimer t(ID);
    #define END_PROFILE_SCOPE }
#else
    #define BEGIN_PROFILE_SCOPE(ID)
    #define END_PROFILE_SCOPE
#endif

using namespace Clang;
using namespace Internal;

namespace Clang {

// The indexing result, containing the symbols found, reported by the indexer processor.
struct IndexingResult
{
    IndexingResult(const QVector<Symbol> &symbol,
                   const QSet<QString> &processedFiles,
                   const Unit &unit)
        : m_symbolsInfo(symbol)
        , m_processedFiles(processedFiles)
        , m_unit(unit)
    {}
    QVector<Symbol> m_symbolsInfo;
    QSet<QString> m_processedFiles;
    Unit m_unit;
};

class IndexerPrivate : public QObject
{
    Q_OBJECT
public:
    IndexerPrivate(Indexer *indexer);

    // This enumeration is used to index a vector. So be careful when changing.
    enum FileType {
        ImplementationFile = 0,
        HeaderFile,
        TotalFileTypes
    };

    struct FileData
    {
        FileData() : m_pchInfo(PCHInfo::createEmpty()), m_upToDate(false) {}
        FileData(const QString &fileName,
                 const QStringList &compilationOptions,
                 const PCHInfoPtr pchInfo,
                 bool upToDate = false)
            : m_fileName(fileName)
            , m_compilationOptions(compilationOptions)
            , m_pchInfo(pchInfo)
            , m_upToDate(upToDate)
            , m_managementOptions(CXTranslationUnit_DetailedPreprocessingRecord
                                  || CXTranslationUnit_Incomplete)
        {}

        QString m_fileName;
        QStringList m_compilationOptions;
        PCHInfoPtr m_pchInfo;
        bool m_upToDate;
        unsigned m_managementOptions;
    };

public slots:
    void synchronize(int resultIndex);
    void indexingFinished();
    void dependencyGraphComputed();
    void restoredSymbolsAnalysed();

public:
    enum IndexingMode {
        RelaxedIndexing,        // Index symbols from any file.
        ConstrainedIndexing     // Index symbols only from the requested files.
    };

    void startLoading();
    void concludeLoading();

    void computeDependencyGraph();
    void analyzeRestoredSymbols();

    void run();
    void run(const QStringList &fileNames);
    QFuture<IndexingResult> runCore(const QHash<QString, FileData> &headers,
                                    const QHash<QString, FileData> &impls,
                                    IndexingMode mode);
    bool isBusy() const;
    void cancel(bool wait);
    void reset();

    bool addFile(const QString &fileName,
                 const QStringList &compilationOptions,
                 PCHInfoPtr pchInfo);
    void addOrUpdateFileData(const QString &fileName,
                             const QStringList &compilationOptions,
                             PCHInfoPtr pchInfo,
                             bool upToDate);
    QStringList allFiles() const;
    bool isTrackingFile(const QString &fileName, FileType type) const;
    static FileType identifyFileType(const QString &fileName);
    static void populateFileNames(QStringList *all, const QList<FileData> &data);
    QStringList compilationOptions(const QString &fileName) const;

    bool deserealizeSymbols();
    void serializeSymbols() const;

    QList<Symbol> symbols(Symbol::Kind kind) const;
    QList<Symbol> symbols(const QString &fileName, const Symbol::Kind kind) const;

    Indexer *m_q;
    QVector<QHash<QString, FileData> > m_files;
    Index m_index;
    QFutureWatcher<IndexingResult> m_indexingWatcher;
    QSet<int> m_processedResults;
    bool m_hasQueuedFullRun;
    QSet<QString> m_queuedFilesRun;
    QString m_storagePath;
    bool m_isLoaded;
    DependencyGraph m_dependencyGraph;
    QScopedPointer<QFutureWatcher<void> >m_loadingWatcher;
};


// The indexer processor, which actually compiles the translation units, visits the AST,
// collects the symbols, and eventually report them to the indexer. It relies only on local data.
class IndexerProcessor : public QObject
{
    Q_OBJECT

public:
    typedef QHash<QString, IndexerPrivate::FileData> FileCont;
    typedef QHash<QString, IndexerPrivate::FileData>::iterator FileContIt;

    IndexerProcessor(const FileCont &headers,
                     const FileCont &impls,
                     IndexerPrivate::IndexingMode mode);

    static void inclusionVisit(CXFile file,
                               CXSourceLocation *,
                               unsigned,
                               CXClientData clientData);
    static CXChildVisitResult astVisit(CXCursor cursor,
                                       CXCursor parentCursor,
                                       CXClientData clientData);

    struct ComputeTranslationUnit
    {
        typedef Unit result_type;

        ComputeTranslationUnit(QFutureInterface<IndexingResult> *interface)
            : m_interface(interface)
        {}

        Unit operator()(FileContIt it);

        QFutureInterface<IndexingResult> *m_interface;
    };

    struct ComputeIndexingInfo
    {
        typedef int result_type;

        ComputeIndexingInfo(QFutureInterface<IndexingResult> *interface,
                            IndexerProcessor *proc)
            : m_proc(proc)
            , m_interface(interface)
        {}

        void operator()(int, Unit unit);

        IndexerProcessor *m_proc;
        QFutureInterface<IndexingResult> *m_interface;
        QHash<unsigned, QString> m_qualification;
        QVector<Symbol> m_symbols;
        QSet<QString> m_includes;
    };

    void process(QFutureInterface<IndexingResult> &interface);

    static const int kProcessingBatchSize;

    FileCont m_currentFiles;
    QHash<QString, FileContIt> m_knownHeaders;
    QSet<QString> m_newlySeenHeaders;
    IndexerPrivate::IndexingMode m_mode;
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

const int IndexerProcessor::kProcessingBatchSize = qMax(1, (QThread::idealThreadCount() - 1) * 2);

Unit IndexerProcessor::ComputeTranslationUnit::operator()(FileContIt it)
{
    const IndexerPrivate::FileData &fileData = it.value();

    if (fileData.m_upToDate || m_interface->isCanceled())
        return Unit();

    Unit unit(fileData.m_fileName);
    unit.setCompilationOptions(fileData.m_compilationOptions);
    unit.setPchInfo(fileData.m_pchInfo);
    unit.setManagementOptions(fileData.m_managementOptions);
    unit.setUnsavedFiles(UnsavedFiles()); // @TODO: Consider unsaved files...
    unit.parse();

    // Progress values are not really accurate.
    m_interface->setProgressValue(m_interface->progressValue() + 1);

    return unit;
}

void IndexerProcessor::ComputeIndexingInfo::operator()(int, Unit unit)
{
    if (!unit.isLoaded())
        return;

    if (m_interface->isCanceled()) {
        unit.makeUnique();
        return;
    }

    m_qualification.clear();
    m_symbols.clear();
    m_symbols.reserve(10); // Reasonable guess...?
    m_includes.clear();

#ifdef DEBUG_DIAGNOSTICS
    unsigned numDiagnostics = unit.getNumDiagnostics();
    for (unsigned i = 0; i < numDiagnostics; ++i) {
        ScopedCXDiagnostic diagnostic(unit.getDiagnostic(i));
        if (!diagnostic)
            continue;
        CXDiagnosticSeverity severity = clang_getDiagnosticSeverity(diagnostic);
        if (severity == CXDiagnostic_Error || severity == CXDiagnostic_Fatal) {
            qDebug() << "Error:" << unit.fileName() << " - severity:" << severity;
            qDebug() << "\t"<< getQString(clang_getDiagnosticSpelling(diagnostic));
        }
    }
#endif

    // Visit to gather symbol info.
    clang_visitChildren(unit.getTranslationUnitCursor(), IndexerProcessor::astVisit, this);

    // Mark the file for this unit as up-to-date.
    m_proc->m_currentFiles[unit.fileName()].m_upToDate = true;

    // We also mark any other header seen during this pass as up-to-date. This would not be
    // strictly correct, but it's the general approach we use in the model in order to try
    // to speed up things.
    unit.getInclusions(IndexerProcessor::inclusionVisit, &m_includes);
    foreach (const QString &fileName, m_includes) {
        if (m_proc->m_currentFiles.contains(fileName))
            m_proc->m_currentFiles[fileName].m_upToDate = true;
        else
            m_proc->m_newlySeenHeaders.insert(fileName);
    }

    // Make symbols available.
    m_interface->reportResult(IndexingResult(m_symbols, m_includes, unit));
}

IndexerProcessor::IndexerProcessor(const FileCont &headers,
                                   const FileCont &impls,
                                   IndexerPrivate::IndexingMode mode)
    : m_currentFiles(impls)
    , m_mode(mode)
{
    // Headers are processed later so we keep track of them separately.
    foreach (const IndexerPrivate::FileData &fileData, headers) {
        FileContIt it = m_currentFiles.insert(fileData.m_fileName, fileData);
        m_knownHeaders.insert(fileData.m_fileName, it);
    }
}

void IndexerProcessor::inclusionVisit(CXFile file,
                                      CXSourceLocation *,
                                      unsigned,
                                      CXClientData clientData)
{
    const QString &fileName = normalizeFileName(getQString(clang_getFileName(file)));
    IndexerPrivate::FileType fileType = IndexerPrivate::identifyFileType(fileName);
    if (fileType != IndexerPrivate::HeaderFile)
        return;

    QSet<QString> *seenHeaders = static_cast<QSet<QString> *>(clientData);
    seenHeaders->insert(fileName);
}

CXChildVisitResult IndexerProcessor::astVisit(CXCursor cursor,
                                              CXCursor parentCursor,
                                              CXClientData clientData)
{
    ComputeIndexingInfo *visitorData = static_cast<ComputeIndexingInfo *>(clientData);

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
            qualification = visitorData->m_qualification.value(parentHash) %
                    QLatin1String("::") % spelling;
        } else {
            qualification = spelling;
        }
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
            Symbol symbolInfo;
            symbolInfo.m_location = getSpellingLocation(clang_getCursorLocation(cursor));

            // We don't track symbols found in a previous pass through the same header.
            const QString &fileName = symbolInfo.m_location.fileName();
            if (!fileName.trimmed().isEmpty()
                    && !visitorData->m_proc->m_newlySeenHeaders.contains(fileName)
                    && ((!visitorData->m_proc->m_currentFiles.contains(fileName)
                         && visitorData->m_proc->m_mode == IndexerPrivate::RelaxedIndexing)
                        || (visitorData->m_proc->m_currentFiles.contains(fileName)
                            && !visitorData->m_proc->m_currentFiles.value(fileName).m_upToDate))) {
                symbolInfo.m_name = spelling;
                symbolInfo.m_qualification = currentQualification;

                if (cursorKind == CXCursor_ClassDecl
                        || cursorKind == CXCursor_StructDecl
                        || cursorKind == CXCursor_UnionDecl
                        || cursorKind == CXCursor_ClassTemplate
                        || cursorKind == CXCursor_ClassTemplatePartialSpecialization) {
                    symbolInfo.m_kind = Symbol::Class;
                } else if (cursorKind == CXCursor_FunctionDecl) {
                    symbolInfo.m_kind = Symbol::Function;
                } else if (cursorKind == CXCursor_CXXMethod) {
                    symbolInfo.m_kind = Symbol::Method;
                } else if (cursorKind == CXCursor_FunctionTemplate) {
                    CXCursor semaParent = clang_getCursorSemanticParent(cursor);
                    CXCursorKind semaParentKind = clang_getCursorKind(semaParent);
                    if (semaParentKind == CXCursor_ClassDecl
                            || semaParentKind == CXCursor_StructDecl
                            || semaParentKind == CXCursor_ClassTemplate
                            || semaParentKind == CXCursor_ClassTemplatePartialSpecialization) {
                        symbolInfo.m_kind = Symbol::Method;
                    } else {
                        symbolInfo.m_kind = Symbol::Function;
                    }
                } else if (cursorKind == CXCursor_Constructor) {
                    symbolInfo.m_kind = Symbol::Constructor;
                } else if (cursorKind == CXCursor_Destructor) {
                    symbolInfo.m_kind = Symbol::Destructor;
                }
                visitorData->m_symbols.append(symbolInfo);
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
    BEGIN_PROFILE_SCOPE(0);

    QVector<FileContIt> currentFiles;
    currentFiles.reserve(kProcessingBatchSize);
    FileContIt it = m_currentFiles.begin();
    FileContIt eit = m_currentFiles.end();

    // This range is an approximation (more than one file might be updated in a single pass).
    int outdated = 0;
    for (FileContIt tit = it; tit != eit; ++tit) {
        if (!tit->m_upToDate)
            ++outdated;
    }
    interface.setProgressRange(0, outdated);

    // First process implementation files and then headers which were not included by any unit.
    while (it != eit && !interface.isCanceled()) {
        FileContIt currentIt = it++;
        if (!m_knownHeaders.contains(currentIt->m_fileName)) {
            currentFiles.append(currentIt);
            if (currentFiles.size() == kProcessingBatchSize || it == eit) {
                QtConcurrent::blockingMappedReduced<int>(currentFiles,
                                                         ComputeTranslationUnit(&interface),
                                                         ComputeIndexingInfo(&interface, this),
                                                         QtConcurrent::UnorderedReduce);
                currentFiles.erase(currentFiles.begin(), currentFiles.end()); // Keep the memory.
            }
        }
    }

    const QList<FileContIt> &knownHeaders = m_knownHeaders.values();
    const int total = knownHeaders.size();
    int current = 0;
    while (current < total && !interface.isCanceled()) {
        FileContIt currentIt = knownHeaders.at(current++);
        if (!m_currentFiles.value(currentIt.value().m_fileName).m_upToDate) {
            currentFiles.append(currentIt);
            if (currentFiles.size() == kProcessingBatchSize || current == total) {
                QtConcurrent::blockingMappedReduced<int>(currentFiles,
                                                         ComputeTranslationUnit(&interface),
                                                         ComputeIndexingInfo(&interface, this),
                                                         QtConcurrent::UnorderedReduce);
                currentFiles.erase(currentFiles.begin(), currentFiles.end()); // Keep the memory.
            }
        }
    }

    if (!currentFiles.isEmpty()) {
        QtConcurrent::blockingMappedReduced<int>(currentFiles,
                                                 ComputeTranslationUnit(&interface),
                                                 ComputeIndexingInfo(&interface, this),
                                                 QtConcurrent::UnorderedReduce);
    }

    END_PROFILE_SCOPE;
}


IndexerPrivate::IndexerPrivate(Indexer *indexer)
    : m_q(indexer)
    , m_files(TotalFileTypes)
    , m_hasQueuedFullRun(false)
    , m_isLoaded(false)
    , m_loadingWatcher(new QFutureWatcher<void>)
{
    connect(&m_indexingWatcher, SIGNAL(resultReadyAt(int)), this, SLOT(synchronize(int)));
    connect(&m_indexingWatcher, SIGNAL(finished()), this, SLOT(indexingFinished()));
}

QFuture<IndexingResult> IndexerPrivate::runCore(const QHash<QString, FileData> &headers,
                                                const QHash<QString, FileData> &impls,
                                                IndexingMode mode)
{
    IndexerProcessor *processor = new IndexerProcessor(headers, impls, mode);
    QFuture<IndexingResult> future = QtConcurrent::run(&IndexerProcessor::process, processor);
    connect(&m_indexingWatcher, SIGNAL(finished()), processor, SLOT(deleteLater()));
    m_indexingWatcher.setFuture(future);
    return future;
}

void IndexerPrivate::run()
{
    Q_ASSERT(m_isLoaded);

    if (!m_indexingWatcher.isRunning()) {
        QFuture<IndexingResult> future =
                runCore(m_files.value(HeaderFile),
                        m_files.value(ImplementationFile),
                        RelaxedIndexing);
        emit m_q->indexingStarted(future);
    } else {
        m_hasQueuedFullRun = true;
        m_indexingWatcher.cancel();
    }
}

void IndexerPrivate::run(const QStringList &fileNames)
{
    Q_ASSERT(m_isLoaded);

    if (!m_indexingWatcher.isRunning()) {
        QVector<QHash<QString, FileData> > files(TotalFileTypes);
        foreach (const QString &fileName, fileNames) {
            FileType type = identifyFileType(fileName);
            if (!isTrackingFile(fileName, type)) {
                // @TODO
                continue;
            }

            FileData *data = &m_files[type][fileName];
            data->m_upToDate = false;
            files[type].insert(fileName, *data);
            m_index.removeFile(fileName);
        }
        runCore(files.value(HeaderFile),
                files.value(ImplementationFile),
                ConstrainedIndexing);
    } else {
        m_queuedFilesRun.unite(fileNames.toSet());
    }
}

bool IndexerPrivate::isBusy() const
{
    return m_indexingWatcher.isRunning() || m_loadingWatcher->isRunning();
}

void IndexerPrivate::cancel(bool wait)
{
    m_dependencyGraph.discard();

    m_loadingWatcher->cancel();
    m_indexingWatcher.cancel();
    if (wait) {
        m_loadingWatcher->waitForFinished();
        m_indexingWatcher.waitForFinished();
    }
}

void IndexerPrivate::reset()
{
    cancel(true);
    serializeSymbols();

    for (int i = 0; i < TotalFileTypes; ++i)
        m_files[i].clear();
    m_processedResults.clear();
    m_hasQueuedFullRun = false;
    m_queuedFilesRun.clear();
    m_storagePath.clear();
    m_index.clear();
    m_isLoaded = false;
}

void IndexerPrivate::synchronize(int resultIndex)
{
    IndexingResult result = m_indexingWatcher.resultAt(resultIndex);
    result.m_unit.makeUnique();

    foreach (const Symbol &symbol, result.m_symbolsInfo) {
        addOrUpdateFileData(symbol.m_location.fileName(),
                    result.m_unit.compilationOptions(),
                    result.m_unit.pchInfo(),
                    true);

        // Make the symbol available in the database.
        m_index.insertSymbol(symbol, result.m_unit.timeStamp());
    }

    // There might be files which were processed but did not "generate" any indexable symbol,
    // but we still need to make the index aware of them.
    result.m_processedFiles.insert(result.m_unit.fileName());
    foreach (const QString &fileName, result.m_processedFiles) {
        if (!m_index.containsFile(fileName))
            m_index.insertFile(fileName, result.m_unit.timeStamp());
    }

    // If this unit is being kept alive, update in the manager.
    if (LiveUnitsManager::instance()->isTracking(result.m_unit.fileName()))
        LiveUnitsManager::instance()->updateUnit(result.m_unit.fileName(), result.m_unit);

    m_processedResults.insert(resultIndex);
}

void IndexerPrivate::indexingFinished()
{
    if (m_indexingWatcher.isCanceled()) {
        // There's no API guarantee that results are reported in order, so we are conservative.
        for (int i = m_indexingWatcher.future().resultCount() - 1; i >= 0; --i) {
            if (!m_processedResults.contains(i))
                synchronize(i);
        }
    }
    m_processedResults.clear();

    if (m_hasQueuedFullRun) {
        m_hasQueuedFullRun = false;
        run();
    } else if (!m_queuedFilesRun.isEmpty()) {
        const QStringList &files = m_queuedFilesRun.toList();
        m_queuedFilesRun.clear();
        run(files);
    }

    emit m_q->indexingFinished();
}

void IndexerPrivate::addOrUpdateFileData(const QString &fileName,
                                 const QStringList &compilationOptions,
                                 PCHInfoPtr pchInfo,
                                 bool upToDate)
{
    Q_ASSERT(QDir::isAbsolutePath(fileName));

    QString cleanFileName(normalizeFileName(fileName));

    FileType fileType = identifyFileType(cleanFileName);
    if (isTrackingFile(cleanFileName, fileType)) {
        m_files[fileType][cleanFileName].m_compilationOptions = compilationOptions;
        m_files[fileType][cleanFileName].m_pchInfo = pchInfo;
        m_files[fileType][cleanFileName].m_upToDate = upToDate;
    } else {
        m_files[fileType].insert(cleanFileName,
                                 FileData(cleanFileName, compilationOptions, pchInfo, upToDate));
    }

    if (!upToDate)
        m_index.removeFile(cleanFileName);
}

bool IndexerPrivate::addFile(const QString &fileName,
                             const QStringList &compilationOptions,
                             PCHInfoPtr pchInfo)
{
    if (isBusy()
            || fileName.trimmed().isEmpty()
            || !QFileInfo(fileName).isFile())
        return false;

    addOrUpdateFileData(fileName, compilationOptions, pchInfo, false);

    return true;
}

QStringList IndexerPrivate::allFiles() const
{
    QStringList all;
    populateFileNames(&all, m_files.at(ImplementationFile).values());
    populateFileNames(&all, m_files.at(HeaderFile).values());
    return all;
}

bool IndexerPrivate::isTrackingFile(const QString &fileName, FileType type) const
{
    return m_files.value(type).contains(normalizeFileName(fileName));
}

QStringList IndexerPrivate::compilationOptions(const QString &fileName) const
{
    FileType type = identifyFileType(fileName);
    return m_files.value(type).value(normalizeFileName(fileName)).m_compilationOptions;
}

IndexerPrivate::FileType IndexerPrivate::identifyFileType(const QString &fileName)
{
    if (fileName.endsWith(".cpp")
            || fileName.endsWith(".cxx")
            || fileName.endsWith(".cc")
            || fileName.endsWith(".c")) {
        return ImplementationFile;
    }

    // Everything that is not an implementation file is treated as a header. This makes things
    // easier when handling standard library files and any other file that does not use
    // conventional suffixes.
    return HeaderFile;
}

void IndexerPrivate::populateFileNames(QStringList *all, const QList<FileData> &data)
{
    foreach (const FileData &fileData, data)
        all->append(fileData.m_fileName);
}


namespace {

struct DepedencyVisitor
{
    DepedencyVisitor(IndexerPrivate *indexer) : m_indexer(indexer) {}

    bool acceptFile(const QString &includer)
    {
        IndexerPrivate::FileType fileType = IndexerPrivate::identifyFileType(includer);
        if (m_indexer->isTrackingFile(includer, fileType)) {
            m_match = m_indexer->m_files.at(fileType).value(includer);
            return true;
        }
        return false;
    }

    IndexerPrivate *m_indexer;
    IndexerPrivate::FileData m_match;
};

} // Anonymous

void IndexerPrivate::startLoading()
{
    // In the case of existent persisted symbols, we restore them and make them visible
    // to the indexer. However, we need a dependency graph in order to identify the proper
    // options.

    if (deserealizeSymbols() && !m_index.isEmpty())
        computeDependencyGraph();
    else
        concludeLoading();
}

void IndexerPrivate::concludeLoading()
{
    m_isLoaded = true;
    run();
}

void IndexerPrivate::computeDependencyGraph()
{
    for (int fileType = ImplementationFile; fileType < TotalFileTypes; ++fileType) {
        QHash<QString, FileData>::iterator it = m_files[fileType].begin();
        for (; it != m_files[fileType].end(); ++it)
            m_dependencyGraph.addFile(it.value().m_fileName, it.value().m_compilationOptions);
    }

    m_loadingWatcher.reset(new QFutureWatcher<void>);
    connect(m_loadingWatcher.data(), SIGNAL(finished()), this, SLOT(dependencyGraphComputed()));
    m_loadingWatcher->setFuture(m_dependencyGraph.compute());
}

void IndexerPrivate::dependencyGraphComputed()
{
    if (m_loadingWatcher->isCanceled())
        return;

    m_loadingWatcher.reset(new QFutureWatcher<void>);
    connect(m_loadingWatcher.data(), SIGNAL(finished()), this, SLOT(restoredSymbolsAnalysed()));
    m_loadingWatcher->setFuture(QtConcurrent::run(this, &IndexerPrivate::analyzeRestoredSymbols));
}

void IndexerPrivate::analyzeRestoredSymbols()
{
    // @TODO: We only check for time stamps, so we still need to handle somehow the case in
    // which the project options (for example a .pro file) changed while "outside" a Creator
    // session.

    foreach (const QString &fileName, m_index.files()) {
        bool upToDate = m_index.validate(fileName);

        FileType fileType = identifyFileType(fileName);
        if (isTrackingFile(fileName, fileType)) {
            // When the file is already being tracked we simply need to update its state.
            if (upToDate)
                m_files[fileType][fileName].m_upToDate = true;
        } else {
            // If it's not being tracked we need to find at least one tracked dependency
            // so we can use its options.
            DepedencyVisitor visitor(this);
            m_dependencyGraph.collectDependencies(fileName,
                                                  DependencyGraph::FilesWhichInclude,
                                                  &visitor);
            if (!visitor.m_match.m_fileName.isEmpty()) {
                addOrUpdateFileData(fileName,
                                    visitor.m_match.m_compilationOptions,
                                    visitor.m_match.m_pchInfo,
                                    upToDate);
            } else {
                m_index.removeFile(fileName);
            }
        }

        if (!upToDate && m_index.containsFile(fileName))
            m_index.removeFile(fileName);
    }
}

void IndexerPrivate::restoredSymbolsAnalysed()
{
    if (m_loadingWatcher->isCanceled())
        return;

    concludeLoading();
}

bool IndexerPrivate::deserealizeSymbols()
{
    if (m_storagePath.isEmpty())
        return false;

    Utils::FileReader reader;
    if (reader.fetch(m_storagePath)) {
        m_index.deserialize(reader.data());
        return true;
    }

    return false;
}

void IndexerPrivate::serializeSymbols() const
{
    if (m_storagePath.isEmpty())
        return;

    Utils::FileSaver saver(m_storagePath);
    saver.write(m_index.serialize());
    if (!saver.finalize())
        qWarning("Failed to serialize index");
}

QList<Symbol> IndexerPrivate::symbols(Symbol::Kind kind) const
{
    if (m_loadingWatcher->isRunning())
        return QList<Symbol>();

    return m_index.symbols(kind);
}

QList<Symbol> IndexerPrivate::symbols(const QString &fileName, const Symbol::Kind kind) const
{
    if (m_loadingWatcher->isRunning())
        return QList<Symbol>();

    if (kind == Symbol::Unknown)
        return m_index.symbols(fileName);

    return m_index.symbols(fileName, kind);
}


Indexer::Indexer()
    : m_d(new IndexerPrivate(this))
{}

Indexer::~Indexer()
{}

void Indexer::regenerate()
{
    if (!m_d->m_isLoaded) {
        if (m_d->m_loadingWatcher->isRunning())
            return;
        m_d->startLoading();
    } else {
        m_d->run();
    }
}

void Indexer::initialize(const QString &storagePath)
{
    Q_ASSERT(!m_d->m_isLoaded);

    m_d->m_storagePath = storagePath;
}

void Indexer::evaluateFile(const QString &fileName)
{
    if (!m_d->m_isLoaded)
        return;

    m_d->run(QStringList(normalizeFileName(fileName)));
}

bool Indexer::isBusy() const
{
    return m_d->isBusy();
}

void Indexer::cancel(bool waitForFinished)
{
    return m_d->cancel(waitForFinished);
}

void Indexer::finalize()
{
    m_d->reset();
}

bool Indexer::addFile(const QString &fileName, const QStringList &compilationOptions, Clang::PCHInfoPtr pchInfo)
{
    return m_d->addFile(fileName, compilationOptions, pchInfo);
}

QStringList Indexer::allFiles() const
{
    return m_d->allFiles();
}

QStringList Indexer::compilationOptions(const QString &fileName) const
{
    return m_d->compilationOptions(fileName);
}

QList<Symbol> Indexer::allFunctions() const
{
    return m_d->symbols(Symbol::Function);
}

QList<Symbol> Indexer::allClasses() const
{
    return m_d->symbols(Symbol::Class);
}

QList<Symbol> Indexer::allMethods() const
{
    return m_d->symbols(Symbol::Method);
}

QList<Symbol> Indexer::allConstructors() const
{
    return m_d->symbols(Symbol::Constructor);
}

QList<Symbol> Indexer::allDestructors() const
{
    return m_d->symbols(Symbol::Destructor);
}

QList<Symbol> Indexer::functionsFromFile(const QString &fileName) const
{
    return m_d->symbols(fileName, Symbol::Function);
}

QList<Symbol> Indexer::classesFromFile(const QString &fileName) const
{
    return m_d->symbols(fileName, Symbol::Class);
}

QList<Symbol> Indexer::methodsFromFile(const QString &fileName) const
{
    return m_d->symbols(fileName, Symbol::Method);
}

QList<Symbol> Indexer::constructorsFromFile(const QString &fileName) const
{
    return m_d->symbols(fileName, Symbol::Constructor);
}

QList<Symbol> Indexer::destructorsFromFile(const QString &fileName) const
{
    return m_d->symbols(fileName, Symbol::Destructor);
}

QList<Symbol> Indexer::allFromFile(const QString &fileName) const
{
    return m_d->symbols(fileName, Symbol::Unknown);
}

#include "indexer.moc"
