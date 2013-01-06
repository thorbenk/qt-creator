#ifndef CXPRETTYPRINTER_H
#define CXPRETTYPRINTER_H

#include <clang-c/Index.h>
#include <QString>

namespace ClangCodeModel {
namespace Internal {

class CXPrettyPrinter
{
public:
    CXPrettyPrinter();

    QString toString(CXCompletionChunkKind kind) const;
    QString toString(CXAvailabilityKind kind) const;
    QString toString(CXCursorKind kind) const;
    QString toString(CXDiagnosticSeverity severity) const;

    QString jsonForCompletionMeta(CXCodeCompleteResults *results);
    QString jsonForCompletionString(const CXCompletionString &string);
    QString jsonForCompletion(const CXCompletionResult &result);
    QString jsonForDiagnsotic(const CXDiagnostic &diagnostic);

private:
    int m_indent;
    QString m_printed;

    void writeCompletionContexts(CXCodeCompleteResults *results);
    void writeCompletionStringJson(const CXCompletionString &string);
    void writeCompletionChunkJson(const CXCompletionString &string, unsigned i);
    void writeCompletionAnnotationJson(const CXCompletionString &string, unsigned i);

    void writeDiagnosticJson(const CXDiagnostic &diag);
    void writeFixItJson(const CXDiagnostic &diag, unsigned i);

    void writeRangeJson(const CXSourceRange &range);
    void writeLocationJson(const CXSourceLocation &location);

    void writeLineEnd();
};

} // namespace Internal
} // namespace ClangCodeModel

#endif // CXPRETTYPRINTER_H
