#include "application.h"

#include "logger.h"

#include "chunk.h"
#include "compiler_debug.h"

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

    Chunk chunk;
    chunk.addCode(OpCode::OP_Return);

    uint8_t constant = chunk.addConstant(1.2);
    chunk.addCode(OpCode::OP_Constant);
    chunk.addCode(constant);

    disassembleChunk(chunk, "test chunk");

    return 0;
}
