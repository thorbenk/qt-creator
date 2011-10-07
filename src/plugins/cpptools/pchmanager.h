#ifndef CPPTOOLS_INTERNAL_PCHMANAGER_H
#define CPPTOOLS_INTERNAL_PCHMANAGER_H

#include "completionprojectsettings.h"
#include "ModelManagerInterface.h"

#include <QObject>

namespace CppTools {
namespace Internal {

class PCHManager: public QObject
{
    Q_OBJECT

public:
    typedef CPlusPlus::CppModelManagerInterface::ProjectPart ProjectPart;

    PCHManager(CPlusPlus::CppModelManagerInterface *modelManager);

    static void updatePchInfo(CompletionProjectSettings *cps,
                              const QList<ProjectPart::Ptr> &projectParts);

public slots:
    void completionProjectSettingsChanged();

private:
    CPlusPlus::CppModelManagerInterface *m_modelManager;
};

} // namespace Internal
} // namespace CppTools

#endif // CPPTOOLS_INTERNAL_PCHMANAGER_H
