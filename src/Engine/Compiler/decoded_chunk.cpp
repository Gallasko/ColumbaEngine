#include "stdafx.h"
#include "decoded_chunk.h"
#include "vm.h"
#include "logger.h"
#include "compiler_debug.h"
#include "decoded_fusion.h"

#include <array>
#include <map>
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

        // Decode all instructions and build jump target map (an entry for every offset, one past the end too)
        decoded->jumpTargets.indexOf.assign(chunk.code.size() + 1, OffsetIndex::none);

        size_t offset = 0;
        size_t instructionIndex = 0;

        while (offset < chunk.code.size())
        {
            // Record jump target mapping (bytecode offset to instruction index)
            // This is needed for ALL control flow instructions:
            // OP_Jump, OP_Long_Jump, OP_Jump_If_False, OP_Long_Jump_If_False, OP_Loop, OP_Long_Loop
            decoded->jumpTargets.set(offset, instructionIndex);

            size_t nextOffset;
            DecodedInstruction instr;
            DecodedInstructionMeta meta;
            decodeInstruction(chunk, offset, vm, instr, meta, nextOffset);

            decoded->instructions.push_back(instr);
            decoded->meta.push_back(meta);
            offset = nextOffset;
            instructionIndex++;
        }

        // Each field access and table literal its own cache, pointed at by the instruction (the name
        // the decoder resolved moves into the field access's)
        for (size_t i = 0; i < decoded->instructions.size(); ++i)
        {
            const auto op = static_cast<OpCode>(decoded->meta[i].originalOpcode);
            DecodedInstruction& at = decoded->instructions[i];

            if (op == OpCode::OP_Get_Property or op == OpCode::OP_Set_Property)
            {
                decoded->propertyCaches.emplace_back();
                decoded->propertyCaches.back().name = at.propertyNamePtr;
                at.propertyCache = &decoded->propertyCaches.back();
            }
            else if (op == OpCode::OP_Build_Table)
            {
                decoded->literalCaches.emplace_back();
                at.literalCache = &decoded->literalCaches.back();
            }
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
            decoded->jumpTargets.set(chunk.code.size(), decoded->instructions.size() - 1);
            instructionIndex++;
        }

        decoded->totalInstructions = instructionIndex;

        // Decode-time superinstruction fusion (must run before the pointer
        // resolvers: it compacts the arrays and remaps jumpTargets).
        if (vm != nullptr and vm->enableDecodeFusion)
        {
            fuseInstructions(decoded, chunk, vm);
            fuseIndexedAccess(decoded);
            fuseDeferredOperands(decoded, chunk, vm);
            compactElided(decoded);
        }

        // Optimize: pre-resolve constant pointers
        resolveConstantPointers(decoded, chunk);

        resolveJumpTargets(decoded);

        // Optimize: pre-resolve constant-global ops to dense VM global slots.
        resolveGlobalSlots(decoded, chunk, vm);

        if (vm != nullptr and vm->enableDecodeFusion)
            fuseLenCalls(decoded, vm);

        if (vm != nullptr and vm->enableDecodeFusion and not vm->profiler.isEnabled())
            pairInstructions(decoded, vm);

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
                case fusion::Producer::NoneValue:
                    out.kind = fusion::Src::Const;
                    out.constIndex = fusion::kNoneConstIndex;
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
                if (constSrc->constIndex == kNoneConstIndex)
                    out.constantPtr = &noneConstant;
                else if (constSrc->constIndex >= chunk.constants.size())
                    return false;
                else
                    out.constantPtr = const_cast<Value*>(&chunk.constants[constSrc->constIndex]);
            }
            return true;
        };

        // In-place compound assignment: when the store destination is also a
        // Local source (`x = x <op> operand`, or `x = operand <op> x` for a
        // commutative op), collapse to a single read-modify-write handler that
        // updates the slot in place. A commutative op with an ordered case (add
        // on strings) takes a swapped handler that keeps the operand on the left
        // for that case only. `outB` reports the operand source used
        // (for the profiler name). Returns false when no in-place form applies.
        auto buildCompound = [&](BinOp op, const SrcDesc& a, const SrcDesc& b,
                                 uint8_t dstSlot, DecodedInstruction& out, Src& outB) -> bool
        {
            const SrcDesc* other = nullptr; // the operand that is not the destination (byte2 / const)
            bool swapped = false; // other is the left operand in the source
            if (a.kind == Src::Local and a.byteVal == dstSlot)
                other = &b;
            else if (isCommutative(op) and b.kind == Src::Local and b.byteVal == dstSlot)
            {
                other = &a;
                swapped = true;
            }
            else
                return false;

            OpDecodedHandler handler = swapped ? selectFusedLocalCompoundSwapped(op, other->kind) : selectFusedLocalCompound(op, other->kind);
            if (handler == nullptr)
                return false;

            out = DecodedInstruction{};
            out.decodedHandler = handler;
            out.operands.indexed.byte1 = dstSlot;
            out.operands.indexed.byte2 = other->byteVal;
            if (other->kind == Src::Const)
            {
                if (other->constIndex == kNoneConstIndex)
                    out.constantPtr = &noneConstant;
                else if (other->constIndex >= chunk.constants.size())
                    return false;
                else
                    out.constantPtr = const_cast<Value*>(&chunk.constants[other->constIndex]);
            }
            outB = other->kind;
            return true;
        };

        // A store consumer is either the fused OP_Set_Local_Pop (legacy
        // bytecode) or the plain OP_Set_Local + OP_Pop pair the compiler
        // emits now that the bytecode-level fusion passes are gone. That Pop may
        // have merged with those of the locals a body ends on: the store then
        // takes one off the PopN that stays (`popN`: its index, 0 for none). A
        // jump landing on it comes with all of it to pop, so it is left whole.
        struct StoreConsumer { bool found = false; uint8_t dst = 0; size_t len = 0; size_t popN = 0; };
        auto storeConsumerAt = [&](size_t idx) -> StoreConsumer {
            if (idx >= n)
                return {};
            if (isStoreConsumer(im[idx].originalOpcode))
                return {true, in[idx].operands.byte, 1};
            if (idx + 1 < n
                and im[idx].originalOpcode == static_cast<uint8_t>(OpCode::OP_Set_Local)
                and im[idx + 1].originalOpcode == static_cast<uint8_t>(OpCode::OP_Pop))
                return {true, in[idx].operands.byte, 2};
            if (idx + 1 < n
                and im[idx].originalOpcode == static_cast<uint8_t>(OpCode::OP_Set_Local)
                and im[idx + 1].originalOpcode == static_cast<uint8_t>(OpCode::OP_PopN)
                and in[idx + 1].operands.byte >= 2
                and not isTarget(idx + 1))
                return {true, in[idx].operands.byte, 1, idx + 1};
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
            size_t  reducedPopN = 0; // StoreConsumer::popN of the store taken

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
                        reducedPopN = sc.popN;
                        return true;
                    }
                    if (buildBinary(op, a, b, Sink::Store, sc.dst, fused))
                    {
                        windowLen = headLen + sc.len;
                        fusedName = fusionName(op, a.kind, b.kind, Sink::Store);
                        winPops = stackSrcCount(a.kind, b.kind);
                        winPushes = 0;
                        reducedPopN = sc.popN;
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
                        const uint32_t target = decoded->jumpTargets.at(branchTarget);
                        const ptrdiff_t rel = (target == OffsetIndex::none)
                            ? PTRDIFF_MAX
                            : static_cast<ptrdiff_t>(target) - static_cast<ptrdiff_t>(i);
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
            //    handler (replaces the removed SetLocalPopFusion bytecode pass),
            //    the Set_Local alone when its Pop merged into a PopN
            if (windowLen == 0 and im[i].originalOpcode == static_cast<uint8_t>(OpCode::OP_Set_Local))
            {
                const StoreConsumer sc = storeConsumerAt(i);

                if (sc.found and windowSafe(i, sc.len))
                {
                    fused = DecodedInstruction{};
                    fused.decodedHandler = op_set_local_pop_decoded;
                    fused.operands.byte = in[i].operands.byte;
                    windowLen = sc.len;
                    winPops = 1;
                    winPushes = 0;
                    reducedPopN = sc.popN;
                    fusedName = internFusionName("FUSED_SetLocalPop");
                }
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

            // H) Pop or PopN + the increment G fuses. A `continue` lands on the
            //    increment, after popping for itself: windowSafe then keeps them
            //    apart. A Pop right after a Set_Index is the one fuseIndexedAccess
            //    absorbs (of a PopN, it takes one off the count). The release can
            //    run anything a Pop can: a barrier for that sweep, as the Pop is.
            const bool plainPop = im[i].originalOpcode == static_cast<uint8_t>(OpCode::OP_Pop);
            const bool popN = im[i].originalOpcode == static_cast<uint8_t>(OpCode::OP_PopN);
            if (windowLen == 0 and i + 2 < n and (popN or plainPop)
                and not (plainPop and i > 0 and im[i - 1].originalOpcode == static_cast<uint8_t>(OpCode::OP_Set_Index))
                and im[i + 1].originalOpcode == static_cast<uint8_t>(OpCode::OP_Short_Int)
                and windowSafe(i, 3))
            {
                const uint8_t next = im[i + 2].originalOpcode;
                const bool incr = (next == static_cast<uint8_t>(OpCode::OP_Post_Incr_Local));
                const bool decr = (next == static_cast<uint8_t>(OpCode::OP_Post_Decr_Local));
                if (incr or decr)
                {
                    fused = DecodedInstruction{};
                    fused.decodedHandler = incr ? &fusedPopIncrDecrLocal<true>
                                                : &fusedPopIncrDecrLocal<false>;
                    fused.operands.indexed.byte1 = in[i + 1].operands.byte; // slot
                    fused.operands.indexed.byte2 = popN ? in[i].operands.byte : 1;
                    windowLen = 3;
                    fusedName = internFusionName(incr ? "FUSED_Pop+IncrLocal"
                                                      : "FUSED_Pop+DecrLocal");
                }
            }

            // I) Get_Local + Get_Property → a read of the field through the slot.
            //    Its effect stays unknown: __get may run, the indexed sweep must
            //    stop here.
            if (windowLen == 0 and p1 == Producer::Local and i + 1 < n
                and im[i + 1].originalOpcode == static_cast<uint8_t>(OpCode::OP_Get_Property)
                and windowSafe(i, 2))
            {
                fused = DecodedInstruction{};
                fused.decodedHandler = &fusedGetPropertyLocal;
                fused.propertyCache = in[i + 1].propertyCache;
                fused.operands.indexed.byte1 = in[i].operands.byte; // slot
                windowLen = 2;
                fusedName = internFusionName("FUSED_GetProperty(L)");
            }

            // --- Emit -------------------------------------------------------
            // (a store whose Pop is in a PopN: one instruction, the Set_Local alone)
            if (windowLen >= 2 or reducedPopN != 0)
            {
                // Not reached yet: it is read from `in` with its new count
                if (reducedPopN != 0)
                    decoded->instructions[reducedPopN].operands.byte -= 1;

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
        for (auto& index : decoded->jumpTargets.indexOf)
        {
            if (index != OffsetIndex::none)
                index = static_cast<uint32_t>(oldToNew[index]);
        }

        decoded->instructions = std::move(outI);
        decoded->meta         = std::move(outM);
        decoded->totalInstructions = decoded->instructions.size();

        // 3. Resolve fused branch targets now that the array is final.
        for (const auto& fx : branchFixups)
        {
            decoded->fusedBranches.push_back({fx.newIndex, fx.relative});

            DecodedInstruction& di = decoded->instructions[fx.newIndex];
            const uint32_t target = decoded->jumpTargets.at(fx.targetOffset);
            if (target == OffsetIndex::none)
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
                const ptrdiff_t rel = static_cast<ptrdiff_t>(target)
                                    - static_cast<ptrdiff_t>(fx.newIndex);
                di.operands.indexed.byte3 = static_cast<uint8_t>(rel & 0xFF);
                di.operands.indexed.byte4 = static_cast<uint8_t>((rel >> 8) & 0xFF);
            }
            else
            {
                di.jumpTargetPtr = decoded->instructions.data() + target;
            }

            // The branch's opcode/operands are fused away, so later passes
            // can't recover this target — record the index explicitly.
            decoded->fusedBranchTargets.push_back(target);
        }

        LOG_INFO("ChunkDecoder", "Fused " << fusedWindows << " windows ("
                 << n << " -> " << decoded->instructions.size() << " instructions)");
    }

    // Second fusion sweep, over the already-fused stream: indexed accesses
    // whose TARGET is a plain local.
    //
    //   Get_Local v; <pure index expr>; Get_Index        → nop; expr; FUSED_GetIndex(L,S)
    //   Get_Local v; Get_Local i; Get_Index              → nop; nop; FUSED_GetIndex(L,L)
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
    // Elided instructions are only marked here, keeping instruction indices
    // and resolved branch pointers stable for the sweep; compactElided takes
    // them out once it is done.
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
            if (ins[idx].decodedHandler == &fusedGetIndexLocals)
            {
                pops = 0; pushes = 1;
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
                case OpCode::OP_None:
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
                    // The index is a local right there: neither goes through the stack
                    if (j == i + 2 and im[i + 1].fusedLength < 2 and ins[i + 1].decodedHandler == &op_get_local_decoded)
                    {
                        ins[j].decodedHandler = &fusedGetIndexLocals;
                        ins[j].operands.indexed.byte2 = ins[i + 1].operands.byte;
                        ins[i + 1].decodedHandler = &fusedElidedProducer;
                        im[i + 1].fusedName = internFusionName("ELIDED_Producer");
                        im[j].fusedName = internFusionName("FUSED_GetIndex(L,L)");
                    }
                    else
                    {
                        ins[j].decodedHandler = &fusedGetIndexLocal;
                        im[j].fusedName = internFusionName("FUSED_GetIndex(L,S)");
                    }

                    ins[j].operands.indexed.byte1 = slot;
                    ins[i].decodedHandler = &fusedElidedProducer;
                    im[i].fusedName = internFusionName("ELIDED_Producer");
                    ++rewrites;
                    break;
                }

                // Write access: handle below index + value; the generic
                // Set_Index leaves the handle on the stack for the trailing
                // Pop — both are absorbed by the fused form. A Pop merged with
                // those of the locals that follow comes off their count.
                const bool setIndex = plain and op == OpCode::OP_Set_Index and depth == 2
                    and ins[j].decodedHandler == &op_set_index_decoded
                    and j + 1 < n and not isTarget(j + 1);

                const bool lonePop = setIndex and im[j + 1].fusedLength < 2
                    and static_cast<OpCode>(im[j + 1].originalOpcode) == OpCode::OP_Pop;

                uint8_t* popCount = nullptr;

                if (setIndex and ins[j + 1].decodedHandler == &op_pop_n_decoded)
                    popCount = &ins[j + 1].operands.byte;
                else if (setIndex and (ins[j + 1].decodedHandler == &fusedPopIncrDecrLocal<true>
                                       or ins[j + 1].decodedHandler == &fusedPopIncrDecrLocal<false>))
                    popCount = &ins[j + 1].operands.indexed.byte2;

                if (lonePop or (popCount != nullptr and *popCount >= 2))
                {
                    ins[j].decodedHandler = &fusedSetIndexLocal;
                    ins[j].operands.indexed.byte1 = slot;
                    im[j].fusedName = internFusionName("FUSED_SetIndex(L,S,S)");

                    if (lonePop)
                    {
                        ins[j + 1].decodedHandler = &fusedElidedProducer;
                        im[j + 1].fusedName = internFusionName("ELIDED_Pop");
                    }
                    else
                        *popCount -= 1;

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

        // What is left: an index that is a local, on a target that is not (the row of `t[a][i]`).
        //   Get_Local i; Get_Index → nop; FUSED_GetIndex(S,L)
        // A branch landing on the Get_Local lands on the fused read, which does its work too; none
        // may land between the two
        for (size_t i = 0; i + 1 < n; ++i)
        {
            if (im[i].fusedLength >= 2 or ins[i].decodedHandler != &op_get_local_decoded)
                continue;

            const size_t j = i + 1;

            if (im[j].fusedLength >= 2 or ins[j].decodedHandler != &op_get_index_decoded or isTarget(j))
                continue;

            ins[j].decodedHandler = &fusedGetIndexByLocal;
            ins[j].operands.indexed.byte1 = ins[i].operands.byte;
            im[j].fusedName = internFusionName("FUSED_GetIndex(S,L)");

            ins[i].decodedHandler = &fusedElidedProducer;
            im[i].fusedName = internFusionName("ELIDED_Producer");
            ++rewrites;
        }

        if (rewrites > 0)
            LOG_INFO("ChunkDecoder", "Fused " << rewrites << " local indexed accesses");
    }

    // Third sweep, after fuseIndexedAccess: a local, a constant or a short int pushed before the
    // right-hand side of the op that takes it, the deeper of its two operands.
    //
    //   Get_Local x; <expr>; Add                    → nop; expr; FUSED_Add(L,S)->Push
    //   Get_Local x; <expr>; FUSED_Add(S,S)->Store x → nop; expr; FUSED_Add(L=,S)->Local
    //   Constant k;  <expr>; Divide                 → nop; expr; FUSED_Divide(C,S)->Push
    //   Get_Local o; <expr>; Set_Property f; Pop    → nop; expr; FUSED_SetProperty(L,S); Pop
    //
    // The op reads the operand where it is, after the expression has run, which is the same value
    // when nothing in the expression writes it: a constant never changes; a local only by a store
    // (none passes the scan) or by a closure that took it, so a call (or what may run script code:
    // a property read) is let through only for a constant or a local no closure of this function
    // takes. Like fuseIndexedAccess, the scan follows the stack depth above the operand and stops at
    // anything of unknown effect, anything digging below it and a branch target inside.
    void ChunkDecoder::fuseDeferredOperands(DecodedChunk* decoded, const Chunk& chunk, VM* vm)
    {
        using namespace fusion;

        const size_t n = decoded->instructions.size();
        if (n < 3)
            return;

        std::vector<DecodedInstruction>&     ins = decoded->instructions;
        std::vector<DecodedInstructionMeta>& im  = decoded->meta;

        auto opcodeOf = [&](size_t idx) { return static_cast<OpCode>(im[idx].originalOpcode); };
        auto plainAt = [&](size_t idx) { return im[idx].fusedLength < 2; };

        // The locals a closure of this function takes: read from each OP_Closure's upvalue pairs in
        // the bytecode (op_closure_decoded reads them there too). One unreadable: all are taken
        std::vector<bool> captured(256, false);
        for (size_t k = 0; k < n; ++k)
        {
            if (not plainAt(k) or opcodeOf(k) != OpCode::OP_Closure)
                continue;

            const uint8_t constantIndex = ins[k].operands.byte;
            const bool readable = constantIndex < chunk.constants.size() and IS_FUNC(chunk.constants[constantIndex]);
            ObjFunction* function = readable ? vm->asFunction(chunk.constants[constantIndex]) : nullptr;

            if (function == nullptr or im[k].bytecodeOffset + 2 + 2 * function->upvalueCount > chunk.code.size())
            {
                std::fill(captured.begin(), captured.end(), true);
                break;
            }

            for (int u = 0; u < function->upvalueCount; ++u)
            {
                const size_t at = im[k].bytecodeOffset + 2 + 2 * u;
                if (chunk.code[at] != 0)
                    captured[chunk.code[at + 1]] = true;
            }
        }

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

        // The stack effect of what may stand between the operand and its op, false for anything
        // else. `callsSafe`: the operand cannot change in a call (a constant, a local no closure takes)
        auto effectOf = [&](size_t idx, bool callsSafe, uint8_t& pops, uint8_t& pushes) -> bool {
            const OpDecodedHandler h = ins[idx].decodedHandler;

            if (h == &fusedElidedProducer)                         { pops = 0; pushes = 0; return true; }
            if (h == &fusedGetIndexLocal or h == &fusedGetIndexByLocal) { pops = 1; pushes = 1; return true; }
            if (h == &fusedGetIndexLocals)                         { pops = 0; pushes = 1; return true; }
            if (h == &fusedGetPropertyLocal)                       { pops = 0; pushes = 1; return callsSafe; }

            if (not plainAt(idx))
            {
                // Reads that push their result: binary ops, unary ops, Len. A store, a step or a
                // branch writes or leaves; they push nothing
                if (im[idx].stackPops == 0xFF or im[idx].stackPushes != 1)
                    return false;
                pops = im[idx].stackPops;
                pushes = 1;
                return true;
            }

            switch (opcodeOf(idx))
            {
                case OpCode::OP_Get_Local:
                case OpCode::OP_Constant:
                case OpCode::OP_Short_Int:
                case OpCode::OP_True:
                case OpCode::OP_False:
                case OpCode::OP_None:
                case OpCode::OP_Get_Upvalue:
                case OpCode::OP_Get_Constant_Global:
                case OpCode::OP_Get_Long_Constant_Global:
                    pops = 0; pushes = 1;
                    return true;
                case OpCode::OP_Not:
                case OpCode::OP_Negate:
                    pops = 1; pushes = 1;
                    return true;
                case OpCode::OP_Get_Index:
                    pops = 2; pushes = 1;
                    return true;
                case OpCode::OP_Get_Property:
                    pops = 1; pushes = 1;
                    return callsSafe;
                case OpCode::OP_Call:
                    pops = static_cast<uint8_t>(ins[idx].operands.byte + 1); pushes = 1;
                    return callsSafe and ins[idx].operands.byte < 0xFF;
                case OpCode::OP_Invoke:
                    pops = static_cast<uint8_t>(ins[idx].operands.indexed.byte2 + 1); pushes = 1;
                    return callsSafe and ins[idx].operands.indexed.byte2 < 0xFF;
                default:
                    if (classifyBinary(im[idx].originalOpcode) != BinOp::None)
                    {
                        pops = 2; pushes = 1;
                        return true;
                    }
                    return false;
            }
        };

        // The base op of a fused (S,S) instruction with this handler and sink, BinOp::None if not one
        auto stackStackOp = [&](size_t idx, Sink sink) -> BinOp {
            const BinOp op = classifyBinary(im[idx].originalOpcode);
            if (op == BinOp::None or plainAt(idx))
                return BinOp::None;
            return ins[idx].decodedHandler == selectFusedBinary(op, Src::Stack, Src::Stack, sink) ? op : BinOp::None;
        };

        auto elide = [&](size_t idx, const char* name) {
            ins[idx].decodedHandler = &fusedElidedProducer;
            im[idx].fusedName = internFusionName(name);
        };

        constexpr size_t MaxWindow = 32;
        size_t rewrites = 0;

        for (size_t i = 0; i + 2 < n; ++i)
        {
            if (not plainAt(i))
                continue;

            SrcDesc a;
            if (ins[i].decodedHandler == &op_get_local_decoded)
            {
                a.kind = Src::Local;
                a.byteVal = ins[i].operands.byte;
            }
            else if (opcodeOf(i) == OpCode::OP_Short_Int)
            {
                a.kind = Src::ShortInt;
                a.byteVal = ins[i].operands.byte;
            }
            else if (opcodeOf(i) == OpCode::OP_Constant and ins[i].operands.byte < chunk.constants.size())
            {
                a.kind = Src::Const;
                a.constIndex = ins[i].operands.byte;
            }
            else
                continue;

            const bool callsSafe = a.kind != Src::Local or not captured[a.byteVal];
            Value* constantPtr = a.kind == Src::Const ? const_cast<Value*>(&chunk.constants[a.constIndex]) : nullptr;

            int depth = 0; // values above the operand's place

            for (size_t j = i + 1; j < n and (j - i) <= MaxWindow; ++j)
            {
                if (isTarget(j))
                    break;

                // The op that takes the operand: the value under the one just made
                if (depth == 1)
                {
                    const BinOp plainOp = plainAt(j) ? classifyBinary(im[j].originalOpcode) : BinOp::None;
                    OpDecodedHandler handler = nullptr;
                    const std::string* name = nullptr;
                    uint8_t pushes = 1;

                    if (plainOp != BinOp::None)
                    {
                        handler = selectFusedBinary(plainOp, a.kind, Src::Stack, Sink::Push);
                        name = fusionName(plainOp, a.kind, Src::Stack, Sink::Push);
                        if (handler != nullptr)
                        {
                            ins[j].decodedHandler = handler;
                            ins[j].operands.indexed.byte1 = a.byteVal;
                            if (constantPtr != nullptr)
                                ins[j].constantPtr = constantPtr;
                        }
                    }
                    else if (BinOp op = stackStackOp(j, Sink::Store); op != BinOp::None)
                    {
                        const uint8_t dst = ins[j].operands.indexed.byte3;
                        pushes = 0;

                        if (a.kind == Src::Local and a.byteVal == dst)
                        {
                            // `x = x <op> expr`: the slot read and written in place
                            handler = selectFusedLocalCompound(op, Src::Stack);
                            name = compoundFusionName(op, Src::Stack);
                            if (handler != nullptr)
                            {
                                ins[j].decodedHandler = handler;
                                ins[j].operands.indexed.byte1 = dst;
                            }
                        }
                        else
                        {
                            handler = selectFusedBinary(op, a.kind, Src::Stack, Sink::Store);
                            name = fusionName(op, a.kind, Src::Stack, Sink::Store);
                            if (handler != nullptr)
                            {
                                ins[j].decodedHandler = handler;
                                ins[j].operands.indexed.byte1 = a.byteVal;
                                if (constantPtr != nullptr)
                                    ins[j].constantPtr = constantPtr;
                            }
                        }
                    }
                    else if (a.kind != Src::Const)
                    {
                        // A compare and branch: its target pointer stays (no constant takes it)
                        for (Sink sink : {Sink::BranchIfFalse, Sink::BranchIfTrue})
                        {
                            if (BinOp op = stackStackOp(j, sink); op != BinOp::None)
                            {
                                handler = selectFusedBinary(op, a.kind, Src::Stack, sink);
                                name = fusionName(op, a.kind, Src::Stack, sink);
                                pushes = 0;
                                if (handler != nullptr)
                                {
                                    ins[j].decodedHandler = handler;
                                    ins[j].operands.indexed.byte1 = a.byteVal;
                                }
                                break;
                            }
                        }

                        // `o.f = expr;`: the Pop after stays, the value is still left for it
                        if (handler == nullptr and a.kind == Src::Local and plainAt(j)
                            and opcodeOf(j) == OpCode::OP_Set_Property
                            and j + 1 < n and plainAt(j + 1) and opcodeOf(j + 1) == OpCode::OP_Pop)
                        {
                            handler = &fusedSetPropertyLocal;
                            name = internFusionName("FUSED_SetProperty(L,S)");
                            ins[j].decodedHandler = handler;
                            ins[j].operands.indexed.byte2 = a.byteVal;
                        }
                    }

                    if (handler != nullptr)
                    {
                        im[j].fusedLength = std::max<uint8_t>(im[j].fusedLength, 2);
                        im[j].fusedName = name;
                        im[j].stackPops = 1;
                        im[j].stackPushes = pushes;
                        elide(i, "ELIDED_Producer");
                        ++rewrites;
                        break;
                    }
                }

                uint8_t pops = 0, pushes = 0;
                if (not effectOf(j, callsSafe, pops, pushes))
                    break;
                if (depth < static_cast<int>(pops))
                    break; // would take the operand (or dig below it)
                depth += static_cast<int>(pushes) - static_cast<int>(pops);
            }
        }

        if (rewrites > 0)
            LOG_INFO("ChunkDecoder", "Fused " << rewrites << " deferred operands");
    }

    namespace
    {
        using HandlerPair = std::pair<OpDecodedHandler, OpDecodedHandler>;
        using PairRules = std::map<HandlerPair, OpDecodedHandler>;

        template <OpDecodedHandler First, OpDecodedHandler Next>
        void pairOf(PairRules& rules)
        {
            rules[{First, Next}] = &fusion::fusedThen<First, Next>;
        }

        // A loop's step, then its test at the bottom of the loop (LoopRotation): `i++` and `i < n`
        template <OpDecodedHandler Step, typename Op>
        void pairStepWithTests(PairRules& rules)
        {
            using fusion::Src;

            pairOf<Step, &fusion::fusedCmpBranchIfTrue<Op, Src::Local, Src::Local, false>>(rules);
            pairOf<Step, &fusion::fusedCmpBranchIfTrue<Op, Src::Local, Src::ShortInt, false>>(rules);
            pairOf<Step, &fusion::fusedCmpBranchIfTrue<Op, Src::Local, Src::Const, true>>(rules);
        }

        template <OpDecodedHandler Step>
        void pairStep(PairRules& rules)
        {
            pairStepWithTests<Step, fusion::FLess>(rules);
            pairStepWithTests<Step, fusion::FLessEqual>(rules);
            pairStepWithTests<Step, fusion::FGreater>(rules);
            pairStepWithTests<Step, fusion::FGreaterEqual>(rules);
            pairStepWithTests<Step, fusion::FNotEqual>(rules);
        }

        // Three that follow each other: a loop's step, the Pop of the locals its body declared (they
        // are popped after the step, which is in the body), its test
        using HandlerTriple = std::array<OpDecodedHandler, 3>;
        using TripleRules = std::map<HandlerTriple, OpDecodedHandler>;

        template <OpDecodedHandler Step, OpDecodedHandler Pop, typename Op>
        void tripleStepPopWithTests(TripleRules& rules)
        {
            using fusion::Src;

            rules[{Step, Pop, &fusion::fusedCmpBranchIfTrue<Op, Src::Local, Src::Local, false>}]
                = &fusion::fusedThen3<Step, Pop, &fusion::fusedCmpBranchIfTrue<Op, Src::Local, Src::Local, false>>;
            rules[{Step, Pop, &fusion::fusedCmpBranchIfTrue<Op, Src::Local, Src::ShortInt, false>}]
                = &fusion::fusedThen3<Step, Pop, &fusion::fusedCmpBranchIfTrue<Op, Src::Local, Src::ShortInt, false>>;
            rules[{Step, Pop, &fusion::fusedCmpBranchIfTrue<Op, Src::Local, Src::Const, true>}]
                = &fusion::fusedThen3<Step, Pop, &fusion::fusedCmpBranchIfTrue<Op, Src::Local, Src::Const, true>>;
        }

        template <OpDecodedHandler Step, OpDecodedHandler Pop>
        void tripleStepPop(TripleRules& rules)
        {
            tripleStepPopWithTests<Step, Pop, fusion::FLess>(rules);
            tripleStepPopWithTests<Step, Pop, fusion::FLessEqual>(rules);
            tripleStepPopWithTests<Step, Pop, fusion::FGreater>(rules);
            tripleStepPopWithTests<Step, Pop, fusion::FGreaterEqual>(rules);
            tripleStepPopWithTests<Step, Pop, fusion::FNotEqual>(rules);
        }

        const TripleRules& tripleRules()
        {
            using namespace fusion;

            static const TripleRules rules = [] {
                TripleRules r;

                tripleStepPop<&fusedIncrDecrLocal<true>, &op_pop_n_decoded>(r);
                tripleStepPop<&fusedIncrDecrLocal<false>, &op_pop_n_decoded>(r);
                tripleStepPop<&fusedIncrDecrLocal<true>, &op_pop_decoded>(r);
                tripleStepPop<&fusedIncrDecrLocal<false>, &op_pop_decoded>(r);

                return r;
            }();

            return rules;
        }

        const PairRules& pairRules()
        {
            using namespace fusion;

            static const PairRules rules = [] {
                PairRules r;

                pairStep<&fusedIncrDecrLocal<true>>(r);
                pairStep<&fusedIncrDecrLocal<false>>(r);
                pairStep<&fusedPopIncrDecrLocal<true>>(r);
                pairStep<&fusedPopIncrDecrLocal<false>>(r);

                // `t[a][b]`: the row is read where it stands
                r[{&fusedGetIndexLocals, &fusedGetIndexByLocal}] = &fusedGetIndex2Locals;

                // `line[i] == " "`: the element is compared where it stands
                r[{&fusedGetIndexLocals, &fusedCmpBranchIfFalse<FEqual, Src::Stack, Src::Const, true>}] = &fusedGetIndexLocalsCmpConst<FEqual>;
                r[{&fusedGetIndexLocals, &fusedCmpBranchIfFalse<FNotEqual, Src::Stack, Src::Const, true>}] = &fusedGetIndexLocalsCmpConst<FNotEqual>;

                // `return x`, `return 1`, `return a + b`, the none a function ends on
                pairOf<&op_get_local_decoded, &op_return_decoded>(r);
                pairOf<&op_short_int_decoded, &op_return_decoded>(r);
                pairOf<&op_constant_decoded, &op_return_decoded>(r);
                pairOf<&op_true_decoded, &op_return_decoded>(r);
                pairOf<&op_false_decoded, &op_return_decoded>(r);
                pairOf<&op_none_decoded, &op_return_decoded>(r);
                pairOf<&op_add_decoded, &op_return_decoded>(r);
                pairOf<&op_subtract_decoded, &op_return_decoded>(r);
                pairOf<&op_multiply_decoded, &op_return_decoded>(r);
                pairOf<&fusedGetPropertyLocal, &op_return_decoded>(r);

                // `o.f = x;`: the write and its Pop (a __set it runs resumes at the Pop)
                pairOf<&fusedSetPropertyLocal, &op_pop_decoded>(r);

                return r;
            }();

            return rules;
        }
    }

    void ChunkDecoder::pairInstructions(DecodedChunk* decoded, VM* vm)
    {
        std::vector<DecodedInstruction>& ins = decoded->instructions;
        const PairRules& rules = pairRules();
        const TripleRules& triples = tripleRules();

        // The second is looked at as it was decoded: one paired before it changed only its first
        for (size_t i = 0; i + 1 < ins.size(); ++i)
        {
            if (i + 2 < ins.size())
            {
                auto triple = triples.find({ins[i].decodedHandler, ins[i + 1].decodedHandler, ins[i + 2].decodedHandler});

                if (triple != triples.end())
                {
                    ins[i].decodedHandler = triple->second;
                    ++vm->pairedInstructions;
                    continue;
                }
            }

            auto it = rules.find({ins[i].decodedHandler, ins[i + 1].decodedHandler});

            if (it != rules.end())
            {
                ins[i].decodedHandler = it->second;
                ++vm->pairedInstructions;
            }
        }
    }

    void ChunkDecoder::fuseLenCalls(DecodedChunk* decoded, VM* vm)
    {
        std::vector<DecodedInstruction>& ins = decoded->instructions;
        std::vector<DecodedInstructionMeta>& im = decoded->meta;
        size_t rewrites = 0;

        // The argument and the call stay behind it: what jumps or falls to them runs them as they were
        for (size_t i = 0; i + 2 < ins.size(); ++i)
        {
            if (im[i].fusedLength != 0 or ins[i].decodedHandler != &op_get_constant_global_decoded)
                continue;

            if (ins[i + 2].decodedHandler != &op_call_decoded or ins[i + 2].operands.byte != 1)
                continue;

            const std::string name = vm->globalNameForSlot(ins[i].operands.dword);

            if (name != "len" and name != "__len")
                continue;

            if (ins[i + 1].decodedHandler == &op_get_local_decoded)
            {
                ins[i].decodedHandler = &fusion::fusedLen<false>;
                im[i].fusedName = fusion::internFusionName("FUSED_Len(L)");
            }
            else if (ins[i + 1].decodedHandler == &fusion::fusedGetIndexLocals)
            {
                ins[i].decodedHandler = &fusion::fusedLen<true>;
                im[i].fusedName = fusion::internFusionName("FUSED_Len(L[L])");
            }
            else
                continue;

            ++rewrites;
        }

        if (rewrites > 0)
            LOG_INFO("ChunkDecoder", "Fused " << rewrites << " len calls");
    }

    void ChunkDecoder::compactElided(DecodedChunk* decoded)
    {
        using namespace fusion;

        std::vector<DecodedInstruction>&     ins = decoded->instructions;
        std::vector<DecodedInstructionMeta>& im  = decoded->meta;

        const size_t n = ins.size();

        // Where each instruction ends up. An elided one maps to the instruction after it, which is
        // where a branch to it was going to fall through to anyway (the last one always stays: it
        // is the terminator, never elided)
        std::vector<uint32_t> oldToNew(n, 0);
        uint32_t kept = 0;

        for (size_t i = 0; i < n; ++i)
        {
            oldToNew[i] = kept;

            if (ins[i].decodedHandler != &fusedElidedProducer)
                ++kept;
        }

        if (kept == n)
            return;

        // Branches first, while they still sit at their old index. Instructions only move down
        // inside the same array, so its pointers stay valid.
        for (auto& branch : decoded->fusedBranches)
        {
            DecodedInstruction& di = ins[branch.index];
            const uint32_t newIndex = oldToNew[branch.index];

            if (branch.relative)
            {
                const int16_t distance = static_cast<int16_t>(di.operands.indexed.byte3
                                                           | (di.operands.indexed.byte4 << 8));
                const uint32_t target = oldToNew[static_cast<ptrdiff_t>(branch.index) + distance];

                // Only ever shorter than the distance it replaces: it fits
                const ptrdiff_t moved = static_cast<ptrdiff_t>(target) - static_cast<ptrdiff_t>(newIndex);
                di.operands.indexed.byte3 = static_cast<uint8_t>(moved & 0xFF);
                di.operands.indexed.byte4 = static_cast<uint8_t>((moved >> 8) & 0xFF);
            }
            else
            {
                di.jumpTargetPtr = ins.data() + oldToNew[di.jumpTargetPtr - ins.data()];
            }

            branch.index = newIndex;
        }

        for (auto& target : decoded->fusedBranchTargets)
            target = oldToNew[target];

        for (auto& index : decoded->jumpTargets.indexOf)
        {
            if (index != OffsetIndex::none)
                index = oldToNew[index];
        }

        for (size_t i = 0; i < n; ++i)
        {
            if (ins[i].decodedHandler == &fusedElidedProducer)
                continue;

            ins[oldToNew[i]] = ins[i];
            im[oldToNew[i]]  = im[i];
        }

        ins.resize(kept);
        im.resize(kept);

        decoded->totalInstructions = kept;

        LOG_INFO("ChunkDecoder", "Compacted " << (n - kept) << " elided instructions");
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
            else if (opcode == static_cast<uint8_t>(OpCode::OP_Get_Long_Constant_Global))
            {
                // Three index bytes, high first (OP_LongConstant's): read before the slot overwrites them
                const uint32_t nameIdx = (static_cast<uint32_t>(instr.operands.indexed.byte1) << 16) |
                                         (static_cast<uint32_t>(instr.operands.indexed.byte2) << 8) |
                                          static_cast<uint32_t>(instr.operands.indexed.byte3);
                if (nameIdx >= chunk.constants.size())
                    continue;
                const std::string name = vm->asString(chunk.constants[nameIdx], chunk);
                instr.operands.dword = vm->globalSlot(name);
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
            const uint32_t target = decoded->jumpTargets.at(targetBytecodeOffset);
            if (target != OffsetIndex::none)
            {
                instr.jumpTargetPtr = decoded->instructions.data() + target;
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
