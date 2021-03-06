/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company.  For licensing terms and
** conditions see http://www.qt.io/terms-conditions.  For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "locatorsettingspage.h"
#include "locatorconstants.h"

#include "locator.h"
#include "ilocatorfilter.h"
#include "directoryfilter.h"

#include <coreplugin/coreconstants.h>
#include <utils/qtcassert.h>

#include <QCoreApplication>

Q_DECLARE_METATYPE(Core::ILocatorFilter*)

using namespace Core;
using namespace Core::Internal;

LocatorSettingsPage::LocatorSettingsPage(Locator *plugin)
    : m_plugin(plugin), m_widget(0)
{
    setId(Constants::FILTER_OPTIONS_PAGE);
    setDisplayName(QCoreApplication::translate("Locator", Constants::FILTER_OPTIONS_PAGE));
    setCategory(Constants::SETTINGS_CATEGORY_CORE);
    setDisplayCategory(QCoreApplication::translate("Core", Constants::SETTINGS_TR_CATEGORY_CORE));
    setCategoryIcon(QLatin1String(Constants::SETTINGS_CATEGORY_CORE_ICON));
}

QWidget *LocatorSettingsPage::widget()
{
    if (!m_widget) {
        m_widget = new QWidget;
        m_ui.setupUi(m_widget);
        m_ui.refreshInterval->setToolTip(m_ui.refreshIntervalLabel->toolTip());
        connect(m_ui.filterList, &QListWidget::currentItemChanged,
                this, &LocatorSettingsPage::updateButtonStates);
        connect(m_ui.filterList, SIGNAL(itemActivated(QListWidgetItem*)),
                this, SLOT(configureFilter(QListWidgetItem*)));
        connect(m_ui.editButton, SIGNAL(clicked()),
                this, SLOT(configureFilter()));
        connect(m_ui.addButton, &QPushButton::clicked,
                this, &LocatorSettingsPage::addCustomFilter);
        connect(m_ui.removeButton, &QPushButton::clicked,
                this, &LocatorSettingsPage::removeCustomFilter);

        m_ui.refreshInterval->setValue(m_plugin->refreshInterval());
        m_filters = m_plugin->filters();
        m_customFilters = m_plugin->customFilters();
        saveFilterStates();
        updateFilterList();
    }
    return m_widget;
}

void LocatorSettingsPage::apply()
{
    // Delete removed filters and clear added filters
    qDeleteAll(m_removedFilters);
    m_removedFilters.clear();
    m_addedFilters.clear();

    // Pass the new configuration on to the plugin
    m_plugin->setFilters(m_filters);
    m_plugin->setCustomFilters(m_customFilters);
    m_plugin->setRefreshInterval(m_ui.refreshInterval->value());
    requestRefresh();
    m_plugin->saveSettings();
    saveFilterStates();
}

void LocatorSettingsPage::finish()
{
    // If settings were applied, this shouldn't change anything. Otherwise it
    // makes sure the filter states aren't changed permanently.
    restoreFilterStates();

    // Delete added filters and clear removed filters
    qDeleteAll(m_addedFilters);
    m_addedFilters.clear();
    m_removedFilters.clear();

    // Further cleanup
    m_filters.clear();
    m_customFilters.clear();
    m_refreshFilters.clear();
    delete m_widget;
}

void LocatorSettingsPage::requestRefresh()
{
    if (!m_refreshFilters.isEmpty())
        m_plugin->refresh(m_refreshFilters);
}

void LocatorSettingsPage::saveFilterStates()
{
    m_filterStates.clear();
    foreach (ILocatorFilter *filter, m_filters)
        m_filterStates.insert(filter, filter->saveState());
}

void LocatorSettingsPage::restoreFilterStates()
{
    foreach (ILocatorFilter *filter, m_filterStates.keys())
        filter->restoreState(m_filterStates.value(filter));
}

void LocatorSettingsPage::updateFilterList()
{
    m_ui.filterList->clear();
    foreach (ILocatorFilter *filter, m_filters) {
        if (filter->isHidden())
            continue;

        QString title;
        if (filter->isIncludedByDefault())
            title = filter->displayName();
        else
            title = tr("%1 (prefix: %2)").arg(filter->displayName()).arg(filter->shortcutString());
        QListWidgetItem *item = new QListWidgetItem(title);
        item->setData(Qt::UserRole, qVariantFromValue(filter));
        m_ui.filterList->addItem(item);
    }
    if (m_ui.filterList->count() > 0)
        m_ui.filterList->setCurrentRow(0);
}

void LocatorSettingsPage::updateButtonStates()
{
    QListWidgetItem *item = m_ui.filterList->currentItem();
    ILocatorFilter *filter = (item ? item->data(Qt::UserRole).value<ILocatorFilter *>() : 0);
    m_ui.editButton->setEnabled(filter && filter->isConfigurable());
    m_ui.removeButton->setEnabled(filter && m_customFilters.contains(filter));
}

void LocatorSettingsPage::configureFilter(QListWidgetItem *item)
{
    if (!item)
        item = m_ui.filterList->currentItem();
    QTC_ASSERT(item, return);
    ILocatorFilter *filter = item->data(Qt::UserRole).value<ILocatorFilter *>();
    QTC_ASSERT(filter, return);

    if (!filter->isConfigurable())
        return;
    bool needsRefresh = false;
    filter->openConfigDialog(m_widget, needsRefresh);
    if (needsRefresh && !m_refreshFilters.contains(filter))
        m_refreshFilters.append(filter);
    updateFilterList();
}

void LocatorSettingsPage::addCustomFilter()
{
    ILocatorFilter *filter = new DirectoryFilter(
                Id(Constants::CUSTOM_FILTER_BASEID).withSuffix(m_customFilters.size() + 1));
    bool needsRefresh = false;
    if (filter->openConfigDialog(m_widget, needsRefresh)) {
        m_filters.append(filter);
        m_addedFilters.append(filter);
        m_customFilters.append(filter);
        m_refreshFilters.append(filter);
        updateFilterList();
    }
}

void LocatorSettingsPage::removeCustomFilter()
{
    QListWidgetItem *item = m_ui.filterList->currentItem();
    QTC_ASSERT(item, return);
    ILocatorFilter *filter = item->data(Qt::UserRole).value<ILocatorFilter *>();
    QTC_ASSERT(m_customFilters.contains(filter), return);
    m_filters.removeAll(filter);
    m_customFilters.removeAll(filter);
    m_refreshFilters.removeAll(filter);
    if (m_addedFilters.contains(filter)) {
        m_addedFilters.removeAll(filter);
        delete filter;
    } else {
        m_removedFilters.append(filter);
    }
    updateFilterList();
}
