#ifndef CLANG_CLANGHIGHLIGHTINGSUPPORT_H
#define CLANG_CLANGHIGHLIGHTINGSUPPORT_H

#include "clangutils.h"
#include "cppcreatemarkers.h"
#include "fastindexer.h"

#include <cpptools/cpphighlightingsupport.h>

#include <QObject>
#include <QScopedPointer>

namespace ClangCodeModel {
namespace Internal {

class DiagnosticsHandler: public QObject
{
    Q_OBJECT

public:
    DiagnosticsHandler(TextEditor::ITextEditor *textEditor);

protected slots:
    void setDiagnostics(const QList<ClangCodeModel::Diagnostic> &diagnostics);

private:
    TextEditor::ITextEditor *m_editor;
};

} // namespace Internal

class ClangHighlightingSupport: public CppTools::CppHighlightingSupport
{
public:
    ClangHighlightingSupport(TextEditor::ITextEditor *textEditor, Internal::FastIndexer *fastIndexer);
    ~ClangHighlightingSupport();

    virtual QFuture<Use> highlightingFuture(const CPlusPlus::Document::Ptr &doc,
                                            const CPlusPlus::Snapshot &snapshot) const;

private:
    Internal::FastIndexer *m_fastIndexer;
    ClangCodeModel::SemanticMarker::Ptr m_semanticMarker;
    QScopedPointer<Internal::DiagnosticsHandler> m_diagnosticsHandler;
};

class ClangHighlightingSupportFactory: public CppTools::CppHighlightingSupportFactory
{
public:
    ClangHighlightingSupportFactory(Internal::FastIndexer *fastIndexer)
        : m_fastIndexer(fastIndexer)
    {}

    virtual ~ClangHighlightingSupportFactory();

    virtual CppTools::CppHighlightingSupport *highlightingSupport(TextEditor::ITextEditor *editor);

    virtual bool hightlighterHandlesDiagnostics() const
    { return true; }

private:
    Internal::FastIndexer *m_fastIndexer;
};

} // namespace ClangCodeModel

#endif // CLANG_CLANGHIGHLIGHTINGSUPPORT_H
