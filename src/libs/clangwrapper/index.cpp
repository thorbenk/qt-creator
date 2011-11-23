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

#include "index.h"

#include <QtCore/QStringList>
#include <QtCore/QLinkedList>
#include <QtCore/QHash>
#include <QtCore/QDataStream>
#include <QtCore/QPair>
#include <QtCore/QFileInfo>

inline uint qHash(const QStringList &all)
{
    return qHash(all.join(QString()));
}

namespace Clang {
namespace Internal {

class IndexPrivate
{
public:
    void insertSymbol(const Symbol &symbol, const QDateTime &timeStamp);
    QList<Symbol> symbols(const QString &fileName) const;
    QList<Symbol> symbols(const QString &fileName, Symbol::Kind kind) const;
    QList<Symbol> symbols(const QString &fileName, Symbol::Kind kind, const QString &uqName) const;
    QList<Symbol> symbols(const QString &fileName, const QString &uqName) const;
    QList<Symbol> symbols(Symbol::Kind kind) const;

    void insertFile(const QString &fileName, const QDateTime &timeStamp);
    void removeFile(const QString &fileName);
    void removeFiles(const QStringList &fileNames);
    bool containsFile(const QString &fileName) const;
    QStringList files() const;

    void clear();

    bool isEmpty() const;

    void trackTimeStamp(const Symbol &symbol, const QDateTime &timeStamp);
    void trackTimeStamp(const QString &fileName, const QDateTime &timeStamp);

    bool validate(const QString &fileName) const;

    QByteArray serialize() const;
    void deserialize(const QByteArray &data);

private:
    typedef QLinkedList<Symbol> SymbolCont;
    typedef SymbolCont::iterator SymbolIt;

    typedef QHash<QString, QList<SymbolIt> > NameIndex;
    typedef QHash<Symbol::Kind, NameIndex> KindIndex;
    typedef QHash<QString, KindIndex> FileIndex;

    typedef QList<SymbolIt>::iterator SymbolIndexIt;
    typedef NameIndex::iterator NameIndexIt;
    typedef KindIndex::iterator KindIndexIt;
    typedef FileIndex::iterator FileIndexIt;
    typedef FileIndex::const_iterator FileIndexCIt;

    void insertSymbol(const Symbol &symbol);
    void removeSymbol(SymbolIndexIt it);

    QPair<bool, SymbolIndexIt> findEquivalentSymbol(const Symbol &symbol);
    void updateEquivalentSymbol(SymbolIndexIt it, const Symbol &symbol);

    void createIndexes(SymbolIt it);
    QList<SymbolIt> removeIndexes(const QString &fileName);

    static QList<Symbol> symbolsFromIterators(const QList<SymbolIt> &symbolList);

    // @TODO: Sharing of compilation options...

    SymbolCont m_container;
    FileIndex m_files;
    QHash<QString, QDateTime> m_timeStamps;
};

}
}

using namespace Clang;
using namespace Internal;

void IndexPrivate::createIndexes(SymbolIt it)
{
    m_files[it->m_location.fileName()][it->m_kind][it->m_name].append(it);
}

QList<QLinkedList<Symbol>::iterator> IndexPrivate::removeIndexes(const QString &fileName)
{
    QList<SymbolIt> iterators;
    KindIndex kindIndex = m_files.take(fileName);
    KindIndexIt it = kindIndex.begin();
    KindIndexIt eit = kindIndex.end();
    for (; it != eit; ++it) {
        NameIndex nameIndex = *it;
        NameIndexIt nit = nameIndex.begin();
        NameIndexIt neit = nameIndex.end();
        for (; nit != neit; ++nit)
            iterators.append(*nit);
    }
    return iterators;
}

void IndexPrivate::insertSymbol(const Symbol &symbol)
{
    SymbolIt it = m_container.insert(m_container.begin(), symbol);
    createIndexes(it);
}

void IndexPrivate::insertSymbol(const Symbol &symbol, const QDateTime &timeStamp)
{

    const QPair<bool, SymbolIndexIt> &find = findEquivalentSymbol(symbol);
    if (find.first)
        updateEquivalentSymbol(find.second, symbol);
    else
        insertSymbol(symbol);

    trackTimeStamp(symbol, timeStamp);
}

QPair<bool, IndexPrivate::SymbolIndexIt> IndexPrivate::findEquivalentSymbol(const Symbol &symbol)
{
    // Despite the loop below finding a symbol should be efficient, since we already filter
    // the file name, the kind, and the qualified name through the indexing mechanism. In many
    // cases it will iterate only once.
    QList<SymbolIt> &byName = m_files[symbol.m_location.fileName()][symbol.m_kind][symbol.m_name];
    for (SymbolIndexIt it = byName.begin(); it != byName.end(); ++it) {
        const Symbol &candidateSymbol = *(*it);
        // @TODO: Overloads, template specializations
        if (candidateSymbol.m_qualification == symbol.m_qualification)
            return qMakePair(true, it);
    }

    return qMakePair(false, QList<SymbolIt>::iterator());
}

void IndexPrivate::updateEquivalentSymbol(SymbolIndexIt it, const Symbol &symbol)
{
    SymbolIt symbolIt = *it;

    Q_ASSERT(symbolIt->m_kind == symbol.m_kind);
    Q_ASSERT(symbolIt->m_qualification == symbol.m_qualification);
    Q_ASSERT(symbolIt->m_name == symbol.m_name);
    Q_ASSERT(symbolIt->m_location.fileName() == symbol.m_location.fileName());

    symbolIt->m_location = symbol.m_location;
}

void IndexPrivate::removeSymbol(SymbolIndexIt it)
{
    SymbolIt symbolIt = *it;

    m_container.erase(symbolIt);

    KindIndex &kindIndex = m_files[symbolIt->m_location.fileName()];
    NameIndex &nameIndex = kindIndex[symbolIt->m_kind];
    QList<SymbolIt> &byName = nameIndex[symbolIt->m_name];
    byName.erase(it);
    if (byName.isEmpty()) {
        nameIndex.remove(symbolIt->m_name);
        if (nameIndex.isEmpty()) {
            kindIndex.remove(symbolIt->m_kind);
            if (kindIndex.isEmpty())
                m_files.remove(symbolIt->m_location.fileName());
        }
    }
}

QList<Symbol> IndexPrivate::symbols(const QString &fileName) const
{
    QList<Symbol> all;
    const QList<NameIndex> &byKind = m_files.value(fileName).values();
    foreach (const NameIndex &nameIndex, byKind) {
        const QList<QList<SymbolIt> > &byName = nameIndex.values();
        foreach (const QList<SymbolIt> &symbols, byName)
            all.append(symbolsFromIterators(symbols));
    }
    return all;
}

QList<Symbol> IndexPrivate::symbols(const QString &fileName, Symbol::Kind kind) const
{
    QList<Symbol> all;
    const QList<QList<SymbolIt> > &byName = m_files.value(fileName).value(kind).values();
    foreach (const QList<SymbolIt> &symbols, byName)
        all.append(symbolsFromIterators(symbols));
    return all;
}

QList<Symbol> IndexPrivate::symbols(const QString &fileName,
                                    Symbol::Kind kind,
                                    const QString &uqName) const
{
    return symbolsFromIterators(m_files.value(fileName).value(kind).value(uqName));
}

QList<Symbol> IndexPrivate::symbols(Symbol::Kind kind) const
{
    QList<Symbol> all;
    FileIndexCIt it = m_files.begin();
    FileIndexCIt eit = m_files.end();
    for (; it != eit; ++it)
        all.append(symbols(it.key(), kind));
    return all;
}

QList<Symbol> IndexPrivate::symbolsFromIterators(const QList<SymbolIt> &symbolList)
{
    QList<Symbol> all;
    foreach (SymbolIt symbolIt, symbolList)
        all.append(*symbolIt);
    return all;
}

void IndexPrivate::trackTimeStamp(const Symbol &symbol, const QDateTime &timeStamp)
{
    trackTimeStamp(symbol.m_location.fileName(), timeStamp);
}

void IndexPrivate::trackTimeStamp(const QString &fileName, const QDateTime &timeStamp)
{
    // We keep track of time stamps on a per file basis (most recent one).
    m_timeStamps[fileName] = timeStamp;
}

bool IndexPrivate::validate(const QString &fileName) const
{
    const QDateTime &timeStamp = m_timeStamps.value(fileName);
    if (!timeStamp.isValid())
        return false;

    QFileInfo fileInfo(fileName);
    if (fileInfo.lastModified() > timeStamp)
        return false;

    return true;
}

void IndexPrivate::insertFile(const QString &fileName, const QDateTime &timeStamp)
{
    trackTimeStamp(fileName, timeStamp);
}

QStringList IndexPrivate::files() const
{
    return m_timeStamps.keys();
}

bool IndexPrivate::containsFile(const QString &fileName) const
{
    return m_timeStamps.contains(fileName);
}

void IndexPrivate::removeFile(const QString &fileName)
{
    const QList<SymbolIt> &iterators = removeIndexes(fileName);
    foreach (SymbolIt it, iterators)
        m_container.erase(it);

    m_timeStamps.remove(fileName);
}

void IndexPrivate::removeFiles(const QStringList &fileNames)
{
    foreach (const QString &fileName, fileNames)
        removeFile(fileName);
}

void IndexPrivate::clear()
{
    m_container.clear();
    m_files.clear();
    m_timeStamps.clear();
}

bool IndexPrivate::isEmpty() const
{
    return m_timeStamps.isEmpty();
}

QByteArray IndexPrivate::serialize() const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);

    stream << (quint32)0x0A0BFFEE;
    stream << (quint16)1;
    stream.setVersion(QDataStream::Qt_4_7);
    stream << m_container;
    stream << m_timeStamps;

    return data;
}

void IndexPrivate::deserialize(const QByteArray &data)
{
    clear();

    // @TODO: Version compatibility handling.

    QDataStream stream(data);

    quint32 header;
    stream >> header;
    if (header != 0x0A0BFFEE)
        return;

    quint16 indexVersion;
    stream >> indexVersion;
    if (indexVersion != 1)
        return;

    stream.setVersion(QDataStream::Qt_4_7);

    SymbolCont symbols;
    stream >> symbols;
    stream >> m_timeStamps;

    // @TODO: Overload the related functions with batch versions.
    foreach (const Symbol &symbol, symbols)
        insertSymbol(symbol);
}


Index::Index()
    : d(new IndexPrivate)
{}

Index::~Index()
{}

void Index::insertSymbol(const Symbol &symbol, const QDateTime &timeStamp)
{
    d->insertSymbol(symbol, timeStamp);
}

QList<Symbol> Index::symbols(const QString &fileName) const
{
    return d->symbols(fileName);
}

QList<Symbol> Index::symbols(const QString &fileName, Symbol::Kind kind) const
{
    return d->symbols(fileName, kind);
}

QList<Symbol> Index::symbols(const QString &fileName, Symbol::Kind kind, const QString &uqName) const
{
    return d->symbols(fileName, kind, uqName);
}

QList<Symbol> Index::symbols(Symbol::Kind kind) const
{
    return d->symbols(kind);
}

void Index::insertFile(const QString &fileName, const QDateTime &timeStamp)
{
    d->insertFile(fileName, timeStamp);
}

QStringList Index::files() const
{
    return d->files();
}

bool Index::containsFile(const QString &fileName) const
{
    return d->containsFile(fileName);
}

void Index::removeFile(const QString &fileName)
{
    d->removeFile(fileName);
}

void Index::removeFiles(const QStringList &fileNames)
{
    d->removeFiles(fileNames);
}

void Index::clear()
{
    d->clear();
}

bool Index::isEmpty() const
{
    return d->isEmpty();
}

bool Index::validate(const QString &fileName) const
{
    return d->validate(fileName);
}

QByteArray Index::serialize() const
{
    return d->serialize();
}

void Index::deserialize(const QByteArray &data)
{
    d->deserialize(data);
}
