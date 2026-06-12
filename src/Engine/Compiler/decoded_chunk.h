#pragma once

#include "chunk.h"
#include <vector>
#include <unordered_map>

namespace pg
{
    // Forward declarations
    struct VM;
    struct DecodedInstruction;

    // Decoded handlers return the next instruction to execute. The dispatch
    // loop is `while (instr) instr = instr->decodedHandler(vm, *instr);` so
    // the instruction pointer lives in a register, not in VM memory.
    // Fall-through handlers return `&instr + 1` (instructions are
    // contiguous); jumps return the pre-resolved jumpTargetPtr; final
    // returns / runtime errors record the result via vm_return and return
    // nullptr to stop the loop.
    typedef const DecodedInstruction* (*OpDecodedHandler)(VM* vm, const DecodedInstruction& instr);

    // ============================================================================
    // PRE-DECODED INSTRUCTION FORMAT
    // ============================================================================
    // This structure represents a fully decoded instruction ready for execution.
    // All operands are pre-extracted, handler is pre-looked-up.
    // Goal: Eliminate instruction fetch and decode overhead at runtime.
    // ============================================================================

    // Hot half of a decoded instruction: ONLY what handlers read on the
    // dispatch path. 24 bytes — 2.6 instructions per cache line (the old
    // combined struct was 56 bytes). Everything decode-time or debug-only
    // lives in the parallel DecodedInstructionMeta array.
    struct DecodedInstruction
    {
        OpDecodedHandler decodedHandler = nullptr; // Pre-resolved handler that receives the full instruction

        // Per-opcode pre-resolved pointer. The three uses are mutually
        // exclusive: an op is a jump, a constant load, or a property/invoke
        // op — never two at once.
        union
        {
            // Jump target (control-flow ops). Filled by resolveJumpTargets
            // once the instructions vector is final; jump handlers return
            // it directly.
            const DecodedInstruction* jumpTargetPtr = nullptr;

            // Constant pointer (OP_Constant/OP_LongConstant) — eliminates
            // the chunk.constants[index] lookup during execution.
            Value* constantPtr;

            // Property name (OP_Get_Property, OP_Set_Property, OP_Invoke)
            // — eliminates the chunk.constantStrings[index] lookup.
            const std::string* propertyNamePtr;
        };

        // Operand storage (union to save space)
        union
        {
            uint8_t bytes[4];        // Generic byte access
            uint32_t dword;          // 32-bit operand
            uint16_t word;           // 16-bit operand
            uint8_t byte;            // 8-bit operand

            struct OperandBytes     // For multi-operand instructions (e.g., OP_AddLL)
            {
                uint8_t byte1;
                uint8_t byte2;
                uint8_t byte3;
                uint8_t byte4;
            } indexed;
        } operands;
    };

    // Dispatch reads this struct once per instruction — keep it lean. If a
    // new field is genuinely hot, it must fit here; anything else belongs
    // in DecodedInstructionMeta.
    static_assert(sizeof(DecodedInstruction) == 24,
                  "DecodedInstruction grew past 24 bytes — move cold fields to DecodedInstructionMeta");

    // Cold half: decode-time bookkeeping and debug/profiling metadata,
    // stored in DecodedChunk::meta parallel to the instructions array
    // (same index). Read by the decoder, the profiler/debug-trace loop
    // variants, error reporting, and the rare op_closure handler — never
    // on the per-instruction dispatch path.
    struct DecodedInstructionMeta
    {
        size_t  bytecodeOffset = 0;  // Offset of the instruction in chunk.code
        int     lineNumber = -1;     // For error reporting
        uint8_t originalOpcode = 0;  // For debugging/profiling
        uint8_t flags = 0;           // Instruction properties (see OpCodeInfo flags)
        uint8_t operandBytes = 0;    // Number of operand bytes

        // Flag checks (matching OpCodeInfo flags)
        bool isPure() const { return (flags & 0x01) != 0; }
        bool isBatchable() const { return (flags & 0x20) != 0; }
        bool hasControlFlow() const { return (flags & 0x08) == 0; }  // NO_BRANCH flag is NOT set
    };

    // ============================================================================
    // DECODED CHUNK - Execution-Optimized Representation
    // ============================================================================
    // This is the "compiled" version of a Chunk, optimized for execution speed.
    // Created once from Chunk, then executed many times without decode overhead.
    // ============================================================================

    struct DecodedChunk
    {
        // Array of pre-decoded instructions (the "executable code")
        std::vector<DecodedInstruction> instructions;

        // Cold metadata, parallel to `instructions` (same index). Look up
        // an instruction's metadata via `meta[instr - instructions.data()]`.
        std::vector<DecodedInstructionMeta> meta;

        // Batch metadata: groups of consecutive pure instructions
        struct PureBatch
        {
            size_t startIndex;       // First instruction in batch
            size_t count;            // Number of instructions in batch
            int8_t netStackEffect;   // Total stack effect of batch
        };
        std::vector<PureBatch> pureBatches;

        // Jump target mapping: bytecode offset → decoded instruction index
        // Needed for control flow instructions
        std::unordered_map<size_t, size_t> jumpTargets;

        // Back-reference to original bytecode chunk
        const Chunk* originalChunk;

        // Performance hints
        bool hasLoops;               // Contains OP_Loop
        bool hasCalls;               // Contains OP_Call
        size_t hotPathLength;        // Longest pure batch (for statistics)
        size_t totalInstructions;    // Total number of instructions

        DecodedChunk() : originalChunk(nullptr), hasLoops(false), hasCalls(false),
                        hotPathLength(0), totalInstructions(0) {}

        // Find decoded instruction index from bytecode offset
        size_t findInstructionIndex(size_t bytecodeOffset) const
        {
            auto it = jumpTargets.find(bytecodeOffset);
            if (it != jumpTargets.end())
            {
                return it->second;
            }
            // Fallback: linear search (shouldn't happen if jumpTargets is built correctly)
            for (size_t i = 0; i < meta.size(); ++i)
            {
                if (meta[i].bytecodeOffset == bytecodeOffset)
                {
                    return i;
                }
            }
            return 0;  // Last resort
        }

        // Check if instruction index is at the start of a pure batch
        const PureBatch* findBatchAt(size_t instructionIndex) const
        {
            for (const auto& batch : pureBatches)
            {
                if (batch.startIndex == instructionIndex)
                {
                    return &batch;
                }
            }
            return nullptr;
        }
    };

    // ============================================================================
    // CHUNK DECODER - Converts Bytecode to Decoded Format
    // ============================================================================

    class ChunkDecoder
    {
    public:
        // Main API: decode a bytecode chunk into execution-optimized format
        DecodedChunk* decode(const Chunk& chunk, VM* vm);

    private:
        // Decode a single instruction at given offset, filling the hot
        // instruction and its cold metadata.
        void decodeInstruction(
            const Chunk& chunk,
            size_t offset,
            VM* vm,
            DecodedInstruction& instr,
            DecodedInstructionMeta& meta,
            size_t& nextOffset  // Output: where next instruction starts
        );

        // Analyze and group pure instruction sequences
        void analyzePureBatches(DecodedChunk* decoded);

        // Build jump target map for control flow
        void buildJumpTargets(const Chunk& chunk, DecodedChunk* decoded);

        // Optimize: pre-resolve constant pointers
        void resolveConstantPointers(DecodedChunk* decoded, const Chunk& chunk);

        void resolveJumpTargets(DecodedChunk* decoded);
    };

} // namespace pg