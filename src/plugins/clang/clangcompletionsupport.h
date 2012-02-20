#ifndef CLANG_CLANGCOMPLETIONSUPPORT_H
#define CLANG_CLANGCOMPLETIONSUPPORT_H

#include "clangcompletion.h"

#include <cpptools/cppcompletionsupport.h>

namespace Clang {

class ClangCompletionSupport: public CppTools::CppCompletionSupport
{
public:
    ClangCompletionSupport(TextEditor::ITextEditor *editor);
    ~ClangCompletionSupport();

    virtual TextEditor::IAssistInterface *createAssistInterface(
            ProjectExplorer::Project *project, QTextDocument *document,
            int position, TextEditor::AssistReason reason) const;

private:
    Clang::ClangCompleter::Ptr m_clangCompletionWrapper;
};

class ClangCompletionSupportFactory: public CppTools::CppCompletionSupportFactory
{
public:
    virtual ~ClangCompletionSupportFactory();

    virtual CppTools::CppCompletionSupport *completionSupport(TextEditor::ITextEditor *editor);
};

} // namespace Clang

#endif // CLANG_CLANGCOMPLETIONSUPPORT_H
