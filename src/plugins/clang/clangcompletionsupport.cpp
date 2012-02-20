#include "clangcompletionsupport.h"
#include "clangutils.h"

#include <coreplugin/idocument.h>
#include <texteditor/itexteditor.h>

using namespace Clang;

ClangCompletionSupport::ClangCompletionSupport(TextEditor::ITextEditor *editor)
    : CppCompletionSupport(editor)
    , m_clangCompletionWrapper(new Clang::ClangCompleter)
{
}

ClangCompletionSupport::~ClangCompletionSupport()
{}

TextEditor::IAssistInterface *ClangCompletionSupport::createAssistInterface(
        ProjectExplorer::Project *project, QTextDocument *document,
        int position, TextEditor::AssistReason reason) const
{
    Q_UNUSED(project);

    CPlusPlus::CppModelManagerInterface *modelManager = CPlusPlus::CppModelManagerInterface::instance();
    QList<CPlusPlus::CppModelManagerInterface::ProjectPart::Ptr> parts = modelManager->projectPart(editor()->document()->fileName());
    QStringList includePaths, frameworkPaths, options;
    if (!parts.isEmpty()) {
        const CPlusPlus::CppModelManagerInterface::ProjectPart::Ptr part = parts.at(0);
        options = Clang::Utils::createClangOptions(part);
        includePaths = part->includePaths;
        frameworkPaths = part->frameworkPaths;
    }

    return new Clang::ClangCompletionAssistInterface(
                m_clangCompletionWrapper,
                document, position, editor()->document(), reason,
                options, includePaths, frameworkPaths);
}


ClangCompletionSupportFactory::~ClangCompletionSupportFactory()
{
}

CppTools::CppCompletionSupport *ClangCompletionSupportFactory::completionSupport(TextEditor::ITextEditor *editor)
{
    return new ClangCompletionSupport(editor);
}
