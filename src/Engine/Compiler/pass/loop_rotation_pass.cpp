#include "stdafx.h"

#include "loop_rotation_pass.h"
#include "../bytecode_rewriter.h"
#include "logger.h"

#include <vector>

namespace pg
{
    namespace
    {
        // Big-endian encoding, matching patchJump/emitLoop and the rewriter's
        // extract/write helpers. Short = 2-byte operand, long = 4-byte.
        std::vector<uint8_t> makeBranchBytes(OpCode op, uint32_t dist, bool isLong)
        {
            if (isLong)
            {
                return {static_cast<uint8_t>(op),
                        static_cast<uint8_t>((dist >> 24) & 0xFF),
                        static_cast<uint8_t>((dist >> 16) & 0xFF),
                        static_cast<uint8_t>((dist >> 8)  & 0xFF),
                        static_cast<uint8_t>( dist        & 0xFF)};
            }

            return {static_cast<uint8_t>(op),
                    static_cast<uint8_t>((dist >> 8) & 0xFF),
                    static_cast<uint8_t>( dist       & 0xFF)};
        }
    }

    bool LoopRotationPass::isUndecodableBranch(OpCode opcode) const
    {
        // Register-based conditional jump uses a different operand layout and
        // should never appear inside a rotatable loop's condition/body; if it
        // does, we bail rather than risk a miscomputed target.
        return opcode == OpCode::OP_Jump_If_False_R;
    }

    bool LoopRotationPass::branchTarget(const Chunk& chunk, size_t offset, OpCode opcode,
                                        size_t& target, bool& isBackward) const
    {
        const auto& code = chunk.code;

        auto rd16 = [&](size_t o) -> uint16_t {
            return static_cast<uint16_t>((code[o + 1] << 8) | code[o + 2]);
        };
        auto rd32 = [&](size_t o) -> uint32_t {
            return (static_cast<uint32_t>(code[o + 1]) << 24) |
                   (static_cast<uint32_t>(code[o + 2]) << 16) |
                   (static_cast<uint32_t>(code[o + 3]) << 8)  |
                    static_cast<uint32_t>(code[o + 4]);
        };

        switch (opcode)
        {
            case OpCode::OP_Jump:
            case OpCode::OP_Jump_If_False:
            case OpCode::OP_Jump_If_False_Popping:
                if (offset + 2 >= code.size())
                    return false;
                isBackward = false;
                target = offset + 3 + rd16(offset);
                return true;

            case OpCode::OP_Long_Jump:
            case OpCode::OP_Long_Jump_If_False:
            case OpCode::OP_Long_Jump_If_False_Popping:
                if (offset + 4 >= code.size())
                    return false;
                isBackward = false;
                target = offset + 5 + rd32(offset);
                return true;

            case OpCode::OP_Loop:
            case OpCode::OP_Jump_If_True_Popping:
                if (offset + 2 >= code.size())
                    return false;
                isBackward = true;
                target = offset + 3 - rd16(offset);
                return true;

            case OpCode::OP_Long_Loop:
            case OpCode::OP_Long_Jump_If_True_Popping:
                if (offset + 4 >= code.size())
                    return false;
                isBackward = true;
                target = offset + 5 - rd32(offset);
                return true;

            default:
                return false;
        }
    }

    bool LoopRotationPass::branchesStayWithin(const Chunk& chunk, BytecodeRewriter* rewriter,
                                              size_t begin, size_t end,
                                              size_t lowBound, size_t highBound) const
    {
        for (size_t p = begin; p < end; )
        {
            OpCode op = static_cast<OpCode>(chunk.code[p]);

            if (isUndecodableBranch(op))
                return false;

            size_t target = 0;
            bool isBackward = false;
            if (branchTarget(chunk, p, op, target, isBackward))
            {
                if (target < lowBound or target > highBound)
                    return false;
            }

            size_t step = rewriter->getActualInstructionSize(chunk, p);
            if (step == 0)
                return false;
            p += step;
        }

        return true;
    }

    std::optional<LoopRotationPass::LoopShape>
    LoopRotationPass::findRotatableLoop(const Chunk& chunk, BytecodeRewriter* rewriter)
    {
        const auto& code = chunk.code;

        for (size_t i = 0; i < code.size(); )
        {
            OpCode op = static_cast<OpCode>(code[i]);

            const bool isLoop = (op == OpCode::OP_Loop or op == OpCode::OP_Long_Loop);
            if (not isLoop)
            {
                size_t step = rewriter->getActualInstructionSize(chunk, i);
                if (step == 0)
                    break;
                i += step;
                continue;
            }

            size_t loopSize = pg::getInstructionSize(op);
            size_t loopStart = 0;
            bool back = false;

            // Decode the OP_Loop's backward target (start of the condition).
            if (not branchTarget(chunk, i, op, loopStart, back) or not back or loopStart >= i)
            {
                i += loopSize;
                continue;
            }

            size_t K = i;
            size_t exitOffset = K + loopSize;

            // Find the guard: the first popping jump-if-false in [loopStart, K)
            // whose forward target is exactly the loop exit. Short-circuit
            // &&/|| inside the condition use the NON-popping variants and target
            // within the condition, so they are never mistaken for the guard.
            size_t guardOffset = 0;
            size_t guardSize = 0;
            bool foundGuard = false;
            bool bailed = false;

            for (size_t p = loopStart; p < K; )
            {
                OpCode pop = static_cast<OpCode>(code[p]);

                if (isUndecodableBranch(pop))
                {
                    bailed = true;
                    break;
                }

                if (pop == OpCode::OP_Jump_If_False_Popping or
                    pop == OpCode::OP_Long_Jump_If_False_Popping)
                {
                    size_t t = 0;
                    bool b = false;
                    if (branchTarget(chunk, p, pop, t, b) and not b and t == exitOffset)
                    {
                        guardOffset = p;
                        guardSize = pg::getInstructionSize(pop);
                        foundGuard = true;
                        break;
                    }
                }

                size_t step = rewriter->getActualInstructionSize(chunk, p);
                if (step == 0)
                {
                    bailed = true;
                    break;
                }
                p += step;
            }

            if (bailed or not foundGuard)
            {
                i += loopSize;
                continue;
            }

            size_t bodyStart = guardOffset + guardSize;

            // The condition [loopStart, guardOffset) must be self-contained:
            // every branch inside it must target within [loopStart, guardOffset].
            if (not branchesStayWithin(chunk, rewriter, loopStart, guardOffset, loopStart, guardOffset))
            {
                i += loopSize;
                continue;
            }

            // The body must not branch into the interior of the condition. The
            // only legal back-target at/before bodyStart is loopStart itself
            // (a `continue`, which after rotation routes through the entry jump).
            bool bodyOk = true;
            for (size_t q = bodyStart; q < K; )
            {
                OpCode qop = static_cast<OpCode>(code[q]);

                if (isUndecodableBranch(qop))
                {
                    bodyOk = false;
                    break;
                }

                size_t t = 0;
                bool b = false;
                if (branchTarget(chunk, q, qop, t, b))
                {
                    if (t != loopStart and t < bodyStart)
                    {
                        bodyOk = false;
                        break;
                    }
                }

                size_t step = rewriter->getActualInstructionSize(chunk, q);
                if (step == 0)
                {
                    bodyOk = false;
                    break;
                }
                q += step;
            }

            if (not bodyOk)
            {
                i += loopSize;
                continue;
            }

            LoopShape shape;
            shape.loopStart   = loopStart;
            shape.guardOffset = guardOffset;
            shape.guardSize   = guardSize;
            shape.loopOffset  = K;
            shape.loopSize    = loopSize;
            shape.exitOffset  = exitOffset;
            return shape;
        }

        return std::nullopt;
    }

    bool LoopRotationPass::runPass(Chunk& chunk, BytecodeRewriter* rewriter)
    {
        if (chunk.code.empty())
            return false;

        if (not rewriter)
        {
            LOG_ERROR("LoopRotation", "No rewriter provided");
            return false;
        }

        auto found = findRotatableLoop(chunk, rewriter);
        if (not found.has_value())
            return false;

        const LoopShape shape = found.value();

        const size_t C = shape.guardOffset - shape.loopStart;                 // condition byte length
        const size_t B = shape.loopOffset - (shape.guardOffset + shape.guardSize); // body byte length

        // Condition bytes, captured before any edit. Moved verbatim to the
        // bottom; their internal &&/|| jumps are self-relative and preserved.
        std::vector<uint8_t> condBytes(chunk.code.begin() + shape.loopStart,
                                       chunk.code.begin() + shape.guardOffset);

        // Bottom test: OP_Jump_If_True_Popping back to bodyStart. Its backward
        // distance is B + C + sizeJIT and is invariant under the edits below
        // (every edit shifts the JIT and bodyStart by the same amount).
        const bool jitLong = (B + C + 3) > 0xFFFF;
        const size_t sizeJIT = jitLong ? 5 : 3;
        const uint32_t jitDist = static_cast<uint32_t>(B + C + sizeJIT);
        const OpCode jitOp = jitLong ? OpCode::OP_Long_Jump_If_True_Popping
                                     : OpCode::OP_Jump_If_True_Popping;
        std::vector<uint8_t> jitBytes = makeBranchBytes(jitOp, jitDist, jitLong);

        // Entry jump: forward to condCheck. Distance == B (body length),
        // independent of the entry jump's own size.
        const bool entryLong = B > 0xFFFF;
        const OpCode entryOp = entryLong ? OpCode::OP_Long_Jump : OpCode::OP_Jump;
        std::vector<uint8_t> entryBytes = makeBranchBytes(entryOp, static_cast<uint32_t>(B), entryLong);

        LOG_MILE("LoopRotation", "Rotating loop: loopStart=" << shape.loopStart
                 << " guard=" << shape.guardOffset << " loop=" << shape.loopOffset
                 << " (C=" << C << ", B=" << B << ")");

        // Edits are applied HIGH offset -> LOW offset so the offsets captured
        // from the original chunk stay valid across all three edits. The
        // rewriter auto-adjusts every break/continue/nested jump that crosses
        // an edit point; the new OP_Jump_If_True_Popping is opaque to the
        // rewriter (not in its jump set) so its pre-computed distance survives.

        // Edit A: replace the OP_Loop with [condBytes][JIT].
        std::vector<uint8_t> replA;
        replA.reserve(condBytes.size() + jitBytes.size());
        replA.insert(replA.end(), condBytes.begin(), condBytes.end());
        replA.insert(replA.end(), jitBytes.begin(), jitBytes.end());

        // Re-prime jumpTargets before EACH edit: rewriteAtRaw rebuilds the set
        // internally on a transient (adjusted-but-not-yet-spliced) chunk, which
        // leaves stale targets. The backward-jump-lands-on-rewrite special case
        // that fixes `continue` distances needs an accurate set, so we refresh
        // it from the real chunk state right before every edit.

        // Edit A: replace the OP_Loop with [condBytes][JIT].
        rewriter->refreshJumpTargets(chunk);
        bool ok = rewriter->rewriteAtRaw(chunk, shape.loopOffset, shape.loopSize, replA);

        // Edit B: remove the guard.
        rewriter->refreshJumpTargets(chunk);
        ok &= rewriter->removeInstructions(chunk, shape.guardOffset, shape.guardSize);

        // Edit C: replace the original (now duplicated) condition with the entry jump.
        rewriter->refreshJumpTargets(chunk);
        ok &= rewriter->rewriteAtRaw(chunk, shape.loopStart, C, entryBytes);

        if (not ok)
        {
            LOG_WARNING("LoopRotation", "Rewrite sequence failed at loopStart=" << shape.loopStart);
            return false;
        }

        return true;
    }
}
