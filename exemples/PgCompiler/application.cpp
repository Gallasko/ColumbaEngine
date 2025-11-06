#include "application.h"

#include "logger.h"

#include "chunk.h"
#include "compiler_debug.h"

#include "vm.h"
#include "compiler.h"

#include "long_jump_optimization_pass.h"
#include "constant_uniformity_pass.h"
#include "Interpreter/lexer.h"

#include "pass/basic_operator_local_indexing.h"

using namespace pg;

namespace {
    static const char *const DOM = "App";
}

Value nativeLogInfo(VM* vm, int argCount, Value* args) {
    if (argCount != 1) {
        throw std::runtime_error("logInfo expects exactly one argument");
    }

    if (IS_STRING(args[0]))
    {
        LOG_INFO("DOM", *vm->asString(args[0]));
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

CompilerApp::CompilerApp(const std::string &fileName) : fileName(fileName) {
    LOG_THIS_MEMBER(DOM);

    auto terminalSink = std::shared_ptr<pg::Logger::LogSink>(pg::Logger::registerSink<pg::TerminalSink>());

    terminalSink->addFilter("log", new pg::Logger::LogSink::FilterLogLevel(pg::Logger::InfoLevel::log));
    // terminalSink->addFilter("info", new pg::Logger::LogSink::FilterLogLevel(pg::Logger::InfoLevel::info));
    terminalSink->addFilter("mile", new pg::Logger::LogSink::FilterLogLevel(pg::Logger::InfoLevel::mile));
    terminalSink->addFilter("test", new pg::Logger::LogSink::FilterLogLevel(pg::Logger::InfoLevel::test));
    terminalSink->addFilter("warn", new pg::Logger::LogSink::FilterLogLevel(pg::Logger::InfoLevel::warning));
}

CompilerApp::~CompilerApp() {
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

    std::cout << "PgCompiler REPL - Enter 'exit' to quit\n";
    std::cout << "> ";

    std::string input;
    std::string line;

    VM vm;
    vm.addOptimizationPass(std::make_unique<ConstantUniformityPass>());
    vm.addOptimizationPass(std::make_unique<LongJumpOptimizationPass>());

    vm.enableBytecodeOptimization();
    vm.enableOptimizationDebugging();

    while (std::getline(std::cin, line))
    {
        if (line == "exit")
        {
            break;
        }

        if (!input.empty()) {
            input += "\n";
        }
        input += line;

        // Check if we have a complete statement (simple heuristic)
        // For now, we'll execute after each line, but you can modify this
        // to wait for specific terminators or empty lines
        if (!line.empty()) {
            // Here you would compile and execute the input
            std::cout << "Compiling: " << input << std::endl;
            vm.listOptimizationPasses();
            vm.interpretFromText(input);

            input.clear(); // Reset for next input
        }

        std::cout << "> ";
    }

    std::cout << "Goodbye!\n";
}

void CompilerApp::runFile(bool needCompile)
{
    LOG_THIS_MEMBER(DOM);

    VM vm;
    InterpretResult result;

    if (needCompile)
    {
        // vm.addOptimizationPass(std::make_uniqueh
        vm.addOptimizationPass(std::make_unique<BasicOperatorLocalIndexingPass>());
        vm.addOptimizationPass(std::make_unique<LongJumpOptimizationPass>());

        // vm.enableBytecodeOptimization();
        // vm.enableOptimizationDebugging();

        vm.disableBytecodeOptimization();

        std::cout << sizeof(Value) << " bytes per Value on this platform." << std::endl;

        vm.defineNative("logInfo", nativeLogInfo);

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

        vm.listOptimizationPasses();


        vm.currentFileName = fileName;
        result = vm.interpret(tokens, false, "temp.pgc");
    }
    else
    {
        result = vm.interpretFromBytecodeFile(fileName);
    }

    switch (result)
    {
        case InterpretResult::OK:
            LOG_INFO(DOM, "File executed successfully");

            // LOG_INFO(DOM, "Results: " << vm.testOutput);
            std::cout << vm.testOutput << std::endl;

            break;
        case InterpretResult::COMPILE_ERROR:
            LOG_ERROR(DOM, "Compile error occurred");
            break;
        case InterpretResult::RUNTIME_ERROR:
            LOG_ERROR(DOM, "Runtime error occurred");
            break;
    }
}