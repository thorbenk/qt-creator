/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#include "clangcodemodelplugin.h"
#include "utils.h"

#include <coreplugin/coreconstants.h>
#include <coreplugin/icore.h>
#include <coreplugin/imode.h>
#include <coreplugin/modemanager.h>
#include <coreplugin/id.h>

#include <cpptools/cppmodelmanager.h>

#include <QtPlugin>

namespace ClangCodeModel {
namespace Internal {

ClangCodeModelPlugin::ClangCodeModelPlugin()
    : m_completionAssistProvider(0)
    , m_highlightingFactory(0)
{
}

bool ClangCodeModelPlugin::initialize(const QStringList &arguments, QString *errorMessage)
{
    Q_UNUSED(arguments)
    Q_UNUSED(errorMessage)

    ClangCodeModel::initializeClang();

#ifdef CLANG_COMPLETION
    m_completionAssistProvider.reset(new ClangCompletionAssistProvider);
    CPlusPlus::CppModelManagerInterface::instance()->setCppCompletionAssistProvider(m_completionAssistProvider.data());
#endif // CLANG_COMPLETION

#ifdef CLANG_HIGHLIGHTING
    m_highlightingFactory.reset(new ClangHighlightingSupportFactory);
    CPlusPlus::CppModelManagerInterface::instance()->setHighlightingSupportFactory(m_highlightingFactory.data());
#endif // CLANG_HIGHLIGHTING

#ifdef CLANG_INDEXING
    m_indexer.reset(new ClangIndexer);
    CPlusPlus::CppModelManagerInterface::instance()->setIndexingSupport(m_indexer->indexingSupport());
#endif // CLANG_INDEXING

    return true;
}

void ClangCodeModelPlugin::extensionsInitialized()
{
}

ExtensionSystem::IPlugin::ShutdownFlag ClangCodeModelPlugin::aboutToShutdown()
{
    CPlusPlus::CppModelManagerInterface::instance()->setCppCompletionAssistProvider(0);
    CPlusPlus::CppModelManagerInterface::instance()->setHighlightingSupportFactory(0);
    CPlusPlus::CppModelManagerInterface::instance()->setIndexingSupport(0);

    if (m_completionAssistProvider)
        removeObject(m_completionAssistProvider.data());

    return ExtensionSystem::IPlugin::aboutToShutdown();
}

} // namespace Internal
} // namespace Clang

Q_EXPORT_PLUGIN(ClangCodeModel::Internal::ClangCodeModelPlugin)
