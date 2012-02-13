#ifndef CPPHIGHLIGHTINGSUPPORT_P_H
#define CPPHIGHLIGHTINGSUPPORT_P_H

#include "cpphighlightingsupport.h"

#ifdef CLANG_HIGHLIGHTING
#  include "clangutils.h"
#  include "cppcreatemarkers.h"
#endif // CLANG_HIGHLIGHTING

namespace CppTools {
namespace Internal {

class HighlightingImpl: public QObject
{
    Q_OBJECT

public:
    typedef TextEditor::SemanticHighlighter::Result Use;

public:
    virtual ~HighlightingImpl() = 0;

    virtual QFuture<Use> highlightingFuture(const CPlusPlus::Document::Ptr &doc,
                                            const CPlusPlus::Snapshot &snapshot,
                                            int firstLine,
                                            int lastLine) const = 0;
};

class InternalCodeModelHighlightingImpl: public HighlightingImpl
{
    Q_OBJECT

public:
    virtual ~InternalCodeModelHighlightingImpl();

    virtual QFuture<Use> highlightingFuture(const CPlusPlus::Document::Ptr &doc,
                                            const CPlusPlus::Snapshot &snapshot,
                                            int firstLine, int lastLine) const;
};

#ifdef CLANG_HIGHLIGHTING
class ClangHighlightingImpl: public HighlightingImpl
{
    Q_OBJECT

public:
    ClangHighlightingImpl(CppEditorSupport *editorSupport);
    virtual ~ClangHighlightingImpl();

    virtual QFuture<Use> highlightingFuture(const CPlusPlus::Document::Ptr &doc,
                                            const CPlusPlus::Snapshot &snapshot,
                                            int firstLine, int lastLine) const;

protected slots:
    void setDiagnostics(const QList<Clang::Diagnostic> &diagnostics);

private:
    Clang::SemanticMarker::Ptr m_semanticMarker;
    CppEditorSupport *m_editorSupport;
};
#endif // CLANG_HIGHLIGHTING

}
}

#endif // CPPHIGHLIGHTINGSUPPORT_P_H
