/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
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
** Nokia at info@qt.nokia.com.
**
**************************************************************************/

#ifndef UNIT_H
#define UNIT_H

#include "typedefs.h"

#include <clang-c/Index.h>

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QExplicitlySharedDataPointer>

namespace Clang {
namespace Internal {

class UnitData;

/*
 * This is a minimal wrapper around clang's translation unit functionality.
 * It should should contain only the very basic primitives which allow other
 * components such as code completion, code navigation, and others to access
 * data which directly depends on the translation unit.
 *
 * In other words, what's wrapped here is only the functions that receive a
 * CXTranslationUnit as a parameter. And this class itself is then the corresponding
 * abstraction of the CXTranslationUnit.
 *
 * Notes:
 *  - This class is not thread-safe.
 *  - It's reponsibility of the client to make sure that the wrapped translation
 *    unit is consistent with the other data such as cursor and locations being used.
 *  - The data of the TU is shared.
 *
 *
 * @TODO: This is similar but not exactly the same as the current ClangWrapper class.
 * That class is now tuned to specific features, so it's not generic enough to be used
 * an underlying component and aslo don't provide the data in a fine granularity as
 * needed here. At some point we should split ClangWrapper into its different logical
 * components and use this is the underlying structure.
 */
class Unit
{
public:
    Unit();
    Unit(const QString &fileName);
    Unit(const Unit &unit);
    Unit &operator=(const Unit &unit);
    ~Unit();

    const QString fileName() const;
    bool isNull() const;

    void setCompilationOptions(const QStringList &compOptions);
    void setUnsavedFiles(const UnsavedFiles &unsavedFiles);
    void setManagementOptions(unsigned managementOptions);

    // Methods for generating the TU. Name mappings are direct, for example:
    //   - parse corresponds to clang_parseTranslationUnit
    //   - createFromSourceFile corresponds to clang_createTranslationUnitFromSourceFile
    //   - etc
    void parse();
    void reparse();
    void create();
    void createFromSourceFile();

    // Simple forwarding methods (the ones that take the TU as a parameter).
    //   - getFile forwards to clang_getFile
    //   - getCursor forwards to clang_getCursor
    //   - etc
    CXFile getFile() const;
    CXCursor getCursor(const CXSourceLocation &location) const;
    CXSourceLocation getLocation(const CXFile &file, unsigned line, unsigned column) const;
    CXCursor getTranslationUnitCursor() const;
    CXString getTranslationUnitSpelling() const;

private:
    QExplicitlySharedDataPointer<UnitData> m_data;
};

} // Internal
} // Clang

#endif // UNIT_H
