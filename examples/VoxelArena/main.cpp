#include <cctype>
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
        else if (a == "--name" and i + 1 < argc)
            options.name = argv[++i];
        else if (a == "--host")
        {
            options.net = LaunchOptions::Net::Host;

            if (i + 1 < argc and std::isdigit(static_cast<unsigned char>(argv[i + 1][0])))
                options.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        }
        else if (a == "--join" and i + 1 < argc)
        {
            options.net = LaunchOptions::Net::Join;
            options.address = argv[++i];

            // address:port
            if (const auto colon = options.address.rfind(':'); colon != std::string::npos)
            {
                options.port = static_cast<uint16_t>(std::stoi(options.address.substr(colon + 1)));
                options.address = options.address.substr(0, colon);
            }
        }
    }

    // Several on one desktop (a host and who joined it) tell their windows apart by the title
    std::string title = "Voxel Arena";

    if (options.net == LaunchOptions::Net::Host)
        title += " (host)";
    else if (options.net == LaunchOptions::Net::Join)
        title += " (" + options.name + ")";

    GameApp app(title, options);

    return app.exec();
}
