#ifndef APPLICATION_H
#define APPLICATION_H

#include "engine.h"

class EditorApp
{
public:
    EditorApp(const std::string& appName);
    ~EditorApp();

    int exec();

private:
    pg::Engine engine;
};

#endif