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

#include "winrtdebugsupport.h"
#include "winrtrunconfiguration.h"
#include "winrtutils.h"

#include <debugger/debuggerplugin.h>
#include <debugger/debuggerstartparameters.h>
#include <debugger/debuggerrunner.h>
#include <debugger/debuggerkitinformation.h>
#include <projectexplorer/target.h>

#if defined(_MSC_VER) && _MSC_VER >= 1700
#  include <ShlObj.h>
#endif

using namespace ProjectExplorer;
using namespace Debugger;

namespace WinRt {
namespace Internal {

// Wrapper for Debugger::RunControl, which gets initialized as the parent
WinRtDebugSupport::WinRtDebugSupport(Debugger::DebuggerRunControl *rc)
    : QObject(rc) // Die with runControl
    , m_runControl(rc)
{
    connect(m_runControl, SIGNAL(finished()), SLOT(onDebuggerFinished()));
}

Debugger::DebuggerRunControl *WinRtDebugSupport::createRunControl(WinRtRunConfiguration *rc, QString *errorMessage)
{
    DebuggerStartParameters sp;
    sp.masterEngineType = CdbEngineType;
    sp.breakOnMain = false;
    sp.startMode = AttachExternal;
    sp.closeMode = KillAtClose;
    sp.languages = CppLanguage;
    const ProjectExplorer::Kit *kit = rc->target()->kit();
    sp.masterEngineType = DebuggerKitInformation::engineType(kit);
    if (sp.masterEngineType != CdbEngineType) {
        *errorMessage = tr("No CDB-debugger configured in kit \"%1\".").arg(kit->displayName());
        return 0;
    }
    sp.debuggerCommand = DebuggerKitInformation::debuggerCommand(kit).toString();
    if (sp.debuggerCommand.isEmpty()) {
        *errorMessage = tr("No debugger configured in kit \"%1\"").arg(kit->displayName());
        return 0;
    }
    sp.attachPID = WinRtDebugSupport::startAppx(rc->activationID(), rc->arguments(), errorMessage);
    if (sp.attachPID < 0)
        return 0;

    DebuggerRunControl *runControl = DebuggerPlugin::createDebugger(sp, rc, errorMessage);
    if (!runControl) {
        WinRtDebugSupport::stopAppx(sp.attachPID);
        return 0;
    }
    new WinRtDebugSupport(runControl);
    return runControl;
}

void WinRtDebugSupport::onDebuggerFinished()
{
    qint64 pid = m_runControl->applicationProcessHandle().pid();
    if (isAppxRunning(pid))
        stopAppx(pid);
}

#if defined(_MSC_VER) && _MSC_VER >= 1700

// Launches an appx application by application ID
qint64 WinRtDebugSupport::startAppx(const QString &activationID, const QString &arguments, QString *errorMessage)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IApplicationActivationManager *launcher;
    HRESULT hr = CoCreateInstance(CLSID_ApplicationActivationManager, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IApplicationActivationManager,
                                  reinterpret_cast<void**>(&launcher));
    if (FAILED(hr)) {
        if (errorMessage)
            *errorMessage = tr("Could not instantiate the application launcher for WinRT debugging: %1").arg(comErrorString(hr));
        CoUninitialize();
        return -1;
    }
    DWORD pid;
    hr = launcher->ActivateApplication(reinterpret_cast<LPCWSTR>(activationID.utf16()),
                                       reinterpret_cast<LPCWSTR>(arguments.utf16()), AO_NONE, &pid);
    launcher->Release();
    CoUninitialize();
    if (FAILED(hr)) {
        if (errorMessage)
            *errorMessage = tr("Unable to activate Appx: %1\n").arg(comErrorString(hr));
        // Suggestions for common error codes...
        // 0x3 seems to get fixed with a re-register of the app
        return -1;
    }
    return pid;
}

void WinRtDebugSupport::stopAppx(qint64 pid)
{
    // Terminate a bad idea? Use IPackageDebugSettings instead?
    HANDLE handle = OpenProcess(PROCESS_TERMINATE, true, pid);
    TerminateProcess(handle, 127);
    CloseHandle(handle);
}

bool WinRtDebugSupport::isAppxRunning(qint64 pid, int *exitCode)
{
    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, true, pid);
    DWORD dwExitCode;
    bool ok = GetExitCodeProcess(handle, &dwExitCode);
    CloseHandle(handle);
    if (exitCode)
        *exitCode = dwExitCode;

    if (ok && dwExitCode == STILL_ACTIVE)
        return true;

    return false;
}

#else // defined(_MSC_VER) && _MSC_VER >= 1700

qint64 WinRtDebugSupport::startAppx(const QString &, const QString &, QString *)
{
    return 0;
}

void WinRtDebugSupport::stopAppx(qint64)
{
}

bool WinRtDebugSupport::isAppxRunning(qint64, int *)
{
    return false;
}

#endif // !_MSC_VER

} // namespace Internal
} // namespace WinRt
