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

#ifndef WINRT_INTERNAL_PACKAGEMANAGER_H
#define WINRT_INTERNAL_PACKAGEMANAGER_H

#include <QObject>
#include <QString>
#include <QSharedPointer>
#include <QMetaType>

namespace WinRt {
namespace Internal {
class PackageManagerPrivate;

class WinRtPackage
{
public:
    QString name;
    QString fullName;
    QString familyName;
    QString publisher;
    QString publisherId;
    QString path;
    QString version;
};

typedef QSharedPointer<WinRtPackage> WinRtPackagePtr;


class PackageManager : public QObject
{
    Q_OBJECT
public:
    enum AddPackageFlags {
        None = 0,
        DevelopmentMode = 0x1
    };

    enum Error {
        NoError,
        UnknownError,
        Canceled,
        DeveloperLicenseRequired
    };

    explicit PackageManager(QObject *parent = 0);
    ~PackageManager();

    QList<WinRtPackagePtr> listPackages() const;

    bool startAddPackage(const QString &manifestFile, AddPackageFlags flags,
                         QString *errorMessage);
    bool startRemovePackage(const QString &fullName, QString *errorMessage);

    bool operationInProgress() const;

    static void launchDeveloperRegistration();

signals:
    void packageAdded(const QString &manifestFile);
    void packageAddFailed(const QString &manifestFile, const QString &message, PackageManager::Error error);
    void packageRemoved(const QString &fullName);
    void packageRemovalFailed(const QString &fullName, const QString &message, PackageManager::Error error);

private:
    friend class PackageManagerPrivate;

    PackageManagerPrivate *d;
};
Q_DECLARE_METATYPE(PackageManager::Error)

} // namespace Internal
} // namespace WinRt

Q_DECLARE_METATYPE(WinRt::Internal::WinRtPackagePtr)


#endif // WINRT_INTERNAL_PACKAGEMANAGER_H
