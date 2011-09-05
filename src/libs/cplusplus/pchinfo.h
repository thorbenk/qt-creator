#ifndef CPLUSPLUS_PCHINFO_H
#define CPLUSPLUS_PCHINFO_H

#include "CPlusPlusForwardDeclarations.h"

#include <QString>
#include <QSharedPointer>
#include <QTemporaryFile>

namespace CPlusPlus {

class PCHInfo;
typedef QSharedPointer<PCHInfo> PCHInfoPtr;

class CPLUSPLUS_EXPORT PCHInfo
{
    PCHInfo();

public:
    ~PCHInfo();

    static PCHInfoPtr createEmpty();
    static PCHInfoPtr createWithFileName();

    QString fileName() const
    { return m_file.fileName(); }

private:
    QTemporaryFile m_file;
};

} // namespace CPlusPlus

#endif // CPLUSPLUS_PCHINFO_H
