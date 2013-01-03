#ifndef CLANGPROJECTSETTINGS_H
#define CLANGPROJECTSETTINGS_H

#include "clang_global.h"

#include <projectexplorer/project.h>

#include <QObject>
#include <QString>

namespace ClangCodeModel {

class CLANG_EXPORT ClangProjectSettings: public QObject
{
    Q_OBJECT

public:
    enum PchUsage {
        PchUse_Unknown = 0,
        PchUse_None = 1,
        PchUse_BuildSystem_Exact = 2,
        PchUse_BuildSystem_Fuzzy = 3,
        PchUse_Custom = 4
    };

public:
    ClangProjectSettings(ProjectExplorer::Project *project);
    virtual ~ClangProjectSettings();

    ProjectExplorer::Project *project() const;

    PchUsage pchUsage() const;
    void setPchUsage(PchUsage pchUsage);

    QString customPchFile() const;
    void setCustomPchFile(const QString &customPchFile);

signals:
    void pchSettingsChanged();

public slots:
    void pullSettings();
    void pushSettings();

private:
    ProjectExplorer::Project *m_project;
    PchUsage m_pchUsage;
    QString m_customPchFile;
};

} // ClangCodeModel namespace

#endif // CLANGPROJECTSETTINGS_H
