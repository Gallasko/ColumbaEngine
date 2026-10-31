#pragma once

#include <string>
#include <vector>

#include "protocol.h"

namespace arena::net
{
    /** What a client accepts from a server: more than this and the mods are refused. */
    constexpr size_t MAX_MOD_FILES = 1000;
    constexpr size_t MAX_MOD_BYTES = 8 * 1024 * 1024;

    /** The game's mod files (.json and .pg) under dir, with their paths relative to it. */
    std::vector<ModFile> readModFiles(const std::string& dir);

    /**
     * A path a server may write to: relative, inside the folder, a .json or .pg file, plain characters
     * only, and no name Windows keeps for itself. What comes from a server is never trusted.
     */
    bool safeModPath(const std::string& path);

    /**
     * Replaces the contents of dir with these files. Everything is checked first: nothing is touched
     * when a file is refused (error says why).
     */
    bool writeModFiles(const std::vector<ModFile>& files, const std::string& dir, std::string& error);
}
