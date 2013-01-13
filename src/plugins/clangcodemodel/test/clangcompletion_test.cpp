/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

/**
 * @file clangcompletion_test.cpp
 * @brief Performs test for C/C++ code completion
 *
 * All test cases given as strings with @ character that points to completion
 * location.
 */

#ifdef WITH_TESTS

// Disabled because there still no tool to detect system Objective-C headers
#define ENABLE_OBJC_TESTS 0

#include <QtTest>
#include <QDebug>

#include "completiontesthelper.h"
#include "../clangcodemodelplugin.h"

using namespace ClangCodeModel;
using namespace ClangCodeModel::Internal;

////////////////////////////////////////////////////////////////////////////////
// Test cases

/**
 * \defgroup Regression tests
 *
 * This group tests possible regressions in non-standard completion chunks
 * handling: for example, macro arguments and clang's code snippets.
 *
 * @{
 */

void ClangCodeModelPlugin::test_CXX_regressions()
{
    QFETCH(QString, file);
    QFETCH(QStringList, unexpected);
    QFETCH(QStringList, mustHave);

    CompletionTestHelper helper;
    helper << file;

    QStringList proposals = helper.codeCompleteTexts();

    foreach (const QString &p, unexpected)
        QTEST_ASSERT(false == proposals.contains(p));

    foreach (const QString &p, mustHave)
        QTEST_ASSERT(true == proposals.contains(p));
}

void ClangCodeModelPlugin::test_CXX_regressions_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QStringList>("unexpected");
    QTest::addColumn<QStringList>("mustHave");

    QString file;
    QStringList unexpected;
    QStringList mustHave;

    file = QLatin1String("cxx_regression_1.cpp");
    mustHave << QLatin1String("sqr");
    QTest::newRow("case: method call completion") << file << unexpected << mustHave;
    mustHave.clear();
    unexpected.clear();

    file = QLatin1String("cxx_regression_2.cpp");
    unexpected << QLatin1String("i_second");
    unexpected << QLatin1String("c_second");
    unexpected << QLatin1String("f_second");
    mustHave << QLatin1String("i_first");
    mustHave << QLatin1String("c_first");
    QTest::newRow("case: multiple anonymous structs") << file << unexpected << mustHave;
    mustHave.clear();
    unexpected.clear();

    file = QLatin1String("cxx_regression_3.cpp");
    mustHave << QLatin1String("i8");
    mustHave << QLatin1String("i64");
    QTest::newRow("case: nested class resolution") << file << unexpected << mustHave;
    mustHave.clear();
    unexpected.clear();

    file = QLatin1String("cxx_regression_4.cpp");
    mustHave << QLatin1String("action");
    QTest::newRow("case: local (in function) class resolution") << file << unexpected << mustHave;
    mustHave.clear();
    unexpected.clear();

    file = QLatin1String("cxx_regression_5.cpp");
    mustHave << QLatin1String("doB");
    unexpected << QLatin1String("doA");
    QTest::newRow("case: nested template class resolution") << file << unexpected << mustHave;
    mustHave.clear();
    unexpected.clear();

    file = QLatin1String("cxx_regression_6.cpp");
    mustHave << QLatin1String("OwningPtr");
    QTest::newRow("case: using particular symbol from namespace") << file << unexpected << mustHave;
    mustHave.clear();
    unexpected.clear();

    file = QLatin1String("cxx_regression_7.cpp");
    mustHave << QLatin1String("dataMember");
    mustHave << QLatin1String("anotherMember");
    QTest::newRow("case: template class inherited from template parameter") << file << unexpected << mustHave;
    mustHave.clear();
    unexpected.clear();

    file = QLatin1String("cxx_regression_8.cpp");
    mustHave << QLatin1String("utils::");
    unexpected << QLatin1String("utils");
    QTest::newRow("case: namespace completion in function body") << file << unexpected << mustHave;
    mustHave.clear();
    unexpected.clear();
}

#endif
