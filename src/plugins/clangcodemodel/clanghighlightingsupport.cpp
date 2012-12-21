#include "clanghighlightingsupport.h"

#include <coreplugin/idocument.h>
#include <texteditor/basetexteditor.h>
#include <texteditor/itexteditor.h>

#include <QTextBlock>
#include <QTextEdit>


using namespace ClangCodeModel;
using namespace ClangCodeModel::Internal;
using namespace CppTools;

DiagnosticsHandler::DiagnosticsHandler(TextEditor::ITextEditor *textEditor)
    : m_editor(textEditor)
{
}

void DiagnosticsHandler::setDiagnostics(const QList<ClangCodeModel::Diagnostic> &diagnostics)
{
    TextEditor::BaseTextEditorWidget *ed = qobject_cast<TextEditor::BaseTextEditorWidget *>(m_editor->widget());
    // set up the format for the errors
    QTextCharFormat errorFormat;
    errorFormat.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    errorFormat.setUnderlineColor(Qt::red);

    // set up the format for the warnings.
    QTextCharFormat warningFormat;
    warningFormat.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    warningFormat.setUnderlineColor(Qt::darkYellow);

    QList<QTextEdit::ExtraSelection> selections;
    foreach (const ClangCodeModel::Diagnostic &m, diagnostics) {
        QTextEdit::ExtraSelection sel;

        switch (m.severity()) {
        case ClangCodeModel::Diagnostic::Error:
        case ClangCodeModel::Diagnostic::Fatal:
            sel.format = errorFormat;
            break;

        case ClangCodeModel::Diagnostic::Warning:
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
            int i = m.location().column() - 1;
            if (i == text.size() || text.at(i).isSpace()) {
                // backward scan
                --i;
                for ( ; i > 0; --i) {
                    if (text.at(i).isSpace()) {
                        c.setPosition(linePos + i + 1, QTextCursor::KeepAnchor);
                        break;
                    }
                }
            } else {
                // forward scan
                for ( ; i < text.size(); ++i)
                    if (text.at(i).isSpace()) {
                        ++i;
                        break;
                    }
                c.setPosition(linePos + i, QTextCursor::KeepAnchor);
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

ClangHighlightingSupport::ClangHighlightingSupport(TextEditor::ITextEditor *textEditor)
    : CppHighlightingSupport(textEditor)
    , m_semanticMarker(new ClangCodeModel::SemanticMarker)
    , m_diagnosticsHandler(new DiagnosticsHandler(textEditor))
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
    foreach (const CPlusPlus::CppModelManagerInterface::ProjectPart::Ptr &part, parts) {
        options = Utils::createClangOptions(part, fileName);
        if (!options.isEmpty())
            break;
    }

    //### FIXME: the range is way too big.. can't we just update the visible lines?
    CreateMarkers *createMarkers = CreateMarkers::create(m_semanticMarker, fileName, options, firstLine, lastLine);
    QObject::connect(createMarkers, SIGNAL(diagnosticsReady(const QList<ClangCodeModel::Diagnostic> &)),
                     m_diagnosticsHandler.data(), SLOT(setDiagnostics(const QList<ClangCodeModel::Diagnostic> &)));
    return createMarkers->start();
}

ClangHighlightingSupportFactory::~ClangHighlightingSupportFactory()
{
}

CppHighlightingSupport *ClangHighlightingSupportFactory::highlightingSupport(TextEditor::ITextEditor *editor)
{
    return new ClangHighlightingSupport(editor);
}
