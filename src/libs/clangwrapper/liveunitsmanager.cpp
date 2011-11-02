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

#include "liveunitsmanager.h"

using namespace Clang;
using namespace Internal;

LiveUnitsManager::LiveUnitsManager()
{}

LiveUnitsManager *LiveUnitsManager::instance()
{
    static LiveUnitsManager manager;
    return &manager;
}

void LiveUnitsManager::requestTracking(const QString &fileName)
{
    if (!isTracking(fileName))
        m_units.insert(fileName, Unit(fileName));
}

bool LiveUnitsManager::isTracking(const QString &fileName) const
{
    return m_units.contains(fileName);
}

void LiveUnitsManager::cancelTrackingRequest(const QString &fileName)
{
    if (!isTracking(fileName))
        return;

    // If no one else is tracking this particular unit, we remove it.
    if (m_units[fileName].isUnique())
        m_units.remove(fileName);
}

void LiveUnitsManager::updateUnit(const QString &fileName, const Unit &unit)
{
    if (!isTracking(fileName))
        return;

    m_units[fileName] = unit;

    emit unitAvailable(unit);
}

Unit LiveUnitsManager::unit(const QString &fileName)
{
    return m_units.value(fileName);
}
