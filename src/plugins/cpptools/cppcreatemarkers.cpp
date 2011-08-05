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
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#include "clangutils.h"
#include "cppcreatemarkers.h"

#include <qtconcurrent/runextensions.h>

#include <QCoreApplication>
#include <QMutexLocker>
#include <QThreadPool>

#include <QDebug>

#undef DEBUG_TIMING

using namespace Clang;
using namespace CppTools;

CreateMarkers::Future CreateMarkers::go(ClangWrapper::Ptr clangWrapper, unsigned firstLine, unsigned lastLine)
{
    if (clangWrapper.isNull())
        return CreateMarkers::Future();
    else
        return (new CreateMarkers(clangWrapper, firstLine, lastLine))->start();
}

CreateMarkers::CreateMarkers(ClangWrapper::Ptr clangWrapper, unsigned firstLine, unsigned lastLine)
    : m_clangWrapper(clangWrapper)
    , m_firstLine(firstLine)
    , m_lastLine(lastLine)
{
    Q_ASSERT(!clangWrapper.isNull());

    _fileName = m_clangWrapper->fileName();
    _flushRequested = false;
    _flushLine = 0;

    m_unsavedFiles = ClangUtils::createUnsavedFiles(CPlusPlus::CppModelManagerInterface::instance()->workingCopy());
}

CreateMarkers::~CreateMarkers()
{ }

void CreateMarkers::run()
{
    Q_ASSERT(!m_clangWrapper.isNull());

    QMutexLocker lock(m_clangWrapper->mutex());
    if (isCanceled())
        return;

#ifdef DEBUG_TIMING
    QTime t; t.start();
#endif // DEBUG_TIMING

    _diagnosticMessages.clear();
    _usages.clear();

    m_clangWrapper->reparse(m_unsavedFiles);
#ifdef DEBUG_TIMING
    qDebug() << "*** Reparse for highlighting took" << t.elapsed() << "ms.";
#endif // DEBUG_TIMING

    foreach (const Clang::Diagnostic &d, m_clangWrapper->diagnostics())
        if (d.fileName() == m_clangWrapper->fileName())
            warning(d.line(), d.column(), d.spelling(), d.length());
    if (isCanceled())
        return;

    foreach (const Clang::SourceMarker &m, m_clangWrapper->sourceMarkersInRange(m_firstLine, m_lastLine))
        addUse(SourceMarker(m.line(), m.column(), m.length(), m.kind()));
    if (isCanceled())
        return;

    flush();
    reportFinished();

#ifdef DEBUG_TIMING
    qDebug() << "*** Highlighting took" << t.elapsed() << "ms.";
#endif // DEBUG_TIMING
}

bool CreateMarkers::warning(unsigned line, unsigned column, const QString &text, unsigned length)
{
    CPlusPlus::Document::DiagnosticMessage m(CPlusPlus::Document::DiagnosticMessage::Warning, _fileName, line, column, text, length);
    _diagnosticMessages.append(m);
    return false;
}

void CreateMarkers::addUse(const SourceMarker &marker)
{
//    if (! enclosingFunctionDefinition()) {
        if (_usages.size() >= 50) {
            if (_flushRequested && marker.line != _flushLine)
                flush();
            else if (! _flushRequested) {
                _flushRequested = true;
                _flushLine = marker.line;
            }
        }
//    }

    _usages.append(marker);
}

void CreateMarkers::flush()
{
    _flushRequested = false;
    _flushLine = 0;

    if (_usages.isEmpty())
        return;

    reportResults(_usages);
    _usages.clear();
}
