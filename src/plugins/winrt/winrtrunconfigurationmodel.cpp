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

#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QScopedArrayPointer>

#if defined(_MSC_VER) && _MSC_VER >= 1700

#  include <wrl.h>
#  include <windows.management.deployment.h>
#  include <Windows.ApplicationModel.h>
#  include <windows.foundation.collections.h>
#  include <windows.system.userprofile.h>
#  include <sddl.h>

using namespace ABI::Windows;
using namespace ABI::Windows::ApplicationModel;
using namespace ABI::Windows::Storage;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Management::Deployment;

#endif // defined(_MSC_VER) && _MSC_VER >= 1700

enum { WinRtPackagePtrRole = Qt::UserRole + 1 };

namespace WinRt {
namespace Internal {

#if defined(_MSC_VER) && _MSC_VER >= 1700

// Get security ID string.

static WCHAR *sidString()
{
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        qErrnoWarning("OpenProcessToken() failed.");
        return 0;
    }
    DWORD tokenSize = 0; // Determine size.
    if (!GetTokenInformation(token, TokenUser, NULL, 0, &tokenSize)
        && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        CloseHandle(token);
        qErrnoWarning("GetTokenInformation() failed.");
        return 0;
    }
    QScopedArrayPointer<char> tokenUserData(new char[tokenSize]);
    PTOKEN_USER tokenUser = reinterpret_cast<PTOKEN_USER>(tokenUserData.data());
    if (!GetTokenInformation(token, TokenUser, tokenUser, tokenSize, &tokenSize)) {
        CloseHandle(token);
        qErrnoWarning("GetTokenInformation() failed.");
        return 0;
    }
    CloseHandle(token);

    WCHAR *sidStr;
    if (!ConvertSidToStringSid(tokenUser->User.Sid, &sidStr)) {
        qErrnoWarning("ConvertSidToStringSid() failed.");
        return 0;
    }
    return sidStr;
}

static inline QList<WinRtPackagePtr> readPackages(IPackageManager *manager, const HSTRING &sid)
{
    QList<WinRtPackagePtr> packages;
    IIterable<Package*> *appxPackages;
    if (FAILED(manager->FindPackagesByUserSecurityId(sid, &appxPackages))) {
        qWarning("IPackageManager::FindPackagesByUserSecurityId() failed.");
        return packages;
    }
    IIterator<Package*> *it;
    appxPackages->First(&it);
    boolean hasCurrent;
    it->get_HasCurrent(&hasCurrent);
    while (hasCurrent) {
        WinRtPackagePtr package(new WinRtPackage);
        IPackage *appxPackage;
        it->get_Current(&appxPackage);

        IPackageId *packageId;
        appxPackage->get_Id(&packageId);
        HSTRING publisher;
        packageId->get_Publisher(&publisher);
        package->publisher = QString::fromWCharArray(WindowsGetStringRawBuffer(publisher, nullptr));
        HSTRING publisherId;
        packageId->get_PublisherId(&publisherId);
        package->publisherId = QString::fromWCharArray(WindowsGetStringRawBuffer(publisherId, nullptr));
        HSTRING name;
        packageId->get_Name(&name);
        package->name = QString::fromWCharArray(WindowsGetStringRawBuffer(name, nullptr));
        HSTRING fullName;
        packageId->get_FullName(&fullName);
        package->fullName = QString::fromWCharArray(WindowsGetStringRawBuffer(fullName, nullptr));

        ABI::Windows::ApplicationModel::PackageVersion version;
        packageId->get_Version(&version);
        package->version = QString::number(version.Major)
            + QLatin1Char('.') + QString::number(version.Minor);

        HSTRING familyName;
        packageId->get_FamilyName(&familyName);
        package->familyName = QString::fromWCharArray(WindowsGetStringRawBuffer(familyName, nullptr));
        qDeleteRt(packageId);

        IStorageFolder *storageFolder;
        appxPackage->get_InstalledLocation(&storageFolder);
        IStorageItem *storageItem;
        if (SUCCEEDED(storageFolder->QueryInterface(&storageItem))) {
            HSTRING folderPath;
            storageItem->get_Path(&folderPath);
            QFileInfo pathInfo(QString::fromWCharArray(WindowsGetStringRawBuffer(folderPath,nullptr)));
            package->path = pathInfo.canonicalFilePath().append(QLatin1String("/AppxManifest.xml"));
            qDeleteRt(storageItem);
        }
        qDeleteRt(storageFolder);

        packages.append(package);
        it->MoveNext(&hasCurrent);
    }
    qDeleteRt(appxPackages);
    return packages;
}

QList<WinRtPackagePtr> listPackages()
{
    QList<WinRtPackagePtr> result;

    WCHAR *sidStr = sidString();
    if (!sidStr)
        return result;

    HSTRING sid;
    HSTRING_HEADER sidHeader;
    WindowsCreateStringReference(sidStr, lstrlen(sidStr), &sidHeader, &sid);
    IPackageManager *manager = qNewRt<IPackageManager>(RuntimeClass_Windows_Management_Deployment_PackageManager);
    result = readPackages(manager, sid);
    LocalFree(sidStr);
    qDeleteRt(manager);
    return result;
}

#else // #if defined(_MSC_VER) && _MSC_VER >= 1700

QList<WinRtPackagePtr> listPackages()
{
    return QList<WinRtPackagePtr>();
}

#endif // !_MSC_VER


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
