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

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QScopedPointer>
#include <QtCore/QFuture>

namespace Clang {

class IndexerPrivate;

class QTCREATOR_CLANGWRAPPER_EXPORT Indexer
{
public:
    Indexer();
    ~Indexer();

    void regenerate();
    bool isWorking() const;
    QFuture<void> workingFuture() const;
    void stopWorking(bool waitForFinished);
    void destroy();

    bool addFile(const QString &fileName, const QStringList &compilationOptions);
    QStringList allFiles() const;

    QList<IndexedSymbolInfo> allFunctions() const;
    QList<IndexedSymbolInfo> allClasses() const;
    QList<IndexedSymbolInfo> allMethods() const;
    QList<IndexedSymbolInfo> allConstructors() const;
    QList<IndexedSymbolInfo> allDestructors() const;
    QList<IndexedSymbolInfo> functionsFromFile(const QString &fileName) const;
    QList<IndexedSymbolInfo> classesFromFile(const QString &fileName) const;
    QList<IndexedSymbolInfo> methodsFromFile(const QString &fileName) const;
    QList<IndexedSymbolInfo> constructorsFromFile(const QString &fileName) const;
    QList<IndexedSymbolInfo> destructorsFromFile(const QString &fileName) const;
    QList<IndexedSymbolInfo> allFromFile(const QString &fileName) const;

private:
    QScopedPointer<IndexerPrivate> m_d;
};

} // Clang

#endif // INDEXER_H
