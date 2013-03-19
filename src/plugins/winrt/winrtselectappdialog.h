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

#ifndef WINRT_INTERNAL_WINRTSELECTAPPDIALOG_H
#define WINRT_INTERNAL_WINRTSELECTAPPDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
class QModelIndex;
class QPushButton;
class QSortFilterProxyModel;
QT_END_NAMESPACE

namespace WinRt {
namespace Internal {

namespace Ui {
class WinRtSelectAppDialog;
}

class WinRtRunConfigurationModel;

class WinRtSelectAppDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WinRtSelectAppDialog(QWidget *parent = 0);
    ~WinRtSelectAppDialog();

    QString appId() const;

protected:
    void contextMenuEvent(QContextMenuEvent *);

private slots:
    void currentIndexChanged();
    void refresh();

private:
    void adjustColumns();
    QString appIdForIndex(const QModelIndex &filterIndex) const;

    Ui::WinRtSelectAppDialog *m_ui;
    QPushButton *m_selectButton;
    QPushButton *m_refreshButton;
    WinRtRunConfigurationModel *m_model;
    QSortFilterProxyModel *m_filterModel;
};

} // namespace Internal
} // namespace WinRt

#endif // WINRT_INTERNAL_WINRTSELECTAPPDIALOG_H
