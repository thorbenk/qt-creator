#include "pchinfo.h"

#include <QDir>

using namespace CPlusPlus;

PCHInfo::PCHInfo()
{
}

PCHInfo::~PCHInfo()
{
}

PCHInfoPtr PCHInfo::createEmpty()
{
    return PCHInfoPtr(new PCHInfo);
}

PCHInfoPtr PCHInfo::createWithFileName()
{
    PCHInfoPtr result(new PCHInfo);

    // The next 2 lines are just here to generate the file name....
    result->m_file.open();
    result->m_file.close();
    return result;
}
