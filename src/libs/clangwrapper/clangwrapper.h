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

#ifndef CLANGWRAPPER_H
#define CLANGWRAPPER_H

#include "clangwrapper_global.h"

#include <QList>
#include <QMutex>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace Clang {

class SourceMarker;

class QTCREATOR_CLANGWRAPPER_EXPORT CodeCompletionResult
{
    friend class TranslationUnit;

public:
    CodeCompletionResult() {}
    CodeCompletionResult(unsigned priority);

    unsigned priority() const
    { return _priority; }

    bool isValid() const
    { return !text.isEmpty(); }

private:
    unsigned _priority;

public: //### HACK
    QString text;
    QString hint;
    bool isFunctionCompletion;
};

class QTCREATOR_CLANGWRAPPER_EXPORT Diagnostic
{
public:
    enum Severity {
        Ignored = 0,
        Note = 1,
        Warning = 2,
        Error = 3,
        Fatal = 4
    };

public:
    Diagnostic(Severity severity, const QString &fileName, unsigned line, unsigned column, unsigned length, const QString &spelling)
        : m_severity(severity)
        , m_fileName(fileName)
        , m_line(line)
        , m_column(column)
        , m_length(length)
        , m_spelling(spelling)
    {}

    Severity severity() const
    { return m_severity; }

    const QString &severityAsString() const;

    QString fileName() const
    { return m_fileName; }

    unsigned line() const
    { return m_line; }

    unsigned column() const
    { return m_column; }

    unsigned length() const
    { return m_length; }

    const QString &spelling() const
    { return m_spelling; }

private:
    Severity m_severity;
    QString m_fileName;
    unsigned m_line;
    unsigned m_column;
    unsigned m_length;
    QString m_spelling;
};

class QTCREATOR_CLANGWRAPPER_EXPORT ClangWrapper
{
    Q_DISABLE_COPY(ClangWrapper)

    class PrivateData;

public: // data structures
    typedef QSharedPointer<ClangWrapper> Ptr;

    struct QTCREATOR_CLANGWRAPPER_EXPORT UnsavedFile {
        UnsavedFile(const QString &fileName, const QByteArray &contents);

        QString fileName;
        QByteArray contents;
    };
    typedef QList<UnsavedFile> UnsavedFiles;

public: // methods
    ClangWrapper();
    ~ClangWrapper();

    QString fileName() const;
    void setFileName(const QString &fileName);

    QStringList options() const;
    void setOptions(const QStringList &options) const;

    bool reparse(const UnsavedFiles &unsavedFiles);
    QList<SourceMarker> sourceMarkersInRange(unsigned firstLine,
                                             unsigned lastLine);

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
    QList<Diagnostic> diagnostics() const;

    static QString precompile(const QString &headerFileName,
                              const QStringList &options);

    bool objcEnabled() const;

    QMutex *mutex() const
    { return &m_mutex; }

private: // instance fields
    PrivateData *m_d;
    mutable QMutex m_mutex;
};

} // namespace Clang

Q_DECLARE_METATYPE(Clang::CodeCompletionResult)

#endif // CLANGWRAPPER_H
