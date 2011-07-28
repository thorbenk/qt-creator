#include "completionprojectsettings.h"
#include "completionprojectsettingspage.h"
#include "cppmodelmanager.h"

#include <QButtonGroup>

using namespace CppTools;
using namespace CppTools::Internal;

using namespace ProjectExplorer;

QString CompletionProjectSettingsPanelFactory::id() const
{
    return QLatin1String(COMPLETION_PROJECT_SETTINGS_PANEL_ID);
}

QString CompletionProjectSettingsPanelFactory::displayName() const
{
    return QCoreApplication::translate("CompletionProjectSettingsPanelFactory",
                                       "Code Completion Settings");
}

bool CompletionProjectSettingsPanelFactory::supports(Project *project)
{
    //### TODO
    Q_UNUSED(project);
    return true;
}

PropertiesPanel *CompletionProjectSettingsPanelFactory::createPanel(Project *project)
{
    PropertiesPanel *panel = new PropertiesPanel;
    panel->setDisplayName(QCoreApplication::translate("CompletionProjectSettingsPanelFactory",
                                                      "Code Completion"));
    panel->setWidget(new CompletionProjectSettingsWidget(project)),
    panel->setIcon(QIcon(":/projectexplorer/images/EditorSettings.png")); //### TODO
    return panel;
}

CompletionProjectSettingsWidget::CompletionProjectSettingsWidget(Project *project)
    : QWidget()
    , m_project(project)
{
    Q_ASSERT(project);
    CompletionProjectSettings *cps = CppModelManager::instance()->completionSettingsFromProject(m_project);

    m_ui.setupUi(this);

    //### TODO: fix label

    QButtonGroup *pchGroup = new QButtonGroup(this);
    pchGroup->addButton(m_ui.noPchButton, CompletionProjectSettings::PchUseNone);
    pchGroup->addButton(m_ui.buildSystemPchButton, CompletionProjectSettings::PchUseBuildSystem);
    pchGroup->addButton(m_ui.customPchButton, CompletionProjectSettings::PchUseCustom);
    pchGroup->button(cps->pchUsage())->setChecked(true);
    connect(pchGroup, SIGNAL(buttonClicked(int)),
            this, SLOT(pchUsageChanged(int)));

    connect(m_ui.customHeaderLineEdit, SIGNAL(editingFinished()),
            this, SLOT(customPchFileChanged()));
}

void CompletionProjectSettingsWidget::pchUsageChanged(int id)
{
    CompletionProjectSettings *cps = CppModelManager::instance()->completionSettingsFromProject(m_project);
    cps->setPchUsage(static_cast<CompletionProjectSettings::PchUsage>(id));

    switch (id) {
    case CompletionProjectSettings::PchUseNone:
    case CompletionProjectSettings::PchUseBuildSystem:
        m_ui.customHeaderLabel->setEnabled(false);
        m_ui.customHeaderLineEdit->setEnabled(false);
        m_ui.customHeaderChooseButton->setEnabled(false);
        break;

    case CompletionProjectSettings::PchUseCustom:
        m_ui.customHeaderLabel->setEnabled(true);
        m_ui.customHeaderLineEdit->setEnabled(true);
        m_ui.customHeaderChooseButton->setEnabled(true);
        break;

    default:
        break;
    }
}

void CompletionProjectSettingsWidget::customPchFileChanged()
{
    CompletionProjectSettings *cps = CppModelManager::instance()->completionSettingsFromProject(m_project);
    if (cps->pchUsage() != CompletionProjectSettings::PchUseCustom)
        return;
    QString fileName = m_ui.customHeaderLineEdit->text();
    if (!QFile(fileName).exists())
        return;

    cps->setCustomPchFile(fileName);
}
