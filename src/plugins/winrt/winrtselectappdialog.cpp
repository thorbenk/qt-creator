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
#include "ui_winrtselectappdialog.h"

#include <coreplugin/editormanager/editormanager.h>

#include <QPushButton>
#include <QMenu>
#include <QAction>
#include <QSortFilterProxyModel>
#include <QContextMenuEvent>
#include <QApplication>
#include <QClipboard>
#include <QDir>

namespace WinRt {
namespace Internal {

WinRtSelectAppDialog::WinRtSelectAppDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::WinRtSelectAppDialog)
    , m_model(new WinRtRunConfigurationModel(this))
    , m_filterModel(new QSortFilterProxyModel(this))
{
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
}

void WinRtSelectAppDialog::currentIndexChanged()
{
    m_selectButton->setEnabled(m_ui->table->currentIndex().isValid());
}

void WinRtSelectAppDialog::refresh()
{
    m_model->setPackages(listPackages());
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
    QAction *editManifestAction = menu.addAction(tr("Open Manifest"));
    QAction *a = menu.exec(mapToGlobal(e->pos()));
    if (a == copyPathAction) {
        qApp->clipboard()->setText(QDir::toNativeSeparators(package->path));
        return;
    } else if (a == editManifestAction) {
        Core::EditorManager::instance()->openEditor(package->path);
    }
}

} // namespace Internal
} // namespace WinRt
