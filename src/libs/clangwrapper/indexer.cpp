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
#include "cxraii.h"
#include "sourcelocation.h"
#include "liveunitsmanager.h"

#include <clang-c/Index.h>

#include <qtconcurrent/QtConcurrentTools>

#include <QtCore/QDebug>
#include <QtCore/QVector>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QFileInfo>
#include <QtCore/QFuture>
#include <QtCore/QTime>
#include <QtCore/QtConcurrentMap>
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

// Create tags and speclizations in order to make IndexedSymbolInfo satisfy
// the requirements of the database.
struct FileNameKey { typedef QString ValueType; };
template <>
inline QString getKey<FileNameKey>(const IndexedSymbolInfo &info)
{ return info.m_location.fileName(); }

struct SymbolTypeKey { typedef IndexedSymbolInfo::SymbolType ValueType; };
template <>
inline IndexedSymbolInfo::SymbolType getKey<SymbolTypeKey>(const IndexedSymbolInfo &info)
{ return info.m_type; }


// The indexing result, containing the symbols found, reported by the indexer processor.
struct IndexingResult
{
    IndexingResult(const QVector<IndexedSymbolInfo> &info,
                   const Unit &unit)
        : m_symbolsInfo(info)
        , m_unit(unit)
    {}
    QVector<IndexedSymbolInfo> m_symbolsInfo;
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
        UnknownFile,
        TotalFileTypes
    };

    struct FileData
    {
        FileData() : m_pchInfo(PCHInfo::createEmpty()), m_upToDate(false) {}
        FileData(const QString &fileName,
                 const QStringList &compilationOptions,
                 const PCHInfoPtr pchInfo,
                 unsigned managementOptions = CXTranslationUnit_DetailedPreprocessingRecord,
                 bool upToDate = false)
            : m_fileName(fileName)
            , m_compilationOptions(compilationOptions)
            , m_pchInfo(pchInfo)
            , m_managementOptions(managementOptions)
            , m_upToDate(upToDate)
        {}

        QString m_fileName;
        QStringList m_compilationOptions;
        PCHInfoPtr m_pchInfo;
        unsigned m_managementOptions;
        bool m_upToDate;
    };

public slots:
    void synchronize(int resultIndex);
    void indexingFinished();

public:
    void run();
    void run(const QStringList &fileNames);
    QFuture<IndexingResult> runCore(const QHash<QString, FileData> &headers,
                                    const QHash<QString, FileData> &impls);
    void cancel(bool wait);
    void clear();

    bool addFile(const QString &fileName, const QStringList &compilationOptions, PCHInfoPtr pchInfo);
    QStringList allFiles() const;
    bool isTrackingFile(const QString &fileName) const;
    QStringList compilationOptions(const QString &fileName) const;
    static FileType identifyFileType(const QString &fileName);
    static void populateFileNames(QStringList *all, const QList<FileData> &data);

    Indexer *m_q;
    QVector<QHash<QString, FileData> > m_files;
    Database<IndexedSymbolInfo, FileNameKey, SymbolTypeKey> m_database;
    QFutureWatcher<IndexingResult> m_indexingWatcher;
    QSet<int> m_processedResults;
    bool m_hasQueuedFullRun;
    QSet<QString> m_queuedFilesRun;
};


// The indexer processor, which actually compiles the translation units, visits the AST,
// collects the symbols, and eventually report them to the indexer. It relies only on local data.
class IndexerProcessor : public QObject
{
    Q_OBJECT

public:
    typedef QHash<QString, IndexerPrivate::FileData> FileCont;
    typedef QHash<QString, IndexerPrivate::FileData>::iterator FileContIt;

    IndexerProcessor(const FileCont &headers, const FileCont &impls);

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
        QVector<IndexedSymbolInfo> m_symbolsInfo;
    };

    void process(QFutureInterface<IndexingResult> &interface);

    static const int kProcessingBatchSize;

    FileCont m_allFiles;
    QHash<QString, FileContIt> m_knownHeaders;
    QSet<QString> m_newlySeenHeaders;
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

const int IndexerProcessor::kProcessingBatchSize = 4;

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
    if (!unit.isValid())
        return;

    if (m_interface->isCanceled()) {
        unit.invalidate();
        return;
    }

    m_qualification.clear();
    m_symbolsInfo.clear();
    m_symbolsInfo.reserve(10); // Reasonable guess...?

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

    // Mark this is file as up-to-date.
    m_proc->m_allFiles[unit.fileName()].m_upToDate = true;

    // Track inclusions.
    unit.getInclusions(IndexerProcessor::inclusionVisit, this);

    // Make symbols available.
    m_interface->reportResult(IndexingResult(m_symbolsInfo, unit));
}

IndexerProcessor::IndexerProcessor(const FileCont &headers, const FileCont &impls)
    : m_allFiles(impls)
{
    // Headers are processed later so we keep track of them separately.
    foreach (const IndexerPrivate::FileData &fileData, headers) {
        FileContIt it = m_allFiles.insert(fileData.m_fileName, fileData);
        m_knownHeaders.insert(fileData.m_fileName, it);
    }
}

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
    ComputeIndexingInfo *inclusionData = static_cast<ComputeIndexingInfo *>(clientData);
    if (inclusionData->m_proc->m_allFiles.contains(fileName))
        inclusionData->m_proc->m_allFiles[fileName].m_upToDate = true;
    else
        inclusionData->m_proc->m_newlySeenHeaders.insert(fileName);
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
            IndexedSymbolInfo symbolInfo;
            symbolInfo.m_location = getInstantiationLocation(clang_getCursorLocation(cursor));

            // We don't track symbols found in a previous pass through the same header.
            // More details in the inclusion visit.
            const QString &fileName = symbolInfo.m_location.fileName();
            if (!fileName.trimmed().isEmpty()
                    && !visitorData->m_proc->m_newlySeenHeaders.contains(fileName)
                    && visitorData->m_proc->m_allFiles.contains(fileName)
                    && !visitorData->m_proc->m_allFiles.value(fileName).m_upToDate) {
                symbolInfo.m_name = spelling;
                symbolInfo.m_qualification = currentQualification;
                //symbolInfo.m_icon
                if (cursorKind == CXCursor_ClassDecl
                        || cursorKind == CXCursor_StructDecl
                        || cursorKind == CXCursor_UnionDecl
                        || cursorKind == CXCursor_ClassTemplate
                        || cursorKind == CXCursor_ClassTemplatePartialSpecialization) {
                    symbolInfo.m_type = IndexedSymbolInfo::Class;
                } else if (cursorKind == CXCursor_FunctionDecl) {
                    symbolInfo.m_type = IndexedSymbolInfo::Function;
                } else if (cursorKind == CXCursor_CXXMethod) {
                    symbolInfo.m_type = IndexedSymbolInfo::Method;
                } else if (cursorKind == CXCursor_FunctionTemplate) {
                    CXCursor semaParent = clang_getCursorSemanticParent(cursor);
                    CXCursorKind semaParentKind = clang_getCursorKind(semaParent);
                    if (semaParentKind == CXCursor_ClassDecl
                            || semaParentKind == CXCursor_StructDecl
                            || semaParentKind == CXCursor_ClassTemplate
                            || semaParentKind == CXCursor_ClassTemplatePartialSpecialization) {
                        symbolInfo.m_type = IndexedSymbolInfo::Method;
                    } else {
                        symbolInfo.m_type = IndexedSymbolInfo::Function;
                    }
                } else if (cursorKind == CXCursor_Constructor) {
                    symbolInfo.m_type = IndexedSymbolInfo::Constructor;
                } else if (cursorKind == CXCursor_Destructor) {
                    symbolInfo.m_type = IndexedSymbolInfo::Destructor;
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
    BEGIN_PROFILE_SCOPE(0);

    // This range is actually just an approximation.
    interface.setProgressRange(0, m_allFiles.count() + 1);

    // First process implementation files and then headers which were not included by any unit.
    QVector<FileContIt> currentFiles;
    currentFiles.reserve(kProcessingBatchSize);
    FileContIt it = m_allFiles.begin();
    FileContIt eit = m_allFiles.end();
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
        if (!m_allFiles.value(currentIt.value().m_fileName).m_upToDate) {
            currentFiles.append(currentIt);
            if (currentFiles.size() > kProcessingBatchSize || current == total) {
                QtConcurrent::blockingMappedReduced<int>(currentFiles,
                                                         ComputeTranslationUnit(&interface),
                                                         ComputeIndexingInfo(&interface, this),
                                                         QtConcurrent::UnorderedReduce);
                currentFiles.erase(currentFiles.begin(), currentFiles.end()); // Keep the memory.
            }
        }
    }

    END_PROFILE_SCOPE;
}


IndexerPrivate::IndexerPrivate(Indexer *indexer)
    : m_q(indexer)
    , m_files(TotalFileTypes)
    , m_hasQueuedFullRun(false)
{
    connect(&m_indexingWatcher, SIGNAL(resultReadyAt(int)), this, SLOT(synchronize(int)));
    connect(&m_indexingWatcher, SIGNAL(finished()), this, SLOT(indexingFinished()));
}

QFuture<IndexingResult> IndexerPrivate::runCore(const QHash<QString, FileData> &headers,
                                                const QHash<QString, FileData> &impls)
{
    IndexerProcessor *processor = new IndexerProcessor(headers, impls);
    QFuture<IndexingResult> future = QtConcurrent::run(&IndexerProcessor::process, processor);
    connect(&m_indexingWatcher, SIGNAL(finished()), processor, SLOT(deleteLater()));
    m_indexingWatcher.setFuture(future);
    return future;
}

void IndexerPrivate::run()
{
    if (!m_indexingWatcher.isRunning()) {
        QFuture<IndexingResult> future =
                runCore(m_files.value(HeaderFile), m_files.value(ImplementationFile));
        emit m_q->indexingStarted(future);
    } else {
        m_hasQueuedFullRun = true;
        m_indexingWatcher.cancel();
    }
}

void IndexerPrivate::run(const QStringList &fileNames)
{
    if (!m_indexingWatcher.isRunning()) {
        QVector<QHash<QString, FileData> > files(TotalFileTypes);
        foreach (const QString &fileName, fileNames) {
            if (!isTrackingFile(fileName)) {
                // @TODO
                continue;
            }
            m_database.remove(FileNameKey(), fileName);
            FileType type = identifyFileType(fileName);
            FileData *data = &m_files[type][fileName];
            data->m_upToDate = false;

            files[type].insert(fileName, *data);
        }
        runCore(files.value(HeaderFile), files.value(ImplementationFile));
    } else {
        m_queuedFilesRun.unite(fileNames.toSet());
    }
}

void IndexerPrivate::cancel(bool wait)
{
    m_indexingWatcher.cancel();
    if (wait)
        m_indexingWatcher.waitForFinished();
}

void IndexerPrivate::clear()
{
    cancel(true);
    for (int i = 0; i < TotalFileTypes; ++i)
        m_files[i].clear();
    m_database.clear();
}

void IndexerPrivate::synchronize(int resultIndex)
{
    IndexingResult result = m_indexingWatcher.resultAt(resultIndex);
    foreach (const IndexedSymbolInfo &info, result.m_symbolsInfo) {
        const QString &fileName = info.m_location.fileName();
        FileType fileType = identifyFileType(fileName);
        if (m_files.value(fileType).contains(fileName)) {
            m_files[fileType][fileName].m_upToDate = true;
        } else {
            m_files[fileType].insert(fileName, FileData(fileName,
                                                        result.m_unit.compilationOptions(),
                                                        result.m_unit.pchInfo(),
                                                        true));
        }

        // Make the symbol info available in the database.
        m_database.insert(info);
    }

    // If this unit is being kept alive, update the manager. Otherwise invalidate it.
    if (LiveUnitsManager::instance()->isTracking(result.m_unit.fileName()))
        LiveUnitsManager::instance()->updateUnit(result.m_unit.fileName(), result.m_unit);
    else
        result.m_unit.invalidate();

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

bool IndexerPrivate::addFile(const QString &fileName, const QStringList &compilationOptions, PCHInfoPtr pchInfo)
{
    if (m_indexingWatcher.isRunning()
            || fileName.trimmed().isEmpty()
            || !QFileInfo(fileName).exists())
        return false;

    FileType fileType = identifyFileType(fileName);
    if (m_files.at(fileType).contains(fileName)) {
        m_files[fileType][fileName].m_upToDate = false;
        m_database.remove(FileNameKey(), fileName);
    } else {
        m_files[fileType].insert(fileName, FileData(fileName, compilationOptions, pchInfo));
    }

    return true;
}

QStringList IndexerPrivate::allFiles() const
{
    QStringList all;
    populateFileNames(&all, m_files.at(ImplementationFile).values());
    populateFileNames(&all, m_files.at(HeaderFile).values());
    return all;
}

bool IndexerPrivate::isTrackingFile(const QString &fileName) const
{
    FileType type = identifyFileType(fileName);
    return m_files.value(type).contains(fileName);
}

QStringList IndexerPrivate::compilationOptions(const QString &fileName) const
{
    FileType type = identifyFileType(fileName);
    return m_files.value(type).value(fileName).m_compilationOptions;
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
    : m_d(new IndexerPrivate(this))
{}

Indexer::~Indexer()
{}

void Indexer::regenerate()
{
    m_d->run();
}

void Indexer::evaluateFile(const QString &fileName)
{
    m_d->run(QStringList(fileName));
}

bool Indexer::isWorking() const
{
    return m_d->m_indexingWatcher.isRunning();
}

void Indexer::stopWorking(bool waitForFinished)
{
    return m_d->cancel(waitForFinished);
}

void Indexer::destroy()
{
    m_d->clear();
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

QList<IndexedSymbolInfo> Indexer::allFunctions() const
{
    return m_d->m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Function);
}

QList<IndexedSymbolInfo> Indexer::allClasses() const
{
    return m_d->m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Class);
}

QList<IndexedSymbolInfo> Indexer::allMethods() const
{
    return m_d->m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Method);
}

QList<IndexedSymbolInfo> Indexer::allConstructors() const
{
    return m_d->m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Constructor);
}

QList<IndexedSymbolInfo> Indexer::allDestructors() const
{
    return m_d->m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Destructor);
}

QList<IndexedSymbolInfo> Indexer::functionsFromFile(const QString &fileName) const
{
    return m_d->m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Function,
                                  FileNameKey(), fileName);
}

QList<IndexedSymbolInfo> Indexer::classesFromFile(const QString &fileName) const
{
    return m_d->m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Class,
                                  FileNameKey(), fileName);
}

QList<IndexedSymbolInfo> Indexer::methodsFromFile(const QString &fileName) const
{
    return m_d->m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Method,
                                  FileNameKey(), fileName);
}

QList<IndexedSymbolInfo> Indexer::constructorsFromFile(const QString &fileName) const
{
    return m_d->m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Constructor,
                                  FileNameKey(), fileName);
}

QList<IndexedSymbolInfo> Indexer::destructorsFromFile(const QString &fileName) const
{
    return m_d->m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Destructor,
                                  FileNameKey(), fileName);
}

QList<IndexedSymbolInfo> Indexer::allFromFile(const QString &fileName) const
{
    return m_d->m_database.values(FileNameKey(), fileName);
}

#include "indexer.moc"
