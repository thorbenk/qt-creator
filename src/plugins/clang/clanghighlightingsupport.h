#ifndef CLANG_CLANGHIGHLIGHTINGSUPPORT_H
#define CLANG_CLANGHIGHLIGHTINGSUPPORT_H

#include "clangutils.h"
#include "cppcreatemarkers.h"

#include <cpptools/cpphighlightingsupport.h>

namespace Clang {

class ClangHighlightingSupport: public CppTools::CppHighlightingSupport
{
    Q_OBJECT

public:
    ClangHighlightingSupport(TextEditor::ITextEditor *textEditor);
    ~ClangHighlightingSupport();

    virtual QFuture<Use> highlightingFuture(const CPlusPlus::Document::Ptr &doc,
                                            const CPlusPlus::Snapshot &snapshot) const;

protected slots:
    void setDiagnostics(const QList<Clang::Diagnostic> &diagnostics);

private:
    Clang::SemanticMarker::Ptr m_semanticMarker;
};

class ClangHighlightingSupportFactory: public CppTools::CppHighlightingSupportFactory
{
public:
    virtual ~ClangHighlightingSupportFactory();

    virtual CppTools::CppHighlightingSupport *highlightingSupport(TextEditor::ITextEditor *editor);
};

} // namespace Clang

#endif // CLANG_CLANGHIGHLIGHTINGSUPPORT_H
