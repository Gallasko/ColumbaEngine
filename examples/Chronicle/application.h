#ifndef CHRONICLE_APPLICATION_H
#define CHRONICLE_APPLICATION_H

#include <string>

#include "engine.h"

namespace chronicle
{
    struct LaunchOptions
    {
        std::string devScene;          // "" = the Life scene
        std::string theme = "day";
        bool fresh = false;            // --fresh: a new life at 7
        bool noSave = false;           // --no-save: the mockup's life, never written
        std::string savePath = "save/chronicle/life.sz";
        int width = 1320;              // --size WxH: the window at launch
        int height = 1020;
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
