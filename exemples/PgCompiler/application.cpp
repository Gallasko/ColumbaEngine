#include "application.h"

#include "logger.h"

#include "chunk.h"
#include "compiler_debug.h"

#include "vm.h"
#include "compiler.h"

#include "long_jump_optimization_pass.h"
#include "constant_uniformity_pass.h"
#include "constant_propagation_pass.h"
#include "Interpreter/lexer.h"

using namespace pg;

namespace {
    static const char *const DOM = "App";
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
        runFile();
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
    vm.addOptimizationPass(std::make_unique<ConstantPropagationPass>());
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

void CompilerApp::runFile()
{
    LOG_THIS_MEMBER(DOM);

    VM vm;
    vm.addOptimizationPass(std::make_unique<ConstantUniformityPass>());
    vm.addOptimizationPass(std::make_unique<ConstantPropagationPass>());
    vm.addOptimizationPass(std::make_unique<LongJumpOptimizationPass>());

    vm.enableBytecodeOptimization();
    vm.enableOptimizationDebugging();
    
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
    
    InterpretResult result = vm.interpret(tokens);
    
    switch (result)
    {
        case InterpretResult::OK:
            LOG_INFO(DOM, "File executed successfully");
            break;
        case InterpretResult::COMPILE_ERROR:
            LOG_ERROR(DOM, "Compile error occurred");
            break;
        case InterpretResult::RUNTIME_ERROR:
            LOG_ERROR(DOM, "Runtime error occurred");
            break;
    }
}