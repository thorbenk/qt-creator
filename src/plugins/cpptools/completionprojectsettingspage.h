#ifndef CPPTOOLS_INTERNAL_COMPLETIONPROJECTSETTINGSPAGE_H
#define CPPTOOLS_INTERNAL_COMPLETIONPROJECTSETTINGSPAGE_H

#include "ui_completionprojectsettingspage.h"

#include <projectexplorer/iprojectproperties.h>

namespace CppTools {

class CompletionProjectSettings;

namespace Internal {

const char * const COMPLETION_PROJECT_SETTINGS_PANEL_ID("CppTools.CompletionProjectSettingsPanel");

class CompletionProjectSettingsPanelFactory: public ProjectExplorer::IProjectPanelFactory
{
public:
    QString id() const;
    QString displayName() const;
    virtual int priority() const;
    ProjectExplorer::PropertiesPanel *createPanel(ProjectExplorer::Project *project);
    bool supports(ProjectExplorer::Project *project);
};

class CompletionProjectSettingsWidget: public QWidget
{
    Q_OBJECT

public:
    CompletionProjectSettingsWidget(ProjectExplorer::Project *project);

private slots:
    void pchUsageChanged(int id);
    void customPchFileChanged();
    void customPchButtonClicked();

private:
    Ui::CompletionProjectSettingsPage m_ui;
    ProjectExplorer::Project *m_project;
};

} // namespace Internal
} // namespace CppTools

#endif // CPPTOOLS_INTERNAL_COMPLETIONPROJECTSETTINGSPAGE_H
