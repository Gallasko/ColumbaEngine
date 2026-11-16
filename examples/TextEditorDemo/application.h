#ifndef APPLICATION_H
#define APPLICATION_H

#include "engine.h"

// A window holding one text editor, to try the widget by hand
class GameApp
{
public:
    GameApp(const std::string& appName);
    ~GameApp();

    int exec();

private:
    pg::Engine engine;
};

#endif
