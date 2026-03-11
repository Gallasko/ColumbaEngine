#ifndef APPLICATION_H
#define APPLICATION_H

#include "ECS/entitysystem.h"
#include "ECS/loggersystem.h"
#include "ECS/ecsmodule.h"

class InterpreterApp
{
public:
    InterpreterApp(const std::string& fileName);
    ~InterpreterApp();

    int exec();

private:
    std::string fileName;
};

#endif