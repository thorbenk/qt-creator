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

#ifndef CLANG_REUSE_H
#define CLANG_REUSE_H

#include "sourcelocation.h"

#include <clang-c/Index.h>

#include <QtCore/QString>

QT_BEGIN_NAMESPACE
class QFileInfo;
QT_END_NAMESPACE

namespace Clang {
namespace Internal {

class Unit;

QString getQString(const CXString &cxString, bool disposeCXString = true);

SourceLocation getInstantiatonLocation(const CXSourceLocation &loc); // Deprecated
SourceLocation getSpellingLocation(const CXSourceLocation &loc);
SourceLocation getExpansionLocation(const CXSourceLocation &loc);

// There are slight differences of behavior from apparently similar Qt file processing functions.
// For instance, QFileInfo::absoluteFilePath will uppercase driver letters, while the corresponding
// QDir function will not do so. Besides, we need to keep paths clean. So in order to avoid
// inconsistencies the functions below should be used for any indexing related task.
QString normalizeFileName(const QString &fileName);
QString normalizeFileName(const QFileInfo &fileInfo);

QStringList formattedDiagnostics(const Unit &unit);

} // Internal
} // Clang

#endif // CLANG_REUSE_H
