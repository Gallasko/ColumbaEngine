#ifndef APPLICATION_H
#define APPLICATION_H

#include "Compiler/chunk.h"

#include "ECS/entitysystem.h"
#include "ECS/loggersystem.h"
#include "ECS/ecsmodule.h"

class CompilerApp
{
public:
    CompilerApp(const std::string& fileName, bool enableProfiling = false, int argc = 0, char** argv = nullptr);
    ~CompilerApp();

    void setLoggerSink();

    int exec();

    void runREPL();
    void runFile(bool needCompile = true);

private:
    std::string fileName;
    bool profilingEnabled;
    int m_argc;
    char** m_argv;
};

#endif