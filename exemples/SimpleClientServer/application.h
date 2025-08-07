#ifndef APPLICATION_H
#define APPLICATION_H

#include "window.h"

class GameApp
{
public:
    GameApp(const std::string& appName);
    ~GameApp();

    int exec(int argc, char** argv);

private:
    std::string appName;
};

#endif