#pragma once

#include "bytecode_pass.h"
#include "chunk.h"
#include <vector>
#include <cstdint>

namespace pg {

    class LoopSimplificationPass : public BytecodePass {
    public:
        std::string getName() const override { return "LoopSimplification"; }
        
        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) override;
        
        bool changesSize() const override { return true; }
        
        bool requiresMultiplePasses() const override { return false; }
        
    private:
        void setupSimpleCountingLoopPattern(BytecodeRewriter* rewriter);
    };

}