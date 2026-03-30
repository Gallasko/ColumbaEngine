#include "stdafx.h"

#include "application.h"

#include "logger.h"

#include "ECS/entitysystem.h"

#include "ECS/loggersystem.h"

#include "Compiler/chunk.h"
#include "Compiler/compiler_debug.h"

#include "Compiler/vm.h"
#include "Compiler/compiler.h"

#include "constant_uniformity_pass.h"
#include "Interpreter/lexer.h"

#include "Compiler/pass/long_jump_optimization_pass.h"
#include "Compiler/pass/basic_operator_local_indexing.h"

#include "Compiler/pass/remove_def_get_global_redunduncy.h"

#include "Compiler/pass/constant_var_access.h"
#include "Compiler/pass/fuse_op_pop.h"
#include "Compiler/pass/constant_folding.h"
#include "Compiler/pass/increment_optimization_pass.h"
#include "Compiler/pass/simplify_constant_pass.h"

#include "Helpers/mathmodule.h"
#include "Helpers/randommodule.h"
#include "Helpers/stringmodule.h"

using namespace pg;

namespace {
    static const char *const DOM = "App";
}

Value nativeLogInfo(VM* vm, int argCount, Value* args)
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

CompilerApp::CompilerApp(const std::string &fileName, bool enableProfiling, int argc, char** argv)
    : fileName(fileName), profilingEnabled(enableProfiling), m_argc(argc), m_argv(argv)
{
    LOG_THIS_MEMBER(DOM);
}

void CompilerApp::setLoggerSink()
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

CompilerApp::~CompilerApp()
{
    LOG_THIS_MEMBER(DOM);
}

int CompilerApp::exec()
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

void CompilerApp::runREPL()
{
    LOG_THIS_MEMBER(DOM);

    std::string input;
    std::string line;
    int braceDepth = 0;

    EntitySystem ecs;
    // ecs.setVMOptimizationLevel(VmOptimizationLevel::O0);

    setLoggerSink();

    std::unique_ptr<VM> vm(new VM());

    if (profilingEnabled)
    {
        vm->enableProfiling();
    }

    ecs.setupVm(*vm);
    // vm->addOptimizationPass(std::make_unique<ConstantUniformityPass>());
    // vm->addOptimizationPass(std::make_unique<LongJumpOptimizationPass>());
    // vm->addOptimizationPass(std::make_unique<RemoveDefGetGlobalRedunduncy>());

    // vm->enableBytecodeOptimization();
    // vm->enableOptimizationDebugging();

    // vm->addNativeModule("math", MathModule());

    // Register command-line argument access functions
    vm->defineNative("getArg", [this](VM* vm, int argCount, Value* args) -> Value {
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
            return vm->createString("");
        }

        return vm->createString(m_argv[index]);
    });

    vm->defineNative("getArgCount", [this](VM*, int argCount, Value*) -> Value {
        if (argCount != 0)
        {
            throw std::runtime_error("getArgCount expects no arguments");
        }

        return makeIntValue(m_argc);
    });

    std::cout << "PgCompiler REPL - Enter 'exit' to quit\n";
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

void CompilerApp::runFile(bool needCompile)
{
    LOG_THIS_MEMBER(DOM);

    setLoggerSink();

    EntitySystem ecs;

    ecs.setVMOptimizationLevel(optimizationLevel);

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
        // vm->addOptimizationPass(std::make_uniqueh

        if (optimizationLevel != VmOptimizationLevel::O0)
        {
            vm->enableBytecodeOptimization();

            if (needGeneratedBytecodeOutput)
                vm->enableOptimizationDebugging();
        }

        // vm->disableBytecodeOptimization();

        std::cout << sizeof(Value) << " bytes per Value on this platform." << std::endl;

        // Register individual native functions

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

        result = vm->interpret(tokens, needCompileOnly, "temp.pgc");
    }
    else
    {
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

            // LOG_INFO(DOM, "Results: " << vm->testOutput);
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