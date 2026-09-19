#include "stdafx.h"

#include "application.h"

#include "logger.h"

#include "ECS/entitysystem.h"

#include "Compiler/chunk.h"
#include "Compiler/compiler_debug.h"

#include "Compiler/vm.h"
#include "Compiler/compiler.h"

#include "Interpreter/lexer.h"

// This application intentionally mirrors examples/PgCompiler/application.cpp
// step by step - the only functional difference is the front-end selection
// (ScriptFrontEnd::Ast). Keeping the two in lockstep makes any behavior
// difference between them a front-end bug by construction.

using namespace pg;

namespace {
    static const char *const DOM = "AstApp";
}

static Value nativeLogInfo(VM* vm, int argCount, Value* args)
{
    if (argCount != 1) {
        throw std::runtime_error("logInfo expects exactly one argument");
    }

    if (IS_STRING(args[0]))
    {
        LOG_INFO("DOM", vm->asString(args[0]));
    }
    else if (IS_INT(args[0]))
    {
        LOG_INFO("DOM", AS_INT(args[0]));
    }
    else if (IS_BOOL(args[0]))
    {
        LOG_INFO("DOM", AS_BOOL(args[0]));
    }
    else if (IS_DOUBLE(args[0]))
    {
        LOG_INFO("DOM", AS_DOUBLE(args[0]));
    }
    else
    {
        LOG_INFO("DOM", "Unsupported type for logInfo");
    }

    return makeBoolValue(true);
}

InterpreterApp::InterpreterApp(const std::string &fileName, bool enableProfiling, int argc, char** argv)
    : fileName(fileName), profilingEnabled(enableProfiling), m_argc(argc), m_argv(argv)
{
    LOG_THIS_MEMBER(DOM);
}

void InterpreterApp::setLoggerSink()
{
    LOG_THIS_MEMBER(DOM);

    auto terminalSink = std::shared_ptr<pg::Logger::LogSink>(pg::Logger::registerSink<pg::TerminalSink>());

    auto* sink = dynamic_cast<pg::TerminalSink*>(terminalSink.get());
    if (sink)
    {
        sink->setVerboseInfo(false);  // Disable verbose info (domain, file, function)
    }

    terminalSink->addFilter("log", new pg::Logger::LogSink::FilterLogLevel(pg::Logger::InfoLevel::log));
    terminalSink->addFilter("mile", new pg::Logger::LogSink::FilterLogLevel(pg::Logger::InfoLevel::mile));
    terminalSink->addFilter("test", new pg::Logger::LogSink::FilterLogLevel(pg::Logger::InfoLevel::test));
    terminalSink->addFilter("warn", new pg::Logger::LogSink::FilterLogLevel(pg::Logger::InfoLevel::warning));

    bool needInfo = false;
    for (int i = 1; i < m_argc; i++)
    {
        std::string arg = m_argv[i];

        if (arg == "--info" or arg == "-i")
        {
            needInfo = true;
        }
        else if (arg == "--generated-bytecode" or arg == "-gb")
        {
            needGeneratedBytecodeOutput = true;
        }
        else if (arg == "--compile-only" or arg == "-c")
        {
            needCompileOnly = true;
        }
        else if (arg == "-O0")
        {
            optimizationLevel = VmOptimizationLevel::O0;
        }
        else if (arg == "-O1")
        {
            optimizationLevel = VmOptimizationLevel::O1;
        }
        else if (arg == "-O2")
        {
            optimizationLevel = VmOptimizationLevel::O2;
        }
        else if (arg == "-O3")
        {
            optimizationLevel = VmOptimizationLevel::O3;
        }
    }

    if (not needInfo)
        terminalSink->addFilter("info", new pg::Logger::LogSink::FilterLogLevel(pg::Logger::InfoLevel::info));
}

InterpreterApp::~InterpreterApp()
{
    LOG_THIS_MEMBER(DOM);
}

int InterpreterApp::exec()
{
    LOG_THIS_MEMBER(DOM);

    if (not fileName.empty())
    {
        if (fileName.find(".pgc") != std::string::npos)
            runFile(false);
        else
            runFile();
    }
    else
        runREPL();

    return 0;
}

void InterpreterApp::runREPL()
{
    LOG_THIS_MEMBER(DOM);

    std::string input;
    std::string line;
    int braceDepth = 0;

    EntitySystem ecs;

    ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

    setLoggerSink();

    std::unique_ptr<VM> vm(new VM());

    if (profilingEnabled)
    {
        vm->enableProfiling();
    }

    ecs.setupVm(*vm);

    // Register command-line argument access functions
    vm->defineNative("getArg", [this](VM* vmArg, int argCount, Value* args) -> Value {
        if (argCount != 1)
        {
            throw std::runtime_error("getArg expects exactly 1 argument (index)");
        }

        if (not IS_INT(args[0]))
        {
            throw std::runtime_error("getArg expects an integer argument");
        }

        int64_t index = AS_INT(args[0]);

        if (index < 0 or index >= m_argc)
        {
            return vmArg->createString("");
        }

        return vmArg->createString(m_argv[index]);
    });

    vm->defineNative("getArgCount", [this](VM*, int argCount, Value*) -> Value {
        if (argCount != 0)
        {
            throw std::runtime_error("getArgCount expects no arguments");
        }

        return makeIntValue(m_argc);
    });

    std::cout << "PgInterpreter REPL (AST front-end) - Enter 'exit' to quit\n";
    std::cout << "> ";

    while (std::getline(std::cin, line))
    {
        if (line == "exit" && input.empty())
        {
            break;
        }

        if (not input.empty())
        {
            input += "\n";
        }

        input += line;

        // Count braces to determine if we're in a block
        for (char c : line)
        {
            if (c == '{')
            {
                braceDepth++;
            }
            else if (c == '}')
            {
                braceDepth--;
            }
        }

        // Execute only when all blocks are closed and we have input
        if (braceDepth == 0 && not input.empty())
        {
            std::cout << "[=] Executing...\n";
            vm->interpretFromText(input);

            input.clear();
            std::cout << "> ";
        }
        else if (braceDepth > 0)
        {
            // Show continuation prompt based on nesting depth
            std::cout << ">";
            for (int i = 0; i < braceDepth; i++)
            {
                std::cout << ">";
            }
            std::cout << " ";
        }
        else
        {
            std::cout << "> ";
        }
    }

    std::cout << "Goodbye!\n";
}

void InterpreterApp::runFile(bool needCompile)
{
    LOG_THIS_MEMBER(DOM);

    setLoggerSink();

    EntitySystem ecs;

    ecs.setVMOptimizationLevel(optimizationLevel);
    ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

    std::unique_ptr<VM> vm(new VM());

    if (profilingEnabled)
    {
        vm->enableProfiling();
    }

    ecs.setupVm(*vm);

    vm->defineNative("log", nativeLogInfo);

    // Register command-line argument access functions
    vm->defineNative("getArg", [this](VM* vmArg, int argCount, Value* args) -> Value {
        if (argCount != 1)
        {
            throw std::runtime_error("getArg expects exactly 1 argument (index)");
        }

        if (not IS_INT(args[0]))
        {
            throw std::runtime_error("getArg expects an integer argument");
        }

        int64_t index = AS_INT(args[0]);

        if (index < 0 or index >= m_argc)
        {
            return vmArg->createString("");
        }

        return vmArg->createString(m_argv[index]);
    });

    vm->defineNative("getArgCount", [this](VM*, int argCount, Value*) -> Value {
        if (argCount != 0)
        {
            throw std::runtime_error("getArgCount expects no arguments");
        }

        return makeIntValue(m_argc);
    });

    InterpretResult result;

    if (needCompile)
    {
        if (optimizationLevel != VmOptimizationLevel::O0)
        {
            vm->enableBytecodeOptimization();

            if (needGeneratedBytecodeOutput)
                vm->enableOptimizationDebugging();
        }

        std::cout << sizeof(Value) << " bytes per Value on this platform." << std::endl;
        std::cout << "Front-end: Ast (AstCompiler)" << std::endl;

        Lexer lexer;

        try
        {
            lexer.readFromFile(fileName);
        }
        catch(const std::exception& e)
        {
            LOG_ERROR(DOM, "Failed to read file '" << fileName << "': " << e.what());
            return;
        }

        auto tokens = lexer.getTokens();

        if (optimizationLevel != VmOptimizationLevel::O0)
        {
            vm->listOptimizationPasses();
        }

        vm->currentFileName = fileName;

        result = vm->interpret(tokens, needCompileOnly, "temp_ast.pgc");
    }
    else
    {
        // .pgc bytecode does not go through a front-end at all; identical to PgCompiler
        result = vm->interpretFromBytecodeFile(fileName);
    }

    if (profilingEnabled)
    {
        vm->printProfilingReport();
        vm->printProfilingBytecodeReport();
        vm->printAllFunctionsBytecodeWithPerformance();
    }

    switch (result)
    {
        case InterpretResult::OK:
            LOG_INFO(DOM, "File executed successfully");

            std::cout << vm->testOutput << std::endl;

            break;
        case InterpretResult::COMPILE_ERROR:
            LOG_ERROR(DOM, "Compile error occurred");
            break;
        case InterpretResult::RUNTIME_ERROR:
            LOG_ERROR(DOM, "Runtime error occurred");
            break;
    }
}
