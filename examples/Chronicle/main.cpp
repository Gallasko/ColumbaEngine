#include <cstdio>
#include <cstdlib>
#include <ios>
#include <string>

#include "application.h"
#include "Core/motion.h"
#include "Core/bugsend.h"

int main(int argc, char* argv[])
{
    std::ios_base::sync_with_stdio(false);

    chronicle::LaunchOptions opt;

#ifdef __EMSCRIPTEN__
    // A browser gives no command line: a player with no save begins a life of his own
    opt.freshWithoutSave = true;
#endif

    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "--dev" and i + 1 < argc)
            opt.devScene = argv[++i];
        else if (a == "--theme" and i + 1 < argc)
            opt.theme = argv[++i];
        else if (a == "--reduced-motion")
            chronicle::Motion::setReduced(true);
        else if (a == "--fresh")
            opt.fresh = true;
        else if (a == "--guide" and i + 1 < argc and std::sscanf(argv[i + 1], "%d", &opt.guide) == 1 and opt.guide >= 0)
            ++i;
        else if (a == "--age" and i + 1 < argc and std::sscanf(argv[i + 1], "%f", &opt.age) == 1 and opt.age >= 7.0f)
            ++i;
        else if (a == "--no-save")
            opt.noSave = true;
        else if (a == "--save" and i + 1 < argc)
        {
            opt.savePath = argv[++i];
            opt.saveAsked = true;
        }
        else if (a == "--size" and i + 1 < argc and std::sscanf(argv[i + 1], "%dx%d", &opt.width, &opt.height) == 2 and opt.width > 0 and opt.height > 0)
            ++i;
        else if (a == "--month-ms" and i + 1 < argc and std::sscanf(argv[i + 1], "%f", &opt.monthMs) == 1 and opt.monthMs > 0.0f)
            ++i;
        else if (a == "--walk" and i + 1 < argc)
            opt.walk = argv[++i];
        else if (a == "--walk-seed" and i + 1 < argc and std::sscanf(argv[i + 1], "%d", &opt.walkSeed) == 1 and opt.walkSeed > 0)
            ++i;
        else if (a == "--bug-url" and i + 1 < argc)
            chronicle::setBugUrl(argv[++i]);
        else if (a == "--build" and i + 1 < argc)
            opt.build = argv[++i];
        else if (a == "--set" and i + 1 < argc and std::string(argv[i + 1]).find('=') != std::string::npos)
        {
            const std::string set = argv[++i];
            const auto eq = set.find('=');

            opt.sets.emplace_back(set.substr(0, eq), std::atoi(set.c_str() + eq + 1));
        }
        else
        {
            std::fprintf(stderr, "usage: Chronicle [--dev <Scene>] [--theme day|candle] [--reduced-motion] [--fresh] [--guide N] [--age N] [--no-save] [--save <path>] [--size WxH] [--month-ms N] [--walk <zone>] [--walk-seed N] [--bug-url <url>] [--build <id>|list] [--set <stat>=<n>]...\n");
            return 2;
        }
    }

    static chronicle::GameApp app("Chronicle", opt);

    return app.exec();
}
