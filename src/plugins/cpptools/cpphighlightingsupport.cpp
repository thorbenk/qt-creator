/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#include "cppchecksymbols.h"
#include "cpphighlightingsupport.h"
#include "cpphighlightingsupport_p.h"
#include "cpptoolseditorsupport.h"

#include <coreplugin/ifile.h>
#include <cplusplus/LookupContext.h>
#include <texteditor/basetexteditor.h>
#include <texteditor/itexteditor.h>

#include <QtGui/QTextBlock>
#include <QtGui/QTextEdit>

namespace CppTools {
namespace Internal {

HighlightingImpl::~HighlightingImpl()
{}

InternalCodeModelHighlightingImpl::~InternalCodeModelHighlightingImpl()
{}

QFuture<CppHighlightingSupport::Use> InternalCodeModelHighlightingImpl::highlightingFuture(
        const CPlusPlus::Document::Ptr &doc,
        const CPlusPlus::Snapshot &snapshot,
        int firstLine, int lastLine) const
{
    Q_UNUSED(firstLine);
    Q_UNUSED(lastLine);

    CPlusPlus::LookupContext context(doc, snapshot);
    return CPlusPlus::CheckSymbols::go(doc, context);
}

#ifdef CLANG_HIGHLIGHTING
ClangHighlightingImpl::ClangHighlightingImpl(CppEditorSupport *editorSupport)
    : m_semanticMarker(new Clang::SemanticMarker)
    , m_editorSupport(editorSupport)
{}

ClangHighlightingImpl::~ClangHighlightingImpl()
{}

QFuture<CppHighlightingSupport::Use> ClangHighlightingImpl::highlightingFuture(
        const CPlusPlus::Document::Ptr &doc,
        const CPlusPlus::Snapshot &snapshot,
        int firstLine, int lastLine) const
{
    Q_UNUSED(doc);
    Q_UNUSED(snapshot);

    const QString fileName = m_editorSupport->textEditor()->file()->fileName();
    CPlusPlus::CppModelManagerInterface *modelManager = CPlusPlus::CppModelManagerInterface::instance();
    QList<CPlusPlus::CppModelManagerInterface::ProjectPart::Ptr> parts = modelManager->projectPart(fileName);
    QStringList options;
    if (!parts.isEmpty())
        options = CppTools::ClangUtils::createClangOptions(parts.at(0));

    //### FIXME: the range is way too big.. can't we just update the visible lines?
    CppTools::CreateMarkers *createMarkers = CppTools::CreateMarkers::create(m_semanticMarker, fileName, options, firstLine, lastLine);
    connect(createMarkers, SIGNAL(diagnosticsReady(const QList<Clang::Diagnostic> &)),
            this, SLOT(setDiagnostics(const QList<Clang::Diagnostic> &)));
    return createMarkers->start();
}

void ClangHighlightingImpl::setDiagnostics(const QList<Clang::Diagnostic> &diagnostics)
{
    TextEditor::BaseTextEditorWidget *ed = qobject_cast<TextEditor::BaseTextEditorWidget*>(m_editorSupport->textEditor()->widget());

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
#endif // CLANG_HIGHLIGHTING

}
}

using namespace CppTools;
using namespace CppTools::Internal;

CppHighlightingSupport::CppHighlightingSupport(CppEditorSupport *editorSupport)
    : m_editorSupport(editorSupport)
{
#ifdef CLANG_HIGHLIGHTING
    m_impl = new ClangHighlightingImpl(editorSupport);
#else // !CLANG_HIGHLIGHTING
    m_impl = new InternalCodeModelHighlightingImpl;
#endif // CLANG_HIGHLIGHTING
}

CppHighlightingSupport::~CppHighlightingSupport()
{
    delete m_impl;
}

QFuture<CppHighlightingSupport::Use> CppHighlightingSupport::highlightingFuture(
        const CPlusPlus::Document::Ptr &doc,
        const CPlusPlus::Snapshot &snapshot,
        int firstLine, int lastLine) const
{
    return m_impl->highlightingFuture(doc, snapshot, firstLine, lastLine);
}

void CppHighlightingSupport::setUseClang(bool useClang)
{
#ifdef CLANG_HIGHLIGHTING
    if (useClang && dynamic_cast<InternalCodeModelHighlightingImpl *>(m_impl)) {
        delete m_impl;
        m_impl = new ClangHighlightingImpl(m_editorSupport);
    } else if (!useClang && dynamic_cast<ClangHighlightingImpl *>(m_impl)) {
        delete m_impl;
        m_impl = new InternalCodeModelHighlightingImpl;
    }
#else // !CLANG_HIGHLIGHTING
    Q_UNUSED(useClang);
#endif // CLANG_HIGHLIGHTING
}
