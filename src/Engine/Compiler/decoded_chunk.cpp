#include "stdafx.h"
#include "decoded_chunk.h"
#include "vm.h"
#include "logger.h"
#include "compiler_debug.h"
#include "decoded_fusion.h"

#include <unordered_set>
#include <cstdint>

namespace pg
{
    // Computes the branch-target bytecode offset of a decoded control-flow
    // instruction. Single source of truth shared by resolveJumpTargets and
    // the fusion pass (a jump landing inside a fusion window forbids it).
    // Returns false for non-branching instructions.
    static bool branchTargetOffsetOf(const DecodedInstruction& instr,
                                     const DecodedInstructionMeta& meta,
                                     size_t& outOffset)
    {
        // Jump offsets are stored relative to the end of the instruction:
        // target = bytecodeOffset + instructionSize + signedOffset.
        switch (static_cast<OpCode>(meta.originalOpcode))
        {
            case OpCode::OP_Jump:
            case OpCode::OP_Jump_If_False:
            case OpCode::OP_Jump_If_False_Popping:
                // Regular jumps use 2-byte operands (big-endian, signed)
                outOffset = meta.bytecodeOffset + 1 + meta.operandBytes +
                            static_cast<int16_t>((instr.operands.indexed.byte1 << 8) |
                                                  instr.operands.indexed.byte2);
                return true;

            case OpCode::OP_Loop:
            case OpCode::OP_Jump_If_True_Popping:
                // Backward conditional/unconditional branch: unsigned distance
                // subtracted from the end of the instruction (same as OP_Loop).
                outOffset = meta.bytecodeOffset + 1 + meta.operandBytes -
                            static_cast<uint16_t>((instr.operands.indexed.byte1 << 8) |
                                                   instr.operands.indexed.byte2);
                return true;

            case OpCode::OP_Long_Jump:
            case OpCode::OP_Long_Jump_If_False:
            case OpCode::OP_Long_Jump_If_False_Popping:
                // Long jumps use 4-byte operands (big-endian, signed)
                outOffset = meta.bytecodeOffset + 1 + meta.operandBytes +
                            static_cast<int32_t>((instr.operands.indexed.byte1 << 24) |
                                                 (instr.operands.indexed.byte2 << 16) |
                                                 (instr.operands.indexed.byte3 << 8)  |
                                                  instr.operands.indexed.byte4);
                return true;

            case OpCode::OP_Long_Loop:
            case OpCode::OP_Long_Jump_If_True_Popping:
                outOffset = meta.bytecodeOffset + 1 + meta.operandBytes -
                            static_cast<uint32_t>((instr.operands.indexed.byte1 << 24) |
                                                  (instr.operands.indexed.byte2 << 16) |
                                                  (instr.operands.indexed.byte3 << 8)  |
                                                   instr.operands.indexed.byte4);
                return true;

            case OpCode::OP_Jump_If_False_R:
                // Register-based conditional jump: <slot> <offset_hi> <offset_lo>
                outOffset = meta.bytecodeOffset + 1 + meta.operandBytes +
                            static_cast<int16_t>((instr.operands.indexed.byte2 << 8) |
                                                  instr.operands.indexed.byte3);
                return true;

            default:
                return false;
        }
    }
    ObjFunction::~ObjFunction()
    {
        if (decodedChunk != nullptr)
        {
            delete decodedChunk;
            decodedChunk = nullptr;
        }
    }

    // Synthetic terminator appended by ChunkDecoder::decode when a chunk's
    // last instruction is not OP_Return (possible for hand-crafted or
    // corrupted bytecode files). The dispatch loop has no bounds check —
    // it relies on every chunk ending in an instruction that never falls
    // through — so this guarantees the invariant for untrusted input.
    static const DecodedInstruction* op_halt_decoded(VM* vm, const DecodedInstruction&)
    {
        vm->runtimeError("Reached end of bytecode without OP_Return.");
        vm->vm_return(InterpretResult::RUNTIME_ERROR);
        return nullptr; // stops the dispatch loop
    }

    DecodedChunk* ChunkDecoder::decode(const Chunk& chunk, VM* vm)
    {
        DecodedChunk* decoded = new DecodedChunk();
        decoded->originalChunk = &chunk;
        decoded->totalInstructions = 0;

        if (chunk.code.empty())
        {
            return decoded;
        }

        // Reserve space (avoid reallocations)
        decoded->instructions.reserve(chunk.code.size());
        decoded->meta.reserve(chunk.code.size());

        // Decode all instructions and build jump target map
        size_t offset = 0;
        size_t instructionIndex = 0;

        while (offset < chunk.code.size())
        {
            // Record jump target mapping (bytecode offset to instruction index)
            // This is needed for ALL control flow instructions:
            // OP_Jump, OP_Long_Jump, OP_Jump_If_False, OP_Long_Jump_If_False, OP_Loop, OP_Long_Loop
            decoded->jumpTargets[offset] = instructionIndex;

            size_t nextOffset;
            DecodedInstruction instr;
            DecodedInstructionMeta meta;
            decodeInstruction(chunk, offset, vm, instr, meta, nextOffset);

            decoded->instructions.push_back(instr);
            decoded->meta.push_back(meta);
            offset = nextOffset;
            instructionIndex++;
        }

        // Guarantee the no-fall-through terminator the dispatch loop relies
        // on. Must happen BEFORE resolveJumpTargets: push_back can
        // reallocate the vector, which would dangle the resolved pointers.
        if (decoded->meta.back().originalOpcode != static_cast<uint8_t>(OpCode::OP_Return))
        {
            DecodedInstruction halt{};
            halt.decodedHandler = op_halt_decoded;
            decoded->instructions.push_back(halt);

            DecodedInstructionMeta haltMeta{};
            haltMeta.flags          = OpCodeInfo::NO_BRANCH; // hasControlFlow() == false → skipped by resolveJumpTargets
            haltMeta.originalOpcode = static_cast<uint8_t>(OpCode::OP_Return); // skipped by resolveConstantPointers
            haltMeta.lineNumber     = chunk.lines.empty() ? -1 : chunk.lines.back();
            haltMeta.bytecodeOffset = chunk.code.size();
            decoded->meta.push_back(haltMeta);

            // A forward jump targeting end-of-code resolves to the halt.
            decoded->jumpTargets[chunk.code.size()] = decoded->instructions.size() - 1;
            instructionIndex++;
        }

        decoded->totalInstructions = instructionIndex;

        // Decode-time superinstruction fusion (must run before the pointer
        // resolvers: it compacts the arrays and remaps jumpTargets).
        if (vm != nullptr and vm->enableDecodeFusion)
        {
            fuseInstructions(decoded, chunk, vm);
            fuseIndexedAccess(decoded);
        }

        // Optimize: pre-resolve constant pointers
        resolveConstantPointers(decoded, chunk);

        resolveJumpTargets(decoded);

        // Optimize: pre-resolve constant-global ops to dense VM global slots.
        resolveGlobalSlots(decoded, chunk, vm);

        // TODO Future: analyze pure batches for instruction fusion
        // This will allow us to detect patterns like GET_LOCAL + GET_LOCAL + ADD
        // and replace them with fused instructions like ADD_LL
        // analyzePureBatches(decoded);

        LOG_INFO("ChunkDecoder", "Decoded " << decoded->instructions.size() << " instructions");

        return decoded;
    }

    void ChunkDecoder::decodeInstruction(
        const Chunk& chunk,
        size_t offset,
        VM* vm,
        DecodedInstruction& instr,
        DecodedInstructionMeta& meta,
        size_t& nextOffset)
    {
        uint8_t opcode = chunk.code[offset];
        meta.originalOpcode = opcode;
        meta.bytecodeOffset = offset;

        // Look up handler and metadata
        const OpCodeInfo& info = vm->operations[opcode];
        instr.decodedHandler = info.decodedHandler;
        meta.flags = info.flags;
        meta.operandBytes = info.operandBytes;

        // Get line number for error reporting
        if (offset < chunk.lines.size())
        {
            meta.lineNumber = chunk.lines[offset];
        }
        else
        {
            meta.lineNumber = -1;
        }

        // Decode operands based on metadata
        // The operandBytes field tells us how many bytes follow the opcode
        // If operandBytes is 0 (not set), fall back to getInstructionSize()
        size_t operandBytes = info.operandBytes;
        if (operandBytes == 0)
        {
            // Fallback: use getInstructionSize() from chunk.h
            int totalSize = getInstructionSize(static_cast<OpCode>(opcode));
            operandBytes = (totalSize > 1) ? (totalSize - 1) : 0;
            meta.operandBytes = operandBytes;
        }

        // Clear operands first
        instr.operands.dword = 0;

        switch (operandBytes)
        {
            case 0:
                // No operands (e.g., OP_Add, OP_Return, OP_Pop)
                break;

            case 1:
                // Single byte operand (e.g., OP_Constant, OP_Get_Local, OP_Call)
                if (offset + 1 < chunk.code.size())
                {
                    instr.operands.byte = chunk.code[offset + 1];
                }
                break;

            case 2:
                // Two byte operands (e.g., OP_Jump, OP_AddLL has 2 operands of 1 byte each)
                if (offset + 2 < chunk.code.size())
                {
                    instr.operands.indexed.byte1 = chunk.code[offset + 1];
                    instr.operands.indexed.byte2 = chunk.code[offset + 2];
                }
                break;

            case 3:
                // Three byte operands (e.g., OP_LongConstant)
                if (offset + 3 < chunk.code.size())
                {
                    instr.operands.indexed.byte1 = chunk.code[offset + 1];
                    instr.operands.indexed.byte2 = chunk.code[offset + 2];
                    instr.operands.indexed.byte3 = chunk.code[offset + 3];
                }
                break;

            case 4:
                // Four byte operands (e.g., OP_Long_Jump)
                if (offset + 4 < chunk.code.size())
                {
                    instr.operands.indexed.byte1 = chunk.code[offset + 1];
                    instr.operands.indexed.byte2 = chunk.code[offset + 2];
                    instr.operands.indexed.byte3 = chunk.code[offset + 3];
                    instr.operands.indexed.byte4 = chunk.code[offset + 4];
                }
                break;
        }

        // Pre-resolve property-name pointer for ops that index into
        // chunk.constantStrings, so handlers can dereference directly
        // instead of doing constantStrings[index] at runtime.
        switch (static_cast<OpCode>(opcode))
        {
            case OpCode::OP_Get_Property:
            case OpCode::OP_Set_Property:
                if (instr.operands.byte < chunk.constantStrings.size())
                    instr.propertyNamePtr = &chunk.constantStrings[instr.operands.byte];
                break;
            case OpCode::OP_Invoke:
                if (instr.operands.indexed.byte1 < chunk.constantStrings.size())
                    instr.propertyNamePtr = &chunk.constantStrings[instr.operands.indexed.byte1];
                break;
            default:
                break;
        }

        nextOffset = offset + 1 + operandBytes;

        // OP_Closure carries a variable-length upvalue payload (2 bytes per
        // upvalue) after its constant operand. The payload is read by
        // op_closure_decoded through the frame ip, not dispatched — skip it
        // here so its bytes aren't decoded as spurious instructions.
        if (static_cast<OpCode>(opcode) == OpCode::OP_Closure
            and instr.operands.byte < chunk.constants.size())
        {
            const Value& functionValue = chunk.constants[instr.operands.byte];
            if (IS_FUNC(functionValue))
            {
                ObjFunction* function = vm->asFunction(functionValue);
                if (function != nullptr)
                {
                    nextOffset += 2 * function->upvalueCount;
                }
            }
        }
    }

    // ======================================================================
    // Decode-time superinstruction fusion.
    //
    // Walks the decoded stream with the stack-effect classifications from
    // decoded_fusion.h and collapses producer/op/consumer windows into one
    // fused DecodedInstruction. The bytecode is untouched — fused handlers
    // exist only in the decoded form. See decoded_fusion.h for the encoding
    // contract (operand bytes, single union pointer).
    // ======================================================================

    namespace
    {
        // A fusable input source extracted from a producer instruction (or
        // the stack / a legacy fused op's embedded operand).
        struct SrcDesc
        {
            fusion::Src kind = fusion::Src::Stack;
            uint8_t     byteVal = 0;     // slot index or short-int literal
            uint32_t    constIndex = 0;  // when kind == Const
        };

        bool producerToSrc(const DecodedInstruction& instr, fusion::Producer p, SrcDesc& out)
        {
            switch (p)
            {
                case fusion::Producer::Local:
                    out.kind = fusion::Src::Local;
                    out.byteVal = instr.operands.byte;
                    return true;
                case fusion::Producer::ShortInt:
                    out.kind = fusion::Src::ShortInt;
                    out.byteVal = instr.operands.byte;
                    return true;
                case fusion::Producer::Const:
                    out.kind = fusion::Src::Const;
                    out.constIndex = instr.operands.byte;
                    return true;
                default:
                    return false;
            }
        }
    }

    void ChunkDecoder::fuseInstructions(DecodedChunk* decoded, const Chunk& chunk, VM*)
    {
        using namespace fusion;

        const size_t n = decoded->instructions.size();
        if (n < 2)
            return;

        const std::vector<DecodedInstruction>&     in  = decoded->instructions;
        const std::vector<DecodedInstructionMeta>& im  = decoded->meta;

        // 1. Collect the bytecode offsets that are branch targets: a jump
        // landing INSIDE a window (anywhere but its head) forbids fusion.
        std::unordered_set<size_t> targetOffsets;
        for (size_t i = 0; i < n; ++i)
        {
            size_t target = 0;
            if (branchTargetOffsetOf(in[i], im[i], target))
                targetOffsets.insert(target);
        }

        auto isTarget = [&](size_t idx) {
            return targetOffsets.find(im[idx].bytecodeOffset) != targetOffsets.end();
        };
        // Window [head, head+len) is safe iff no member but the head is a
        // branch target.
        auto windowSafe = [&](size_t head, size_t len) {
            for (size_t k = 1; k < len; ++k)
                if (isTarget(head + k))
                    return false;
            return true;
        };

        // Builds the fused instruction for (op, A, B, sink). Returns false
        // when the combination must not fuse (selection rules, const bounds).
        auto buildBinary = [&](BinOp op, const SrcDesc& a, const SrcDesc& b, Sink sink,
                               uint8_t dstSlot, DecodedInstruction& out) -> bool
        {
            OpDecodedHandler handler = selectFusedBinary(op, a.kind, b.kind, sink);
            if (handler == nullptr)
                return false;

            out = DecodedInstruction{};
            out.decodedHandler = handler;
            out.operands.indexed.byte1 = a.byteVal;
            out.operands.indexed.byte2 = b.byteVal;
            out.operands.indexed.byte3 = dstSlot;

            const SrcDesc* constSrc = (a.kind == Src::Const) ? &a
                                    : (b.kind == Src::Const) ? &b : nullptr;
            if (constSrc != nullptr)
            {
                if (constSrc->constIndex >= chunk.constants.size())
                    return false;
                out.constantPtr = const_cast<Value*>(&chunk.constants[constSrc->constIndex]);
            }
            return true;
        };

        // In-place compound assignment: when the store destination is also a
        // Local source (`x = x <op> operand`, or `x = operand <op> x` for a
        // commutative op), collapse to a single read-modify-write handler that
        // updates the slot in place. `outB` reports the operand source used
        // (for the profiler name). Returns false when no in-place form applies.
        auto buildCompound = [&](BinOp op, const SrcDesc& a, const SrcDesc& b,
                                 uint8_t dstSlot, DecodedInstruction& out, Src& outB) -> bool
        {
            const SrcDesc* other = nullptr; // becomes right operand B (byte2 / const)
            if (a.kind == Src::Local and a.byteVal == dstSlot)
                other = &b;
            else if (isCommutative(op) and b.kind == Src::Local and b.byteVal == dstSlot)
                other = &a;
            else
                return false;

            OpDecodedHandler handler = selectFusedLocalCompound(op, other->kind);
            if (handler == nullptr)
                return false;

            out = DecodedInstruction{};
            out.decodedHandler = handler;
            out.operands.indexed.byte1 = dstSlot;
            out.operands.indexed.byte2 = other->byteVal;
            if (other->kind == Src::Const)
            {
                if (other->constIndex >= chunk.constants.size())
                    return false;
                out.constantPtr = const_cast<Value*>(&chunk.constants[other->constIndex]);
            }
            outB = other->kind;
            return true;
        };

        // A store consumer is either the fused OP_Set_Local_Pop (legacy
        // bytecode) or the plain OP_Set_Local + OP_Pop pair the compiler
        // emits now that the bytecode-level fusion passes are gone.
        struct StoreConsumer { bool found = false; uint8_t dst = 0; size_t len = 0; };
        auto storeConsumerAt = [&](size_t idx) -> StoreConsumer {
            if (idx >= n)
                return {};
            if (isStoreConsumer(im[idx].originalOpcode))
                return {true, in[idx].operands.byte, 1};
            if (idx + 1 < n
                and im[idx].originalOpcode == static_cast<uint8_t>(OpCode::OP_Set_Local)
                and im[idx + 1].originalOpcode == static_cast<uint8_t>(OpCode::OP_Pop))
                return {true, in[idx].operands.byte, 2};
            return {};
        };

        std::vector<DecodedInstruction>     outI;
        std::vector<DecodedInstructionMeta> outM;
        outI.reserve(n);
        outM.reserve(n);
        std::vector<size_t> oldToNew(n, 0);

        // Fused conditional branches record their target OFFSET here; it is
        // resolved once the compacted arrays are final. `relative` branches
        // (a Const operand occupies the union pointer) store a 16-bit relative
        // instruction offset in the operand bytes instead of a jumpTargetPtr.
        struct BranchFixup { size_t newIndex; size_t targetOffset; bool relative; };
        std::vector<BranchFixup> branchFixups;

        size_t fusedWindows = 0;
        size_t i = 0;
        while (i < n)
        {
            DecodedInstruction fused{};
            size_t windowLen = 0;
            bool   hasBranch = false;
            bool   branchRelative = false;
            size_t branchTarget = 0;
            const std::string* fusedName = nullptr;
            // Stack effect of the fused instruction, consumed by the
            // indexed-access sweep. 0xFF = unknown (scan barrier).
            uint8_t winPops = 0xFF;
            uint8_t winPushes = 0xFF;

            auto stackSrcCount = [](Src a, Src b) -> uint8_t {
                return static_cast<uint8_t>((a == Src::Stack ? 1 : 0)
                                          + (b == Src::Stack ? 1 : 0));
            };

            // Shared tail for every binary-op shape: given the sources and
            // the index right after the op, try store consumer, then popping
            // conditional branch, then plain push. headLen = instructions up
            // to and including the binary op.
            auto tryBinary = [&](BinOp op, const SrcDesc& a, const SrcDesc& b,
                                 size_t headLen) -> bool
            {
                const size_t after = i + headLen;

                const StoreConsumer sc = storeConsumerAt(after);
                if (sc.found and windowSafe(i, headLen + sc.len))
                {
                    Src compoundB;
                    if (buildCompound(op, a, b, sc.dst, fused, compoundB))
                    {
                        windowLen = headLen + sc.len;
                        fusedName = compoundFusionName(op, compoundB);
                        winPops = (compoundB == Src::Stack) ? 1 : 0;
                        winPushes = 0;
                        return true;
                    }
                    if (buildBinary(op, a, b, Sink::Store, sc.dst, fused))
                    {
                        windowLen = headLen + sc.len;
                        fusedName = fusionName(op, a.kind, b.kind, Sink::Store);
                        winPops = stackSrcCount(a.kind, b.kind);
                        winPushes = 0;
                        return true;
                    }
                }

                const bool afterFalseBranch =
                    after < n and isPoppingCondBranch(im[after].originalOpcode);
                const bool afterTrueBranch =
                    after < n and isPoppingTrueCondBranch(im[after].originalOpcode);

                if ((afterFalseBranch or afterTrueBranch)
                    and windowSafe(i, headLen + 1)
                    and branchTargetOffsetOf(in[after], im[after], branchTarget))
                {
                    // The true variant (loop-rotation bottom test) branches
                    // BACKWARD; branchTargetOffsetOf already resolved that.
                    const Sink branchSink = afterTrueBranch ? Sink::BranchIfTrue
                                                            : Sink::BranchIfFalse;

                    // A Const operand forces a relative-offset branch target
                    // (constantPtr owns the union pointer), so only fuse when
                    // the target is within int16 range. The pre-fusion distance
                    // (old indices, measured from the window head i) is a safe
                    // upper bound on |distance| for both forward and backward
                    // branches — fusion only shrinks the gap. Non-Const branches
                    // use the pointer (no limit).
                    const bool usesConst = (a.kind == Src::Const or b.kind == Src::Const);
                    bool encodable = true;
                    if (usesConst)
                    {
                        auto it = decoded->jumpTargets.find(branchTarget);
                        const ptrdiff_t rel = (it == decoded->jumpTargets.end())
                            ? PTRDIFF_MAX
                            : static_cast<ptrdiff_t>(it->second) - static_cast<ptrdiff_t>(i);
                        encodable = (rel >= INT16_MIN and rel <= INT16_MAX);
                    }

                    if (encodable
                        and buildBinary(op, a, b, branchSink, 0, fused))
                    {
                        windowLen = headLen + 1;
                        hasBranch = true;
                        branchRelative = usesConst;
                        fusedName = fusionName(op, a.kind, b.kind, branchSink);
                        return true;
                    }
                }

                if (headLen >= 2 and windowSafe(i, headLen)
                    and buildBinary(op, a, b, Sink::Push, 0, fused))
                {
                    windowLen = headLen;
                    fusedName = fusionName(op, a.kind, b.kind, Sink::Push);
                    winPops = stackSrcCount(a.kind, b.kind);
                    winPushes = 1;
                    return true;
                }
                return false;
            };

            const Producer p1 = classifyProducer(im[i].originalOpcode);

            // --- Window shapes, longest head first --------------------------
            // A) producer producer binop [consumer]
            if (p1 != Producer::None and i + 2 < n)
            {
                const Producer p2 = classifyProducer(im[i + 1].originalOpcode);
                const BinOp    op = classifyBinary(im[i + 2].originalOpcode);
                if (p2 != Producer::None and op != BinOp::None)
                {
                    SrcDesc a, b;
                    producerToSrc(in[i], p1, a);
                    producerToSrc(in[i + 1], p2, b);
                    tryBinary(op, a, b, 3);
                }
            }

            // B) producer binop [consumer]  (deeper operand stays on the stack)
            if (windowLen == 0 and p1 != Producer::None and i + 1 < n)
            {
                const BinOp op = classifyBinary(im[i + 1].originalOpcode);
                if (op != BinOp::None)
                {
                    SrcDesc a; // Src::Stack
                    SrcDesc b;
                    producerToSrc(in[i], p1, b);
                    tryBinary(op, a, b, 2);
                }
            }

            // C) producer(Local) unary — `!flag` / `-x` over a local
            if (windowLen == 0 and p1 == Producer::Local and i + 1 < n)
            {
                const UnOp op = classifyUnary(im[i + 1].originalOpcode);
                if (op != UnOp::None and windowSafe(i, 2))
                {
                    fused = DecodedInstruction{};
                    fused.decodedHandler = selectFusedUnary(op);
                    fused.operands.indexed.byte1 = in[i].operands.byte;
                    windowLen = 2;
                    winPops = 0;
                    winPushes = 1;
                    fusedName = internFusionName(op == UnOp::Not ? "FUSED_Not(L)->Push"
                                                                 : "FUSED_Negate(L)->Push");
                }
            }

            // D) legacy fused binop (AddLL, LessLL, …) + consumer — lets old
            //    serialized bytecode re-fuse its trailing store/branch.
            if (windowLen == 0 and i + 1 < n)
            {
                BinOp op; Src la, lb;
                if (classifyLegacyBinary(im[i].originalOpcode, op, la, lb))
                {
                    SrcDesc a, b;
                    a.kind = la;
                    b.kind = lb;
                    if (la == Src::Const) a.constIndex = in[i].operands.indexed.byte1;
                    else                  a.byteVal    = in[i].operands.indexed.byte1;
                    if (lb == Src::Const) b.constIndex = in[i].operands.indexed.byte2;
                    else                  b.byteVal    = in[i].operands.indexed.byte2;

                    // headLen 1 = the legacy op itself; only fuse if a
                    // consumer follows (alone it's already fused).
                    tryBinary(op, a, b, 1);
                }
            }

            // E) binop + consumer (both operands from the stack)
            if (windowLen == 0)
            {
                const BinOp op = classifyBinary(im[i].originalOpcode);
                if (op != BinOp::None)
                {
                    SrcDesc a, b; // both Src::Stack by default
                    tryBinary(op, a, b, 1);
                }
            }

            // F) bare Set_Local + Pop → the existing op_set_local_pop_decoded
            //    handler (replaces the removed SetLocalPopFusion bytecode pass)
            if (windowLen == 0 and i + 1 < n
                and im[i].originalOpcode == static_cast<uint8_t>(OpCode::OP_Set_Local)
                and im[i + 1].originalOpcode == static_cast<uint8_t>(OpCode::OP_Pop)
                and windowSafe(i, 2))
            {
                fused = DecodedInstruction{};
                fused.decodedHandler = op_set_local_pop_decoded;
                fused.operands.byte = in[i].operands.byte;
                windowLen = 2;
                winPops = 1;
                winPushes = 0;
                fusedName = internFusionName("FUSED_SetLocalPop");
            }

            // G) Short_Int slot + Post_Incr/Decr_Local → fused in-place local
            //    increment (IncrementOptimization's statement form: the slot
            //    index round-trips through the stack today)
            if (windowLen == 0 and i + 1 < n
                and im[i].originalOpcode == static_cast<uint8_t>(OpCode::OP_Short_Int)
                and windowSafe(i, 2))
            {
                const uint8_t next = im[i + 1].originalOpcode;
                const bool incr = (next == static_cast<uint8_t>(OpCode::OP_Post_Incr_Local));
                const bool decr = (next == static_cast<uint8_t>(OpCode::OP_Post_Decr_Local));
                if (incr or decr)
                {
                    fused = DecodedInstruction{};
                    fused.decodedHandler = incr ? &fusedIncrDecrLocal<true>
                                                : &fusedIncrDecrLocal<false>;
                    fused.operands.indexed.byte1 = in[i].operands.byte; // slot
                    windowLen = 2;
                    winPops = 0;
                    winPushes = 0;
                    fusedName = internFusionName(incr ? "FUSED_IncrLocal"
                                                      : "FUSED_DecrLocal");
                }
            }

            // --- Emit -------------------------------------------------------
            if (windowLen >= 2)
            {
                const size_t newIdx = outI.size();
                for (size_t k = 0; k < windowLen; ++k)
                    oldToNew[i + k] = newIdx;

                DecodedInstructionMeta m = im[i]; // head's offset/line/opcode
                m.fusedLength = static_cast<uint8_t>(windowLen);
                m.fusedName   = fusedName;
                m.stackPops   = winPops;
                m.stackPushes = winPushes;

                if (hasBranch)
                    branchFixups.push_back({newIdx, branchTarget, branchRelative});

                outI.push_back(fused);
                outM.push_back(m);
                fusedWindows++;
                i += windowLen;
            }
            else
            {
                oldToNew[i] = outI.size();
                outI.push_back(in[i]);
                outM.push_back(im[i]);
                ++i;
            }
        }

        if (fusedWindows == 0)
            return;

        // 2. Remap the offset → index table to the compacted array (offsets
        // of fused-away members map to their window head, which also keeps
        // findInstructionIndex / the nested-resume path coherent).
        for (auto& entry : decoded->jumpTargets)
            entry.second = oldToNew[entry.second];

        decoded->instructions = std::move(outI);
        decoded->meta         = std::move(outM);
        decoded->totalInstructions = decoded->instructions.size();

        // 3. Resolve fused branch targets now that the array is final.
        for (const auto& fx : branchFixups)
        {
            DecodedInstruction& di = decoded->instructions[fx.newIndex];
            auto it = decoded->jumpTargets.find(fx.targetOffset);
            if (it == decoded->jumpTargets.end())
            {
                LOG_ERROR("ChunkDecoder", "Unresolved fused branch target (offset "
                          << fx.targetOffset << ") — falling back to "
                          << (fx.relative ? "fall-through" : "instruction 0"));
                if (fx.relative)
                {
                    di.operands.indexed.byte3 = 1; // &instr + 1 (fall through)
                    di.operands.indexed.byte4 = 0;
                }
                else
                    di.jumpTargetPtr = decoded->instructions.data();
                continue;
            }

            if (fx.relative)
            {
                // Const operand: constantPtr owns the union pointer, so store
                // the target as a signed 16-bit relative instruction offset in
                // operand bytes 3/4 (range guaranteed at fusion time). The
                // separate operand union leaves constantPtr untouched.
                const ptrdiff_t rel = static_cast<ptrdiff_t>(it->second)
                                    - static_cast<ptrdiff_t>(fx.newIndex);
                di.operands.indexed.byte3 = static_cast<uint8_t>(rel & 0xFF);
                di.operands.indexed.byte4 = static_cast<uint8_t>((rel >> 8) & 0xFF);
            }
            else
            {
                di.jumpTargetPtr = decoded->instructions.data() + it->second;
            }

            // The branch's opcode/operands are fused away, so later passes
            // can't recover this target — record the index explicitly.
            decoded->fusedBranchTargets.push_back(it->second);
        }

        LOG_INFO("ChunkDecoder", "Fused " << fusedWindows << " windows ("
                 << n << " -> " << decoded->instructions.size() << " instructions)");
    }

    // Second fusion sweep, over the already-fused stream: indexed accesses
    // whose TARGET is a plain local.
    //
    //   Get_Local v; <pure index expr>; Get_Index        → nop; expr; FUSED_GetIndex(L,S)
    //   Get_Local v; <pure exprs>; Set_Index; Pop        → nop; exprs; FUSED_SetIndex(L,S,S); nop
    //
    // The container handle is then read straight from the slot (BORROWED —
    // the slot's reference keeps it alive), eliminating a retain/release
    // pair, the handle's push/pop round-trip and a dispatch per access.
    //
    // Unlike fuseInstructions' contiguous windows, the producer and the
    // index op are separated by the index expression, so matching walks a
    // conservative stack-depth simulation: only instructions with a known
    // effect (whitelisted plain ops, or fused ops that recorded pops/pushes
    // at fusion time) may sit in between, and none of them may dig below the
    // handle's stack position. Anything else — calls, branches, stores,
    // unknown effects, or a branch target landing inside the window — bails.
    // Elided instructions become 1-dispatch no-ops rather than being
    // compacted away, keeping instruction indices and resolved branch
    // pointers stable.
    void ChunkDecoder::fuseIndexedAccess(DecodedChunk* decoded)
    {
        using namespace fusion;

        const size_t n = decoded->instructions.size();
        if (n < 2)
            return;

        std::vector<DecodedInstruction>&     ins = decoded->instructions;
        std::vector<DecodedInstructionMeta>& im  = decoded->meta;

        // Branch targets: entering a window anywhere but its head would skip
        // the elided producer.
        std::unordered_set<size_t> targetOffsets;
        for (size_t i = 0; i < n; ++i)
        {
            size_t target = 0;
            if (branchTargetOffsetOf(ins[i], im[i], target))
                targetOffsets.insert(target);
        }
        std::unordered_set<size_t> targetIndices(decoded->fusedBranchTargets.begin(),
                                                 decoded->fusedBranchTargets.end());
        auto isTarget = [&](size_t idx) {
            return targetIndices.find(idx) != targetIndices.end()
                or targetOffsets.find(im[idx].bytecodeOffset) != targetOffsets.end();
        };

        // Conservative per-instruction stack effect. Returns false for
        // anything the scan must treat as a barrier.
        auto effectOf = [&](size_t idx, uint8_t& pops, uint8_t& pushes) -> bool {
            if (ins[idx].decodedHandler == &fusedElidedProducer)
            {
                pops = 0; pushes = 0;
                return true;
            }
            if (ins[idx].decodedHandler == &fusedGetIndexLocal)
            {
                pops = 1; pushes = 1;
                return true;
            }
            if (im[idx].fusedLength >= 2)
            {
                if (im[idx].stackPops == 0xFF or im[idx].stackPushes == 0xFF)
                    return false;
                pops = im[idx].stackPops;
                pushes = im[idx].stackPushes;
                return true;
            }
            switch (static_cast<OpCode>(im[idx].originalOpcode))
            {
                case OpCode::OP_Get_Local:
                case OpCode::OP_Constant:
                case OpCode::OP_Short_Int:
                case OpCode::OP_True:
                case OpCode::OP_False:
                    pops = 0; pushes = 1;
                    return true;
                case OpCode::OP_Add:
                case OpCode::OP_Subtract:
                case OpCode::OP_Multiply:
                case OpCode::OP_Divide:
                case OpCode::OP_Modulo:
                case OpCode::OP_Get_Index:
                    pops = 2; pushes = 1;
                    return true;
                default:
                    return false;
            }
        };

        constexpr size_t MaxWindow = 24;
        size_t rewrites = 0;

        for (size_t i = 0; i + 1 < n; ++i)
        {
            // A plain, un-fused Get_Local producer.
            if (im[i].fusedLength >= 2)
                continue;
            if (ins[i].decodedHandler != &op_get_local_decoded)
                continue;

            const uint8_t slot = ins[i].operands.byte;
            int depth = 0; // group depth above the handle's stack position

            for (size_t j = i + 1; j < n and (j - i) <= MaxWindow; ++j)
            {
                if (isTarget(j))
                    break;

                const bool plain = im[j].fusedLength < 2;
                const OpCode op = static_cast<OpCode>(im[j].originalOpcode);

                // Read access: the handle sits just below the index.
                if (plain and op == OpCode::OP_Get_Index and depth == 1
                    and ins[j].decodedHandler == &op_get_index_decoded)
                {
                    ins[j].decodedHandler = &fusedGetIndexLocal;
                    ins[j].operands.indexed.byte1 = slot;
                    im[j].fusedName = internFusionName("FUSED_GetIndex(L,S)");
                    ins[i].decodedHandler = &fusedElidedProducer;
                    im[i].fusedName = internFusionName("ELIDED_Producer");
                    ++rewrites;
                    break;
                }

                // Write access: handle below index + value; the generic
                // Set_Index leaves the handle on the stack for the trailing
                // Pop — both are absorbed by the fused form.
                if (plain and op == OpCode::OP_Set_Index and depth == 2
                    and ins[j].decodedHandler == &op_set_index_decoded
                    and j + 1 < n
                    and im[j + 1].fusedLength < 2
                    and static_cast<OpCode>(im[j + 1].originalOpcode) == OpCode::OP_Pop
                    and not isTarget(j + 1))
                {
                    ins[j].decodedHandler = &fusedSetIndexLocal;
                    ins[j].operands.indexed.byte1 = slot;
                    im[j].fusedName = internFusionName("FUSED_SetIndex(L,S,S)");
                    ins[j + 1].decodedHandler = &fusedElidedProducer;
                    im[j + 1].fusedName = internFusionName("ELIDED_Pop");
                    ins[i].decodedHandler = &fusedElidedProducer;
                    im[i].fusedName = internFusionName("ELIDED_Producer");
                    ++rewrites;
                    break;
                }

                uint8_t pops = 0, pushes = 0;
                if (not effectOf(j, pops, pushes))
                    break;
                if (depth < static_cast<int>(pops))
                    break; // would consume the handle (or dig below it)
                depth += static_cast<int>(pushes) - static_cast<int>(pops);
            }
        }

        if (rewrites > 0)
            LOG_INFO("ChunkDecoder", "Fused " << rewrites << " local indexed accesses");
    }

    void ChunkDecoder::analyzePureBatches(DecodedChunk*)
    {
        // TODO: Future implementation for instruction fusion
        // This will identify sequences of instructions without control flow
        // that can be optimized or fused together
        //
        // Example patterns to detect:
        // - GET_LOCAL + GET_LOCAL + ADD -> ADD_LL (already exists)
        // - Multiple CONSTANT loads
        // - Arithmetic chains
        //
        // For now, we're just focusing on pre-decoding to eliminate
        // the fetch/decode overhead at runtime
    }

    void ChunkDecoder::resolveConstantPointers(DecodedChunk* decoded, const Chunk& chunk)
    {
        // For OP_Constant instructions, pre-compute pointer to constant value
        // This eliminates chunk.constants[index] lookup during execution
        // Turning it from: Value constant = chunk.constants[*ip++]
        // Into: Value constant = *instr.constantPtr (much faster!)

        for (size_t i = 0; i < decoded->instructions.size(); ++i)
        {
            DecodedInstruction& instr = decoded->instructions[i];

            // Fused instructions resolved their constant pointers (if any)
            // during fusion; their head opcode no longer describes them.
            if (decoded->meta[i].fusedLength != 0)
                continue;

            const uint8_t opcode = decoded->meta[i].originalOpcode;

            if (opcode == static_cast<uint8_t>(OpCode::OP_Constant))
            {
                uint8_t constantIndex = instr.operands.byte;
                if (constantIndex < chunk.constants.size())
                {
                    instr.constantPtr = const_cast<Value*>(&chunk.constants[constantIndex]);
                }
            }
            else if (opcode == static_cast<uint8_t>(OpCode::OP_LongConstant))
            {
                uint32_t constantIndex = (instr.operands.indexed.byte1 << 16) |
                                        (instr.operands.indexed.byte2 << 8) |
                                         instr.operands.indexed.byte3;
                if (constantIndex < chunk.constants.size())
                {
                    instr.constantPtr = const_cast<Value*>(&chunk.constants[constantIndex]);
                }
            }
        }
    }

    void ChunkDecoder::resolveGlobalSlots(DecodedChunk* decoded, const Chunk& chunk, VM* vm)
    {
        // The constant-global ops embed the name's constant index. Resolve it
        // once here to a dense VM global slot (stored in operands.dword) so the
        // handler is a single globalCells[] index instead of a string hash. For
        // set/define, also pre-resolve the value constant pointer (the value is
        // a constant; the chunk's constant storage is stable).
        if (vm == nullptr)
            return;

        for (size_t i = 0; i < decoded->instructions.size(); ++i)
        {
            // Constant-global ops are never part of a fusion window.
            if (decoded->meta[i].fusedLength != 0)
                continue;

            DecodedInstruction& instr  = decoded->instructions[i];
            const uint8_t       opcode = decoded->meta[i].originalOpcode;

            if (opcode == static_cast<uint8_t>(OpCode::OP_Get_Constant_Global))
            {
                const uint8_t nameIdx = instr.operands.byte;
                if (nameIdx >= chunk.constants.size())
                    continue;
                const std::string name = vm->asString(chunk.constants[nameIdx], chunk);
                instr.operands.dword = vm->globalSlot(name); // overwrites nameIdx
            }
            else if (opcode == static_cast<uint8_t>(OpCode::OP_Set_Constant_Global)
                  or opcode == static_cast<uint8_t>(OpCode::OP_Define_Constant_Global))
            {
                // byte1 = value const idx, byte2 = name const idx. Read both
                // before overwriting operands.dword (which aliases them).
                const uint8_t valueIdx = instr.operands.indexed.byte1;
                const uint8_t nameIdx  = instr.operands.indexed.byte2;
                if (valueIdx >= chunk.constants.size() or nameIdx >= chunk.constants.size())
                    continue;
                const std::string name = vm->asString(chunk.constants[nameIdx], chunk);
                const uint32_t slot = vm->globalSlot(name);
                instr.constantPtr    = const_cast<Value*>(&chunk.constants[valueIdx]);
                instr.operands.dword = slot;
            }
        }
    }

    void ChunkDecoder::resolveJumpTargets(DecodedChunk* decoded)
    {
        // For control flow instructions (jumps and loops), resolve the
        // target instruction POINTER. Jump handlers return it straight to
        // the dispatch loop — no offset mapping or indexing at runtime.
        // The instructions vector is final at this point (decode() appends
        // nothing after the synthetic-halt step), so the pointers are stable.

        for (size_t i = 0; i < decoded->instructions.size(); ++i)
        {
            DecodedInstruction&           instr = decoded->instructions[i];
            const DecodedInstructionMeta& meta  = decoded->meta[i];

            // Fused instructions resolved their branch targets (if any)
            // during fusion.
            if (meta.fusedLength != 0)
                continue;

            size_t targetBytecodeOffset = 0;
            if (not branchTargetOffsetOf(instr, meta, targetBytecodeOffset))
                continue;  // Not a control flow instruction

            // Resolve to an instruction pointer in the decoded array.
            auto it = decoded->jumpTargets.find(targetBytecodeOffset);
            if (it != decoded->jumpTargets.end())
            {
                instr.jumpTargetPtr = decoded->instructions.data() + it->second;
            }
            else
            {
                // Parity with the old index-0 fallback; a registered
                // control-flow op must never carry a null target.
                LOG_ERROR("ChunkDecoder", "Unresolved jump target at bytecode offset "
                          << meta.bytecodeOffset << " (target " << targetBytecodeOffset << ")");
                instr.jumpTargetPtr = decoded->instructions.data();
            }
        }
    }

} // namespace pg
