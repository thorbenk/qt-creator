#ifndef CLANGSYMBOLSEARCHER_H
#define CLANGSYMBOLSEARCHER_H

#include "clangindexer.h"

#include <cpptools/cppindexingsupport.h>

#include <QLinkedList>

namespace ClangCodeModel {

class Symbol;

namespace Internal {

class ClangSymbolSearcher: public CppTools::SymbolSearcher
{
    Q_OBJECT

    typedef CppTools::SymbolSearcher::Parameters Parameters;
    typedef Find::SearchResultItem SearchResultItem;

public:
    ClangSymbolSearcher(ClangIndexer *indexer, const Parameters &parameters, QSet<QString> fileNames, QObject *parent = 0);
    virtual ~ClangSymbolSearcher();
    virtual void runSearch(QFutureInterface<SearchResultItem> &future);

    void search(const QLinkedList<Symbol> &allSymbols);

private:
    ClangIndexer *m_indexer;
    const Parameters m_parameters;
    const QSet<QString> m_fileNames;
    QFutureInterface<SearchResultItem> *m_future;
};

} // namespace Internal
} // namespace ClangCodeModel

#endif // CLANGSYMBOLSEARCHER_H
