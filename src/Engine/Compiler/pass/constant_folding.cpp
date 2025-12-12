#include "constant_folding.h"

#include "../vm.h"

namespace pg
{
    namespace
    {
        constexpr const char* const DOM = "ConstantFoldingPass";

        uint8_t applyBinaryOpOnConstant(Chunk& chunk, const CapturedInstruction& ins1, const CapturedInstruction& ins2, std::function<Value(const Value&, const Value&)> op)
        {
            auto index1 = ins1.getConstantIndex();
            auto index2 = ins2.getConstantIndex();

            auto value1 = chunk.constants[index1];
            auto value2 = chunk.constants[index2];

            auto res = op(value1, value2);

            auto newConstIndex = chunk.addConstantIndex(res);

            return newConstIndex;
        }
    }

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

        auto constantLimitation = [&chunk] (const std::vector<CapturedInstruction>&) { return chunk.constants.size() < 254; };

        rewriter->addAdvancedRule(pattern, [&chunk, vm](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
            // Create new bytecode sequence for optimized addition
            std::vector<uint8_t> newBytecode;

            auto newConstIndex = applyBinaryOpOnConstant(chunk, captured[0], captured[1],
                [&vm](const Value& a, const Value& b) -> Value {
                    return vm->addValues(a, b);
            });

            newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_Constant));
            newBytecode.push_back(newConstIndex);

            return newBytecode;
        }, constantLimitation); // Todo remove this limitation, by allowing long constant

        pattern = {
            PatternElement::constant(true),
            PatternElement::constant(true),
            PatternElement::match(OpCode::OP_Subtract),
        };

        rewriter->addAdvancedRule(pattern, [&chunk, vm](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
            // Create new bytecode sequence for optimized addition
            std::vector<uint8_t> newBytecode;

            auto newConstIndex = applyBinaryOpOnConstant(chunk, captured[0], captured[1],
                [&vm](const Value& a, const Value& b) -> Value {
                    return vm->subtractValues(a, b);
            });

            newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_Constant));
            newBytecode.push_back(newConstIndex);

            return newBytecode;
        }, constantLimitation); // Todo remove this limitation, by allowing long constant

        pattern = {
            PatternElement::constant(true),
            PatternElement::match(OpCode::OP_Negate),
        };

        rewriter->addAdvancedRule(pattern, [&chunk, vm](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
            // Create new bytecode sequence for optimized addition
            std::vector<uint8_t> newBytecode;

            auto index1 = captured[0].getConstantIndex();

            auto value1 = chunk.constants[index1];

            auto res = vm->negateValue(value1);

            auto newConstIndex = chunk.addConstantIndex(res);

            newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_Constant));
            newBytecode.push_back(newConstIndex);

            return newBytecode;
        }, constantLimitation); // Todo remove this limitation, by allowing long constant

        return rewriter->rewrite(chunk);
    }
}