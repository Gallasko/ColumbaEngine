#include "basic_operator_local_indexing.h"

#include "logger.h"

#include <vector>

namespace pg
{
    namespace
    {
        static const char* const DOM = "BasicOperatorLocalIndexingPass";
    }

    bool BasicOperatorLocalIndexingPass::runPass(Chunk& chunk, BytecodeRewriter* rewriter)
    {
        if (not rewriter)
        {
            LOG_ERROR(DOM, "No rewriter provided");

            return false;
        }

        std::vector<PatternElement> pattern = {
            PatternElement::match(OpCode::OP_Get_Local, true),
            PatternElement::match(OpCode::OP_Get_Local, true),
            PatternElement::match(OpCode::OP_Add),
        };

        rewriter->addAdvancedRule(pattern, [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
            // Extract local indices from captured Get_Local instructions
            uint8_t localIndex1 = captured[0].operands[0];
            uint8_t localIndex2 = captured[1].operands[0];

            // Create new bytecode sequence for optimized addition
            std::vector<uint8_t> newBytecode;

            // OP_Add
            newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_AddLL));
            newBytecode.push_back(localIndex1);
            newBytecode.push_back(localIndex2);

            return newBytecode;
        });

        return rewriter->rewrite(chunk);
    }
}