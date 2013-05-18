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

#include "packagemanager.h"
#include "winrtutils.h"

#include <QFileInfo>
#include <QDebug>
#include <QPointer>
#include <QHash>
#include <QUrl>

#if defined(_MSC_VER) && _MSC_VER >= 1700

#  include <wrl.h>
#  include <windows.management.deployment.h>
#  include <Windows.ApplicationModel.h>
#  include <windows.foundation.collections.h>
#  include <windows.system.userprofile.h>
#  include <sddl.h>
#  include <shellapi.h>

using namespace Microsoft::WRL;
using namespace ABI::Windows;
using namespace ABI::Windows::ApplicationModel;
using namespace ABI::Windows::Storage;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Management::Deployment;

typedef IAsyncOperationWithProgress<DeploymentResult*, struct DeploymentProgress>
        AsyncDeployOperationProgress;

typedef IAsyncOperationWithProgressCompletedHandler<DeploymentResult*, struct DeploymentProgress>
        AsyncDeployOperationProgressHandler;

namespace WinRt {
namespace Internal {

static IUri *createUri(const QString &s, QString *errorMessage = 0)
{
    IUri *uri = 0;
    const HRESULT hr = CreateUri((LPCWSTR)s.utf16(), Uri_CREATE_ALLOW_RELATIVE, 0, &uri);
    if (FAILED(hr)) {
        if (errorMessage)
            *errorMessage = PackageManager::tr("Failed to create URI from \"%1\": %2").
                arg(s, comErrorString(hr));
        return 0;
    }
    return uri;
}

static IUriRuntimeClass *createUriRunTimeClass(const QString &url, QString *errorMessage = 0)
{
    IUriRuntimeClassFactory *factory = 0;

    const HStringHandle className(RuntimeClass_Windows_Foundation_Uri);
    HRESULT hr = GetActivationFactory(className, &factory);
    if (FAILED(hr)) {
        if (errorMessage)
            *errorMessage = PackageManager::tr("GetActivationFactory() failed creating URI \"%1\": %2").
                arg(url, comErrorString(hr));
        return 0;
    }
    IUriRuntimeClass *result = 0;
    const HStringHandle urlH(url);
    hr = factory->CreateUri(urlH, &result);
    factory->Release();
    if (FAILED(hr)) {
        if (errorMessage)
            *errorMessage = PackageManager::tr("IUriRuntimeClassFactory::Create() failed creating URI \"%1\": %2").
                arg(url, comErrorString(hr));

        return 0;
    }
    return result;
}

static PackageManager::Error evaluateDeploymentResult(
        AsyncDeployOperationProgress  *asyncInfo,
        ABI::Windows::Foundation::AsyncStatus status,
        QString *errorMessage)
{
    PackageManager::Error error = PackageManager::NoError;
    errorMessage->clear();
    switch (status) {
    case ABI::Windows::Foundation::AsyncStatus::Started:
    case ABI::Windows::Foundation::AsyncStatus::Completed:
        break;
    case ABI::Windows::Foundation::AsyncStatus::Canceled:
        error = PackageManager::Canceled; // Should not occur, deployment cannot be canceled.
        *errorMessage = PackageManager::tr("Operation canceled.");
        break;
    case ABI::Windows::Foundation::AsyncStatus::Error: {
        error = PackageManager::UnknownError;
        ABI::Windows::Management::Deployment::IDeploymentResult *result;
        if (SUCCEEDED(asyncInfo->GetResults(&result))) {
            HSTRING hError;
            if (SUCCEEDED(result->get_ErrorText(&hError)))
                *errorMessage = hStringToQString(hError);
            HRESULT errorCode;
            if (SUCCEEDED(result->get_ExtendedErrorCode(&errorCode))) {
                switch (HRESULT_CODE(errorCode)) {
                case ERROR_INSTALL_POLICY_FAILURE:
                    error = PackageManager::DeveloperLicenseRequired;
                    break;
                default:
                    break; // TODO: handle more error cases
                }
            }
        }
        if (errorMessage->isEmpty())
            *errorMessage = PackageManager::tr("Unknown error.");
    }
        break;
    }
    return error;
}

// ---------- PackageManagerPrivate

class PackageManagerPrivate {
public:
    typedef QHash<AsyncDeployOperationProgress *, QString> ProgressManifestHash;
    typedef QHash<AsyncDeployOperationProgress *, QString> ProgressNameHash;

    typedef ABI::Windows::Management::Deployment::IPackageManager IPackageManager;

    explicit PackageManagerPrivate(PackageManager *qq) : q(qq), m_manager(0) {}
    ~PackageManagerPrivate();

    IPackageManager *packageManager() const;

    ProgressManifestHash pendingInstalls;
    ProgressNameHash pendingRemoves;

    HRESULT packageAdded(AsyncDeployOperationProgress  *asyncInfo, ABI::Windows::Foundation::AsyncStatus status);
    HRESULT packageRemoved(AsyncDeployOperationProgress  *asyncInfo, ABI::Windows::Foundation::AsyncStatus status);

private:
    PackageManager *q;
    mutable IPackageManager *m_manager;
};

PackageManagerPrivate::~PackageManagerPrivate()
{
    if (m_manager)
        qDeleteRt(m_manager);
}

PackageManagerPrivate::IPackageManager *PackageManagerPrivate::packageManager() const
{
    if (!m_manager)
        m_manager = qNewRt<IPackageManager>(RuntimeClass_Windows_Management_Deployment_PackageManager);
    return m_manager;
}

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

static inline QList<WinRtPackagePtr> readPackages(PackageManagerPrivate::IPackageManager *manager, const HSTRING &sid)
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
        IStorageItem *storageItem;
        if (SUCCEEDED(appxPackage->get_InstalledLocation(&storageFolder))
                && SUCCEEDED(storageFolder->QueryInterface(&storageItem))) {
            HSTRING folderPath;
            storageItem->get_Path(&folderPath);
            QFileInfo pathInfo(QString::fromWCharArray(WindowsGetStringRawBuffer(folderPath,nullptr)));
            package->path = pathInfo.canonicalFilePath().append(QLatin1String("/AppxManifest.xml"));
            qDeleteRt(storageItem);
            qDeleteRt(storageFolder);
        }

        packages.append(package);
        it->MoveNext(&hasCurrent);
    }
    qDeleteRt(appxPackages);
    return packages;
}

PackageManager::PackageManager(QObject *parent)
    : QObject(parent)
    , d(new PackageManagerPrivate(this))
{
}

PackageManager::~PackageManager()
{
    if (operationInProgress()) {
        qWarning("%s invoked while deployment is in progress.", Q_FUNC_INFO);
        d->pendingInstalls.clear(); // Ensure progress instances cannot be found in map to prevent crash.
        d->pendingRemoves.clear();
    }  else {
        delete d;
    }
}

static inline QString msgNoPackageManager()
{
    return PackageManager::tr("Unable to obtain package manager interface.");
}

QList<WinRtPackagePtr> PackageManager::listPackages() const
{
    QList<WinRtPackagePtr> result;

    WCHAR *sidStr = sidString();
    if (!sidStr)
        return result;

    HStringHandle  sidHString(sidStr);
    if (PackageManagerPrivate::IPackageManager *manager = d->packageManager())
        result = readPackages(manager, sidHString);
    LocalFree(sidStr);
    return result;
}

bool PackageManager::startAddPackage(const QString &manifestFile,
                                     AddPackageFlags flags,
                                     QString *errorMessage)
{
    PackageManagerPrivate::IPackageManager *manager = d->packageManager();
    if (!manager) {
        *errorMessage = msgNoPackageManager();
        return false;
    }
    IUriRuntimeClass *uri = createUriRunTimeClass(QUrl::fromLocalFile(manifestFile).toString(), errorMessage);
    if (!uri)
        return false;

    AsyncDeployOperationProgress *progress = 0;
    DeploymentOptions options = (flags & DevelopmentMode) ?
                DeploymentOptions_DevelopmentMode : DeploymentOptions_None;
    const HRESULT hr = manager->RegisterPackageAsync(uri, 0, options, &progress);
    uri->Release();
    if (FAILED(hr)) {
        *errorMessage = tr("RegisterPackageAsync() \"%1\" failed: %2").
                        arg(manifestFile, comErrorString(hr));
        return false;
    }

    d->pendingInstalls.insert(progress, manifestFile);
    progress->put_Completed(Callback<AsyncDeployOperationProgressHandler>(d, &PackageManagerPrivate::packageAdded).Get());
    return true;
}

HRESULT PackageManagerPrivate::packageAdded(AsyncDeployOperationProgress  *asyncInfo, ABI::Windows::Foundation::AsyncStatus status)
{
    QString errorMessage;
    const PackageManager::Error error = evaluateDeploymentResult(asyncInfo, status, &errorMessage);
    const ProgressManifestHash::Iterator it = pendingInstalls.find(asyncInfo);
    if (it == pendingInstalls.end()) {
        qWarning("%s: asyncInfo not found", Q_FUNC_INFO);
        return S_OK;
    }
    if (error == PackageManager::NoError) {
        emit q->packageAdded(it.value());
    } else {
        emit q->packageAddFailed(it.value(), errorMessage, error);
    }
    pendingInstalls.erase(it);
    return S_OK;
}

bool PackageManager::startRemovePackage(const QString &fullName, QString *errorMessage)
{
    PackageManagerPrivate::IPackageManager *manager = d->packageManager();
    if (!manager) {
        *errorMessage = msgNoPackageManager();
        return false;
    }

    const HStringHandle fullNameH(fullName);
    AsyncDeployOperationProgress *progress = 0;
    const HRESULT hr = manager->RemovePackageAsync(fullNameH, &progress);
    if (FAILED(hr)) {
        *errorMessage = tr("RemovePackageAsync() \"%1\" failed: %2").
                        arg(fullName, comErrorString(hr));
        return false;
    }
    d->pendingRemoves.insert(progress, fullName);

    progress->put_Completed(Callback<AsyncDeployOperationProgressHandler>(d, &PackageManagerPrivate::packageRemoved).Get());
    return true;
}

HRESULT PackageManagerPrivate::packageRemoved(AsyncDeployOperationProgress  *asyncInfo, ABI::Windows::Foundation::AsyncStatus status)
{
    QString errorMessage;
    const PackageManager::Error error = evaluateDeploymentResult(asyncInfo, status, &errorMessage);
    const ProgressNameHash::Iterator it = pendingRemoves.find(asyncInfo);
    if (it == pendingRemoves.end()) {
        qWarning("%s: asyncInfo not found", Q_FUNC_INFO);
        return S_OK;
    }
    if (error == PackageManager::NoError) {
        emit q->packageRemoved(it.value());
    } else {
        emit q->packageRemovalFailed(it.value(), errorMessage, error);
    }
    pendingRemoves.erase(it);
    return S_OK;
}

bool PackageManager::operationInProgress() const
{
    return !d->pendingInstalls.isEmpty() || !d->pendingRemoves.isEmpty();
}

void PackageManager::launchDeveloperRegistration()
{
    // Must run elevated, so we use the native API
    SHELLEXECUTEINFO shExecInfo;
    shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    shExecInfo.lpVerb = L"runas";
    shExecInfo.lpFile = L"powershell";
    shExecInfo.lpParameters = L"-windowstyle hidden -command show-windowsdeveloperlicenseregistration";
    shExecInfo.nShow = SW_MINIMIZE;
    ShellExecuteEx(&shExecInfo);
}

#else // defined(_MSC_VER) && _MSC_VER >= 1700

namespace WinRt {
namespace Internal {

PackageManager::PackageManager(QObject *parent)
    : QObject(parent)
    , d(0)
{
}

PackageManager::~PackageManager()
{
}

QList<WinRtPackagePtr> PackageManager::listPackages() const
{
    return QList<WinRtPackagePtr>();
}

bool PackageManager::startAddPackage(const QString &, AddPackageFlags, QString *)
{
    return false;
}

bool PackageManager::startRemovePackage(const QString &, QString *)
{
    return false;
}

bool PackageManager::operationInProgress() const
{
    return false;
}

void PackageManager::launchDeveloperRegistration()
{
}

#endif // !defined(_MSC_VER) && _MSC_VER >= 1700

} // namespace Internal
} // namespace WinRt
