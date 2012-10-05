import qbs.base 1.0

import "../QtcPlugin.qbs" as QtcPlugin

QtcPlugin {
    name: "QbsProjectManager"

    Depends { name: "Qt.widgets" }
    Depends { name: "Core" }
    Depends { name: "ProjectExplorer" }
    Depends { name: "CppTools" }
    Depends { name: "CPlusPlus" }
    Depends { name: "TextEditor" }
    Depends { name: "Locator" }
    Depends { name: "QtSupport" }

    Depends { name: "cpp" }
    cpp.includePaths: [
        ".",
        "..",
        "../../libs",
        buildDirectory
    ]

    files: [
        "QbsProjectManager.mimetypes.xml",
        "qbsprojectmanagerplugin.cpp",
        "qbsprojectmanagerplugin.h"
    ]
}

