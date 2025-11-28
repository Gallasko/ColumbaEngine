#pragma once

#include "../bytecode_pass.h"
#include "../chunk.h"

namespace pg
{
    class BasicOperatorLocalIndexingPass : public BytecodePass
    {
    public:
        std::string getName() const override { return "BasicOperatorLocalIndexing"; }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) override;

        bool changesSize() const override { return true; }

        bool requiresMultiplePasses() const override { return false; }
    };
}
