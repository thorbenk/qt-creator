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

#include "unitsetup.h"
#include "liveunitsmanager.h"
#include "indexer.h"

#include <QtCore/QtConcurrentRun>
#include <QtCore/QFutureWatcher>

using namespace Clang;
using namespace Internal;

UnitSetup::UnitSetup()
{}

UnitSetup::UnitSetup(const QString &fileName, Indexer *indexer)
    : m_fileName(fileName)
    , m_unit(LiveUnitsManager::instance()->unit(fileName))
    , m_indexer(indexer)
{
    connect(LiveUnitsManager::instance(), SIGNAL(unitAvailable(Unit)),
            this, SLOT(assignUnit(Unit)));

    if (!m_unit.isLoaded()) {
        if (!LiveUnitsManager::instance()->isTracking(fileName)) {
            // Start tracking this file so the indexer is aware of it and will consequently
            // update the corresponding unit in the manager.
            LiveUnitsManager::instance()->requestTracking(fileName);
        }
        indexer->evaluateFile(fileName);
    }
}

UnitSetup::~UnitSetup()
{
    m_unit = Unit(); // We want to "release" this unit share so the manager might remove it
        // in the case no one else is tracking it.
    LiveUnitsManager::instance()->cancelTrackingRequest(m_fileName);
}

void UnitSetup::assignUnit(const Unit &unit)
{
    if (m_fileName == unit.fileName() && unit.isLoaded())
        m_unit = unit;
}

void UnitSetup::checkForNewerUnit()
{
    if (LiveUnitsManager::instance()->isTracking(m_fileName))
        assignUnit(LiveUnitsManager::instance()->unit(m_fileName));
}
