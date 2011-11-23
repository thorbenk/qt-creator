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

#include "unit.h"

#include <clang-c/Index.h>

#include <QtCore/QByteArray>
#include <QtCore/QVector>
#include <QtCore/QSharedData>
#include <QtCore/QDateTime>
#include <QtAlgorithms>

#ifdef DEBUG_UNIT_COUNT
#  include <QAtomicInt>
#  include <QDebug>
static QBasicAtomicInt unitDataCount = Q_BASIC_ATOMIC_INITIALIZER(0);
#endif // DEBUG_UNIT_COUNT

namespace Clang {
namespace Internal {

class UnitData : public QSharedData
{
public:
    UnitData();
    UnitData(const QString &fileName);
    ~UnitData();

    void swap(UnitData *unitData);

    void unload();
    bool isLoaded() const;

    void updateTimeStamp();

    void releaseRawOptions();

    CXIndex m_index;
    CXTranslationUnit m_tu;
    QByteArray m_fileName;
    QStringList m_compOptions;
    QVector<const char *> m_rawCompOptions;
    PCHInfoPtr m_pchInfo;
    unsigned m_managOptions;
    UnsavedFiles m_unsaved;
    QDateTime m_timeStamp;
};

} // Internal
} // Clang

using namespace Clang;
using namespace Internal;

UnitData::UnitData()
    : m_index(0)
    , m_tu(0)
    , m_pchInfo(PCHInfo::createEmpty())
{
}

UnitData::UnitData(const QString &fileName)
    : m_index(clang_createIndex(/*excludeDeclsFromPCH*/ 1, /*displayDiagnostics*/ 0))
    , m_tu(0)
    , m_fileName(fileName.toUtf8())
    , m_pchInfo(PCHInfo::createEmpty())
{
}

UnitData::~UnitData()
{
    unload();
    clang_disposeIndex(m_index);
    m_index = 0;
    releaseRawOptions();
}

void UnitData::swap(UnitData *other)
{
    qSwap(m_index, other->m_index);
    qSwap(m_tu, other->m_tu);
    qSwap(m_fileName, other->m_fileName);
    qSwap(m_compOptions, other->m_compOptions);
    qSwap(m_rawCompOptions, other->m_rawCompOptions);
    qSwap(m_pchInfo, other->m_pchInfo);
    qSwap(m_managOptions, other->m_managOptions);
    qSwap(m_unsaved, other->m_unsaved);
    qSwap(m_timeStamp, other->m_timeStamp);
}

void UnitData::unload()
{
    if (m_tu) {
        clang_disposeTranslationUnit(m_tu);
        m_tu = 0;
#ifdef DEBUG_UNIT_COUNT
        qDebug()<<"# translation units:"<< (unitDataCount.fetchAndAddOrdered(-1)-1);
#endif // DEBUG_UNIT_COUNT
    }
}

bool UnitData::isLoaded() const
{
    return m_tu && m_index;
}

void UnitData::updateTimeStamp()
{
    m_timeStamp = QDateTime::currentDateTime();
}

void UnitData::releaseRawOptions()
{
    foreach (const char *s, m_rawCompOptions)
        delete[] s;
}


Unit::Unit()
    : m_data(new UnitData)
{}

Unit::Unit(const QString &fileName)
    : m_data(new UnitData(fileName))
{}

Unit::Unit(const Unit &unit)
    : m_data(unit.m_data)
{}

Unit &Unit::operator =(const Unit &unit)
{
    if (this != &unit)
        m_data = unit.m_data;
    return *this;
}

Unit::~Unit()
{}

const QString Unit::fileName() const
{
    return m_data->m_fileName;
}

bool Unit::isLoaded() const
{
    return m_data->isLoaded();
}

const QDateTime &Unit::timeStamp() const
{
    return m_data->m_timeStamp;
}

QStringList Unit::compilationOptions() const
{
    return m_data->m_compOptions;
}

void Unit::setCompilationOptions(const QStringList &compOptions)
{
    m_data->m_compOptions = compOptions;

    m_data->releaseRawOptions();
    m_data->m_rawCompOptions.resize(compOptions.size());
    for (int i = 0; i < compOptions.size(); ++i)
        m_data->m_rawCompOptions[i] = qstrdup(compOptions[i].toUtf8());
}

PCHInfoPtr Unit::pchInfo() const
{
    return m_data->m_pchInfo;
}

void Unit::setPchInfo(PCHInfoPtr pchInfo)
{
    m_data->m_pchInfo = pchInfo;
}

UnsavedFiles Unit::unsavedFiles() const
{
    return m_data->m_unsaved;
}

void Unit::setUnsavedFiles(const UnsavedFiles &unsavedFiles)
{
    m_data->m_unsaved = unsavedFiles;
}

unsigned Unit::managementOptions() const
{
    return m_data->m_managOptions;
}

void Unit::setManagementOptions(unsigned managementOptions)
{
    m_data->m_managOptions = managementOptions;
}

bool Unit::isUnique() const
{
    return m_data->ref == 1;
}

void Unit::makeUnique()
{
    UnitData *uniqueData = new UnitData;
    m_data->swap(uniqueData); // Notice we swap the data itself and not the shared pointer.
    m_data = QExplicitlySharedDataPointer<UnitData>(uniqueData);
}

void Unit::parse()
{
    m_data->unload();

    const char **arguments = 0;
    if (!m_data->m_rawCompOptions.isEmpty())
        arguments = &m_data->m_rawCompOptions[0];

    m_data->updateTimeStamp();

    m_data->m_tu = clang_parseTranslationUnit(m_data->m_index,
                                              m_data->m_fileName.constData(),
                                              arguments, m_data->m_compOptions.size(),
                                              // @TODO: Extract UnsavedFileData...
                                              0, 0,
                                              m_data->m_managOptions);
#ifdef DEBUG_UNIT_COUNT
    if (m_data->m_tu)
        qDebug()<<"# translation units:"<< (unitDataCount.fetchAndAddOrdered(1)+1);
#endif // DEBUG_UNIT_COUNT
}

void Unit::reparse()
{
    // @TODO
}

void Unit::create()
{
    // @TODO
}

void Unit::createFromSourceFile()
{
    // @TODO
}

CXFile Unit::getFile() const
{
    return clang_getFile(m_data->m_tu, m_data->m_fileName.constData());
}

CXCursor Unit::getCursor(const CXSourceLocation &location) const
{
    return clang_getCursor(m_data->m_tu, location);
}

CXSourceLocation Unit::getLocation(const CXFile &file, unsigned line, unsigned column) const
{
    return clang_getLocation(m_data->m_tu, file, line, column);
}

CXCursor Unit::getTranslationUnitCursor() const
{
    return clang_getTranslationUnitCursor(m_data->m_tu);
}

CXString Unit::getTranslationUnitSpelling() const
{
    return clang_getTranslationUnitSpelling(m_data->m_tu);
}

void Unit::getInclusions(CXInclusionVisitor visitor, CXClientData clientData) const
{
    clang_getInclusions(m_data->m_tu, visitor, clientData);
}

unsigned Unit::getNumDiagnostics() const
{
    return clang_getNumDiagnostics(m_data->m_tu);
}

CXDiagnostic Unit::getDiagnostic(unsigned index) const
{
    return clang_getDiagnostic(m_data->m_tu, index);
}
