#ifndef CLANG_PCHINFO_H
#define CLANG_PCHINFO_H

#include "clangwrapper_global.h"

#include <QString>
#include <QSharedPointer>
#include <QTemporaryFile>

namespace Clang {

class PCHInfo;
typedef QSharedPointer<PCHInfo> PCHInfoPtr;

class QTCREATOR_CLANGWRAPPER_EXPORT PCHInfo
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

} // namespace Clang

#endif // CLANG_PCHINFO_H
