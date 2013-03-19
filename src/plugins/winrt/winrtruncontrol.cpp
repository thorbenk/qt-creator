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

#include "winrtruncontrol.h"
#include "winrtrunconfiguration.h"
#include "winrtdebugsupport.h"

#include <projectexplorer/target.h>
#include <projectexplorer/project.h>
#include <extensionsystem/pluginmanager.h>

#include <QThread>
#include <QDebug>
#include <QTimerEvent>

using namespace ProjectExplorer;
using namespace Debugger;

namespace WinRt {
namespace Internal {

static const char winDebugInterfaceThreadC[] = "WinDebugInterfaceThread";

WinRtRunControl::WinRtRunControl(WinRtRunConfiguration *runConfiguration, RunMode mode)
    : RunControl(runConfiguration, mode)
    , m_timerId(0)
    , m_running(false)
    , m_runConfiguration(runConfiguration)
    , m_debugOutputThread(qobject_cast<QThread*>(ExtensionSystem::PluginManager::getObjectByName(
                                                 QLatin1String(winDebugInterfaceThreadC))))
{
    if (m_debugOutputThread) {
        connect(m_debugOutputThread, SIGNAL(cannotRetrieveDebugOutput()), SLOT(cannotRetrieveDebugOutput()));
        connect(m_debugOutputThread, SIGNAL(debugOutput(qint64,QString)), SLOT(onDebugOutput(qint64,QString)));
    } else {
        qWarning().nospace() << "Unable to locate instance of" << winDebugInterfaceThreadC;
    }
}

void WinRtRunControl::start()
{
    if (m_debugOutputThread && !m_debugOutputThread->isRunning())
        m_debugOutputThread->start();

    QString error;
    qint64 pid = WinRtDebugSupport::startAppx(m_runConfiguration->activationID(),
                                              m_runConfiguration->arguments(), &error);
    if (pid >= 0) {
        m_running = true;
        setApplicationProcessHandle(ProcessHandle(pid));
        appendMessage(tr("Starting app...\n"), Utils::NormalMessageFormat);
        m_timerId = startTimer(500);
        emit started();
    } else {
        appendMessage(tr("Unable to initialize app.\n"), Utils::ErrorMessageFormat);
        appendMessage(error, Utils::ErrorMessageFormat);
        emit finished();
    }
}

RunControl::StopResult WinRtRunControl::stop()
{
    WinRtDebugSupport::stopAppx(applicationProcessHandle().pid());
    return AsynchronousStop;
}

QIcon WinRtRunControl::icon() const
{
    return QIcon(QLatin1String(ProjectExplorer::Constants::ICON_RUN_SMALL));
}

bool WinRtRunControl::isRunning() const
{
    return m_running;
}

void WinRtRunControl::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == m_timerId) {
        int exitCode;
        if (!WinRtDebugSupport::isAppxRunning(applicationProcessHandle().pid(), &exitCode)) {
            m_running = false;
            killTimer(m_timerId);
            appendMessage(tr("App finished with exit code %1.\n").arg(exitCode),
                          Utils::NormalMessageFormat);
            emit finished();
        }
    }
}

void WinRtRunControl::cannotRetrieveDebugOutput()
{
    appendMessage(tr("Cannot retrieve debugging output.\n"), Utils::ErrorMessageFormat);
}

void WinRtRunControl::onDebugOutput(qint64 pid, const QString &message)
{
    if (quint64(pid) == applicationProcessHandle().pid())
        appendMessage(message, Utils::DebugFormat);
}

} // namespace Internal
} // namespace WinRt
