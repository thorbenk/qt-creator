#include "pchinfo.h"

#include <QDir>

using namespace Clang;

PCHInfo::PCHInfo()
    : m_cpp0x(false)
    , m_objc(false)
{
}

PCHInfo::~PCHInfo()
{
}

PCHInfoPtr PCHInfo::createEmpty()
{
    return PCHInfoPtr(new PCHInfo);
}

PCHInfoPtr PCHInfo::createWithFileName(bool cpp0x, bool objc)
{
    PCHInfoPtr result(new PCHInfo);
    result->m_cpp0x = cpp0x;
    result->m_objc = objc;

    // The next 2 lines are just here to generate the file name....
    result->m_file.open();
    result->m_file.close();
    return result;
}
