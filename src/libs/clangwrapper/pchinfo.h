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

#ifndef CLANG_PCHINFO_H
#define CLANG_PCHINFO_H

#include "clangwrapper_global.h"

#include <QString>
#include <QSharedPointer>
#include <QTemporaryFile>

namespace Clang {

class PCHInfo;
typedef QSharedPointer<PCHInfo> PCHInfoPtr;

class QTCREATOR_CLANGWRAPPER_EXPORT PCHInfo
{
    PCHInfo();

public:
    ~PCHInfo();

    static PCHInfoPtr createEmpty();
    static PCHInfoPtr createWithFileName(bool cpp0x, bool objc);

    QString fileName() const
    { return m_file.fileName(); }

    bool cpp0x() const
    { return m_cpp0x; }

    bool objc() const
    { return m_objc; }

private:
    QTemporaryFile m_file;
    bool m_cpp0x;
    bool m_objc;

};

} // namespace Clang

#endif // CLANG_PCHINFO_H
