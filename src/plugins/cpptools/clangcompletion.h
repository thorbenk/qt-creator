#ifndef CPPEDITOR_INTERNAL_CLANGCOMPLETION_H
#define CPPEDITOR_INTERNAL_CLANGCOMPLETION_H

#include "cppcompletionassist.h"
#include "cpptools_global.h"

#include <clangwrapper/ClangWrapper.h>

#include <cplusplus/Icons.h>

#include <texteditor/codeassist/basicproposalitem.h>
#include <texteditor/codeassist/defaultassistinterface.h>
#include <texteditor/codeassist/iassistprocessor.h>

#include <QMutex>
#include <QStringList>

namespace CppTools {

namespace Internal {
class ClangAssistProposalModel;
} // namespace Internal

class CPPTOOLS_EXPORT ClangCompletionAssistInterface: public TextEditor::DefaultAssistInterface
{
public:
    ClangCompletionAssistInterface(Clang::ClangWrapper::Ptr clangWrapper,
                                   QMutex *wrapperMutex,
                                   QTextDocument *document,
                                   int position,
                                   Core::IFile *file,
                                   TextEditor::AssistReason reason,
                                   const QStringList &includePaths,
                                   const QStringList &frameworkPaths);

    Clang::ClangWrapper::Ptr clangWrapper() const
    { return m_clangWrapper; }

    QMutex *mutex() const
    { return m_mutex; }

    const Clang::ClangWrapper::UnsavedFiles &unsavedFiles() const
    { return m_unsavedFiles; }

    bool objcEnabled() const;

    const QStringList &includePaths() const
    { return m_includePaths; }

    const QStringList &frameworkPaths() const
    { return m_frameworkPaths; }

private:
    Clang::ClangWrapper::Ptr m_clangWrapper;
    QMutex *m_mutex;
    Clang::ClangWrapper::UnsavedFiles m_unsavedFiles;
    QStringList m_includePaths, m_frameworkPaths;
};

class CPPTOOLS_EXPORT ClangCompletionAssistProcessor : public TextEditor::IAssistProcessor
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
                                const QString &expression,
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
//    TextEditor::SnippetAssistCollector m_snippetCollector;
//    const CppCompletionAssistProvider *m_provider;
    CPlusPlus::Icons m_icons;
    QStringList m_preprocessorCompletions;
    QScopedPointer<Internal::ClangAssistProposalModel> m_model;
    TextEditor::IAssistProposal *m_hintProposal;
};

} // namespace CppTools

#endif // CPPEDITOR_INTERNAL_CLANGCOMPLETION_H
