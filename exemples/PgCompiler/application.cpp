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

    VM vm;

    Chunk chunk;

    chunk.addConstant(1.2, 123);
    chunk.addConstant(2.5, 123);

    // Todo Test for the passage from constant to long constant ( to maybe even overflow cIndex > 0xFFFFFF )
    // for (int i = 0; i < 300; ++i)
    //     chunk.addConstant(i, 123);

    chunk.addCode(OpCode::OP_Return, 123);

    disassembleChunk(chunk, "test chunk");

    vm.interpret(chunk);

    return 0;
}
