/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "winrtutils.h"

#if defined(_MSC_VER) && _MSC_VER >= 1700

# include <utils/winutils.h>

namespace WinRt {
namespace Internal {

QString comErrorString(HRESULT hr)
{
    switch (hr) {
    case S_OK:
        return QLatin1String("S_OK");
    case S_FALSE:
        return QLatin1String("S_FALSE");
    case E_FAIL:
        break;
    case E_INVALIDARG:
        return QLatin1String("E_INVALIDARG");
    case E_NOINTERFACE:
        return QLatin1String("E_NOINTERFACE");
    case E_OUTOFMEMORY:
        return QLatin1String("E_OUTOFMEMORY");
    case E_UNEXPECTED:
        return QLatin1String("E_UNEXPECTED");
    case E_NOTIMPL:
        return QLatin1String("E_NOTIMPL");
    }
    if (hr == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED))
        return QLatin1String("ERROR_ACCESS_DENIED");;
    if (hr == HRESULT_FROM_WIN32(STATUS_CONTROL_C_EXIT))
        return QLatin1String("STATUS_CONTROL_C_EXIT");
    return QLatin1String("E_FAIL ") + Utils::winErrorMessage(HRESULT_CODE(hr));
}

} // namespace Internal
} // namespace WinRt

#endif // defined(_MSC_VER) && _MSC_VER >= 1700
