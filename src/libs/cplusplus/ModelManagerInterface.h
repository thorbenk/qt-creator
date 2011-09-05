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

#ifndef CPPMODELMANAGERINTERFACE_H
#define CPPMODELMANAGERINTERFACE_H

#include <cplusplus/CppDocument.h>
#include <cplusplus/pchinfo.h>
#include <languageutils/fakemetaobject.h>
#include <projectexplorer/project.h>
#include <projectexplorer/toolchain.h>

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtCore/QFuture>

namespace Core {
    class IEditor;
}

namespace CPlusPlus {
    class LookupContext;
}

namespace ProjectExplorer {
    class Project;
}

namespace CppTools {
    class AbstractEditorSupport;
}

namespace CPlusPlus {

class CPLUSPLUS_EXPORT CppModelManagerInterface : public QObject
{
    Q_OBJECT

public:
    enum Language { CXX, OBJC };

    class CPLUSPLUS_EXPORT ProjectPart
    {
    public: //attributes
        QStringList sourceFiles;
        QByteArray defines;
        QStringList includePaths;
        QStringList frameworkPaths;
        QStringList precompiledHeaders;
        Language language;
        ProjectExplorer::ToolChain::CompilerFlags flags;

        typedef QSharedPointer<ProjectPart> Ptr;

        PCHInfoPtr clangPCH;
        QStringList createClangOptions() const;
        static QStringList createClangOptions(const QStringList &precompiledHeaders,
                                              const QList<QByteArray> &defines,
                                              const QStringList &includePaths,
                                              const QStringList &frameworkPaths);
    };

    class ProjectInfo
    {
    public:
        ProjectInfo()
        { }

        ProjectInfo(QWeakPointer<ProjectExplorer::Project> project)
            : project(project)
        { }

        operator bool() const
        { return ! project.isNull(); }

        bool isValid() const
        { return ! project.isNull(); }

        bool isNull() const
        { return project.isNull(); }
    public: // attributes
        QWeakPointer<ProjectExplorer::Project> project;
        QList<ProjectPart::Ptr> projectParts;
    };

    class WorkingCopy
    {
    public:
        void insert(const QString &fileName, const QString &source, unsigned revision = 0)
        { _elements.insert(fileName, qMakePair(source, revision)); }

        bool contains(const QString &fileName) const
        { return _elements.contains(fileName); }

        QString source(const QString &fileName) const
        { return _elements.value(fileName).first; }

        QPair<QString, unsigned> get(const QString &fileName) const
        { return _elements.value(fileName); }

        QHashIterator<QString, QPair<QString, unsigned> > iterator() const
        { return QHashIterator<QString, QPair<QString, unsigned> >(_elements); }

    private:
        typedef QHash<QString, QPair<QString, unsigned> > Table;
        Table _elements;
    };

    enum ExtraDiagnosticKind
    {
        AllExtraDiagnostics = -1,
        ExportedQmlTypesDiagnostic
    };

public:
    CppModelManagerInterface(QObject *parent = 0);
    virtual ~CppModelManagerInterface();

    static CppModelManagerInterface *instance();

    virtual bool isCppEditor(Core::IEditor *editor) const = 0;

    virtual WorkingCopy workingCopy() const = 0;
    virtual CPlusPlus::Snapshot snapshot() const = 0;

    virtual ProjectInfo projectInfo(ProjectExplorer::Project *project) const = 0;
    virtual void updateProjectInfo(const ProjectInfo &pinfo) = 0;
    virtual QList<ProjectPart::Ptr> projectPart(const QString &fileName) const = 0;

    virtual void addEditorSupport(CppTools::AbstractEditorSupport *editorSupport) = 0;
    virtual void removeEditorSupport(CppTools::AbstractEditorSupport *editorSupport) = 0;

    virtual QList<int> references(CPlusPlus::Symbol *symbol,
                                  const CPlusPlus::LookupContext &context) = 0;

    virtual void renameUsages(CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context,
                              const QString &replacement = QString()) = 0;
    virtual void findUsages(CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context) = 0;

    virtual void findMacroUsages(const CPlusPlus::Macro &macro) = 0;

    virtual void setExtraDiagnostics(const QString &fileName, int key,
                                     const QList<CPlusPlus::Document::DiagnosticMessage> &diagnostics) = 0;
    virtual QList<CPlusPlus::Document::DiagnosticMessage> extraDiagnostics(
            const QString &fileName, int key = AllExtraDiagnostics) const = 0;

Q_SIGNALS:
    void documentUpdated(CPlusPlus::Document::Ptr doc);
    void sourceFilesRefreshed(const QStringList &files);
    void extraDiagnosticsUpdated(QString fileName);

public Q_SLOTS:
    virtual void updateModifiedSourceFiles() = 0;
    virtual QFuture<void> updateSourceFiles(const QStringList &sourceFiles) = 0;
    virtual void GC() = 0;
};

} // namespace CPlusPlus

#endif // CPPMODELMANAGERINTERFACE_H
