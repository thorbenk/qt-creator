#include "completionprojectsettings.h"

using namespace CppTools;

CompletionProjectSettings::CompletionProjectSettings(ProjectExplorer::Project *project)
    : m_project(project)
{
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
