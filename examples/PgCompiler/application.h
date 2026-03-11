#ifndef APPLICATION_H
#define APPLICATION_H

#include "ECS/entitysystem.h"

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
    bool needCompileOnly = false;
    bool needGeneratedBytecodeOutput = false;
    pg::VmOptimizationLevel optimizationLevel = pg::VmOptimizationLevel::O3;
    int m_argc;
    char** m_argv;
};

#endif