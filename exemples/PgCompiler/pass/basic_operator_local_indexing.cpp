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

        std::vector<PatternElement> patternSubLL = {
            PatternElement::match(OpCode::OP_Get_Local, true),
            PatternElement::match(OpCode::OP_Get_Local, true),
            PatternElement::match(OpCode::OP_Subtract),
        };

        rewriter->addAdvancedRule(patternSubLL, [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
            uint8_t localIndex1 = captured[0].operands[0];
            uint8_t localIndex2 = captured[1].operands[0];

            std::vector<uint8_t> newBytecode;

            newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_SubtractLL));
            newBytecode.push_back(localIndex1);
            newBytecode.push_back(localIndex2);

            return newBytecode;
        });

        std::vector<PatternElement> patternSubLC = {
            PatternElement::match(OpCode::OP_Get_Local, true),
            PatternElement::match(OpCode::OP_Constant, true),
            PatternElement::match(OpCode::OP_Subtract),
        };

        rewriter->addAdvancedRule(patternSubLC, [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
            uint8_t localIndex = captured[0].operands[0];
            uint8_t constantIndex = captured[1].operands[0];

            std::vector<uint8_t> newBytecode;

            newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_SubtractLC));
            newBytecode.push_back(localIndex);
            newBytecode.push_back(constantIndex);

            return newBytecode;
        });

        std::vector<PatternElement> patternSubCL = {
            PatternElement::match(OpCode::OP_Constant, true),
            PatternElement::match(OpCode::OP_Get_Local, true),
            PatternElement::match(OpCode::OP_Subtract),
        };

        rewriter->addAdvancedRule(patternSubCL, [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
            uint8_t constantIndex = captured[0].operands[0];
            uint8_t localIndex = captured[1].operands[0];

            std::vector<uint8_t> newBytecode;

            newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_SubtractCL));
            newBytecode.push_back(constantIndex);
            newBytecode.push_back(localIndex);

            return newBytecode;
        });

        return rewriter->rewrite(chunk);
    }
}