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

#include <qtconcurrent/QtConcurrentTools>

#include <QtCore/QScopedPointer>
#include <QtCore/QDebug>
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

struct AstVisitorData
{
    AstVisitorData(const QHash<QString, Indexer::FileData> &headers)
        : m_headerFiles(headers)
    {}
    const QHash<QString, Indexer::FileData> &m_headerFiles;
    QHash<unsigned, QString> m_qualification;
    QList<IndexedSymbolInfo> m_symbolsInfo;
};

struct InclusionVisitorData
{
    QStringList m_includedFiles;
};

void populateFiles(QStringList *all, const QList<Indexer::FileData> &data)
{
    foreach (const Indexer::FileData &fileData, data)
        all->append(fileData.m_fileName);
}

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

Indexer::Indexer()
    : m_files(TotalFileTypes)
    , m_clangIndex(clang_createIndex(/*excludeDeclsFromPCH*/ 0, /*displayDiagnostics*/ 0))
    , m_unitManagementOptions(clang_defaultEditingTranslationUnitOptions())
{
    m_unitManagementOptions &= ~CXTranslationUnit_CacheCompletionResults;
}

Indexer::FileType Indexer::identifyFileType(const QString &fileName)
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

void Indexer::inclusionVisit(CXFile file,
                             CXSourceLocation *,
                             unsigned,
                             CXClientData clientData)
{
    InclusionVisitorData *inclusionData = static_cast<InclusionVisitorData *>(clientData);

    const QString &fileName = getQString(clang_getFileName(file));
    FileType fileType = identifyFileType(fileName);
    if (fileType == HeaderFile)
        inclusionData->m_includedFiles.append(fileName);
}

CXChildVisitResult Indexer::astVisit(CXCursor cursor,
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

            // If the symbol is in a header that we've already parse, we only index the symbol
            // found in the previous pass. See the core process function for more details.
            if ((!visitorData->m_headerFiles.contains(symbolInfo.m_fileName)
                    || !visitorData->m_headerFiles.value(symbolInfo.m_fileName).m_upToDate)
                    && !symbolInfo.m_fileName.trimmed().isEmpty()) {
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
                    symbolInfo.m_type = IndexedSymbolInfo::Method;
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

void Indexer::process(QFutureInterface<void> &futureInterface)
{
    // This range is actually just an approximation.
    futureInterface.setProgressRange(0, m_files.value(ImplementationFile).count() + 1);
    futureInterface.reportStarted();

    QHash<QString, FileData> &headerFiles = m_files[HeaderFile];
    QHash<QString, FileData> &implFiles = m_files[ImplementationFile];
    QHash<QString, FileData>::iterator it = implFiles.begin();
    BEGIN_PROFILE_SCOPE(0);
    for (; it != implFiles.end(); ++it) {
        QStringList includedHeaders;
        process(&it.value(), true, &includedHeaders);
        if (!includedHeaders.isEmpty()) {
            // We keep track of headers included in parsed translation units so we can "optimize"
            // further passes through it. Strictly speaking this would not be correct, since the
            // such headers might have been affected by content that appears before them. However
            // this significantly improves the indexing and it should be ok to live with that.
            foreach (const QString &headerName, includedHeaders) {
                if (headerFiles.contains(headerName)) {
                    headerFiles[headerName].m_upToDate = true;
                } else {
                    headerFiles.insert(headerName, FileData(headerName,
                                                            it.value().m_compilationOptions,
                                                            true));
                }
            }
        }
        futureInterface.setProgressValue(futureInterface.progressValue() + 1);
    }
    END_PROFILE_SCOPE;

    it = headerFiles.begin();
    for (; it != headerFiles.end(); ++it)
        process(&it.value(), false, 0);

    futureInterface.reportFinished();
}

void Indexer::process(FileData *fileData,
                      bool implementationFile,
                      QStringList *includedHeaders)
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
            qDebug() << "Error:" << fileName << " - severity:"<< severity;
            qDebug() << "\t"<< getQString(clang_getDiagnosticSpelling(diagnostic));
        }
        clang_disposeDiagnostic(diagnostic);
    }
#endif

    AstVisitorData *visitorData = new AstVisitorData(m_files[HeaderFile]);
    CXCursor tuCursor = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(tuCursor, Indexer::astVisit, visitorData);
    foreach (const IndexedSymbolInfo &info, visitorData->m_symbolsInfo)
        m_database.insert(info);
    delete visitorData;

    // Track headers included.
    if (implementationFile) {
        Q_ASSERT(includedHeaders);
        QScopedPointer<InclusionVisitorData> inclusionData(new InclusionVisitorData);
        clang_getInclusions(tu, Indexer::inclusionVisit, inclusionData.data());
        includedHeaders->append(inclusionData->m_includedFiles);
    }

    clang_disposeTranslationUnit(tu);
}

void Indexer::regenerate()
{
    m_indexingFuture = QtConcurrent::run(&Indexer::process, this);
}

bool Indexer::isWorking() const
{
    return m_indexingFuture.isRunning();
}

QFuture<void> Indexer::workingFuture() const
{
    return m_indexingFuture;
}

void Indexer::addFile(const QString &fileName, const QStringList &compilationOptions)
{
    if (fileName.trimmed().isEmpty())
        return;

    FileType fileType = identifyFileType(fileName);
    if (m_files.at(fileType).contains(fileName)) {
        m_files[fileType][fileName].m_upToDate = false;
        m_database.remove(FileNameKey(), fileName);
    } else {
        m_files[fileType].insert(fileName, FileData(fileName, compilationOptions));
    }
}

QStringList Indexer::getAllFiles() const
{
    QStringList all;
    populateFiles(&all, m_files.at(ImplementationFile).values());
    populateFiles(&all, m_files.at(HeaderFile).values());
    return all;
}

QList<IndexedSymbolInfo> Indexer::getAllFunctions() const
{
    return m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Method);
}

QList<IndexedSymbolInfo> Indexer::getAllClasses() const
{
    return m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Class);
}

QList<IndexedSymbolInfo> Indexer::getFunctionsFromFile(const QString &fileName) const
{
    return m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Method,
                             FileNameKey(), fileName);
}

QList<IndexedSymbolInfo> Indexer::getClassesFromFile(const QString &fileName) const
{
    return m_database.values(SymbolTypeKey(), IndexedSymbolInfo::Class,
                             FileNameKey(), fileName);
}

QList<IndexedSymbolInfo> Indexer::getAllFromFile(const QString &fileName) const
{
    return m_database.values(FileNameKey(), fileName);
}
