/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
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

#include "cppcompletionassist.h"
#include "cppcompletionsupport.h"
#include "cppmodelmanager.h"
#include "cpptoolseditorsupport.h"

#ifdef CLANG_COMPLETION
#  include "clangcompletion.h"
#  include "clangutils.h"
#endif // CLANG_COMPLETION

#include <coreplugin/ifile.h>
#include <projectexplorer/project.h>
#include <texteditor/codeassist/iassistinterface.h>

namespace CppTools {
namespace Internal {

class CompletionImpl
{
public:
    virtual ~CompletionImpl() = 0;

    virtual TextEditor::IAssistInterface *createAssistInterface(CppEditorSupport *editorSupport,
                                                                ProjectExplorer::Project *project,
                                                                QTextDocument *document,
                                                                int position,
                                                                TextEditor::AssistReason reason) const = 0;
};

CompletionImpl::~CompletionImpl()
{}

class InternalCodeModelCompletionImpl: public CompletionImpl
{
public:
    virtual ~InternalCodeModelCompletionImpl()
    {}

    virtual TextEditor::IAssistInterface *createAssistInterface(CppEditorSupport *editorSupport,
                                                                ProjectExplorer::Project *project,
                                                                QTextDocument *document,
                                                                int position,
                                                                TextEditor::AssistReason reason) const
    {
        CPlusPlus::CppModelManagerInterface *modelManager = CPlusPlus::CppModelManagerInterface::instance();
        QStringList includePaths;
        QStringList frameworkPaths;
        if (project) {
            includePaths = modelManager->projectInfo(project).includePaths();
            frameworkPaths = modelManager->projectInfo(project).frameworkPaths();
        }
        return new CppTools::Internal::CppCompletionAssistInterface(
                    document,
                    position,
                    editorSupport->textEditor()->file(),
                    reason,
                    modelManager->snapshot(),
                    includePaths,
                    frameworkPaths);
    }
};

#ifdef CLANG_COMPLETION
class ClangCompletionImpl: public CompletionImpl
{
public:
    ClangCompletionImpl()
        : m_clangCompletionWrapper(new Clang::ClangCompleter)
    {}

    virtual ~ClangCompletionImpl()
    {}

    virtual TextEditor::IAssistInterface *createAssistInterface(CppEditorSupport *editorSupport,
                                                                ProjectExplorer::Project *project,
                                                                QTextDocument *document,
                                                                int position,
                                                                TextEditor::AssistReason reason) const
    {
        Q_UNUSED(project);

        CPlusPlus::CppModelManagerInterface *modelManager = CPlusPlus::CppModelManagerInterface::instance();
        Core::IFile *file = editorSupport->textEditor()->file();
        QList<CPlusPlus::CppModelManagerInterface::ProjectPart::Ptr> parts = modelManager->projectPart(file->fileName());
        QStringList includePaths, frameworkPaths, options;
        if (!parts.isEmpty()) {
            const CPlusPlus::CppModelManagerInterface::ProjectPart::Ptr part = parts.at(0);
            options = CppTools::ClangUtils::createClangOptions(part);
            includePaths = part->includePaths;
            frameworkPaths = part->frameworkPaths;
        }
        return new CppTools::ClangCompletionAssistInterface(
                    m_clangCompletionWrapper,
                    document, position, file, reason,
                    options, includePaths, frameworkPaths);
    }

private:
    Clang::ClangCompleter::Ptr m_clangCompletionWrapper;
};
#endif // CLANG_COMPLETION

}
}

using namespace CppTools;
using namespace CppTools::Internal;

CppCompletionSupport::CppCompletionSupport(CppEditorSupport *editorSupport)
    : m_editorSupport(editorSupport)
//    , m_impl(new InternalCodeModelCompletionImpl)
    , m_impl(new ClangCompletionImpl)
{}

TextEditor::IAssistInterface *CppCompletionSupport::createAssistInterface(ProjectExplorer::Project *project,
                                                                          QTextDocument *document,
                                                                          int position,
                                                                          TextEditor::AssistReason reason) const
{
    return m_impl->createAssistInterface(m_editorSupport, project, document,
                                         position, reason);
}

void CppCompletionSupport::setUseClang(bool useClang)
{
#ifdef CLANG_HIGHLIGHTING
    if (useClang && dynamic_cast<InternalCodeModelCompletionImpl *>(m_impl)) {
        delete m_impl;
        m_impl = new ClangCompletionImpl;
    } else if (!useClang && dynamic_cast<ClangCompletionImpl *>(m_impl)) {
        delete m_impl;
        m_impl = new InternalCodeModelCompletionImpl;
    }
#else // !CLANG_HIGHLIGHTING
    Q_UNUSED(useClang);
#endif // CLANG_HIGHLIGHTING
}
