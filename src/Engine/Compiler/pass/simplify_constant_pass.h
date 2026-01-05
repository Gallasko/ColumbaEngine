#pragma once

#include "../bytecode_pass.h"
#include "../chunk.h"

#include <vector>

#include "logger.h"

namespace pg
{
    class SimplifyConstantToShort : public BytecodePass
    {
    public:
        std::string getName() const override { return "SimplifyConstantToShort"; }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) override
        {
            if (not rewriter)
            {
                LOG_ERROR("SimplifyConstantToShort", "No rewriter provided");

                return false;
            }

            auto checkConstantIsShortInt = [&chunk](const std::vector<CapturedInstruction>& captured) {
                auto value = chunk.constants[captured[0].getConstantIndex()];

                if (not IS_INT(value))
                    return false;

                return (AS_INT(value) >= 0) and (AS_INT(value) <= UINT8_MAX);
            };

            std::vector<PatternElement> pattern = {
                PatternElement::match(OpCode::OP_Constant, true),
            };

            rewriter->addAdvancedRule(pattern, [&chunk](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
                // Create new bytecode sequence for optimized addition
                std::vector<uint8_t> newBytecode;
                newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_Short_Int));

                auto value = chunk.constants[captured[0].getConstantIndex()];

                newBytecode.push_back(static_cast<uint8_t>(AS_INT(value)));

                return newBytecode;
            }, checkConstantIsShortInt);

            return rewriter->rewrite(chunk);
        }

        bool changesSize() const override { return false; }

        bool requiresMultiplePasses() const override { return false; }
    };
}
