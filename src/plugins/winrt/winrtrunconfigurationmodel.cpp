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

#include "winrtrunconfigurationmodel.h"
#include "winrtutils.h"
#include "packagemanager.h"

#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QScopedArrayPointer>

enum { WinRtPackagePtrRole = Qt::UserRole + 1 };

namespace WinRt {
namespace Internal {

WinRtRunConfigurationModel::WinRtRunConfigurationModel(QObject *parent)
    : QStandardItemModel(0, ColumnCount, parent)
{
    QStringList headers;
    headers << tr("Name") << tr("Version") << tr("Location") << tr("Publisher")
            << tr("Publisher ID");
    setHorizontalHeaderLabels(headers);
}

WinRtPackagePtr WinRtRunConfigurationModel::packageAt(const QModelIndex &index) const
{
    if (index.isValid())
        return packageForItem(itemFromIndex(index));
    return WinRtPackagePtr(new WinRtPackage);
}

WinRtPackagePtr WinRtRunConfigurationModel::packageForItem(const QStandardItem *item)
{
    const QVariant data = item->data(WinRtPackagePtrRole);
    return qvariant_cast<WinRtPackagePtr>(data);
}

QList<QStandardItem *> itemsForPackage(const WinRtPackagePtr &p)
{
    const QVariant data = qVariantFromValue(p);
    const QString installDir = QDir::toNativeSeparators(p->path);
    QList<QStandardItem *> result;
    QString toolTip;

    QTextStream str(&toolTip);
    str << "<html><head/><body><table>"
        << "<tr><td>" << WinRtRunConfigurationModel::tr("Name: ") << "</td><td>"
        << p->name << " (" << p->fullName << ") "
        << WinRtRunConfigurationModel::tr("Version: ") << p->version << "</td></tr>"
        << "<tr><td>" << WinRtRunConfigurationModel::tr("Family: ") << "</td><td>"
        << p->familyName << "</td></tr>"
        << "<tr><td>" << WinRtRunConfigurationModel::tr("Location: ")
        << "</td><td>" << installDir << "</td></tr>"
        << "<tr><td>" << WinRtRunConfigurationModel::tr("Publisher: ")
        << "</td><td>" << p->publisher << " (" << p->publisherId << ") "
        << "</td></tr></table></body></html>";

    for (int c = 0; c < WinRtRunConfigurationModel::ColumnCount; ++c) {
        QStandardItem *item = new QStandardItem;
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setToolTip(toolTip);
        item->setData(data, WinRtPackagePtrRole);
        result.push_back(item);
    }
    result[WinRtRunConfigurationModel::Name]->setText(p->name);
    result[WinRtRunConfigurationModel::InstallDir]->setText(installDir);
    result[WinRtRunConfigurationModel::Publisher]->setText(p->publisher);
    result[WinRtRunConfigurationModel::PublisherId]->setText(p->publisherId);
    result[WinRtRunConfigurationModel::Version]->setText(p->version);
    return result;
}

void WinRtRunConfigurationModel::setPackages(const QList<WinRtPackagePtr> &packages)
{
    if (const int rc = rowCount())
        removeRows(0, rc);
    foreach (const WinRtPackagePtr &p, packages)
        appendRow(itemsForPackage(p));
}

} // namespace Internal
} // namespace WinRt
