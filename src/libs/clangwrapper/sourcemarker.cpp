#include "sourcemarker.h"

using namespace Clang;

SourceMarker::SourceMarker(unsigned line, unsigned column, unsigned length, Kind kind)
    : m_loc(line, column, 0), m_length(length), m_kind(kind)
{
}
