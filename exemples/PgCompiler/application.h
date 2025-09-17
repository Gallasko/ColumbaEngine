#ifndef APPLICATION_H
#define APPLICATION_H

#include "chunk.h"

#include "ECS/entitysystem.h"
#include "ECS/loggersystem.h"
#include "ECS/ecsmodule.h"

class CompilerApp
{
public:
    CompilerApp(const std::string& fileName);
    ~CompilerApp();

    int exec();

private:
    std::string fileName;
};

#endif