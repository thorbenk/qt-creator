/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#include "winrtselectappdialog.h"
#include "winrtrunconfigurationmodel.h"
#include "packagemanager.h"
#include "ui_winrtselectappdialog.h"

#include <coreplugin/editormanager/editormanager.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/project.h>

#include <QPushButton>
#include <QMenu>
#include <QAction>
#include <QSortFilterProxyModel>
#include <QContextMenuEvent>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QApplication>
#include <QCursor>

namespace WinRt {
namespace Internal {

WinRtSelectAppDialog::WinRtSelectAppDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::WinRtSelectAppDialog)
    , m_model(new WinRtRunConfigurationModel(this))
    , m_filterModel(new QSortFilterProxyModel(this))
    , m_packageManager(new PackageManager)
{
    connect(m_packageManager, SIGNAL(packageRemoved(QString)),
            this, SLOT(packageRemoved(QString)));
    connect(m_packageManager, SIGNAL(packageRemovalFailed(QString,QString)),
            this, SLOT(packageRemovalFailed(QString,QString)));
    connect(m_packageManager, SIGNAL(packageAdded(QString)),
            this, SLOT(packageAdded(QString)));
    connect(m_packageManager, SIGNAL(packageAddFailed(QString,QString)),
            this, SLOT(packageAddFailed(QString,QString)));

    m_ui->setupUi(this);
    m_ui->table->setUniformRowHeights(true);
    m_ui->table->setSelectionBehavior(QAbstractItemView::SelectRows);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setMinimumWidth(800);
    m_selectButton = m_ui->buttonBox->addButton(tr("Select"), QDialogButtonBox::AcceptRole);
    connect(m_ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(m_ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    m_refreshButton = m_ui->buttonBox->addButton(tr("Refresh"), QDialogButtonBox::ActionRole);
    connect(m_refreshButton, SIGNAL(clicked()), this, SLOT(refresh()));
    connect(m_ui->filterClearButton, SIGNAL(clicked()),
            m_ui->filterLineEdit, SLOT(clear()));
    connect(m_ui->filterLineEdit, SIGNAL(textChanged(QString)),
            m_filterModel, SLOT(setFilterFixedString(QString)));

    m_filterModel->setSourceModel(m_model);
    m_filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_filterModel->setSortLocaleAware(true);

    m_ui->table->setModel(m_filterModel);
    m_ui->table->header()->setSortIndicator(0, Qt::AscendingOrder);
    connect(m_ui->table->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            SLOT(currentIndexChanged()));
    connect(m_ui->table, SIGNAL(activated(QModelIndex)),
            m_selectButton, SLOT(animateClick()));

    m_addButton = m_ui->buttonBox->addButton(tr("Add..."), QDialogButtonBox::ActionRole);
    connect(m_addButton, SIGNAL(clicked()), this, SLOT(addPackage()));

    refresh();
}

void WinRtSelectAppDialog::adjustColumns()
{
    for (int c = 0; c < WinRtRunConfigurationModel::ColumnCount; ++c)
        if (c != WinRtRunConfigurationModel::InstallDir && c != WinRtRunConfigurationModel::Publisher)
        m_ui->table->resizeColumnToContents(c);
}

WinRtSelectAppDialog::~WinRtSelectAppDialog()
{
    delete m_ui;
    delete m_packageManager;
}

void WinRtSelectAppDialog::currentIndexChanged()
{
    m_selectButton->setEnabled(m_ui->table->currentIndex().isValid());
}

void WinRtSelectAppDialog::refresh()
{
    m_model->setPackages(m_packageManager->listPackages());
    m_filterModel->sort(WinRtRunConfigurationModel::Name);
    adjustColumns();
}

QString WinRtSelectAppDialog::appIdForIndex(const QModelIndex &filterIndex) const
{
    if (filterIndex.isValid()) {
        const QModelIndex index = m_filterModel->mapToSource(filterIndex);
        return m_model->packageAt(index)->familyName;
    }
    return QString();
}

QString WinRtSelectAppDialog::appId() const
{
    return appIdForIndex(m_ui->table->currentIndex());
}

void WinRtSelectAppDialog::done(int r)
{
    if (!m_packageManager->operationInProgress())
        QDialog::done(r);
}

void WinRtSelectAppDialog::addPackage()
{
    ProjectExplorer::Project *project = ProjectExplorer::ProjectExplorerPlugin::currentProject();
    QString directory = project ? project->projectDirectory() : QString();
    const QString manifestFile =
        QFileDialog::getOpenFileName(this, tr("Select Manifest"),
                                     directory, tr("Manifest files (*.xml)"));
    if (manifestFile.isEmpty())
        return;
    QString errorMessage;
    if (m_packageManager->startAddPackage(manifestFile,
                                          PackageManager::DevelopmentMode,
                                          &errorMessage)) {
        QApplication::setOverrideCursor(Qt::BusyCursor);
        setEnabled(false);
    } else {
        QMessageBox::warning(this, tr("Failed to Start Adding Package"),
                             errorMessage);
    }
}

void WinRtSelectAppDialog::packageAdded(const QString &)
{
    setEnabled(true);
    QApplication::restoreOverrideCursor();
    refresh();
}

void WinRtSelectAppDialog::packageAddFailed(const QString &manifestFile, const QString &message)
{
    setEnabled(true);
    QApplication::restoreOverrideCursor();
    QMessageBox::warning(this, tr("Error Adding \"%1\"").arg(manifestFile), message);
}

void WinRtSelectAppDialog::contextMenuEvent(QContextMenuEvent *e)
{
    const QModelIndex filterIndex = m_ui->table->currentIndex();
    if (!filterIndex.isValid()) {
        e->setAccepted(false);
        return;
    }
    const WinRtPackagePtr package = m_model->packageAt(m_filterModel->mapToSource(filterIndex));

    QMenu menu;
    QAction *copyPathAction = menu.addAction(tr("Copy Path"));
    QAction *copyNameAction = menu.addAction(tr("Copy Name"));
    QAction *editManifestAction = menu.addAction(tr("Open Manifest"));
    QAction *unregisterAction = menu.addAction(tr("Remove..."));
    QAction *a = menu.exec(mapToGlobal(e->pos()));
    if (a == copyPathAction) {
        QApplication::clipboard()->setText(QDir::toNativeSeparators(package->path));
    } else if (a == copyNameAction) {
        QApplication::clipboard()->setText(package->fullName);
    } else if (a == editManifestAction) {
        Core::EditorManager::instance()->openEditor(package->path);
    } else if (a == unregisterAction) {
        const QString question = tr("Would you like to remove the package \"%1\"?").arg(package->fullName);
        if (QMessageBox::question(this, tr("Remove Package"), question, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
            return;
        QString errorMessage;
        if (m_packageManager->startRemovePackage(package->fullName, &errorMessage)) {
            QApplication::setOverrideCursor(Qt::BusyCursor);
            setEnabled(false);
        } else {
            QMessageBox::warning(this, tr("Failed to Start Package Removal"),
                                 errorMessage);
        }
    }
}

void WinRtSelectAppDialog::packageRemovalFailed(const QString &fullName, const QString &message)
{
    setEnabled(true);
    QApplication::restoreOverrideCursor();
    QMessageBox::warning(this, tr("Error Removing \"%1\"").arg(fullName), message);
}

void WinRtSelectAppDialog::packageRemoved(const QString & /* fullName */)
{
    setEnabled(true);
    QApplication::restoreOverrideCursor();
    refresh();
}

} // namespace Internal
} // namespace WinRt
