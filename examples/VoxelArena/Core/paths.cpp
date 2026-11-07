#include "paths.h"

#include <cstdlib>
#include <filesystem>
#include <iterator>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
#endif

namespace fs = std::filesystem;

namespace arena
{
    namespace
    {
        std::string env(const char* name)
        {
            const char* value = std::getenv(name);
            return value ? value : "";
        }
    }

    std::string userDataDir()
    {
        if (const auto xdg = env("XDG_DATA_HOME"); not xdg.empty())
            return xdg + "/voxelarena";

#ifdef _WIN32
        if (const auto appData = env("APPDATA"); not appData.empty())
            return appData + "/VoxelArena";
#endif

        if (const auto home = env("HOME"); not home.empty())
            return home + "/.local/share/voxelarena";

        return "./voxelarena";
    }

    std::string userModsDir()
    {
        return userDataDir() + "/mods";
    }

    std::string modSettingsPath()
    {
        return userDataDir() + "/mods.json";
    }

    std::string executableDir()
    {
        std::error_code ec;

#if defined(_WIN32)
        wchar_t buffer[MAX_PATH * 4];
        const DWORD size = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));

        if (size == 0 or size >= std::size(buffer))
            return "";

        return fs::path(std::wstring(buffer, size)).parent_path().string();
#elif defined(__linux__)
        const fs::path exe = fs::read_symlink("/proc/self/exe", ec);
        return ec ? "" : exe.parent_path().string();
#else
        return "";
#endif
    }

    std::string findGameDir(const std::string& asked, const char* sourceTree, const std::string& local)
    {
        if (not asked.empty())
            return asked;

        std::error_code ec;

        if (sourceTree and fs::is_directory(sourceTree, ec))
            return sourceTree;

        if (fs::is_directory(local, ec))
            return local;

        if (const auto exe = executableDir(); not exe.empty() and fs::is_directory(fs::path(exe) / local, ec))
            return (fs::path(exe) / local).string();

        return local;
    }

    ModSources playerModSources(const std::string& asked, const char* sourceTree)
    {
        ModSources sources;
        sources.dirs.push_back(findGameDir(asked, sourceTree, "mods"));

        // Made at once, so a player looking for where mods go finds the folder there
        std::error_code ec;
        const std::string own = userModsDir();
        fs::create_directories(own, ec);

        if (fs::is_directory(own, ec))
            sources.dirs.push_back(own);

        sources.settings = modSettingsPath();
        return sources;
    }
}
