/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#ifndef CLANGCOMPLETER_H
#define CLANGCOMPLETER_H

#include "clangwrapper_global.h"
#include "diagnostic.h"
#include "sourcelocation.h"
#include "utils.h"

#include <QList>
#include <QMap>
#include <QMutex>
#include <QPair>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace Clang {

class SourceMarker;

class QTCREATOR_CLANGWRAPPER_EXPORT CodeCompletionResult
{
public:
    enum Kind {
        Other = 0,
        FunctionCompletionKind,
        ConstructorCompletionKind,
        DestructorCompletionKind,
        VariableCompletionKind,
        ClassCompletionKind,
        EnumCompletionKind,
        EnumeratorCompletionKind,
        NamespaceCompletionKind,
        PreProcessorCompletionKind,
        SignalCompletionKind,
        SlotCompletionKind
    };

    enum Availability {
        Available,
        Deprecated,
        NotAvailable,
        NotAccessible
    };

public:
    CodeCompletionResult();
    CodeCompletionResult(unsigned priority);

    unsigned priority() const
    { return m_priority; }

    bool isValid() const
    { return !m_text.isEmpty(); }

    QString text() const
    { return m_text; }
    void setText(const QString &text)
    { m_text = text; }

    QString hint() const
    { return m_hint; }
    void setHint(const QString &hint)
    { m_hint = hint; }

    Kind completionKind() const
    { return m_completionKind; }
    void setCompletionKind(Kind type)
    { m_completionKind = type; }

    int compare(const CodeCompletionResult &other) const
    {
        if (m_priority < other.m_priority)
            return -1;
        else if (m_priority > other.m_priority)
            return 1;

        if (m_completionKind < other.m_completionKind)
            return -1;
        else if (m_completionKind > other.m_completionKind)
            return 1;

        if (m_text < other.m_text)
            return -1;
        else if (m_text > other.m_text)
            return 1;

        if (m_hint < other.m_hint)
            return -1;
        else if (m_hint > other.m_hint)
            return 1;

        if (!m_hasParameters && other.m_hasParameters)
            return -1;
        else if (m_hasParameters && !other.m_hasParameters)
            return 1;

        if (m_availability < other.m_availability)
            return -1;
        else if (m_availability > other.m_availability)
            return 1;

        return 0;
    }

    bool hasParameters() const
    { return m_hasParameters; }
    void setHasParameters(bool hasParameters)
    { m_hasParameters = hasParameters; }

    Availability availability() const
    { return m_availability; }
    void setAvailability(Availability availability)
    { m_availability = availability; }

private:
    unsigned m_priority;
    Kind m_completionKind;
    QString m_text;
    QString m_hint;
    Availability m_availability;
    bool m_hasParameters;
};

inline QTCREATOR_CLANGWRAPPER_EXPORT uint qHash(const CodeCompletionResult &ccr)
{ return ccr.completionKind() ^ qHash(ccr.text()); }

inline QTCREATOR_CLANGWRAPPER_EXPORT bool operator==(const CodeCompletionResult &ccr1, const CodeCompletionResult &ccr2)
{ return ccr1.compare(ccr2) == 0; }

inline QTCREATOR_CLANGWRAPPER_EXPORT bool operator<(const CodeCompletionResult &ccr1, const CodeCompletionResult &ccr2)
{
    return ccr1.compare(ccr2) < 0;
}

class QTCREATOR_CLANGWRAPPER_EXPORT ClangCompleter
{
    Q_DISABLE_COPY(ClangCompleter)

    class PrivateData;

public: // data structures
    typedef QSharedPointer<ClangCompleter> Ptr;

public: // methods
    ClangCompleter();
    ~ClangCompleter();

    QString fileName() const;
    void setFileName(const QString &fileName);

    QStringList options() const;
    void setOptions(const QStringList &options) const;

    bool reparse(const UnsavedFiles &unsavedFiles);

    /**
     * Do code-completion at the specified position.
     *
     * \param line The line number on which to do code-completion. The first
     * line of a file has line number 1.
     * \param column The column number where to do code-completion. Column
     * numbers start with 1.
     */
    QList<CodeCompletionResult> codeCompleteAt(unsigned line,
                                               unsigned column,
                                               const UnsavedFiles &unsavedFiles);

    static QPair<bool, QStringList> precompile(const QString &headerFileName,
                                               const QStringList &options,
                                               const QString &outFileName);

    bool objcEnabled() const;

    QMutex *mutex() const
    { return &m_mutex; }

private: // instance fields
    PrivateData *m_d;
    mutable QMutex m_mutex;
};

} // namespace Clang

Q_DECLARE_METATYPE(Clang::CodeCompletionResult)

#endif // CLANGCOMPLETER_H
