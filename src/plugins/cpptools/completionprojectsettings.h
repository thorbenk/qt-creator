#ifndef CPPTOOLS_COMPLETIONPROJECTSETTINGS_H
#define CPPTOOLS_COMPLETIONPROJECTSETTINGS_H

#include "cpptools_global.h"

#include <QObject>

namespace ProjectExplorer {
class Project;
} // namespace ProjectExplorer

namespace CppTools {

class CPPTOOLS_EXPORT CompletionProjectSettings : public QObject
{
    Q_OBJECT

    Q_PROPERTY(PchUsage pchUsage READ pchUsage WRITE setPchUsage NOTIFY pchUsageChanged)
    Q_PROPERTY(QString customPchFile READ customPchFile WRITE setCustomPchFile NOTIFY customPchFileChanged)

public:
    enum PchUsage {
        PchUseUnknown = 0,
        PchUseNone = 1,
        PchUseBuildSystemFast = 2,
        PchUseBuildSystemCorrect = 3,
        PchUseCustom = 4
    };

public:
    explicit CompletionProjectSettings(ProjectExplorer::Project *project);

    ProjectExplorer::Project *project() const
    { return m_project; }

    PchUsage pchUsage() const
    { return m_pchUsage; }

    QString customPchFile() const
    { return m_customPchFile; }

signals:
    void pchUsageChanged(int newUsage);
    void customPchFileChanged(QString arg);

public slots:
    void setPchUsage(PchUsage pchUsage);
    void setCustomPchFile(const QString &fileName);

private slots:
    void pushSettings();
    void pullSettings();

private:
    ProjectExplorer::Project *m_project;
    PchUsage m_pchUsage;
    QString m_customPchFile;
};

} // namespace CppTools

#endif // CPPTOOLS_COMPLETIONPROJECTSETTINGS_H
