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
WinRtDebugSupport::WinRtDebugSupport(WinRtRunConfiguration *rc, QString *errorMessage)
{
    DebuggerStartParameters sp;
    sp.masterEngineType = CdbEngineType;
    sp.breakOnMain = false;
    sp.startMode = AttachExternal;
    sp.closeMode = KillAtClose;
    sp.languages = CppLanguage;
    sp.debuggerCommand = DebuggerKitInformation::debuggerCommand(rc->target()->kit()).toString();
    sp.attachPID = startAppx(rc->activationID(), rc->arguments(), errorMessage);

    m_runControl = DebuggerPlugin::createDebugger(sp, rc, errorMessage);
    setParent(m_runControl); // Die with runControl
    connect(m_runControl, SIGNAL(finished()), SLOT(onDebuggerFinished()));
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
    if (FAILED(CoCreateInstance(CLSID_ApplicationActivationManager, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IApplicationActivationManager,
                                reinterpret_cast<void**>(&launcher)))) {
        *errorMessage = tr("Could not instantiate the application launcher for WinRT debugging.");
        CoUninitialize();
        return -1;
    }
    DWORD pid;
    HRESULT hr = launcher->ActivateApplication(reinterpret_cast<LPCWSTR>(activationID.utf16()),
                                               reinterpret_cast<LPCWSTR>(arguments.utf16()), AO_NONE, &pid);
    launcher->Release();
    CoUninitialize();
    if (FAILED(hr)) {
        if (errorMessage)
            *errorMessage = tr("Unable to activate Appx: error 0x%1.\n").arg(HRESULT_CODE(hr), 0, 16);
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
