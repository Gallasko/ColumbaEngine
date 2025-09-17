#include "application.h"

#include "logger.h"

#include "chunk.h"
#include "compiler_debug.h"

#include "vm.h"

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

    std::cout << "REPL not implemented yet." << std::endl;
}

void CompilerApp::runFile()
{
    LOG_THIS_MEMBER(DOM);

    std::cout << "File execution not implemented yet." << std::endl;
}