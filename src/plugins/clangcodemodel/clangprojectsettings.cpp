#include "clangprojectsettings.h"

using namespace ClangCodeModel;

ClangProjectSettings::ClangProjectSettings(ProjectExplorer::Project *project)
    : m_project(project)
    , m_pchUsage(PchUse_None)
{
    Q_ASSERT(project);

    connect(project, SIGNAL(settingsLoaded()),
            this, SLOT(pullSettings()));
    connect(project, SIGNAL(aboutToSaveSettings()),
            this, SLOT(pushSettings()));
}

ClangProjectSettings::~ClangProjectSettings()
{
}

ProjectExplorer::Project *ClangProjectSettings::project() const
{
    return m_project;
}

ClangProjectSettings::PchUsage ClangProjectSettings::pchUsage() const
{
    return m_pchUsage;
}

void ClangProjectSettings::setPchUsage(ClangProjectSettings::PchUsage pchUsage)
{
    if (pchUsage < PchUse_None || pchUsage > PchUse_Custom)
        return;

    if (m_pchUsage != pchUsage) {
        m_pchUsage = pchUsage;
        emit pchSettingsChanged();
    }
}

QString ClangProjectSettings::customPchFile() const
{
    return m_customPchFile;
}

void ClangProjectSettings::setCustomPchFile(const QString &customPchFile)
{
    if (m_customPchFile != customPchFile) {
        m_customPchFile = customPchFile;
        emit pchSettingsChanged();
    }
}

static QLatin1String PchUsageKey("PchUsage");
static QLatin1String CustomPchFileKey("CustomPchFile");
static QLatin1String SettingsNameKey("ClangProjectSettings");

void ClangProjectSettings::pushSettings()
{
    QVariantMap settings;
    settings[PchUsageKey] = m_pchUsage;
    settings[CustomPchFileKey] = m_customPchFile;

    QVariant s(settings);
    m_project->setNamedSettings(SettingsNameKey, s);
}

void ClangProjectSettings::pullSettings()
{
    QVariant s = m_project->namedSettings(SettingsNameKey);
    QVariantMap settings = s.toMap();

    const PchUsage storedPchUsage = static_cast<PchUsage>(
                settings.value(PchUsageKey, PchUse_Unknown).toInt());
    if (storedPchUsage != PchUse_Unknown)
        setPchUsage(storedPchUsage);
    setCustomPchFile(settings.value(CustomPchFileKey).toString());
}
