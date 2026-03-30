#ifndef APPLICATION_H
#define APPLICATION_H

#include "window.h"

class TetrisApp
{
public:
    TetrisApp(const std::string& appName);
    ~TetrisApp();

    int exec();

private:
    std::string appName;
};

#endif