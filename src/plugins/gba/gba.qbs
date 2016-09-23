import qbs 1.0

TiledPlugin {
    cpp.defines: ["GBA_LIBRARY"]

    files: [
        "gbaplugin_global.h",
        "gbaplugin.cpp",
        "gbaplugin.h",
        "plugin.json",
    ]
}
