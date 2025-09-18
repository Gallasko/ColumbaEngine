#include "application.h"

#include "logger.h"

#include "chunk.h"
#include "compiler_debug.h"

#include "vm.h"
#include "compiler.h"

using namespace pg;

namespace {
    static const char *const DOM = "App";
}

CompilerApp::CompilerApp(const std::string &fileName) : fileName(fileName) {
    LOG_THIS_MEMBER(DOM);
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

    std::cout << "File execution not implemented yet." << std::endl;
}