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

#include <QtCore/QLinkedList>
#include <QtCore/QHash>

namespace Clang {
namespace Internal {

class IndexPrivate
{
public:
    void insert(const IndexedSymbolInfo &info);
    QList<IndexedSymbolInfo> values(const QString &fileName);
    QList<IndexedSymbolInfo> values(const QString &fileName, IndexedSymbolInfo::Kind kind);
    QList<IndexedSymbolInfo> values(IndexedSymbolInfo::Kind kind);
    void clear(const QString &fileName);
    void clear();

private:
    typedef QLinkedList<IndexedSymbolInfo> InfoList;
    typedef InfoList::iterator InfoIterator;

    typedef QHash<IndexedSymbolInfo::Kind, QList<InfoIterator> > KindIndex;
    typedef QHash<QString, KindIndex> FileIndex;
    typedef KindIndex::iterator KindIndexIterator;

    void createIndexes(InfoIterator it);
    QList<InfoIterator> removeIndexes(const QString &fileName);
    void removeAllIndexes();

    static QList<IndexedSymbolInfo> values(const QList<InfoIterator> &infoList);

    InfoList m_container;
    FileIndex m_files;
    KindIndex m_kinds;
};

}
}

using namespace Clang;
using namespace Internal;

void IndexPrivate::createIndexes(InfoIterator it)
{
    m_files[it->m_location.fileName()][it->m_kind].append(it);
    m_kinds[it->m_kind].append(it);
}

QList<QLinkedList<IndexedSymbolInfo>::iterator> IndexPrivate::removeIndexes(const QString &fileName)
{
    QList<InfoIterator> result;
    KindIndex kinds = m_files.take(fileName);
    KindIndexIterator it = kinds.begin();
    KindIndexIterator eit = kinds.end();
    for (; it != eit; ++it) {
        QList<InfoIterator> infoList = *it;
        foreach (InfoIterator infoIt, infoList) {
            result.append(infoIt);
            m_kinds.remove(infoIt->m_kind);
        }
    }

    return result;
}

void IndexPrivate::removeAllIndexes()
{
    m_files.clear();
    m_kinds.clear();
}

void IndexPrivate::insert(const IndexedSymbolInfo &info)
{
    InfoIterator it = m_container.insert(m_container.begin(), info);
    createIndexes(it);
}

QList<IndexedSymbolInfo> IndexPrivate::values(const QString &fileName)
{
    QList<IndexedSymbolInfo> all;
    const QList<QList<InfoIterator> > &kinds = m_files.value(fileName).values();
    foreach (const QList<InfoIterator> &infoList, kinds)
        all.append(values(infoList));
    return all;
}

QList<IndexedSymbolInfo> IndexPrivate::values(const QString &fileName, IndexedSymbolInfo::Kind kind)
{
    return values(m_files.value(fileName).value(kind));
}

QList<IndexedSymbolInfo> IndexPrivate::values(IndexedSymbolInfo::Kind kind)
{
    return values(m_kinds.value(kind));
}

QList<IndexedSymbolInfo> IndexPrivate::values(const QList<InfoIterator> &infoList)
{
    QList<IndexedSymbolInfo> all;
    foreach (InfoIterator infoIt, infoList)
        all.append(*infoIt);
    return all;
}

void IndexPrivate::clear(const QString &fileName)
{
    const QList<InfoIterator> &iterators = removeIndexes(fileName);
    foreach (InfoIterator it, iterators)
        m_container.erase(it);
}

void IndexPrivate::clear()
{
    m_container.clear();
    removeAllIndexes();
}


Index::Index()
    : d(new IndexPrivate)
{}

Index::~Index()
{}

void Index::insert(const IndexedSymbolInfo &info)
{
    d->insert(info);
}

QList<IndexedSymbolInfo> Index::values(const QString &fileName)
{
    return d->values(fileName);
}

QList<IndexedSymbolInfo> Index::values(const QString &fileName, IndexedSymbolInfo::Kind kind)
{
    return d->values(fileName, kind);
}

QList<IndexedSymbolInfo> Index::values(IndexedSymbolInfo::Kind kind)
{
    return d->values(kind);
}

void Index::clear(const QString &fileName)
{
    d->clear(fileName);
}

void Index::clear()
{
    d->clear();
}
