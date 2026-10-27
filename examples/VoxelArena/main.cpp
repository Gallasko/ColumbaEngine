#include <filesystem>
#include <iostream>
#include <string>

#include "application.h"

int main(int argc, char* argv[])
{
    // Decouple C++ and C stream for faster runtime
    std::ios_base::sync_with_stdio(false);

    // The engine reads res/ and shader/ from the working directory, like every game here run from the
    // repository root (./release/VoxelArena). Started from anywhere else, move there.
#ifdef ENGINE_SOURCE_DIR
    {
        std::error_code ec;

        if (not std::filesystem::exists("res/setupRenderer.pg", ec) and std::filesystem::exists(std::string(ENGINE_SOURCE_DIR) + "/res/setupRenderer.pg", ec))
        {
            std::filesystem::current_path(ENGINE_SOURCE_DIR, ec);
            std::cout << "Voxel Arena: running from " << ENGINE_SOURCE_DIR << " (for res/ and shader/)" << std::endl;
        }
    }
#endif

    LaunchOptions options;

    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];

        if (a == "--mods" and i + 1 < argc)
            options.modsDir = argv[++i];
        else if (a == "--exec" and i + 1 < argc)
            options.startupCommands.push_back(argv[++i]);
    }

    GameApp app("Voxel Arena", options);

    return app.exec();
}
