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

#ifndef INDEXER_H
#define INDEXER_H

#include "clangwrapper_global.h"
#include "indexedsymbolinfo.h"
#include "database.h"

#include <clang-c/Index.h>

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVector>
#include <QtCore/QHash>
#include <QtCore/QFuture>

QT_BEGIN_NAMESPACE
template <class> class QFutureInterface;
QT_END_NAMESPACE

namespace Clang {

// Create tags to map the necessary members of IndexedSymbolInfo usded as keys in the database.
struct FileNameKey { typedef QString ValueType; };
struct SymbolTypeKey { typedef IndexedSymbolInfo::SymbolType ValueType; };

// Specialize the required functions for the keys.
template <>
inline QString getKey<FileNameKey>(const IndexedSymbolInfo &info)
{ return info.m_fileName; }

template <>
inline IndexedSymbolInfo::SymbolType getKey<SymbolTypeKey>(const IndexedSymbolInfo &info)
{ return info.m_type; }


//-----------------------------------------------------------------------------
// The indexer...
//  - Not thread-safe
//-----------------------------------------------------------------------------
class QTCREATOR_CLANGWRAPPER_EXPORT Indexer
{
public:
    Indexer();

    void regenerate();
    bool isWorking() const;
    QFuture<void> workingFuture() const;

    void addFile(const QString &fileName, const QStringList &compilationOptions);
    QStringList getAllFiles() const;
    QList<IndexedSymbolInfo> getAllFunctions() const;
    QList<IndexedSymbolInfo> getAllClasses() const;
    QList<IndexedSymbolInfo> getFunctionsFromFile(const QString &fileName) const;
    QList<IndexedSymbolInfo> getClassesFromFile(const QString &fileName) const;
    QList<IndexedSymbolInfo> getAllFromFile(const QString &fileName) const;

private:
    // This is enumeration is used to index a vector. So be careful when changing.
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

    friend struct AstVisitorData;
    friend void populateFiles(QStringList *all, const QList<Indexer::FileData> &data);

    static FileType identifyFileType(const QString &fileName);

    static void inclusionVisit(CXFile file,
                               CXSourceLocation *,
                               unsigned,
                               CXClientData clientData);
    static CXChildVisitResult astVisit(CXCursor cursor,
                                       CXCursor parentCursor,
                                       CXClientData clientData);


    void process(QFutureInterface<void> &futureInterface);
    void process(FileData *data, bool implementationFile, QStringList *others);

    QVector<QHash<QString, FileData> > m_files;
    CXIndex m_clangIndex;
    unsigned m_unitManagementOptions;
    Database<IndexedSymbolInfo, FileNameKey, SymbolTypeKey> m_database;
    QFuture<void> m_indexingFuture;
};

} // Clang

#endif // INDEXER_H
