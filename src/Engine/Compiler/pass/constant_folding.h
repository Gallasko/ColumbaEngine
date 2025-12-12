#pragma once

#include "../bytecode_pass.h"
#include "../chunk.h"

#include <vector>

#include "logger.h"



namespace pg
{
    class ConstantFoldingPass : public BytecodePass
    {
    public:
        std::string getName() const override { return "ConstantFoldingPass"; }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr);

        bool changesSize() const override { return true; }

        bool requiresMultiplePasses() const override { return true; }
    };
}
