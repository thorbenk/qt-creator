#include "clangsymbolsearcher.h"
#include "symbol.h"

#include <cpptools/searchsymbols.h>

#include <cassert>

using namespace ClangCodeModel;
using namespace ClangCodeModel::Internal;

ClangSymbolSearcher::ClangSymbolSearcher(ClangIndexer *indexer, const Parameters &parameters, QSet<QString> fileNames, QObject *parent)
    : CppTools::SymbolSearcher(parent)
    , m_indexer(indexer)
    , m_parameters(parameters)
    , m_fileNames(fileNames)
    , m_future(0)
{
    assert(indexer);
}

ClangSymbolSearcher::~ClangSymbolSearcher()
{
}

void ClangSymbolSearcher::runSearch(QFutureInterface<SearchResultItem> &future)
{
    m_future = &future;
    m_indexer->match(this);
    m_future = 0;
}

void ClangSymbolSearcher::search(const QLinkedList<Symbol> &allSymbols)
{
    QString findString = (m_parameters.flags & Find::FindRegularExpression
                          ? m_parameters.text : QRegExp::escape(m_parameters.text));
    if (m_parameters.flags & Find::FindWholeWords)
        findString = QString::fromLatin1("\\b%1\\b").arg(findString);
    QRegExp matcher(findString, (m_parameters.flags & Find::FindCaseSensitively
                                 ? Qt::CaseSensitive : Qt::CaseInsensitive));

    const int chunkSize = 10;
    QVector<Find::SearchResultItem> resultItems;
    resultItems.reserve(100);

    m_future->setProgressRange(0, allSymbols.size() % chunkSize);
    m_future->setProgressValue(0);

    int symbolNr = 0;
    foreach (const Symbol &s, allSymbols) {
        if (symbolNr % chunkSize == 0) {
            m_future->setProgressValue(symbolNr / chunkSize);
            m_future->reportResults(resultItems);
            resultItems.clear();

            if (m_future->isPaused())
                m_future->waitForResume();
            if (m_future->isCanceled())
                return;
        }
        ++symbolNr;

        CppTools::ModelItemInfo info;

        switch (s.m_kind) {
        case Symbol::Enum:
            if (m_parameters.types & SymbolSearcher::Enums) {
                info.type = CppTools::ModelItemInfo::Enum;
                info.symbolType = QLatin1String("enum");
                break;
            } else {
                continue;
            }
        case Symbol::Class:
            if (m_parameters.types & SymbolSearcher::Classes) {
                info.type = CppTools::ModelItemInfo::Class;
                info.symbolType = QLatin1String("class");
                break;
            } else {
                continue;
            }
        case Symbol::Method:
        case Symbol::Function:
        case Symbol::Constructor:
        case Symbol::Destructor:
            if (m_parameters.types & SymbolSearcher::Functions) {
                info.type = CppTools::ModelItemInfo::Method;
                break;
            } else {
                continue;
            }
        case Symbol::Declaration:
            if (m_parameters.types & SymbolSearcher::Declarations) {
                info.type = CppTools::ModelItemInfo::Declaration;
                break;
            } else {
                continue;
            }

        default: continue;
        }

        if (matcher.indexIn(s.m_name) == -1)
            continue;

        info.symbolName = s.m_name;
        info.fullyQualifiedName = s.m_qualification.split(QLatin1String("::")) << s.m_name;
        info.fileName = s.m_location.fileName();
        info.icon = s.iconForSymbol();
        info.line = s.m_location.line();
        info.column = s.m_location.column() - 1;

        Find::SearchResultItem item;
        item.path << s.m_qualification;
        item.text = s.m_name;
        item.icon = info.icon;
        item.textMarkPos = -1;
        item.textMarkLength = 0;
        item.lineNumber = -1;
        item.userData = qVariantFromValue(info);

        resultItems << item;
    }

    if (!resultItems.isEmpty())
        m_future->reportResults(resultItems);
}
