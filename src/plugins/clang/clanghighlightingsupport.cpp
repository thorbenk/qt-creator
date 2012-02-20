#include "clanghighlightingsupport.h"

#include <coreplugin/idocument.h>
#include <texteditor/basetexteditor.h>
#include <texteditor/itexteditor.h>

#include <QTextBlock>
#include <QTextEdit>

using namespace Clang;
using namespace CppTools;

ClangHighlightingSupport::ClangHighlightingSupport(TextEditor::ITextEditor *textEditor)
    : CppHighlightingSupport(textEditor)
    , m_semanticMarker(new Clang::SemanticMarker)
{
}

ClangHighlightingSupport::~ClangHighlightingSupport()
{
}

QFuture<CppHighlightingSupport::Use> ClangHighlightingSupport::highlightingFuture(
        const CPlusPlus::Document::Ptr &doc,
        const CPlusPlus::Snapshot &snapshot) const
{
    Q_UNUSED(doc);
    Q_UNUSED(snapshot);

    TextEditor::BaseTextEditorWidget *ed = qobject_cast<TextEditor::BaseTextEditorWidget *>(editor()->widget());
    int firstLine = 1;
    int lastLine = ed->document()->blockCount();

    const QString fileName = editor()->document()->fileName();
    CPlusPlus::CppModelManagerInterface *modelManager = CPlusPlus::CppModelManagerInterface::instance();
    QList<CPlusPlus::CppModelManagerInterface::ProjectPart::Ptr> parts = modelManager->projectPart(fileName);
    QStringList options;
    if (!parts.isEmpty())
        options = Utils::createClangOptions(parts.at(0));

    //### FIXME: the range is way too big.. can't we just update the visible lines?
    CreateMarkers *createMarkers = CreateMarkers::create(m_semanticMarker, fileName, options, firstLine, lastLine);
//    connect(createMarkers, SIGNAL(diagnosticsReady(const QList<Clang::Diagnostic> &)),
//            this, SLOT(setDiagnostics(const QList<Clang::Diagnostic> &)));
    return createMarkers->start();
}

void ClangHighlightingSupport::setDiagnostics(const QList<Clang::Diagnostic> &diagnostics)
{
    TextEditor::BaseTextEditorWidget *ed = qobject_cast<TextEditor::BaseTextEditorWidget *>(editor()->widget());
    // set up the format for the errors
    QTextCharFormat errorFormat;
    errorFormat.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    errorFormat.setUnderlineColor(Qt::red);

    // set up the format for the warnings.
    QTextCharFormat warningFormat;
    warningFormat.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    warningFormat.setUnderlineColor(Qt::darkYellow);

    QList<QTextEdit::ExtraSelection> selections;
    foreach (const Clang::Diagnostic &m, diagnostics) {
        QTextEdit::ExtraSelection sel;

        switch (m.severity()) {
        case Clang::Diagnostic::Error:
            sel.format = errorFormat;
            break;

        case Clang::Diagnostic::Warning:
            sel.format = warningFormat;
            break;

        default:
            continue;
        }

        QTextCursor c(ed->document()->findBlockByNumber(m.location().line() - 1));
        const int linePos = c.position();
        c.setPosition(linePos + m.location().column() - 1);

        const QString text = c.block().text();
        if (m.length() == 0) {
            for (int i = m.location().column() - 1; i < text.size(); ++i) {
                if (text.at(i).isSpace()) {
                    c.setPosition(linePos + i, QTextCursor::KeepAnchor);
                    break;
                }
            }
        } else {
            c.setPosition(c.position() + m.length(), QTextCursor::KeepAnchor);
        }

        sel.cursor = c;
        sel.format.setToolTip(m.spelling());
        selections.append(sel);
    }

    ed->setExtraSelections(TextEditor::BaseTextEditorWidget::CodeWarningsSelection, selections);
}



ClangHighlightingSupportFactory::~ClangHighlightingSupportFactory()
{
}

CppHighlightingSupport *ClangHighlightingSupportFactory::highlightingSupport(TextEditor::ITextEditor *editor)
{
    return new ClangHighlightingSupport(editor);
}
