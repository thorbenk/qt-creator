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
    static PCHInfoPtr createWithFileName(bool cpp0x, bool objc);

    QString fileName() const
    { return m_file.fileName(); }

    bool cpp0x() const
    { return m_cpp0x; }

    bool objc() const
    { return m_objc; }

private:
    QTemporaryFile m_file;
    bool m_cpp0x;
    bool m_objc;

};

} // namespace CPlusPlus

#endif // CPLUSPLUS_PCHINFO_H
