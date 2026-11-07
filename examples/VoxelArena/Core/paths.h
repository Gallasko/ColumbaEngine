#pragma once

#include <string>
#include <vector>

namespace arena
{
    /**
     * Where the mods are read from: the folders in order (the game's own first, then the player's),
     * and the file that says which mods are turned off ("": every mod is on).
     */
    struct ModSources
    {
        std::vector<std::string> dirs;
        std::string settings;
    };

    /**
     * Where the game keeps what it saves (options, spellbook, levels, the player's mods):
     * $XDG_DATA_HOME/voxelarena when it is set, else %APPDATA%/VoxelArena on Windows and
     * ~/.local/share/voxelarena elsewhere; ./voxelarena when none of them is known.
     */
    std::string userDataDir();

    /** The player's own mods, apart from the game's: <userDataDir>/mods. */
    std::string userModsDir();

    /** The mods the player turned off: <userDataDir>/mods.json, { "off": ["mymod"] }. */
    std::string modSettingsPath();

    /** The folder of the running program, "" when it cannot be told. */
    std::string executableDir();

    /**
     * A folder the game ships: the one asked for (a command line option), else the source tree's
     * (a build on the developer's machine, so an edited file reloads with F5), else local in the
     * working directory, else local next to the program. local itself when none exists.
     */
    std::string findGameDir(const std::string& asked, const char* sourceTree, const std::string& local);

    /**
     * The game's mods (found as findGameDir does), then the player's mods folder, made if it is
     * missing, and the player's mods.json.
     */
    ModSources playerModSources(const std::string& asked, const char* sourceTree);
}
