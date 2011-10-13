#include "pchmanager.h"

#include <clangwrapper/clangwrapper.h>
#include <coreplugin/messagemanager.h>

using namespace Clang;
using namespace CPlusPlus;
using namespace CppTools;
using namespace CppTools::Internal;

PCHManager::PCHManager(CPlusPlus::CppModelManagerInterface *modelManager)
    : m_modelManager(modelManager)
{
    Q_ASSERT(modelManager);
}

/**
 * Fill any ProjectPart::Ptr::clangPCH which doesn't yet have a PCHInfo (meaning: is null).
 */
void PCHManager::updatePchInfo(CompletionProjectSettings *cps, const QList<ProjectPart::Ptr> &projectParts)
{
    int u = cps->pchUsage();
    qDebug() << "switching to" << u;

    Core::MessageManager *msgMgr = Core::MessageManager::instance();

    if (cps->pchUsage() == CompletionProjectSettings::PchUseNone
            || (cps->pchUsage() == CompletionProjectSettings::PchUseCustom && cps->customPchFile().isEmpty())) {
        msgMgr->printToOutputPane("updatePchInfo: switching to none");
        PCHInfoPtr sharedPch = PCHInfo::createEmpty();
        foreach (const ProjectPart::Ptr &projectPart, projectParts) {
            projectPart->clangPCH = sharedPch;
        }
    } else if (cps->pchUsage() == CompletionProjectSettings::PchUseBuildSystemFast) {
        msgMgr->printToOutputPane("updatePchInfo: switching to build system (fast)");
        QHash<QString, QSet<QString> > includes, frameworks;
        QHash<QString, QSet<QByteArray> > definesPerPCH;
        QHash<QString, bool> cpp0x, objc;
        foreach (const ProjectPart::Ptr &projectPart, projectParts) {
            if (projectPart->precompiledHeaders.isEmpty())
                continue;
            QString pch = projectPart->precompiledHeaders.first(); //### TODO: support more than 1 PCH file.

            includes[pch].unite(QSet<QString>::fromList(projectPart->includePaths));
            frameworks[pch].unite(QSet<QString>::fromList(projectPart->frameworkPaths));
            cpp0x[pch] |= projectPart->cpp0xEnabled();
            objc[pch] |= projectPart->objcEnabled();

            QSet<QByteArray> projectDefines = QSet<QByteArray>::fromList(projectPart->defines.split('\n'));
            QMutableSetIterator<QByteArray> iter(projectDefines);
            while (iter.hasNext()){
                QByteArray v = iter.next();
                if (v.startsWith("#define _") || v.isEmpty()) //### TODO: see ProjectPart::createClangOptions
                    iter.remove();
            }

            if (definesPerPCH.contains(pch)) {
                definesPerPCH[pch].intersect(projectDefines);
            } else {
                definesPerPCH[pch] = projectDefines;
            }
        }

        QMap<QString, PCHInfoPtr> inputToOutput;
        foreach (const QString &pch, definesPerPCH.keys()) {
            PCHInfoPtr ptr = PCHInfo::createWithFileName(cpp0x[pch], objc[pch]);
            QStringList options = ProjectPart::createClangOptions(cpp0x[pch],
                                                                  objc[pch],
                                                                  QStringList(),
                                                                  definesPerPCH[pch].toList(),
                                                                  includes[pch].toList(),
                                                                  frameworks[pch].toList());
            QPair<bool, QStringList> msgs = ClangWrapper::precompile(pch, options, ptr->fileName());
            if (msgs.first)
                msgMgr->printToOutputPane(tr("Successfully generated PCH file \"%1\".").arg(ptr->fileName()));
            else
                msgMgr->printToOutputPane(tr("Failed to generate PCH file \"%1\".").arg(ptr->fileName()));
            if (!msgs.second.isEmpty())
                msgMgr->printToOutputPanePopup(msgs.second.join(QLatin1String("\n")));
            inputToOutput[pch] = ptr;
        }

        foreach (const ProjectPart::Ptr &projectPart, projectParts) {
            if (projectPart->precompiledHeaders.isEmpty())
                continue;
            QString pch = projectPart->precompiledHeaders.first(); //### TODO: support more than 1 PCH file.
            projectPart->clangPCH = inputToOutput[pch];
        }

    } else if (cps->pchUsage() == CompletionProjectSettings::PchUseBuildSystemCorrect) {
        msgMgr->printToOutputPane("updatePchInfo: switching to build system (correct)");
        foreach (const ProjectPart::Ptr &projectPart, projectParts) {
            if (projectPart->precompiledHeaders.isEmpty())
                continue;
            QString pch = projectPart->precompiledHeaders.first(); //### TODO: support more than 1 PCH file.

            PCHInfoPtr ptr = PCHInfo::createWithFileName(projectPart->cpp0xEnabled(), projectPart->objcEnabled());
            QStringList options = ProjectPart::createClangOptions(
                        projectPart->cpp0xEnabled(),
                        projectPart->objcEnabled(),
                        QStringList(),
                        projectPart->defines.split('\n'),
                        projectPart->includePaths,
                        projectPart->frameworkPaths);
            QPair<bool, QStringList> msgs = ClangWrapper::precompile(pch, options, ptr->fileName());
            if (msgs.first)
                msgMgr->printToOutputPane(tr("Successfully generated PCH file \"%1\".").arg(ptr->fileName()));
            else
                msgMgr->printToOutputPane(tr("Failed to generate PCH file \"%1\".").arg(ptr->fileName()));
            if (!msgs.second.isEmpty())
                msgMgr->printToOutputPanePopup(msgs.second.join(QLatin1String("\n")));
            projectPart->clangPCH = ptr;
        }
    } else if (cps->pchUsage() == CompletionProjectSettings::PchUseCustom) {
        msgMgr->printToOutputPane("updatePchInfo: switching to custom" + cps->customPchFile());
        QSet<QString> includes, frameworks;
        bool cpp0x = false, objc = false;
        foreach (const ProjectPart::Ptr &projectPart, projectParts) {
            includes.unite(QSet<QString>::fromList(projectPart->includePaths));
            frameworks.unite(QSet<QString>::fromList(projectPart->frameworkPaths));
            cpp0x |= projectPart->cpp0xEnabled();
            objc |= projectPart->objcEnabled();
        }

        QStringList opts = ProjectPart::createClangOptions(cpp0x, objc, QStringList(), QList<QByteArray>(), includes.toList(), frameworks.toList());
        PCHInfoPtr ptr = PCHInfo::createWithFileName(cpp0x, objc);
        QPair<bool, QStringList> msgs = ClangWrapper::precompile(cps->customPchFile(), opts, ptr->fileName());
        if (msgs.first)
            msgMgr->printToOutputPane(tr("Successfully generated PCH file \"%1\".").arg(ptr->fileName()));
        else
            msgMgr->printToOutputPane(tr("Failed to generate PCH file \"%1\".").arg(ptr->fileName()));
        if (!msgs.second.isEmpty())
            msgMgr->printToOutputPanePopup(msgs.second.join(QLatin1String("\n")));
        foreach (const ProjectPart::Ptr &projectPart, projectParts)
            projectPart->clangPCH = ptr;
    }
}

void PCHManager::completionProjectSettingsChanged()
{
    CompletionProjectSettings *cps = qobject_cast<CompletionProjectSettings *>(sender());
    if (!cps)
        return;

    const QList<ProjectPart::Ptr> projectParts = m_modelManager->projectInfo(cps->project()).projectParts;
    foreach (const ProjectPart::Ptr &partPtr, projectParts)
        partPtr->clangPCH.clear();
    updatePchInfo(cps, projectParts);
    emit pchInfoUpdated();
}
