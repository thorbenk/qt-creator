#ifndef CPPEDITOR_INTERNAL_CLANGCOMPLETION_H
#define CPPEDITOR_INTERNAL_CLANGCOMPLETION_H

#include "clangcompleter.h"

#include <cplusplus/Icons.h>

#include <cpptools/cppcompletionassistprovider.h>

#include <texteditor/codeassist/basicproposalitem.h>
#include <texteditor/codeassist/completionassistprovider.h>
#include <texteditor/codeassist/defaultassistinterface.h>
#include <texteditor/codeassist/iassistprocessor.h>

#include <QStringList>
#include <QTextCursor>

namespace Clang {

namespace Internal {
class ClangAssistProposalModel;

class ClangCompletionAssistProvider : public CppTools::CppCompletionAssistProvider
{
public:
    virtual TextEditor::IAssistProcessor *createProcessor() const;
    virtual CppTools::CppCompletionSupport *completionSupport(TextEditor::ITextEditor *editor);
};

} // namespace Internal

class CLANG_EXPORT ClangCompletionAssistInterface: public TextEditor::DefaultAssistInterface
{
public:
    ClangCompletionAssistInterface(Clang::ClangCompleter::Ptr clangWrapper,
                                   QTextDocument *document,
                                   int position,
                                   Core::IDocument *doc,
                                   TextEditor::AssistReason reason,
                                   const QStringList &options,
                                   const QStringList &includePaths,
                                   const QStringList &frameworkPaths);

    Clang::ClangCompleter::Ptr clangWrapper() const
    { return m_clangWrapper; }

    const Clang::UnsavedFiles &unsavedFiles() const
    { return m_unsavedFiles; }

    bool objcEnabled() const;

    const QStringList &options() const
    { return m_options; }

    const QStringList &includePaths() const
    { return m_includePaths; }

    const QStringList &frameworkPaths() const
    { return m_frameworkPaths; }

private:
    Clang::ClangCompleter::Ptr m_clangWrapper;
    Clang::UnsavedFiles m_unsavedFiles;
    QStringList m_options, m_includePaths, m_frameworkPaths;
};

class CLANG_EXPORT ClangCompletionAssistProcessor : public TextEditor::IAssistProcessor
{
public:
    ClangCompletionAssistProcessor();
    virtual ~ClangCompletionAssistProcessor();

    virtual TextEditor::IAssistProposal *perform(const TextEditor::IAssistInterface *interface);

private:
    int startCompletionHelper();
    int startOfOperator(int pos, unsigned *kind, bool wantFunctionCall) const;
    int findStartOfName(int pos = -1) const;
    bool accepts() const;
    TextEditor::IAssistProposal *createContentProposal();

    int startCompletionInternal(const QString fileName,
                                unsigned line, unsigned column,
                                int endOfExpression);

    bool completeInclude(const QTextCursor &cursor);
    void completeInclude(const QString &realPath, const QStringList &suffixes);
    void completePreprocessor();
    void addCompletionItem(const QString &text,
                           const QIcon &icon = QIcon(),
                           int order = 0,
                           const QVariant &data = QVariant());

private:
    int m_startPosition;
    QScopedPointer<const ClangCompletionAssistInterface> m_interface;
    QList<TextEditor::BasicProposalItem *> m_completions;
    CPlusPlus::Icons m_icons;
    QStringList m_preprocessorCompletions;
    QScopedPointer<Internal::ClangAssistProposalModel> m_model;
    TextEditor::IAssistProposal *m_hintProposal;
};

} // namespace Clang

#endif // CPPEDITOR_INTERNAL_CLANGCOMPLETION_H
