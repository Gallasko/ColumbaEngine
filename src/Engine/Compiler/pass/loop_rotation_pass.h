#pragma once

/**
 * @pass_doc
 * @name: Loop Rotation Pass
 * @purpose: Rotates test-at-top loops (while / for-in) into test-at-bottom form,
 *           eliminating the unconditional back-branch. The guard
 *           OP_Jump_If_False_Popping + trailing OP_Loop become a single
 *           OP_Jump_If_True_Popping at the bottom, with a one-time entry jump.
 * @category: control_flow
 * @example_before:
 *   loopStart:
 *     [COND]
 *     OP_Jump_If_False_Popping  exit
 *     [BODY]
 *     OP_Loop                   loopStart
 *   exit:
 * @example_after:
 *   loopStart:
 *     OP_Jump                   condCheck
 *   bodyStart:
 *     [BODY]
 *   condCheck:
 *     [COND]
 *     OP_Jump_If_True_Popping   bodyStart
 *   exit:
 * @benefits:
 *   - One branch per iteration instead of two (guard + unconditional loop)
 *   - The bottom test doubles as the back-branch; the OP_Loop is removed
 * @notes:
 *   - Runs LAST in the pipeline: it consumes the popping/shrunk forms produced
 *     by PoppingJump/LongJump and no later pass observes the new opcodes.
 *   - Only the canonical single-guard shape (while, for-in) is handled. The
 *     C-style for double-loop shape is intentionally left untouched (the guard
 *     target does not match the inner loop's exit, so it is skipped).
 * @end_pass_doc
 */

#include "../bytecode_pass.h"
#include "../chunk.h"
#include <optional>
#include <cstdint>

namespace pg
{
    class LoopRotationPass : public BytecodePass
    {
        // A detected, rotatable loop in its canonical single-guard form.
        struct LoopShape
        {
            size_t loopStart;   // L: backward target of the OP_Loop (start of COND)
            size_t guardOffset; // G: the OP_Jump_If_False_Popping guard
            size_t guardSize;   // encoded size of the guard (3 or 5)
            size_t loopOffset;  // K: the OP_Loop / OP_Long_Loop
            size_t loopSize;    // encoded size of the loop (3 or 5)
            size_t exitOffset;  // K + loopSize (guard's forward target)
        };

    public:
        std::string getName() const override { return "LoopRotation"; }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) override;

        bool changesSize() const override { return true; }

        // Process one loop per invocation; the PassManager re-runs us until no
        // more rotations are found (handles nested loops).
        bool requiresMultiplePasses() const override { return true; }

    private:
        std::optional<LoopShape> findRotatableLoop(const Chunk& chunk, BytecodeRewriter* rewriter);

        // Compute the byte offset a control-flow instruction targets. Returns
        // false for non-branch opcodes. Sets isBackward for loop-style branches.
        bool branchTarget(const Chunk& chunk, size_t offset, OpCode opcode,
                          size_t& target, bool& isBackward) const;

        // True for branch opcodes we cannot safely decode here (bail on these).
        bool isUndecodableBranch(OpCode opcode) const;

        // Verify every branch inside [begin, end) keeps its target within
        // [lowBound, highBound]. Bails (returns false) on any undecodable branch.
        bool branchesStayWithin(const Chunk& chunk, BytecodeRewriter* rewriter,
                                size_t begin, size_t end,
                                size_t lowBound, size_t highBound) const;
    };
}
