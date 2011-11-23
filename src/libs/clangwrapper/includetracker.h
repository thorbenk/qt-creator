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

#ifndef INCLUDETRACKER_H
#define INCLUDETRACKER_H

#include "clangwrapper_global.h"

#include <QtCore/QStringList>
#include <QtCore/QHash>

namespace Clang {
namespace Internal {

class QTCREATOR_CLANGWRAPPER_EXPORT IncludeTracker
{
public:
    IncludeTracker();

    enum ResolutionMode
    {
        // This is the usual form: Given the search order, whenever we find an include path
        // that matches a particular include spelling, we resolve to this one.
        FirstMatchResolution,

        // In this mode we gather every possible match of a particular include spelling
        // for the available include paths. Ideally this should not exist actually, but this
        // is particularly useful for computing the dependency graph used to compare against
        // the units processed by clang. Since we don't (yet) reliably reproduce clang's search
        // order, we have more chances of identifying the "right" file when restoring the index.
        EveryMatchResolution
    };

    void setResolutionMode(ResolutionMode mode);
    ResolutionMode resolutionMode() const;

    QStringList directIncludes(const QString &fileName,
                               const QStringList &compilationOptions) const;

private:
    struct OptionsCache
    {
        QStringList m_searchPaths;
    };

    typedef QHash<QStringList, OptionsCache> CacheSet;
    typedef CacheSet::iterator CacheSetIt;

    enum IncludeKind
    {
        QuotedInclude,
        AngleBracketInclude
    };

    QStringList parseIncludes(const QString &contents,
                              const QString &basePath,
                              CacheSetIt cacheIt) const;
    QSet<QString> resolveInclude(const QString &basePath,
                                 const QString &includeSpelling,
                                 IncludeKind includeKind,
                                 CacheSetIt cacheIt) const;

    static bool isStartOfLine(const QString &content, int current);

    mutable CacheSet m_cache;
    ResolutionMode m_mode;
};

} // Internal
} // Clang

#endif // INCLUDETRACKER_H
