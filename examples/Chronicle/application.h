#ifndef CHRONICLE_APPLICATION_H
#define CHRONICLE_APPLICATION_H

#include <string>

#include "engine.h"

namespace chronicle
{
    struct LaunchOptions
    {
        std::string devScene;
        std::string theme = "day";
    };

    class GameApp
    {
    public:
        GameApp(const std::string& appName, const LaunchOptions& options);
        ~GameApp();

        int exec();

    private:
        pg::Engine engine;
        LaunchOptions opt;
    };
}

#endif
