#include "completionprojectsettings.h"

#include <projectexplorer/project.h>

using namespace CppTools;

CompletionProjectSettings::CompletionProjectSettings(ProjectExplorer::Project *project)
    : m_project(project)
    , m_pchUsage(PchUseUnknown)
{
    connect(m_project, SIGNAL(settingsLoaded()),
            this, SLOT(pullSettings()));
    connect(m_project, SIGNAL(aboutToSaveSettings()),
            this, SLOT(pushSettings()));

    pullSettings();
}

void CompletionProjectSettings::setPchUsage(PchUsage pchUsage)
{
    if (pchUsage < PchUseUnknown || pchUsage > PchUseCustom)
        return;

    if (m_pchUsage != pchUsage) {
        m_pchUsage = pchUsage;
        emit pchUsageChanged(pchUsage);
    }
}

void CompletionProjectSettings::setCustomPchFile(const QString &fileName)
{
    if (m_customPchFile != fileName) {
        m_customPchFile = fileName;
        emit customPchFileChanged(fileName);
    }
}

void CompletionProjectSettings::pushSettings()
{
    QVariantMap settings;
    settings["PchUse"] = m_pchUsage;
    settings["CustomPchFile"] = m_customPchFile;

    QVariant s(settings);
    m_project->setNamedSettings("CompletionProjectSettings", s);
}

void CompletionProjectSettings::pullSettings()
{
    QVariant s = m_project->namedSettings("CompletionProjectSettings");
    QVariantMap settings = s.toMap();

    const PchUsage storedPchUsage = static_cast<PchUsage>(settings.value("PchUse", PchUseUnknown).toInt());
    if (storedPchUsage != PchUseUnknown)
        setPchUsage(storedPchUsage);
    setCustomPchFile(settings.value("CustomPchFile").toString());
}
