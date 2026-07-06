#pragma once

// ===========================================================================
// Decode-time instruction fusion (superinstructions without opcodes).
//
// Each base op is classified once by its stack effect (producer / pure
// binary / unary / consumer). The fusion pass in decoded_chunk.cpp walks the
// decoded instruction stream with these classifications and collapses
// windows like
//
//      Get_Local a; Get_Local b; Add                  → fused<Add, L, L, Push>
//      Get_Local a; Constant  c; Add; Set_Local_Pop d → fused<Add, L, C, Store>
//      Get_Local a; Get_Local b; Less; Jump_If_False_Popping
//                                                     → fused<Less, L, L, BranchIfFalse>
//
// into ONE DecodedInstruction whose handler is a template instantiation
// below. No bytecode opcode exists for these — they live purely in the
// decoded form (Ertl & Gregg's "dynamic superinstructions").
//
// Encoding contract (DecodedInstruction is 24 bytes, see decoded_chunk.h):
//   operands.indexed.byte1 = source A (local slot / short-int literal)
//   operands.indexed.byte2 = source B (local slot / short-int literal)
//   operands.indexed.byte3 = destination slot (Store sink only)
//   union pointer          = constantPtr (one Const source max)
//                            OR jumpTargetPtr (BranchIfFalse sink)
//   ⇒ a fused instruction may use at most ONE Const source, and a branch
//     sink excludes Const sources entirely.
//
// Refcount semantics (ground truth = the legacy fused handlers):
//   Local / Const / ShortInt sources are BORROWED (the retain the original
//   producer did and the release the original consumer did cancel out).
//   A Stack source consumes the value's stack reference, so it is released
//   after use. Results follow the base helpers (addValues etc.) unchanged.
// ===========================================================================

#include "vm.h"
#include "decoded_chunk.h"

#include <mutex>
#include <type_traits>
#include <unordered_set>

namespace pg
{
namespace fusion
{
    // ------------------------------------------------------------------
    // Classification (the "stack effect spec" of each base op)
    // ------------------------------------------------------------------

    enum class Producer : uint8_t { None, Local, Const, ShortInt };

    // BinOp + its classification/trait tables are GENERATED from the kernel
    // spec in /tools/vm_ops_def.pg (see ops_functors.inc note below).
    #include "generated/ops_binop_tables.inc"

    enum class UnOp : uint8_t { None, Not, Negate };

    enum class Sink : uint8_t { Push, Store, BranchIfFalse, BranchIfTrue };

    enum class Src : uint8_t { Stack, Local, Const, ShortInt };

    inline Producer classifyProducer(uint8_t opcode)
    {
        switch (static_cast<OpCode>(opcode))
        {
            case OpCode::OP_Get_Local: return Producer::Local;
            case OpCode::OP_Constant:  return Producer::Const;
            case OpCode::OP_Short_Int: return Producer::ShortInt;
            default:                   return Producer::None;
        }
    }

    inline UnOp classifyUnary(uint8_t opcode)
    {
        switch (static_cast<OpCode>(opcode))
        {
            case OpCode::OP_Not:    return UnOp::Not;
            case OpCode::OP_Negate: return UnOp::Negate;
            default:                return UnOp::None;
        }
    }

    inline bool isStoreConsumer(uint8_t opcode)
    {
        return static_cast<OpCode>(opcode) == OpCode::OP_Set_Local_Pop;
    }

    // Only the POPPING conditional jumps may absorb the condition: the
    // non-popping variants leave the condition on the stack for later use,
    // so eliding the push would change semantics.
    inline bool isPoppingCondBranch(uint8_t opcode)
    {
        return static_cast<OpCode>(opcode) == OpCode::OP_Jump_If_False_Popping
            or static_cast<OpCode>(opcode) == OpCode::OP_Long_Jump_If_False_Popping;
    }

    // The backward popping branch emitted by the loop-rotation pass. Same
    // condition-absorbing property as the false variants, but branches when the
    // comparison is TRUE (back to the loop body).
    inline bool isPoppingTrueCondBranch(uint8_t opcode)
    {
        return static_cast<OpCode>(opcode) == OpCode::OP_Jump_If_True_Popping
            or static_cast<OpCode>(opcode) == OpCode::OP_Long_Jump_If_True_Popping;
    }

    // Legacy fused opcodes (still produced by old serialized bytecode):
    // self-contained binary ops with embedded sources. Re-fusable with a
    // following consumer.
    inline bool classifyLegacyBinary(uint8_t opcode, BinOp& op, Src& a, Src& b)
    {
        switch (static_cast<OpCode>(opcode))
        {
            case OpCode::OP_AddLL:       op = BinOp::Add;       a = Src::Local; b = Src::Local; return true;
            case OpCode::OP_SubtractLL:  op = BinOp::Subtract;  a = Src::Local; b = Src::Local; return true;
            case OpCode::OP_SubtractLC:  op = BinOp::Subtract;  a = Src::Local; b = Src::Const; return true;
            case OpCode::OP_SubtractCL:  op = BinOp::Subtract;  a = Src::Const; b = Src::Local; return true;
            case OpCode::OP_LessLL:      op = BinOp::Less;      a = Src::Local; b = Src::Local; return true;
            case OpCode::OP_LessEqualLL: op = BinOp::LessEqual; a = Src::Local; b = Src::Local; return true;
            default: return false;
        }
    }

    // ------------------------------------------------------------------
    // Op functors — delegate to the existing value semantics (ground truth)
    //
    // The int/int and double/double cases are duplicated inline: the VM
    // helpers live in vm_binary_op.cpp, so without LTO every fused op would
    // pay a cross-TU call plus the full type-dispatch ladder even for the
    // dominant primitive cases. Anything else falls through to the helper,
    // which remains the single source of truth for mixed/complex types.
    // ------------------------------------------------------------------

    // The compiler sometimes outlines these small apply() bodies (isra
    // clones), putting the call back on the hot path — force the issue.
    #if defined(__GNUC__) or defined(__clang__)
        #define PG_FUSION_INLINE inline __attribute__((always_inline))
    #else
        #define PG_FUSION_INLINE inline
    #endif

    // The functor structs are GENERATED from the declarative kernel spec in
    // /tools/vm_ops_def.pg — one entry there renders the fused fast paths,
    // the base stack handlers and the *Values helpers, so fast and slow
    // semantics cannot drift. Regenerate with the GenerateVmOps target.
    #include "generated/ops_functors.inc"

    // applyCond for the branch-sink handlers: comparison functors provide a
    // boxing-free bool; everything else evaluates apply() and truth-tests the
    // result, matching the original `isValueTrue(Op::apply(...))` semantics.
    template <typename Op, typename = void>
    struct HasApplyCond : std::false_type {};

    template <typename Op>
    struct HasApplyCond<Op, std::void_t<decltype(
        Op::applyCond(static_cast<VM*>(nullptr), Value{}, Value{}))>>
        : std::true_type {};

    template <typename Op>
    inline bool applyCondFast(VM* vm, const Value& a, const Value& b)
    {
        if constexpr (HasApplyCond<Op>::value)
            return Op::applyCond(vm, a, b);
        else
            return isValueTrue(Op::apply(vm, a, b));
    }

    // ------------------------------------------------------------------
    // Source readers
    // ------------------------------------------------------------------

    template <Src S>
    inline Value readSrc(VM* vm, const DecodedInstruction& instr, uint8_t byteVal)
    {
        if constexpr (S == Src::Stack)
            return vm->pop();
        else if constexpr (S == Src::Local)
            return vm->currentFrame->slots[byteVal];
        else if constexpr (S == Src::Const)
            return *instr.constantPtr;
        else // Src::ShortInt — the literal lives in the operand byte
            return makeIntValue(byteVal);
    }

    // A Stack source consumes the value's stack reference.
    template <Src S>
    inline void releaseSrc(VM* vm, const Value& v)
    {
        if constexpr (S == Src::Stack)
        {
            if (requiresRefCount(v))
                vm->releaseAndDelete(v);
        }
        else
        {
            (void)vm; (void)v; // borrowed — nothing to do
        }
    }

    // ------------------------------------------------------------------
    // Fused handlers
    // ------------------------------------------------------------------

    // Display names for the profiler / debugging. The interner keeps the
    // strings alive for the program's lifetime with stable addresses
    // (unordered_set is node-based); built only at decode time.
    inline const std::string* internFusionName(std::string name)
    {
        static std::mutex mutex;
        static std::unordered_set<std::string> names;
        std::lock_guard<std::mutex> lock(mutex);
        return &*names.insert(std::move(name)).first;
    }

    inline char srcLetter(Src s)
    {
        switch (s)
        {
            case Src::Stack:    return 'S';
            case Src::Local:    return 'L';
            case Src::Const:    return 'C';
            case Src::ShortInt: return 'I';
        }
        return '?';
    }

    inline const std::string* fusionName(BinOp op, Src a, Src b, Sink sink)
    {
        static const char* sinkNames[] = { "Push", "Store", "Branch", "BranchT" };

        std::string name = "FUSED_";
        name += binOpName(op);
        name += '(';
        name += srcLetter(a);
        name += ',';
        name += srcLetter(b);
        name += ")->";
        name += sinkNames[static_cast<uint8_t>(sink)];
        return internFusionName(std::move(name));
    }

    // Name for the in-place compound assignment `local = local <op> b`: the
    // 'L=' marks the local that is both left operand and destination.
    inline const std::string* compoundFusionName(BinOp op, Src b)
    {
        std::string name = "FUSED_";
        name += binOpName(op);
        name += "(L=,";
        name += srcLetter(b);
        name += ")->Local";
        return internFusionName(std::move(name));
    }

    // NOTE on read order: B is read BEFORE A. When both operands come from
    // the stack, B is the value on top (pushed last) and must be popped
    // first; for all other source kinds the order is irrelevant (reads have
    // no side effects). Both Stack sources consume their stack reference,
    // so both are released after the op (borrowed sources are no-ops).

    template <typename Op, Src A, Src B>
    const DecodedInstruction* fusedBinaryPush(VM* vm, const DecodedInstruction& instr)
    {
        Value b = readSrc<B>(vm, instr, instr.operands.indexed.byte2);

        if constexpr (A == Src::Stack)
        {
            // The deeper operand is the current stack top (B is inline —
            // Stack/Stack push is the base op and never fuses). Replace it
            // in place: changeTop releases the old top, consuming a's stack
            // reference — no pop/push round-trip.
            Value a = vm->peek();
            Value r = Op::apply(vm, a, b);
            releaseSrc<B>(vm, b);
            vm->changeTop(r);
        }
        else
        {
            Value a = readSrc<A>(vm, instr, instr.operands.indexed.byte1);
            Value r = Op::apply(vm, a, b);
            releaseSrc<B>(vm, b);
            vm->push(r);
        }
        return &instr + 1;
    }

    // Mirrors push + op + Set_Local_Pop: the result's reference moves into
    // the slot, the slot's old value is released.
    template <typename Op, Src A, Src B>
    const DecodedInstruction* fusedBinaryStore(VM* vm, const DecodedInstruction& instr)
    {
        Value b = readSrc<B>(vm, instr, instr.operands.indexed.byte2);
        Value a = readSrc<A>(vm, instr, instr.operands.indexed.byte1);
        Value r = Op::apply(vm, a, b);
        releaseSrc<A>(vm, a);
        releaseSrc<B>(vm, b);

        const uint8_t dst = instr.operands.indexed.byte3;
        Value oldValue = vm->currentFrame->slots[dst];
        vm->currentFrame->slots[dst] = r;
        if (requiresRefCount(oldValue))
            vm->releaseAndDelete(oldValue);

        return &instr + 1;
    }

    // In-place compound assignment: slot = slot <op> operand.
    // The destination local is ALSO the left operand, so it is read and written
    // in place — no re-read of the slot as a separate store destination, no
    // extra stack traffic. Generalizes fusedIncrDecrLocal to any binary op and
    // any operand source. Refcount accounting matches fusedBinaryStore for the
    // dst==srcA case: the local's old value is released once, a Stack operand is
    // consumed once, borrowed Local/Const/ShortInt operands are no-ops.
    template <typename Op, Src B>
    const DecodedInstruction* fusedLocalCompound(VM* vm, const DecodedInstruction& instr)
    {
        const uint8_t slot = instr.operands.indexed.byte1; // local == dst == src A
        Value& val = vm->currentFrame->slots[slot];
        Value b = readSrc<B>(vm, instr, instr.operands.indexed.byte2);
        Value r = Op::apply(vm, val, b); // read val BEFORE releasing it
        releaseSrc<B>(vm, b);
        if (requiresRefCount(val))
            vm->releaseAndDelete(val);
        val = r;
        return &instr + 1;
    }

    // Comparison + popping conditional jump: the condition value never
    // touches the stack (it's a primitive bool — nothing to release).
    //
    // RelTarget selects how the branch destination is encoded:
    //   false — the pre-resolved jumpTargetPtr lives in the union pointer
    //           (the fast default, used when neither operand is a Const).
    //   true  — one operand is a Const, so the union pointer already holds
    //           constantPtr; the target is instead a signed 16-bit RELATIVE
    //           instruction offset in operand bytes 3/4, written at fixup time.
    //           This lets `local == const` (etc.) fuse its branch as well.
    template <typename Op, Src A, Src B, bool RelTarget = false>
    const DecodedInstruction* fusedCmpBranchIfFalse(VM* vm, const DecodedInstruction& instr)
    {
        Value b = readSrc<B>(vm, instr, instr.operands.indexed.byte2);
        Value a = readSrc<A>(vm, instr, instr.operands.indexed.byte1);
        const bool cond = applyCondFast<Op>(vm, a, b);
        releaseSrc<A>(vm, a);
        releaseSrc<B>(vm, b);

        if (not cond)
        {
            if constexpr (RelTarget)
            {
                const int16_t rel = static_cast<int16_t>(
                    static_cast<uint16_t>(instr.operands.indexed.byte3)
                    | (static_cast<uint16_t>(instr.operands.indexed.byte4) << 8));
                return &instr + rel;
            }
            else
                return instr.jumpTargetPtr;
        }

        return &instr + 1;
    }

    // Comparison + backward popping conditional jump (loop-rotation bottom
    // test): identical to fusedCmpBranchIfFalse but branches when the result is
    // TRUE, back to the loop body. The target (pointer or relative offset) is
    // resolved to a BACKWARD destination by branchTargetOffsetOf / the fixup.
    template <typename Op, Src A, Src B, bool RelTarget = false>
    const DecodedInstruction* fusedCmpBranchIfTrue(VM* vm, const DecodedInstruction& instr)
    {
        Value b = readSrc<B>(vm, instr, instr.operands.indexed.byte2);
        Value a = readSrc<A>(vm, instr, instr.operands.indexed.byte1);
        const bool cond = applyCondFast<Op>(vm, a, b);
        releaseSrc<A>(vm, a);
        releaseSrc<B>(vm, b);

        if (cond)
        {
            if constexpr (RelTarget)
            {
                const int16_t rel = static_cast<int16_t>(
                    static_cast<uint16_t>(instr.operands.indexed.byte3)
                    | (static_cast<uint16_t>(instr.operands.indexed.byte4) << 8));
                return &instr + rel;
            }
            else
                return instr.jumpTargetPtr;
        }

        return &instr + 1;
    }

    // Statement-form local increment/decrement: fuses the
    // [Short_Int slot][Post_Incr_Local] pair that IncrementOptimization
    // emits for `i += 1` — the slot index goes straight into the operand
    // byte instead of a push/pop round-trip. Mirrors op_post_incr_local's
    // semantics (in-place update, nothing pushed).
    template <bool Increment>
    const DecodedInstruction* fusedIncrDecrLocal(VM* vm, const DecodedInstruction& instr)
    {
        const uint8_t slot = instr.operands.indexed.byte1;
        Value& val = vm->currentFrame->slots[slot];

        // Int fast path first — the type guard is folded into it so the
        // dominant case never leaves this function.
        if (IS_INT(val))
        {
            val = INT_VAL(AS_INT(val) + (Increment ? 1 : -1));
            return &instr + 1;
        }

        if (not IS_DOUBLE(val))
        {
            vm->runtimeError(Increment ? "Operand after an unary (++) must be a number."
                                       : "Operand after an unary (--) must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        Value newValue = Increment ? vm->addValues(val, INT_VAL(1))
                                   : vm->subtractValues(val, INT_VAL(1));
        vm->releaseAndDelete(val);
        val = newValue;

        return &instr + 1;
    }

    // Unary over a local (Const/ShortInt inputs are constant-folded by the
    // bytecode passes; Stack input is the base op).
    template <UnOp U>
    const DecodedInstruction* fusedUnaryLocalPush(VM* vm, const DecodedInstruction& instr)
    {
        Value a = vm->currentFrame->slots[instr.operands.indexed.byte1];

        if constexpr (U == UnOp::Not)
        {
            if (not IS_BOOL(a))
            {
                vm->runtimeError("Operand after an unary (!) must be a boolean.");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return nullptr;
            }
            vm->push(BOOL_VAL(not isValueTrue(a)));
        }
        else // UnOp::Negate
        {
            if (not isValueNumber(a))
            {
                vm->runtimeError("Operand after an unary (-) must be a number.");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return nullptr;
            }
            vm->push(vm->negateValue(a));
        }

        return &instr + 1;
    }

    // ------------------------------------------------------------------
    // Handler selection: (BinOp, SrcA, SrcB, Sink) → template instantiation.
    // Returns nullptr for combinations that must not fuse (two Const
    // sources; Const source with a branch sink; Stack/Stack push = base op).
    // ------------------------------------------------------------------

    template <typename Op, Src A, Src B>
    inline OpDecodedHandler pickSink(Sink sink, bool comparison)
    {
        constexpr int constCount = (A == Src::Const ? 1 : 0) + (B == Src::Const ? 1 : 0);
        if constexpr (constCount > 1)
        {
            (void)sink; (void)comparison;
            return nullptr; // only one union pointer slot
        }
        else
        {
            switch (sink)
            {
                case Sink::Push:
                    if constexpr (A == Src::Stack && B == Src::Stack)
                        return nullptr; // that's just the base op
                    else
                        return &fusedBinaryPush<Op, A, B>;

                case Sink::Store:
                    return &fusedBinaryStore<Op, A, B>;

                case Sink::BranchIfFalse:
                    if (not comparison)
                        return nullptr;
                    // A single Const operand takes the union pointer for its
                    // constantPtr, so the branch target moves to a relative
                    // offset in the operand bytes (RelTarget=true). With no
                    // Const, the fast pre-resolved jumpTargetPtr is used.
                    if constexpr (constCount > 0)
                        return &fusedCmpBranchIfFalse<Op, A, B, true>;
                    else
                        return &fusedCmpBranchIfFalse<Op, A, B, false>;

                case Sink::BranchIfTrue:
                    if (not comparison)
                        return nullptr;
                    // Same Const/pointer split as BranchIfFalse; the target is
                    // backward but the encoding is identical.
                    if constexpr (constCount > 0)
                        return &fusedCmpBranchIfTrue<Op, A, B, true>;
                    else
                        return &fusedCmpBranchIfTrue<Op, A, B, false>;
            }
            return nullptr;
        }
    }

    template <typename Op, Src A>
    inline OpDecodedHandler pickB(Src b, Sink sink, bool comparison)
    {
        switch (b)
        {
            case Src::Stack:    return (A == Src::Stack) ? pickSink<Op, Src::Stack, Src::Stack>(sink, comparison) : nullptr;
            case Src::Local:    return pickSink<Op, A, Src::Local>(sink, comparison);
            case Src::Const:    return pickSink<Op, A, Src::Const>(sink, comparison);
            case Src::ShortInt: return pickSink<Op, A, Src::ShortInt>(sink, comparison);
        }
        return nullptr;
    }

    template <typename Op>
    inline OpDecodedHandler pickA(Src a, Src b, Sink sink, bool comparison)
    {
        switch (a)
        {
            case Src::Stack:    return pickB<Op, Src::Stack>(b, sink, comparison);
            case Src::Local:    return pickB<Op, Src::Local>(b, sink, comparison);
            case Src::Const:    return pickB<Op, Src::Const>(b, sink, comparison);
            case Src::ShortInt: return pickB<Op, Src::ShortInt>(b, sink, comparison);
        }
        return nullptr;
    }

    // In-place compound assignment: only the right operand B varies (the left
    // operand is always the destination local, encoded in byte1).
    template <typename Op>
    inline OpDecodedHandler pickCompoundB(Src b)
    {
        switch (b)
        {
            case Src::Stack:    return &fusedLocalCompound<Op, Src::Stack>;
            case Src::Local:    return &fusedLocalCompound<Op, Src::Local>;
            case Src::Const:    return &fusedLocalCompound<Op, Src::Const>;
            case Src::ShortInt: return &fusedLocalCompound<Op, Src::ShortInt>;
        }
        return nullptr;
    }

    // The selection switches are GENERATED alongside the functors — a new op
    // in the spec flows into the fusion pass with no code edits here.
    #include "generated/ops_select.inc"

    inline OpDecodedHandler selectFusedUnary(UnOp op)
    {
        switch (op)
        {
            case UnOp::Not:    return &fusedUnaryLocalPush<UnOp::Not>;
            case UnOp::Negate: return &fusedUnaryLocalPush<UnOp::Negate>;
            case UnOp::None:   return nullptr;
        }
        return nullptr;
    }

} // namespace fusion
} // namespace pg
