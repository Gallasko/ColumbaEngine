#include <cstdio>
#include <ios>
#include <string>

#include "application.h"
#include "Core/motion.h"

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
        else if (a == "--no-save")
            opt.noSave = true;
        else if (a == "--save" and i + 1 < argc)
            opt.savePath = argv[++i];
        else if (a == "--size" and i + 1 < argc and std::sscanf(argv[i + 1], "%dx%d", &opt.width, &opt.height) == 2 and opt.width > 0 and opt.height > 0)
            ++i;
        else if (a == "--month-ms" and i + 1 < argc and std::sscanf(argv[i + 1], "%f", &opt.monthMs) == 1 and opt.monthMs > 0.0f)
            ++i;
        else
        {
            std::fprintf(stderr, "usage: Chronicle [--dev <Scene>] [--theme day|candle] [--reduced-motion] [--fresh] [--guide N] [--no-save] [--save <path>] [--size WxH] [--month-ms N]\n");
            return 2;
        }
    }

    static chronicle::GameApp app("Chronicle", opt);

    return app.exec();
}
