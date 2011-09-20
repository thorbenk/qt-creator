#include "sourcemarker.h"

using namespace Clang;

SourceMarker::SourceMarker()
    : m_length(0), m_kind(Unknown)
{}

SourceMarker::SourceMarker(const SourceLocation &location, unsigned length, Kind kind)
    : m_loc(location), m_length(length), m_kind(kind)
{
}
