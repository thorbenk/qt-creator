/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "qbsnodes.h"

#include "qbsproject.h"

#include <utils/qtcassert.h>

#include <api/project.h> // qbs

#include <QFileInfo>

// --------------------------------------------------------------------
// Helpers:
// --------------------------------------------------------------------

namespace {

ProjectExplorer::FolderNode *findFolder(ProjectExplorer::FolderNode *root, const QString &name)
{
    foreach (ProjectExplorer::FolderNode *n, root->subFolderNodes()) {
        if (n->displayName() == name)
            return n;
    }
    return 0;
}

} // namespace

namespace QbsProjectManager {
namespace Internal {

// ----------------------------------------------------------------------
// QbsFileNode:
// ----------------------------------------------------------------------

QbsFileNode::QbsFileNode(const QString &filePath, const ProjectExplorer::FileType fileType,
                         bool generated, int line) :
    ProjectExplorer::FileNode(filePath, fileType, generated),
    m_line(line)
{ }

int QbsFileNode::line() const
{
    return m_line;
}

// ---------------------------------------------------------------------------
// QbsBaseProjectNode:
// ---------------------------------------------------------------------------

QbsBaseProjectNode::QbsBaseProjectNode(const QString &path) :
    ProjectExplorer::ProjectNode(path)
{ }

bool QbsBaseProjectNode::hasBuildTargets() const
{
    foreach (ProjectNode *n, subProjectNodes())
        if (n->hasBuildTargets())
            return true;
    return false;
}

QList<ProjectExplorer::ProjectNode::ProjectAction> QbsBaseProjectNode::supportedActions(ProjectExplorer::Node *node) const
{
    Q_UNUSED(node);
    return QList<ProjectExplorer::ProjectNode::ProjectAction>();
}

bool QbsBaseProjectNode::canAddSubProject(const QString &proFilePath) const
{
    Q_UNUSED(proFilePath);
    return false;
}

bool QbsBaseProjectNode::addSubProjects(const QStringList &proFilePaths)
{
    Q_UNUSED(proFilePaths);
    return false;
}

bool QbsBaseProjectNode::removeSubProjects(const QStringList &proFilePaths)
{
    Q_UNUSED(proFilePaths);
    return false;
}

bool QbsBaseProjectNode::addFiles(const ProjectExplorer::FileType fileType, const QStringList &filePaths, QStringList *notAdded)
{
    Q_UNUSED(fileType);
    Q_UNUSED(filePaths);
    Q_UNUSED(notAdded);
    return false;
}

bool QbsBaseProjectNode::removeFiles(const ProjectExplorer::FileType fileType, const QStringList &filePaths, QStringList *notRemoved)
{
    Q_UNUSED(fileType);
    Q_UNUSED(filePaths);
    Q_UNUSED(notRemoved);
    return false;
}

bool QbsBaseProjectNode::deleteFiles(const ProjectExplorer::FileType fileType, const QStringList &filePaths)
{
    Q_UNUSED(fileType);
    Q_UNUSED(filePaths);
    return false;
}

bool QbsBaseProjectNode::renameFile(const ProjectExplorer::FileType fileType, const QString &filePath, const QString &newFilePath)
{
    Q_UNUSED(fileType);
    Q_UNUSED(filePath);
    Q_UNUSED(newFilePath);
    return false;
}

QList<ProjectExplorer::RunConfiguration *> QbsBaseProjectNode::runConfigurationsFor(ProjectExplorer::Node *node)
{
    Q_UNUSED(node);
    return QList<ProjectExplorer::RunConfiguration *>();
}

// --------------------------------------------------------------------
// QbsGroupNode:
// --------------------------------------------------------------------

QbsGroupNode::QbsGroupNode(const qbs::GroupData *grp, const qbs::ProductData *prd) :
    QbsBaseProjectNode(prd->location().fileName)
{
    setDisplayName(grp->name());
    group = grp;

    addFileNodes(QList<ProjectExplorer::FileNode *>()
                 << new QbsFileNode(grp->location().fileName,
                                    ProjectExplorer::ProjectFileType, false, grp->location().line), this);

    QString basePath = QFileInfo(prd->location().fileName).absolutePath();
    if (!basePath.endsWith(QLatin1Char('/')))
        basePath.append(QLatin1Char('/'));
    foreach (const QString &p, grp->allFilePaths()) {
        QString path = p;
        if (path.startsWith(basePath))
            path = path.mid(basePath.count());

        QStringList pathSegments = path.split(QLatin1Char('/'), QString::SkipEmptyParts);
        FolderNode *root = this;
        while (pathSegments.count()) {
            if (pathSegments.count() == 1) {
                addFileNodes(QList<ProjectExplorer::FileNode *>()
                             << new ProjectExplorer::FileNode(p, ProjectExplorer::UnknownFileType, false),
                             root);
            } else {
                const QString folder = pathSegments.at(0);
                FolderNode *n = findFolder(root, folder);
                if (!n) {
                    n = new ProjectExplorer::FolderNode(folder);
                    addFolderNodes(QList<ProjectExplorer::FolderNode *>() << n, root);
                }
                root = n;
            }
            pathSegments.removeFirst();
        };
    }
}

bool QbsGroupNode::isEnabled() const
{
    return group->isEnabled();
}

// --------------------------------------------------------------------
// QbsProductNode:
// --------------------------------------------------------------------

QbsProductNode::QbsProductNode(const qbs::ProductData *prd) :
    QbsBaseProjectNode(prd->location().fileName)
{
    setDisplayName(prd->name());
    product = prd;

    addFileNodes(QList<ProjectExplorer::FileNode *>()
                 << new QbsFileNode(prd->location().fileName,
                                    ProjectExplorer::ProjectFileType, false, prd->location().line), this);

    QList<ProjectExplorer::ProjectNode *> toAdd;
    foreach (const qbs::GroupData &grp, prd->groups())
        toAdd << new QbsGroupNode(&grp, prd);

    addProjectNodes(toAdd);
}

bool QbsProductNode::isEnabled() const
{
    return product->isEnabled();
}

// --------------------------------------------------------------------
// QbsProjectNode:
// --------------------------------------------------------------------

QbsProjectNode::QbsProjectNode(const QString &projectFile) :
    QbsBaseProjectNode(projectFile),
    m_project(0), m_projectData(0)
{
    addFileNodes(QList<ProjectExplorer::FileNode *>()
                 << new ProjectExplorer::FileNode(projectFile, ProjectExplorer::ProjectFileType, false), this);
}

QbsProjectNode::~QbsProjectNode()
{
    delete m_projectData;
    delete m_project;
}

void QbsProjectNode::update(const qbs::Project *prj)
{
    QList<ProjectExplorer::ProjectNode *> toAdd;
    QList<ProjectExplorer::ProjectNode *> toRemove;

    QList<ProjectExplorer::ProjectNode *> oldNodeList = subProjectNodes();

    delete m_projectData;
    m_projectData = 0;

    if (prj) {
        m_projectData = new qbs::ProjectData(prj->projectData());
        foreach (const qbs::ProductData &prd, m_projectData->products()) {
            QbsProductNode *qn = findProductNode(prd.name());
            if (!qn) {
                toAdd << new QbsProductNode(&prd);
            } else {
                oldNodeList.removeOne(qn);

                if (*qn->product != prd) {
                    toRemove << qn;
                    toAdd << new QbsProductNode(&prd);
                }
            }
        }
    }
    toRemove.append(oldNodeList);

    if (m_project) {
        delete m_project;
        m_project = 0;
    }

    m_project = prj;

    removeProjectNodes(toRemove);
    addProjectNodes(toAdd);
}

const qbs::Project *QbsProjectNode::project() const
{
    return m_project;
}

const qbs::ProjectData *QbsProjectNode::projectData() const
{
    return m_projectData;
}

QbsProductNode *QbsProjectNode::findProductNode(const QString &name)
{
    foreach(ProjectExplorer::ProjectNode *n, subProjectNodes()) {
        QbsProductNode *qn = qobject_cast<QbsProductNode *>(n);
        if (qn && (qn->product->name() == name))
            return qn;
    }
    return 0;
}

} // namespace Internal
} // namespace QbsProjectManager
