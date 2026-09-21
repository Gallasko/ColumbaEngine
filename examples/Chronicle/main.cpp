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
        else
        {
            std::fprintf(stderr, "usage: Chronicle [--dev <Scene>] [--theme day|candle] [--reduced-motion]\n");
            return 2;
        }
    }

    static chronicle::GameApp app("Chronicle", opt);

    return app.exec();
}
