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

#ifndef SIMPLELEXER_H
#define SIMPLELEXER_H

#include "clang_global.h"
#include "keywords.h"
#include "token.h"

#include <clang-c/Index.h>
#include <clang/Basic/LangOptions.h>

#include <QtCore/QString>
#include <QtCore/QList>

namespace Clang {

class CLANG_EXPORT RawLexer
{
public:
    RawLexer();

    void includeQt();
    void includeTrigraphs();
    void includeDigraphs();
    void includeC99();
    void includeCpp0x();
    void includeCppOperators();

    void init();

    QList<Token> lex(const QString &code, int *state = 0);

private:
    enum State {
        Normal,
        InComment,
        InDoxygenComment,
        InString
    };

    static void checkDoxygenComment(const QString &lexedCode, Token *token);

    State m_state;
    Keywords m_keywords;
    clang::LangOptions m_langOptions;
};

} // Clang

#endif // SIMPLELEXER_H
