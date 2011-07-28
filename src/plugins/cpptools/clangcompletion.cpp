#include "clangcompletion.h"
#include "clangutils.h"
#include "cppdoxygen.h"
#include "ModelManagerInterface.h"

#include <coreplugin/icore.h>
#include <coreplugin/ifile.h>
#include <coreplugin/mimedatabase.h>

#include <cplusplus/BackwardsScanner.h>
#include <cplusplus/ExpressionUnderCursor.h>
#include <cplusplus/Token.h>

#include <texteditor/basetexteditor.h>
#include <texteditor/convenience.h>
#include <texteditor/codeassist/basicproposalitemlistmodel.h>
#include <texteditor/codeassist/functionhintproposal.h>
#include <texteditor/codeassist/genericproposal.h>
#include <texteditor/codeassist/ifunctionhintproposalmodel.h>

#include <QTextCursor>
#include <QTextDocument>

#undef DEBUG_TIMING

using namespace Clang;
using namespace CPlusPlus;
using namespace CppTools;
using namespace CppTools::Internal;
using namespace TextEditor;

namespace {

int activationSequenceChar(const QChar &ch,
                           const QChar &ch2,
                           const QChar &ch3,
                           unsigned *kind,
                           bool wantFunctionCall)
{
    int referencePosition = 0;
    int completionKind = T_EOF_SYMBOL;
    switch (ch.toLatin1()) {
    case '.':
        if (ch2 != QLatin1Char('.')) {
            completionKind = T_DOT;
            referencePosition = 1;
        }
        break;
    case ',':
        completionKind = T_COMMA;
        referencePosition = 1;
        break;
    case '(':
        if (wantFunctionCall) {
            completionKind = T_LPAREN;
            referencePosition = 1;
        }
        break;
    case ':':
        if (ch3 != QLatin1Char(':') && ch2 == QLatin1Char(':')) {
            completionKind = T_COLON_COLON;
            referencePosition = 2;
        }
        break;
    case '>':
        if (ch2 == QLatin1Char('-')) {
            completionKind = T_ARROW;
            referencePosition = 2;
        }
        break;
    case '*':
        if (ch2 == QLatin1Char('.')) {
            completionKind = T_DOT_STAR;
            referencePosition = 2;
        } else if (ch3 == QLatin1Char('-') && ch2 == QLatin1Char('>')) {
            completionKind = T_ARROW_STAR;
            referencePosition = 3;
        }
        break;
    case '\\':
    case '@':
        if (ch2.isNull() || ch2.isSpace()) {
            completionKind = T_DOXY_COMMENT;
            referencePosition = 1;
        }
        break;
    case '<':
        completionKind = T_ANGLE_STRING_LITERAL;
        referencePosition = 1;
        break;
    case '"':
        completionKind = T_STRING_LITERAL;
        referencePosition = 1;
        break;
    case '/':
        completionKind = T_SLASH;
        referencePosition = 1;
        break;
    case '#':
        completionKind = T_POUND;
        referencePosition = 1;
        break;
    }

    if (kind)
        *kind = completionKind;

    return referencePosition;
}

static QList<CodeCompletionResult> unfilteredCompletion(const ClangCompletionAssistInterface* interface, const QString &fileName, unsigned line, unsigned column)
{
    ClangWrapper::Ptr wrapper = interface->clangWrapper();
    QMutexLocker lock(wrapper->mutex());
    //### TODO: check if we're cancelled after we managed to aquire the mutex

    if (fileName != wrapper->fileName())
        wrapper->setFileName(fileName);

#if DEBUG_TIMING
    qDebug() << "Here we go with ClangCompletionAssistProcessor....";
    QTime t;
    t.start();
#endif // DEBUG_TIMING

    QList<CodeCompletionResult> result = wrapper->codeCompleteAt(line, column + 1, interface->unsavedFiles());

#ifdef DEBUG_TIMING
    qDebug() << "... Completion done in" << t.elapsed() << "ms, with" << result.count() << "items.";
#endif // DEBUG_TIMING

    return result;
}

} // Anonymous

namespace CppTools {
namespace Internal {

// ----------------------
// CppAssistProposalModel
// ----------------------
class ClangAssistProposalModel : public TextEditor::BasicProposalItemListModel
{
public:
    ClangAssistProposalModel()
        : TextEditor::BasicProposalItemListModel()
        , m_sortable(false)
        , m_completionOperator(T_EOF_SYMBOL)
        , m_replaceDotForArrow(false)
    {}

    virtual bool isSortable() const { return m_sortable; }
    bool m_sortable;
    unsigned m_completionOperator;
    bool m_replaceDotForArrow;
};

// -----------------
// CppAssistProposal
// -----------------
class ClangAssistProposal : public TextEditor::GenericProposal
{
public:
    ClangAssistProposal(int cursorPos, TextEditor::IGenericProposalModel *model)
        : TextEditor::GenericProposal(cursorPos, model)
        , m_replaceDotForArrow(static_cast<ClangAssistProposalModel *>(model)->m_replaceDotForArrow)
    {}

    virtual bool isCorrective() const { return m_replaceDotForArrow; }
    virtual void makeCorrection(BaseTextEditor *editor)
    {
        editor->setCursorPosition(basePosition() - 1);
        editor->replace(1, QLatin1String("->"));
        moveBasePosition(1);
    }

private:
    bool m_replaceDotForArrow;
};

// --------------------
// CppFunctionHintModel
// --------------------
class ClangFunctionHintModel : public TextEditor::IFunctionHintProposalModel
{
public:
    ClangFunctionHintModel(const QList<CodeCompletionResult> functionSymbols)
        : m_functionSymbols(functionSymbols)
        , m_currentArg(-1)
    {}

    virtual void reset() {}
    virtual int size() const { return m_functionSymbols.size(); }
    virtual QString text(int index) const;
    virtual int activeArgument(const QString &prefix) const;

private:
    QList<Clang::CodeCompletionResult> m_functionSymbols;
    mutable int m_currentArg;
};

QString ClangFunctionHintModel::text(int index) const
{
#if 0
    Overview overview;
    overview.setShowReturnTypes(true);
    overview.setShowArgumentNames(true);
    overview.setMarkedArgument(m_currentArg + 1);
    Function *f = m_functionSymbols.at(index);

    const QString prettyMethod = overview(f->type(), f->name());
    const int begin = overview.markedArgumentBegin();
    const int end = overview.markedArgumentEnd();

    QString hintText;
    hintText += Qt::escape(prettyMethod.left(begin));
    hintText += "<b>";
    hintText += Qt::escape(prettyMethod.mid(begin, end - begin));
    hintText += "</b>";
    hintText += Qt::escape(prettyMethod.mid(end));
    return hintText;
#else
    return m_functionSymbols.at(index).hint;
#endif
}

int ClangFunctionHintModel::activeArgument(const QString &prefix) const
{
    int argnr = 0;
    int parcount = 0;
    SimpleLexer tokenize;
    QList<Token> tokens = tokenize(prefix);
    for (int i = 0; i < tokens.count(); ++i) {
        const Token &tk = tokens.at(i);
        if (tk.is(T_LPAREN))
            ++parcount;
        else if (tk.is(T_RPAREN))
            --parcount;
        else if (! parcount && tk.is(T_COMMA))
            ++argnr;
    }

    if (parcount < 0)
        return -1;

    if (argnr != m_currentArg)
        m_currentArg = argnr;

    return argnr;
}

} // namespace Internal
} // namespace CppTools

bool ClangCompletionAssistInterface::objcEnabled() const
{
    return m_clangWrapper->objcEnabled();
}

ClangCompletionAssistInterface::ClangCompletionAssistInterface(
        ClangWrapper::Ptr clangWrapper,
        QTextDocument *document,
        int position,
        Core::IFile *file,
        AssistReason reason,
        const QStringList &includePaths,
        const QStringList &frameworkPaths)
    : DefaultAssistInterface(document, position, file, reason)
    , m_clangWrapper(clangWrapper)
    , m_includePaths(includePaths)
    , m_frameworkPaths(frameworkPaths)
{
    Q_ASSERT(!clangWrapper.isNull());

    CppModelManagerInterface *mmi = CppModelManagerInterface::instance();
    Q_ASSERT(mmi);
    m_unsavedFiles = ClangUtils::createUnsavedFiles(mmi->workingCopy());
}

ClangCompletionAssistProcessor::ClangCompletionAssistProcessor()
    : m_preprocessorCompletions(QStringList()
          << QLatin1String("define")
          << QLatin1String("error")
          << QLatin1String("include")
          << QLatin1String("line")
          << QLatin1String("pragma")
          << QLatin1String("undef")
          << QLatin1String("if")
          << QLatin1String("ifdef")
          << QLatin1String("ifndef")
          << QLatin1String("elif")
          << QLatin1String("else")
          << QLatin1String("endif"))
    , m_model(new ClangAssistProposalModel)
    , m_hintProposal(0)

{
}

ClangCompletionAssistProcessor::~ClangCompletionAssistProcessor()
{
}

IAssistProposal *ClangCompletionAssistProcessor::perform(const IAssistInterface *interface)
{
    m_interface.reset(static_cast<const ClangCompletionAssistInterface *>(interface));

    if (interface->reason() != ExplicitlyInvoked && !accepts())
        return 0;

    int index = startCompletionHelper();
    if (index != -1) {
        if (m_hintProposal)
            return m_hintProposal;

        if (m_model->m_completionOperator != T_EOF_SYMBOL)
            m_model->m_sortable = true;
        else
            m_model->m_sortable = false;
        return createContentProposal();
    }

    return 0;
}

int ClangCompletionAssistProcessor::startCompletionHelper()
{
    Q_ASSERT(m_model);

    const int startOfName = findStartOfName();
    m_startPosition = startOfName;
    m_model->m_completionOperator = T_EOF_SYMBOL;

    int endOfOperator = m_startPosition;

    // Skip whitespace preceding this position
    while (m_interface->characterAt(endOfOperator - 1).isSpace())
        --endOfOperator;

    const Core::IFile *file = m_interface->file();
    const QString fileName = file->fileName();

    int endOfExpression = startOfOperator(endOfOperator,
                                          &m_model->m_completionOperator,
                                          /*want function call =*/ true);

    if (m_model->m_completionOperator == T_EOF_SYMBOL) {
        endOfOperator = m_startPosition;
    } else if (m_model->m_completionOperator == T_DOXY_COMMENT) {
        for (int i = 1; i < T_DOXY_LAST_TAG; ++i)
            addCompletionItem(QString::fromLatin1(doxygenTagSpell(i)),
                              m_icons.keywordIcon());
        return m_startPosition;
    }

    // Pre-processor completion
    //### TODO: check if clang can do pp completion
    if (m_model->m_completionOperator == T_POUND) {
        completePreprocessor();
        m_startPosition = startOfName;
        return m_startPosition;
    }

    // Include completion
    if (m_model->m_completionOperator == T_STRING_LITERAL
            || m_model->m_completionOperator == T_ANGLE_STRING_LITERAL
            || m_model->m_completionOperator == T_SLASH) {

        QTextCursor c(m_interface->document());
        c.setPosition(endOfExpression);
        if (completeInclude(c))
            m_startPosition = startOfName;
        return m_startPosition;
    }

    ExpressionUnderCursor expressionUnderCursor;
    QTextCursor tc(m_interface->document());

    if (m_model->m_completionOperator == T_COMMA) {
        tc.setPosition(endOfExpression);
        const int start = expressionUnderCursor.startOfFunctionCall(tc);
        if (start == -1) {
            m_model->m_completionOperator = T_EOF_SYMBOL;
            return -1;
        }

        endOfExpression = start;
        m_startPosition = start + 1;
        m_model->m_completionOperator = T_LPAREN;
    }

    QString expression;
    int startOfExpression = m_interface->position();
    tc.setPosition(endOfExpression);

    if (m_model->m_completionOperator) {
        expression = expressionUnderCursor(tc);
        startOfExpression = endOfExpression - expression.length();

        if (m_model->m_completionOperator == T_LPAREN) {
            if (expression.endsWith(QLatin1String("SIGNAL")))
                m_model->m_completionOperator = T_SIGNAL;

            else if (expression.endsWith(QLatin1String("SLOT")))
                m_model->m_completionOperator = T_SLOT;

            else if (m_interface->position() != endOfOperator) {
                // We don't want a function completion when the cursor isn't at the opening brace
                expression.clear();
                m_model->m_completionOperator = T_EOF_SYMBOL;
                m_startPosition = startOfName;
                startOfExpression = m_interface->position();
            }
        }
    } else if (expression.isEmpty()) {
        while (startOfExpression > 0 && m_interface->characterAt(startOfExpression).isSpace())
            --startOfExpression;
    }

    int line = 0, column = 0;
//    Convenience::convertPosition(m_interface->document(), startOfExpression, &line, &column);
    Convenience::convertPosition(m_interface->document(), endOfOperator, &line, &column);
    return startCompletionInternal(fileName, line, column, expression, endOfOperator);
}

int ClangCompletionAssistProcessor::startOfOperator(int pos,
                                                    unsigned *kind,
                                                    bool wantFunctionCall) const
{
    const QChar ch  = pos > -1 ? m_interface->characterAt(pos - 1) : QChar();
    const QChar ch2 = pos >  0 ? m_interface->characterAt(pos - 2) : QChar();
    const QChar ch3 = pos >  1 ? m_interface->characterAt(pos - 3) : QChar();

    int start = pos - activationSequenceChar(ch, ch2, ch3, kind, wantFunctionCall);
    if (start != pos) {
        QTextCursor tc(m_interface->document());
        tc.setPosition(pos);

        // Include completion: make sure the quote character is the first one on the line
        if (*kind == T_STRING_LITERAL) {
            QTextCursor s = tc;
            s.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
            QString sel = s.selectedText();
            if (sel.indexOf(QLatin1Char('"')) < sel.length() - 1) {
                *kind = T_EOF_SYMBOL;
                start = pos;
            }
        }

        if (*kind == T_COMMA) {
            ExpressionUnderCursor expressionUnderCursor;
            if (expressionUnderCursor.startOfFunctionCall(tc) == -1) {
                *kind = T_EOF_SYMBOL;
                start = pos;
            }
        }

        SimpleLexer tokenize;
        tokenize.setQtMocRunEnabled(true);
        tokenize.setObjCEnabled(true);
        tokenize.setSkipComments(false);
        const QList<Token> &tokens = tokenize(tc.block().text(), BackwardsScanner::previousBlockState(tc.block()));
        const int tokenIdx = SimpleLexer::tokenBefore(tokens, qMax(0, tc.positionInBlock() - 1)); // get the token at the left of the cursor
        const Token tk = (tokenIdx == -1) ? Token() : tokens.at(tokenIdx);

        if (*kind == T_DOXY_COMMENT && !(tk.is(T_DOXY_COMMENT) || tk.is(T_CPP_DOXY_COMMENT))) {
            *kind = T_EOF_SYMBOL;
            start = pos;
        }
        // Don't complete in comments or strings, but still check for include completion
        else if (tk.is(T_COMMENT) || tk.is(T_CPP_COMMENT) ||
                 (tk.isLiteral() && (*kind != T_STRING_LITERAL
                                     && *kind != T_ANGLE_STRING_LITERAL
                                     && *kind != T_SLASH))) {
            *kind = T_EOF_SYMBOL;
            start = pos;
        }
        // Include completion: can be triggered by slash, but only in a string
        else if (*kind == T_SLASH && (tk.isNot(T_STRING_LITERAL) && tk.isNot(T_ANGLE_STRING_LITERAL))) {
            *kind = T_EOF_SYMBOL;
            start = pos;
        }
        else if (*kind == T_LPAREN) {
            if (tokenIdx > 0) {
                const Token &previousToken = tokens.at(tokenIdx - 1); // look at the token at the left of T_LPAREN
                switch (previousToken.kind()) {
                case T_IDENTIFIER:
                case T_GREATER:
                case T_SIGNAL:
                case T_SLOT:
                    break; // good

                default:
                    // that's a bad token :)
                    *kind = T_EOF_SYMBOL;
                    start = pos;
                }
            }
        }
        // Check for include preprocessor directive
        else if (*kind == T_STRING_LITERAL || *kind == T_ANGLE_STRING_LITERAL || *kind == T_SLASH) {
            bool include = false;
            if (tokens.size() >= 3) {
                if (tokens.at(0).is(T_POUND) && tokens.at(1).is(T_IDENTIFIER) && (tokens.at(2).is(T_STRING_LITERAL) ||
                                                                                  tokens.at(2).is(T_ANGLE_STRING_LITERAL))) {
                    const Token &directiveToken = tokens.at(1);
                    QString directive = tc.block().text().mid(directiveToken.begin(),
                                                              directiveToken.length());
                    if (directive == QLatin1String("include") ||
                            directive == QLatin1String("include_next") ||
                            directive == QLatin1String("import")) {
                        include = true;
                    }
                }
            }

            if (!include) {
                *kind = T_EOF_SYMBOL;
                start = pos;
            }
        }
    }

    return start;
}

int ClangCompletionAssistProcessor::findStartOfName(int pos) const
{
    if (pos == -1)
        pos = m_interface->position();
    QChar chr;

    // Skip to the start of a name
    do {
        chr = m_interface->characterAt(--pos);
    } while (chr.isLetterOrNumber() || chr == QLatin1Char('_'));

    return pos + 1;
}

bool ClangCompletionAssistProcessor::accepts() const
{
    const int pos = m_interface->position();
    unsigned token = T_EOF_SYMBOL;

    const int start = startOfOperator(pos, &token, /*want function call=*/ true);
    if (start != pos) {
        if (token == T_POUND) {
            const int column = pos - m_interface->document()->findBlock(start).position();
            if (column != 1)
                return false;
        }

        return true;
    } else {
        // Trigger completion after three characters of a name have been typed, when not editing an existing name
        QChar characterUnderCursor = m_interface->characterAt(pos);
        if (!characterUnderCursor.isLetterOrNumber() && characterUnderCursor != QLatin1Char('_')) {
            const int startOfName = findStartOfName(pos);
            if (pos - startOfName >= 3) {
                const QChar firstCharacter = m_interface->characterAt(startOfName);
                if (firstCharacter.isLetter() || firstCharacter == QLatin1Char('_')) {
                    // Finally check that we're not inside a comment or string (code copied from startOfOperator)
                    QTextCursor tc(m_interface->document());
                    tc.setPosition(pos);

                    SimpleLexer tokenize;
                    tokenize.setQtMocRunEnabled(true);
                    tokenize.setObjCEnabled(true);
                    tokenize.setSkipComments(false);
                    const QList<Token> &tokens = tokenize(tc.block().text(), BackwardsScanner::previousBlockState(tc.block()));
                    const int tokenIdx = SimpleLexer::tokenBefore(tokens, qMax(0, tc.positionInBlock() - 1));
                    const Token tk = (tokenIdx == -1) ? Token() : tokens.at(tokenIdx);

                    if (!tk.isComment() && !tk.isLiteral()) {
                        return true;
                    } else if (tk.isLiteral()
                               && tokens.size() == 3
                               && tokens.at(0).kind() == T_POUND
                               && tokens.at(1).kind() == T_IDENTIFIER) {
                        const QString &line = tc.block().text();
                        const Token &idToken = tokens.at(1);
                        const QStringRef &identifier =
                                line.midRef(idToken.begin(), idToken.end() - idToken.begin());
                        if (identifier == QLatin1String("include")
                                || identifier == QLatin1String("include_next")
                                || (m_interface->objcEnabled() && identifier == QLatin1String("import"))) {
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}

IAssistProposal *ClangCompletionAssistProcessor::createContentProposal()
{
#if 0
    // Duplicates are kept only if they are snippets.
    QSet<QString> processed;
    QList<BasicProposalItem *>::iterator it = m_completions.begin();
    while (it != m_completions.end()) {
        CppAssistProposalItem *item = static_cast<CppAssistProposalItem *>(*it);
        if (!processed.contains(item->text()) || item->data().canConvert<QString>()) {
            ++it;
            if (!item->data().canConvert<QString>()) {
                processed.insert(item->text());
                if (!item->isOverloaded()) {
                    if (Symbol *symbol = qvariant_cast<Symbol *>(item->data())) {
                        if (Function *funTy = symbol->type()->asFunctionType()) {
                            if (funTy->hasArguments())
                                item->markAsOverloaded();
                        }
                    }
                }
            }
        } else {
            it = m_completions.erase(it);
        }
    }
#endif

    m_model->loadContent(m_completions);
    return new ClangAssistProposal(m_startPosition, m_model.take());
}

int ClangCompletionAssistProcessor::startCompletionInternal(const QString fileName,
                                                            unsigned line,
                                                            unsigned column,
                                                            const QString &/*expr*/,
                                                            int endOfExpression)
{
    if (m_model->m_completionOperator == T_LPAREN) {
        // Find the expression that precedes the current name
        int index = endOfExpression;
        while (m_interface->characterAt(index - 1).isSpace())
            --index;

        QTextCursor tc(m_interface->document());
        tc.setPosition(index);
        ExpressionUnderCursor euc;
        index = euc.startOfFunctionCall(tc);
        int nameStart = findStartOfName(index);
        QTextCursor tc2(m_interface->document());
        tc2.setPosition(nameStart);
        tc2.setPosition(index, QTextCursor::KeepAnchor);
        const QString functionName = tc2.selectedText().trimmed();
        int l = line, c = column;
        Convenience::convertPosition(m_interface->document(), nameStart, &l, &c);
#ifdef DEBUG_TIMING
        qDebug()<<"complete constructor or function @" << line<<":"<<column << "->"<<l<<":"<<c;
#endif // DEBUG_TIMING
        const QList<CodeCompletionResult> completions = unfilteredCompletion(m_interface.data(), fileName, l, c);
        QList<CodeCompletionResult> functionCompletions;
        foreach (const CodeCompletionResult &ccr, completions) {
            if (ccr.isFunctionCompletion)
                if (ccr.text == functionName)
                    functionCompletions.append(ccr);
        }

        if (!functionCompletions.isEmpty()) {
            IFunctionHintProposalModel *model = new ClangFunctionHintModel(functionCompletions);
            m_hintProposal = new FunctionHintProposal(m_startPosition, model);
            return m_startPosition;
        }
    }

    QList<CodeCompletionResult> completions = unfilteredCompletion(m_interface.data(), fileName, line, column);
    foreach (const CodeCompletionResult &ccr, completions) {
        if (!ccr.isValid())
            continue;

        BasicProposalItem *item = new BasicProposalItem;
        item->setText(ccr.text);
        item->setOrder(ccr.priority());
        item->setData(qVariantFromValue(ccr));
        m_completions.append(item);
    }

    return m_startPosition;
#if 0
    QString expression = expr.trimmed();

    if (expression.isEmpty()) {
        if (m_model->m_completionOperator == T_SIGNAL || m_model->m_completionOperator == T_SLOT) {
            // Apply signal/slot completion on 'this'
            expression = QLatin1String("this");
        }
    }

    QList<LookupItem> results =
            (*m_model->m_typeOfExpression)(expression, scope, TypeOfExpression::Preprocess);

    if (results.isEmpty()) {
        if (m_model->m_completionOperator == T_SIGNAL || m_model->m_completionOperator == T_SLOT) {
            if (! (expression.isEmpty() || expression == QLatin1String("this"))) {
                expression = QLatin1String("this");
                results = (*m_model->m_typeOfExpression)(expression, scope);
            }

            if (results.isEmpty())
                return -1;

        } else if (m_model->m_completionOperator == T_LPAREN) {
            // Find the expression that precedes the current name
            int index = endOfExpression;
            while (m_interface->characterAt(index - 1).isSpace())
                --index;
            index = findStartOfName(index);

            QTextCursor tc(m_interface->document());
            tc.setPosition(index);

            ExpressionUnderCursor expressionUnderCursor;
            const QString baseExpression = expressionUnderCursor(tc);

            // Resolve the type of this expression
            const QList<LookupItem> results =
                    (*m_model->m_typeOfExpression)(baseExpression, scope,
                                     TypeOfExpression::Preprocess);

            // If it's a class, add completions for the constructors
            foreach (const LookupItem &result, results) {
                if (result.type()->isClassType()) {
                    if (completeConstructorOrFunction(results, endOfExpression, true))
                        return m_startPosition;

                    break;
                }
            }
            return -1;

        } else {
            // nothing to do.
            return -1;

        }
    }

    switch (m_model->m_completionOperator) {
    case T_LPAREN:
        if (completeConstructorOrFunction(results, endOfExpression, false))
            return m_startPosition;
        break;

    case T_DOT:
    case T_ARROW:
        if (completeMember(results))
            return m_startPosition;
        break;

    case T_COLON_COLON:
        if (completeScope(results))
            return m_startPosition;
        break;

    case T_SIGNAL:
        if (completeSignal(results))
            return m_startPosition;
        break;

    case T_SLOT:
        if (completeSlot(results))
            return m_startPosition;
        break;

    default:
        break;
    } // end of switch

    // nothing to do.
    return -1;
#endif
}

bool ClangCompletionAssistProcessor::completeInclude(const QTextCursor &cursor)
{
    QString directoryPrefix;
    if (m_model->m_completionOperator == T_SLASH) {
        QTextCursor c = cursor;
        c.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
        QString sel = c.selectedText();
        int startCharPos = sel.indexOf(QLatin1Char('"'));
        if (startCharPos == -1) {
            startCharPos = sel.indexOf(QLatin1Char('<'));
            m_model->m_completionOperator = T_ANGLE_STRING_LITERAL;
        } else {
            m_model->m_completionOperator = T_STRING_LITERAL;
        }
        if (startCharPos != -1)
            directoryPrefix = sel.mid(startCharPos + 1, sel.length() - 1);
    }

    // Make completion for all relevant includes
    QStringList includePaths = m_interface->includePaths();
    const QString &currentFilePath = QFileInfo(m_interface->file()->fileName()).path();
    if (!includePaths.contains(currentFilePath))
        includePaths.append(currentFilePath);

    const Core::MimeType mimeType =
            Core::ICore::instance()->mimeDatabase()->findByType(QLatin1String("text/x-c++hdr"));
    const QStringList suffixes = mimeType.suffixes();

    foreach (const QString &includePath, includePaths) {
        QString realPath = includePath;
        if (!directoryPrefix.isEmpty()) {
            realPath += QLatin1Char('/');
            realPath += directoryPrefix;
        }
        completeInclude(realPath, suffixes);
    }

    foreach (const QString &frameworkPath, m_interface->frameworkPaths()) {
        QString realPath = frameworkPath;
        if (!directoryPrefix.isEmpty()) {
            realPath += QLatin1Char('/');
            realPath += directoryPrefix;
            realPath += QLatin1String(".framework/Headers");
        }
        completeInclude(realPath, suffixes);
    }

    return !m_completions.isEmpty();
}

void ClangCompletionAssistProcessor::completeInclude(const QString &realPath,
                                                   const QStringList &suffixes)
{
    QDirIterator i(realPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    while (i.hasNext()) {
        const QString fileName = i.next();
        const QFileInfo fileInfo = i.fileInfo();
        const QString suffix = fileInfo.suffix();
        if (suffix.isEmpty() || suffixes.contains(suffix)) {
            QString text = fileName.mid(realPath.length() + 1);
            if (fileInfo.isDir())
                text += QLatin1Char('/');
            addCompletionItem(text, m_icons.keywordIcon());
        }
    }
}

void ClangCompletionAssistProcessor::completePreprocessor()
{
    foreach (const QString &preprocessorCompletion, m_preprocessorCompletions)
        addCompletionItem(preprocessorCompletion);

    if (m_interface->objcEnabled())
        addCompletionItem(QLatin1String("import"));
}

void ClangCompletionAssistProcessor::addCompletionItem(const QString &text,
                                                       const QIcon &icon,
                                                       int order,
                                                       const QVariant &data)
{
    BasicProposalItem *item = new BasicProposalItem;
    item->setText(text);
    item->setIcon(icon);
    item->setOrder(order);
    item->setData(data);
    m_completions.append(item);
}
