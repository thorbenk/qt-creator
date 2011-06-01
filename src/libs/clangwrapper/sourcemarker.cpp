#include "sourcemarker.h"

using namespace Clang;

SourceMarker::SourceMarker(unsigned line, unsigned column, unsigned length, Kind kind)
    : m_line(line), m_column(column), m_length(length), m_kind(kind)
{
}
