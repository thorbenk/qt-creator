#ifndef CLANG_SOURCEMARKER_H
#define CLANG_SOURCEMARKER_H

#include "clangwrapper_global.h"

namespace Clang {

class QTCREATOR_CLANGWRAPPER_EXPORT SourceMarker
{
public:
    enum Kind {
        Unknown = 0,
        Type = 1,
        Local,
        Field,
        Static,
        VirtualMethod,
        Label
    };

    SourceMarker(unsigned line = 0,
                 unsigned column = 0,
                 unsigned length = 0,
                 Kind kind = Unknown);

    bool isValid() const
    { return m_line != 0; }

    bool isInvalid() const
    { return m_line == 0; }

    unsigned line() const
    { return m_line; }

    unsigned column() const
    { return m_column; }

    unsigned length() const
    { return m_length; }

    Kind kind() const
    { return m_kind; }

    bool lessThan(const SourceMarker &other) const
    {
        if (m_line != other.m_line)
            return m_line < other.m_line;
        if (m_column != other.m_column)
            return m_column < other.m_column;
        return m_length < other.m_length;
    }

private:
    unsigned m_line;
    unsigned m_column;
    unsigned m_length;
    Kind m_kind;
};

QTCREATOR_CLANGWRAPPER_EXPORT inline bool operator<(const SourceMarker &one, const SourceMarker &two)
{ return one.lessThan(two); }

} // namespace Clang

#endif // CLANG_SOURCEMARKER_H
