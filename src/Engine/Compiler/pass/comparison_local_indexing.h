#pragma once

/**
 * @pass_doc
 * @name: Comparison Local Indexing Pass
 * @purpose: Fuses Get_Local + Get_Local + comparison into a single _LL op
 * @category: arithmetic
 * @example_before:
 *   OP_Get_Local 2
 *   OP_Get_Local 0
 *   OP_LessEqual
 * @example_after:
 *   OP_LessEqualLL 2 0
 * @benefits:
 *   - 3 instructions become 1 (66% fewer dispatches)
 *   - No transient pushes of local values onto the stack
 *   - Mirrors what BasicOperatorLocalIndexingPass already does for arithmetic
 * @end_pass_doc
 */

#include "../bytecode_pass.h"
#include "../chunk.h"

#include "logger.h"

namespace pg
{
    class ComparisonLocalIndexingPass : public BytecodePass
    {
    public:
        std::string getName() const override { return "ComparisonLocalIndexing"; }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) override
        {
            if (not rewriter)
            {
                LOG_ERROR("ComparisonLocalIndexing", "No rewriter provided");
                return false;
            }

            // Helper to register a Get_Local + Get_Local + <cmp> -> <cmp>LL rule.
            auto addCmpLL = [rewriter](OpCode cmp, OpCode fusedLL) {
                std::vector<PatternElement> pattern = {
                    PatternElement::match(OpCode::OP_Get_Local, true),
                    PatternElement::match(OpCode::OP_Get_Local, true),
                    PatternElement::match(cmp),
                };
                rewriter->addAdvancedRule(pattern, [fusedLL](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
                    std::vector<uint8_t> out;
                    out.push_back(static_cast<uint8_t>(fusedLL));
                    out.push_back(captured[0].operands[0]);
                    out.push_back(captured[1].operands[0]);
                    return out;
                });
            };

            addCmpLL(OpCode::OP_LessEqual, OpCode::OP_LessEqualLL);
            addCmpLL(OpCode::OP_Less,      OpCode::OP_LessLL);
            // Extend with Greater / GreaterEqual / Equal / NotEqual the same
            // way when their _LL handlers exist.

            return rewriter->rewrite(chunk);
        }

        bool changesSize() const override { return true; }
        bool requiresMultiplePasses() const override { return false; }
    };
}
