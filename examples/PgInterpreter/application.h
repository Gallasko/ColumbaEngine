#ifndef APPLICATION_H
#define APPLICATION_H

#include "ECS/entitysystem.h"

/**
 * PgInterpreter application: mirrors PgCompiler's CompilerApp (same flags,
 * same REPL, same file runner) but compiles every script through the AST
 * front-end (ScriptFrontEnd::Ast) instead of the Pratt compiler.
 */
class InterpreterApp
{
public:
    InterpreterApp(const std::string& fileName, bool enableProfiling = false, int argc = 0, char** argv = nullptr);
    ~InterpreterApp();

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
