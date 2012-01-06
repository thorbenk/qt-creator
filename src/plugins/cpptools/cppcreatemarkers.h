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

#ifndef CPPCREATEMARKERS_H
#define CPPCREATEMARKERS_H

#include "cpptools_global.h"

#include <clangwrapper/clangwrapper.h>
#include <clangwrapper/sourcemarker.h>
#include <clangwrapper/semanticmarker.h>

#include <texteditor/semantichighlighter.h>

#include <QSet>
#include <QFuture>
#include <QtConcurrentRun>

namespace CppTools {

class CPPTOOLS_EXPORT CreateMarkers:
        public QObject,
        public QRunnable,
        public QFutureInterface<TextEditor::SemanticHighlighter::Result>
{
    Q_OBJECT

public:
    virtual ~CreateMarkers();

    virtual void run();

    typedef TextEditor::SemanticHighlighter::Result SourceMarker;

    typedef QFuture<SourceMarker> Future;

    Future start()
    {
        this->setRunnable(this);
        this->reportStarted();
        Future future = this->future();
        QThreadPool::globalInstance()->start(this, QThread::LowestPriority);
        return future;
    }

    static CreateMarkers *create(Clang::SemanticMarker::Ptr semanticMarker,
                                 const QString &fileName,
                                 const QStringList &options,
                                 unsigned firstLine, unsigned lastLine);

    void addUse(const SourceMarker &marker);
    void flush();

signals:
    void diagnosticsReady(const QList<Clang::Diagnostic> &diagnostics);

protected:
    CreateMarkers(Clang::SemanticMarker::Ptr semanticMarker,
                  const QString &fileName, const QStringList &options,
                  unsigned firstLine, unsigned lastLine);

private:
    Clang::SemanticMarker::Ptr m_marker;
    QString m_fileName;
    QStringList m_options;
    unsigned m_firstLine;
    unsigned m_lastLine;
    QVector<SourceMarker> m_usages;
    bool m_flushRequested;
    unsigned m_flushLine;

    Clang::UnsavedFiles m_unsavedFiles;
};

} // namespace CppTools

#endif // CPPCREATEMARKERS_H
