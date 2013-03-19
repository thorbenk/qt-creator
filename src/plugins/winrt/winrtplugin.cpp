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

#include "winrtplugin.h"
#include "winrtrunfactories.h"

#include <QtPlugin>
#include <QSysInfo>

namespace WinRt {
namespace Internal {

WinRtPlugin::WinRtPlugin() //: m_registerAction(new QAction(tr("Register Appx Package")))
{
    setObjectName(QLatin1String("WinRtPlugin"));
}

WinRtPlugin::~WinRtPlugin()
{
}

bool WinRtPlugin::initialize(const QStringList &arguments, QString *errorMessage)
{
    Q_UNUSED(arguments)
#ifdef Q_OS_WIN
    if (QSysInfo::windowsVersion() < QSysInfo::WV_WINDOWS8) {
        *errorMessage = tr("The WinRT-plugin requires Windows 8 or higher.");
        return false;
    }
#else // Q_OS_WIN
    *errorMessage = tr("The WinRT-plugin is not supported on this platform.");
    return false;
#endif // !Q_OS_WIN

    addAutoReleasedObject(new Internal::WinRtRunConfigurationFactory);
    addAutoReleasedObject(new Internal::WinRtRunControlFactory);

    return true;
}

void WinRtPlugin::extensionsInitialized()
{
}

} // namespace Internal
} // namespace WinRt

Q_EXPORT_PLUGIN(WinRt::Internal::WinRtPlugin)
