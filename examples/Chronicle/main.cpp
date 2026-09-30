#include <cstdio>
#include <ios>
#include <string>

#include "application.h"
#include "Core/motion.h"

int main(int argc, char* argv[])
{
    std::ios_base::sync_with_stdio(false);

    chronicle::LaunchOptions opt;

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
        else if (a == "--no-save")
            opt.noSave = true;
        else if (a == "--save" and i + 1 < argc)
            opt.savePath = argv[++i];
        else if (a == "--size" and i + 1 < argc and std::sscanf(argv[i + 1], "%dx%d", &opt.width, &opt.height) == 2 and opt.width > 0 and opt.height > 0)
            ++i;
        else
        {
            std::fprintf(stderr, "usage: Chronicle [--dev <Scene>] [--theme day|candle] [--reduced-motion] [--fresh] [--no-save] [--save <path>] [--size WxH]\n");
            return 2;
        }
    }

    static chronicle::GameApp app("Chronicle", opt);

    return app.exec();
}
