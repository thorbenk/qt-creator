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

#include "clang_global.h"
#include "symbol.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QScopedPointer>
#include <QtCore/QFuture>

namespace Clang {

class IndexerPrivate;

class CLANG_EXPORT Indexer : public QObject
{
    Q_OBJECT
public:
    Indexer();
    ~Indexer();

    void initialize(const QString &storagePath);
    void finalize();

    void regenerate();
    void evaluateFile(const QString &fileName);
    bool isBusy() const;
    void cancel(bool waitForFinished);

    bool addFile(const QString &fileName, const QStringList &compilationOptions);
    QStringList allFiles() const;
    QStringList compilationOptions(const QString &fileName) const;

    QList<Symbol> allFunctions() const;
    QList<Symbol> allClasses() const;
    QList<Symbol> allMethods() const;
    QList<Symbol> allConstructors() const;
    QList<Symbol> allDestructors() const;
    QList<Symbol> functionsFromFile(const QString &fileName) const;
    QList<Symbol> classesFromFile(const QString &fileName) const;
    QList<Symbol> methodsFromFile(const QString &fileName) const;
    QList<Symbol> constructorsFromFile(const QString &fileName) const;
    QList<Symbol> destructorsFromFile(const QString &fileName) const;
    QList<Symbol> allFromFile(const QString &fileName) const;

signals:
    void indexingStarted(QFuture<void> future);
    void indexingFinished();

private:
    friend class IndexerPrivate;
    QScopedPointer<IndexerPrivate> m_d;
};

} // Clang

#endif // INDEXER_H
