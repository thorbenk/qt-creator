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

#include "cmakecbpparser.h"

#include <utils/fileutils.h>
#include <utils/stringutils.h>
#include <utils/algorithm.h>
#include <projectexplorer/projectnodes.h>

#include <QElapsedTimer>
#include <QLoggingCategory>

using namespace ProjectExplorer;
using namespace Utils;

namespace CMakeProjectManager {
namespace Internal {

/////
// CMakeCbpParser
////

namespace {
int distance(const QString &targetDirectory, const FileName &fileName)
{
    const QString commonParent = Utils::commonPath(QStringList() << targetDirectory << fileName.toString());
    return targetDirectory.mid(commonParent.size()).count(QLatin1Char('/'))
            + fileName.toString().mid(commonParent.size()).count(QLatin1Char('/'));
}

QByteArray targetFromNinjaLine(QByteArray const& line)
{
    // This function recognizes lines looking like this:
    //
    // # Object build statements for MODULE_LIBRARY target testtarget
    //
    // and returns 'testtarget'
    QByteArray currentTarget;
    int p1 = line.indexOf(" target ");
    if (p1 < 0)
        return currentTarget;
    p1 += 8;
    int p2 = line.indexOf(" ", p1);
    if (p2 < 0)
        p2 = line.size();
    currentTarget = line.mid(p1, p2-p1);
    return currentTarget; 
}

QList<Utils::FileName> filesFromNinjaLine(QByteArray line, const QList<Utils::FileName>& knownFiles)
{
    if (line.startsWith("build")) {
        int p = line.indexOf(": ")+1;
        line = line.mid(p);
    }
    
    QList<Utils::FileName> files;
  
    QList<QByteArray> tokens = line.split (' ');
    foreach (QByteArray token, tokens) {
        token = token.trimmed ();
        token = token.replace ("/./", "/");
        Utils::FileName file = Utils::FileName::fromLatin1 (token);
        if (std::binary_search(knownFiles.begin (), knownFiles.end(), file)) {
            files.append(file);
        }
    }
    
    return files;
}

bool sortFilesToTargetsViaNinja(const QString &ninjaFilename,
                                const QList<Utils::FileName> &fileNames,
                                QList<CMakeBuildTarget> &buildTargets)
{
    QElapsedTimer timer;
    timer.start();

    // attempt to open the 'build.ninja' file
    QFile ninjaFile(ninjaFilename);
    bool ok = ninjaFile.open(QFile::ReadOnly | QFile::Text);
    if (!ok)
        return false;
   
    // read all non-empty lines into a list of lines
    QList<QByteArray> ninjaLines;
    while (!ninjaFile.atEnd()) {
        QByteArray line = ninjaFile.readLine().trimmed();
        if (line.isEmpty())
            continue;
        ninjaLines.append(line);
    }
    if (ninjaLines.isEmpty())
        return false;
    
    QMap<Utils::FileName, QByteArray> fileToTarget;
    for(int i = 0; i < ninjaLines.size(); ++i) {
        QByteArray line = ninjaLines[i];

        // find the first comment line
        if (line.size() > 0 && line[0] != '#')
            continue;
        if (line.startsWith("# Make the all target"))
            continue;
        
        // find the name of the current target
        QByteArray currentTarget = targetFromNinjaLine(line);
        if (currentTarget.isEmpty ())
          continue;

        // go over the next lines, until we hit another comment block
        while (i+1 < ninjaLines.size() && !ninjaLines[i+1].startsWith(QByteArray("#")))
        {
            // each such line may contain references to the files
            // associated to the target 'currentTarget'
            QByteArray line = ninjaLines[++i];
            QList<Utils::FileName> files = filesFromNinjaLine(line, fileNames);
            foreach (Utils::FileName file, files) {
                fileToTarget[file] = currentTarget;
            }
        }
    }

    // make a lookup table 'target name' --> 'target index in buildTargets'
    QMap<QByteArray, int> targetLookup;
    int i = 0;
    foreach (CMakeBuildTarget const& buildTarget, buildTargets) {
        targetLookup[buildTarget.title.toLatin1 ()] = i;
        ++i;
    }

    // add files to targets
    foreach (const Utils::FileName &fileName, fileToTarget.keys ()) {
        QByteArray targetName = fileToTarget[fileName];
        buildTargets[targetLookup[targetName]].files.append(fileName.toString());
        // qDebug() << fileName.toString() << " -> target " << targetName;
    }

    qDebug() << "sorting files to targets via 'build.ninja' took"
             << (timer.elapsed()/1000.0) << " seconds";

    return true;
}

} // namespace

// called after everything is parsed
// this function tries to figure out to which CMakeBuildTarget
// each file belongs, so that it gets the appropriate defines and
// compiler flags
void CMakeCbpParser::sortFiles()
{
    QLoggingCategory log("qtc.cmakeprojectmanager.filetargetmapping");
    QList<FileName> fileNames = Utils::transform(m_fileList, [] (FileNode *node) {
        return node->path();
    });

    Utils::sort(fileNames);
    
    // Try to obtain the relevant information from a 'build.ninja' file
    // if present in the build directory.
    QString ninjaFile = QDir::cleanPath(m_buildDirectory + QDir::separator () + QLatin1String("build.ninja"));
    bool ok = sortFilesToTargetsViaNinja (ninjaFile, fileNames, m_buildTargets);
    if (ok)
      return;

    // Otherwise, use a heuristic to figure out the relation
    // filename <-> CMakeBuildTarget.

    CMakeBuildTarget *last = 0;
    FileName parentDirectory;

    qCDebug(log) << "###############";
    qCDebug(log) << "# Pre Dump    #";
    qCDebug(log) << "###############";
    foreach (const CMakeBuildTarget &target, m_buildTargets)
        qCDebug(log) << target.title << target.sourceDirectory <<
                 target.includeFiles << target.defines << target.files << "\n";

    // find a good build target to fall back
    int fallbackIndex = 0;
    {
        int bestIncludeCount = -1;
        for (int i = 0; i < m_buildTargets.size(); ++i) {
            const CMakeBuildTarget &target = m_buildTargets.at(i);
            if (target.includeFiles.isEmpty())
                continue;
            if (target.sourceDirectory == m_sourceDirectory
                    && target.includeFiles.count() > bestIncludeCount) {
                bestIncludeCount = target.includeFiles.count();
                fallbackIndex = i;
            }
        }
    }

    qCDebug(log) << "###############";
    qCDebug(log) << "# Sorting     #";
    qCDebug(log) << "###############";

    foreach (const FileName &fileName, fileNames) {
        qCDebug(log) << fileName;
        if (fileName.parentDir() == parentDirectory && last) {
            // easy case, same parent directory as last file
            last->files.append(fileName.toString());
            qCDebug(log) << "  into" << last->title;
        } else {
            int bestDistance = std::numeric_limits<int>::max();
            int bestIndex = -1;
            int bestIncludeCount = -1;

            for (int i = 0; i < m_buildTargets.size(); ++i) {
                const CMakeBuildTarget &target = m_buildTargets.at(i);
                if (target.includeFiles.isEmpty())
                    continue;
                int dist = distance(target.sourceDirectory, fileName);
                qCDebug(log) << "distance to target" << target.title << dist;
                if (dist < bestDistance ||
                     (dist == bestDistance &&
                      target.includeFiles.count() > bestIncludeCount)) {
                    bestDistance = dist;
                    bestIncludeCount = target.includeFiles.count();
                    bestIndex = i;
                }
            }

            if (bestIndex == -1 && !m_buildTargets.isEmpty()) {
                bestIndex = fallbackIndex;
                qCDebug(log) << "  using fallbackIndex";
            }

            if (bestIndex != -1) {
                m_buildTargets[bestIndex].files.append(fileName.toString());
                last = &m_buildTargets[bestIndex];
                parentDirectory = fileName.parentDir();
                qCDebug(log) << "  into" << last->title;
            }
        }
    }

    qCDebug(log) << "###############";
    qCDebug(log) << "# After Dump  #";
    qCDebug(log) << "###############";
    foreach (const CMakeBuildTarget &target, m_buildTargets)
        qCDebug(log) << target.title << target.sourceDirectory << target.includeFiles << target.defines << target.files << "\n";
}

bool CMakeCbpParser::parseCbpFile(const QString &fileName, const QString &sourceDirectory)
{
    m_buildDirectory = QFileInfo(fileName).absolutePath();
    m_sourceDirectory = sourceDirectory;

    QFile fi(fileName);
    if (fi.exists() && fi.open(QFile::ReadOnly)) {
        setDevice(&fi);

        while (!atEnd()) {
            readNext();
            if (name() == QLatin1String("CodeBlocks_project_file"))
                parseCodeBlocks_project_file();
            else if (isStartElement())
                parseUnknownElement();
        }

        sortFiles();

        fi.close();
        return true;
    }
    return false;
}

void CMakeCbpParser::parseCodeBlocks_project_file()
{
    while (!atEnd()) {
        readNext();
        if (isEndElement())
            return;
        else if (name() == QLatin1String("Project"))
            parseProject();
        else if (isStartElement())
            parseUnknownElement();
    }
}

void CMakeCbpParser::parseProject()
{
    while (!atEnd()) {
        readNext();
        if (isEndElement())
            return;
        else if (name() == QLatin1String("Option"))
            parseOption();
        else if (name() == QLatin1String("Unit"))
            parseUnit();
        else if (name() == QLatin1String("Build"))
            parseBuild();
        else if (isStartElement())
            parseUnknownElement();
    }
}

void CMakeCbpParser::parseBuild()
{
    while (!atEnd()) {
        readNext();
        if (isEndElement())
            return;
        else if (name() == QLatin1String("Target"))
            parseBuildTarget();
        else if (isStartElement())
            parseUnknownElement();
    }
}

void CMakeCbpParser::parseBuildTarget()
{
    m_buildTarget.clear();

    if (attributes().hasAttribute(QLatin1String("title")))
        m_buildTarget.title = attributes().value(QLatin1String("title")).toString();
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            if (!m_buildTarget.title.endsWith(QLatin1String("/fast")))
                m_buildTargets.append(m_buildTarget);
            return;
        } else if (name() == QLatin1String("Compiler")) {
            parseCompiler();
        } else if (name() == QLatin1String("Option")) {
            parseBuildTargetOption();
        } else if (name() == QLatin1String("MakeCommands")) {
            parseMakeCommands();
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseBuildTargetOption()
{
    if (attributes().hasAttribute(QLatin1String("output"))) {
        m_buildTarget.executable = attributes().value(QLatin1String("output")).toString();
    } else if (attributes().hasAttribute(QLatin1String("type"))) {
        const QStringRef value = attributes().value(QLatin1String("type"));
        if (value == QLatin1String("2") || value == QLatin1String("3"))
            m_buildTarget.targetType = TargetType(value.toInt());
    } else if (attributes().hasAttribute(QLatin1String("working_dir"))) {
        m_buildTarget.workingDirectory = attributes().value(QLatin1String("working_dir")).toString();

        QFile cmakeSourceInfoFile(m_buildTarget.workingDirectory
                                  + QStringLiteral("/CMakeFiles/CMakeDirectoryInformation.cmake"));
        if (cmakeSourceInfoFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&cmakeSourceInfoFile);
            const QLatin1String searchSource("SET(CMAKE_RELATIVE_PATH_TOP_SOURCE \"");
            while (!stream.atEnd()) {
                const QString lineTopSource = stream.readLine().trimmed();
                if (lineTopSource.startsWith(searchSource)) {
                    m_buildTarget.sourceDirectory = lineTopSource.mid(searchSource.size());
                    m_buildTarget.sourceDirectory.chop(2); // cut off ")
                    break;
                }
            }
        }

        if (m_buildTarget.sourceDirectory.isEmpty()) {
            QDir dir(m_buildDirectory);
            const QString relative = dir.relativeFilePath(m_buildTarget.workingDirectory);
            m_buildTarget.sourceDirectory
                    = FileName::fromString(m_sourceDirectory).appendPath(relative).toString();
        }
    }
    while (!atEnd()) {
        readNext();
        if (isEndElement())
            return;
        else if (isStartElement())
            parseUnknownElement();
    }
}

QString CMakeCbpParser::projectName() const
{
    return m_projectName;
}

void CMakeCbpParser::parseOption()
{
    if (attributes().hasAttribute(QLatin1String("title")))
        m_projectName = attributes().value(QLatin1String("title")).toString();

    if (attributes().hasAttribute(QLatin1String("compiler")))
        m_compiler = attributes().value(QLatin1String("compiler")).toString();

    while (!atEnd()) {
        readNext();
        if (isEndElement())
            return;
        else if (isStartElement())
            parseUnknownElement();
    }
}

void CMakeCbpParser::parseMakeCommands()
{
    while (!atEnd()) {
        readNext();
        if (isEndElement())
            return;
        else if (name() == QLatin1String("Build"))
            parseBuildTargetBuild();
        else if (name() == QLatin1String("Clean"))
            parseBuildTargetClean();
        else if (isStartElement())
            parseUnknownElement();
    }
}

void CMakeCbpParser::parseBuildTargetBuild()
{
    if (attributes().hasAttribute(QLatin1String("command")))
        m_buildTarget.makeCommand = attributes().value(QLatin1String("command")).toString();
    while (!atEnd()) {
        readNext();
        if (isEndElement())
            return;
        else if (isStartElement())
            parseUnknownElement();
    }
}

void CMakeCbpParser::parseBuildTargetClean()
{
    if (attributes().hasAttribute(QLatin1String("command")))
        m_buildTarget.makeCleanCommand = attributes().value(QLatin1String("command")).toString();
    while (!atEnd()) {
        readNext();
        if (isEndElement())
            return;
        else if (isStartElement())
            parseUnknownElement();
    }
}

void CMakeCbpParser::parseCompiler()
{
    while (!atEnd()) {
        readNext();
        if (isEndElement())
            return;
        else if (name() == QLatin1String("Add"))
            parseAdd();
        else if (isStartElement())
            parseUnknownElement();
    }
}

void CMakeCbpParser::parseAdd()
{
    // CMake only supports <Add option=\> and <Add directory=\>
    const QXmlStreamAttributes addAttributes = attributes();

    const QString includeDirectory = addAttributes.value(QLatin1String("directory")).toString();
    // allow adding multiple times because order happens
    if (!includeDirectory.isEmpty())
        m_buildTarget.includeFiles.append(includeDirectory);

    QString compilerOption = addAttributes.value(QLatin1String("option")).toString();
    // defining multiple times a macro to the same value makes no sense
    if (!compilerOption.isEmpty() && !m_buildTarget.compilerOptions.contains(compilerOption)) {
        m_buildTarget.compilerOptions.append(compilerOption);
        int macroNameIndex = compilerOption.indexOf(QLatin1String("-D")) + 2;
        if (macroNameIndex != 1) {
            int assignIndex = compilerOption.indexOf(QLatin1Char('='), macroNameIndex);
            if (assignIndex != -1)
                compilerOption[assignIndex] = ' ';
            m_buildTarget.defines.append("#define ");
            m_buildTarget.defines.append(compilerOption.mid(macroNameIndex).toUtf8());
            m_buildTarget.defines.append('\n');
        }
    }

    while (!atEnd()) {
        readNext();
        if (isEndElement())
            return;
        else if (isStartElement())
            parseUnknownElement();
    }
}

void CMakeCbpParser::parseUnit()
{
    //qDebug()<<stream.attributes().value("filename");
    FileName fileName =
            FileName::fromUserInput(attributes().value(QLatin1String("filename")).toString());
    m_parsingCmakeUnit = false;
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            if (!fileName.endsWith(QLatin1String(".rule")) && !m_processedUnits.contains(fileName)) {
                // Now check whether we found a virtual element beneath
                if (m_parsingCmakeUnit) {
                    m_cmakeFileList.append( new ProjectExplorer::FileNode(fileName, ProjectExplorer::ProjectFileType, false));
                } else {
                    bool generated = false;
                    QString onlyFileName = fileName.fileName();
                    if (   (onlyFileName.startsWith(QLatin1String("moc_")) && onlyFileName.endsWith(QLatin1String(".cxx")))
                        || (onlyFileName.startsWith(QLatin1String("ui_")) && onlyFileName.endsWith(QLatin1String(".h")))
                        || (onlyFileName.startsWith(QLatin1String("qrc_")) && onlyFileName.endsWith(QLatin1String(".cxx"))))
                        generated = true;

                    if (fileName.endsWith(QLatin1String(".qrc")))
                        m_fileList.append( new ProjectExplorer::FileNode(fileName, ProjectExplorer::ResourceType, generated));
                    else
                        m_fileList.append( new ProjectExplorer::FileNode(fileName, ProjectExplorer::SourceType, generated));
                }
                m_processedUnits.insert(fileName);
            }
            return;
        } else if (name() == QLatin1String("Option")) {
            parseUnitOption();
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseUnitOption()
{
    if (attributes().hasAttribute(QLatin1String("virtualFolder")))
        m_parsingCmakeUnit = true;

    while (!atEnd()) {
        readNext();

        if (isEndElement())
            break;

        if (isStartElement())
            parseUnknownElement();
    }
}

void CMakeCbpParser::parseUnknownElement()
{
    Q_ASSERT(isStartElement());

    while (!atEnd()) {
        readNext();

        if (isEndElement())
            break;

        if (isStartElement())
            parseUnknownElement();
    }
}

QList<ProjectExplorer::FileNode *> CMakeCbpParser::fileList()
{
    return m_fileList;
}

QList<ProjectExplorer::FileNode *> CMakeCbpParser::cmakeFileList()
{
    return m_cmakeFileList;
}

bool CMakeCbpParser::hasCMakeFiles()
{
    return !m_cmakeFileList.isEmpty();
}

QList<CMakeBuildTarget> CMakeCbpParser::buildTargets()
{
    return m_buildTargets;
}

QString CMakeCbpParser::compilerName() const
{
    return m_compiler;
}


} // namespace Internal
} // namespace CMakeProjectManager
