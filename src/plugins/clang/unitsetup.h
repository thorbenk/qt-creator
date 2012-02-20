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

#ifndef UNITSETUP_H
#define UNITSETUP_H

#include "clang_global.h"
#include "unit.h"

#include <QtCore/QObject>

namespace Clang {

class Indexer;

namespace Internal {

/*
 * This is an utility for better control of a Unit's lifecycle. It can be
 * used by any component which needs to track the latest "live" Unit available
 * for the corresponding file name.
 */
class UnitSetup : public QObject
{
    Q_OBJECT
public:
    UnitSetup();
    UnitSetup(const QString &fileName, Indexer *indexer);
    ~UnitSetup();

    const QString &fileName() const { return m_fileName; }
    Unit unit() const { return m_unit; }
    Indexer *indexer() const { return m_indexer; }

    void checkForNewerUnit();

private slots:
    void assignUnit(const Unit &unit);

private:
    QString m_fileName;
    mutable Unit m_unit;
    Indexer *m_indexer;
};

} // Internal
} // Clang

#endif // UNITSETUP_H
