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

#ifndef TOKEN_H
#define TOKEN_H

#include "clangwrapper_global.h"

#include <QtCore/QString>

namespace Clang {

/*
 * A token class to be used in simple lexing. It should be a lightweight
 * structure, still map the kinds from CXToken, and provide other information
 * (lazily when possible) we frequently use.
 */
class QTCREATOR_CLANGWRAPPER_EXPORT Token
{
public:
    Token();

    enum Kind {
        Punctuation,
        Keyword,
        Identifier,
        Literal,
        Comment
    };

    bool is(Kind kind) const { return m_flags.m_kind == kind; }
    Kind kind() const { return Kind(m_flags.m_kind); }
    unsigned begin() const { return m_begin; }
    unsigned length() const { return m_length; }

    bool isDoxygenComment() const;

    static bool isPunctuationLParen(const Token &token, const QString &code);
    static bool isPunctuationRParen(const Token &token, const QString &code);
    static bool isPunctuationLBrace(const Token &token, const QString &code);
    static bool isPunctuationRBrace(const Token &token, const QString &code);
    static bool isPunctuationLBracket(const Token &token, const QString &code);
    static bool isPunctuationRBracket(const Token &token, const QString &code);
    static bool isPunctuationLABracket(const Token &token, const QString &code);
    static bool isPunctuationRABracket(const Token &token, const QString &code);
    static bool isPunctuationSemiColon(const Token &token, const QString &code);
    static bool isPunctuationPound(const Token &token, const QString &code);
    static bool isPunctuationColon(const Token &token, const QString &code);
    static bool isPunctuationSpace(const Token &token, const QString &code);
    static bool isPunctuationNewLine(const Token &token, const QString &code);
    static bool isLiteralNumeric(const Token &token, const QString &code);
    static bool isLiteralText(const Token &token, const QString &code);

private:
    friend class RawLexer;

    static bool isCharCheck(const Token &token, const QString &code, const QLatin1Char &other);

    unsigned m_begin;
    unsigned m_length;

    struct Flags
    {
        unsigned m_kind           : 3;
        unsigned m_doxygenComment : 1;
    };
    union
    {
        unsigned m_flagsControl;
        Flags m_flags;
    };
};

} // Clang

#endif // TOKEN_H
