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

#ifndef TYPEDEFS_H
#define TYPEDEFS_H

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QMap>

/*
 * A header for globally visible typedefs. This is particularly useful
 * so we don't have to #include files simply because of a typedef. Still,
 * not every typedef should go in here, only the minimal subset of the
 * ones which are needed quite often.
 */
namespace Clang {

typedef QMap<QString, QByteArray> UnsavedFiles;


} // Clang

#endif // TYPEDEFS_H
