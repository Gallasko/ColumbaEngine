#ifndef APPLICATION_H
#define APPLICATION_H

#include "engine.h"
#include "editorstate.h"

#include <memory>

class GameApp
{
public:
    GameApp(const std::string& appName);
    ~GameApp();

    int exec();

private:
    pg::Engine engine;

    // Canvas lifetime is tied to the application; systems hold raw pointers.
    std::unique_ptr<pg::Canvas> canvas;
};

#endif
