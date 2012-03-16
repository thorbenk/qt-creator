#ifndef CLANG_CLANGHIGHLIGHTINGSUPPORT_H
#define CLANG_CLANGHIGHLIGHTINGSUPPORT_H

#include "clangutils.h"
#include "cppcreatemarkers.h"

#include <cpptools/cpphighlightingsupport.h>

#include <QObject>
#include <QScopedPointer>

namespace Clang {
namespace Internal {

class DiagnosticsHandler: public QObject
{
    Q_OBJECT

public:
    DiagnosticsHandler(TextEditor::ITextEditor *textEditor);

protected slots:
    void setDiagnostics(const QList<Clang::Diagnostic> &diagnostics);

private:
    TextEditor::ITextEditor *m_editor;
};

} // namespace Internal

class ClangHighlightingSupport: public CppTools::CppHighlightingSupport
{
public:
    ClangHighlightingSupport(TextEditor::ITextEditor *textEditor);
    ~ClangHighlightingSupport();

    virtual QFuture<Use> highlightingFuture(const CPlusPlus::Document::Ptr &doc,
                                            const CPlusPlus::Snapshot &snapshot) const;

private:
    Clang::SemanticMarker::Ptr m_semanticMarker;
    QScopedPointer<Internal::DiagnosticsHandler> m_diagnosticsHandler;
};

class ClangHighlightingSupportFactory: public CppTools::CppHighlightingSupportFactory
{
public:
    virtual ~ClangHighlightingSupportFactory();

    virtual CppTools::CppHighlightingSupport *highlightingSupport(TextEditor::ITextEditor *editor);

    virtual bool hightlighterHandlesDiagnostics() const
    { return true; }
};

} // namespace Clang

#endif // CLANG_CLANGHIGHLIGHTINGSUPPORT_H
