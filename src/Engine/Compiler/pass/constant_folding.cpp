#include "constant_folding.h"

#include "../vm.h"

namespace pg
{
    bool ConstantFoldingPass::runPass(Chunk& chunk, BytecodeRewriter* rewriter)
    {
        if (not rewriter)
        {
            LOG_ERROR("ConstantFoldingPass", "No rewriter provided");

            return false;
        }

        std::vector<PatternElement> pattern = {
            PatternElement::constant(true),
            PatternElement::constant(true),
            PatternElement::match(OpCode::OP_Add),
        };

        auto* vm = rewriter->getVm();

        rewriter->addAdvancedRule(pattern, [&chunk, vm](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
            // Create new bytecode sequence for optimized addition
            std::vector<uint8_t> newBytecode;

            auto index1 = captured[0].getConstantIndex();
            auto index2 = captured[1].getConstantIndex();

            LOG_INFO("ConstantFoldingPass", "Folding constant addition of constants at indices " << index1 << " and " << index2);

            auto value1 = chunk.constants[index1];
            auto value2 = chunk.constants[index2];

            auto res = vm->addValues(value1, value2);

            LOG_INFO("ConstantFoldingPass", "Computed folded constant value: " << vm->valueToElement(res));

            auto newConstIndex = chunk.addConstantIndex(res);

            newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_Constant));
            newBytecode.push_back(newConstIndex);

            return newBytecode;
        }, [&chunk] (const std::vector<CapturedInstruction>&) { return chunk.constants.size() < 254; }); // Todo remove this limitation, by allowing long constant

        return rewriter->rewrite(chunk);
    }
}