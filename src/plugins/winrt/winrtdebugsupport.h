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

#ifndef WINRT_INTERNAL_WINRTDEBUGSUPPORT_H
#define WINRT_INTERNAL_WINRTDEBUGSUPPORT_H

#include <QObject>

namespace Debugger {
class DebuggerRunControl;
} // namespace Debugger

namespace WinRt {
namespace Internal {

class WinRtRunConfiguration;

class WinRtDebugSupport : public QObject
{
    Q_OBJECT
public:
    static Debugger::DebuggerRunControl *createRunControl(WinRtRunConfiguration *rc, QString *errorMessage);

    static qint64 startAppx(const QString &activationID, const QString &arguments,
                            QString *errorMessage = nullptr);
    static void stopAppx(qint64 pid);
    static bool isAppxRunning(qint64 pid, int *exitCode = 0);

private slots:
    void onDebuggerFinished();

private:
    explicit WinRtDebugSupport(Debugger::DebuggerRunControl *);

    Debugger::DebuggerRunControl *m_runControl;
};

} // namespace Internal
} // namespace WinRt

#endif // WINRT_INTERNAL_WINRTDEBUGSUPPORT_H
