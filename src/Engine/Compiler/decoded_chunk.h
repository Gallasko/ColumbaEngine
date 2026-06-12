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
    // loop is `instr = instr->decodedHandler(vm, *instr);` so the instruction
    // pointer lives in a register, not in VM memory. Fall-through handlers
    // return `&instr + 1` (instructions are contiguous); jumps return the
    // pre-resolved jumpTargetPtr; error paths longjmp via vm_return and the
    // trailing `return nullptr` is unreachable.
    typedef const DecodedInstruction* (*OpDecodedHandler)(VM* vm, const DecodedInstruction& instr);

    // ============================================================================
    // PRE-DECODED INSTRUCTION FORMAT
    // ============================================================================
    // This structure represents a fully decoded instruction ready for execution.
    // All operands are pre-extracted, handler is pre-looked-up.
    // Goal: Eliminate instruction fetch and decode overhead at runtime.
    // ============================================================================

    struct DecodedInstruction
    {
        // --- Hot fields: keep handler, operands and jump target in the
        // --- first 32 bytes so one cache line covers the dispatch path.
        OpDecodedHandler decodedHandler = nullptr; // Pre-resolved handler that receives the full instruction

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

        // Metadata for future optimizations
        uint8_t flags;               // Instruction properties (see OpCodeInfo flags)
        uint8_t originalOpcode;      // For debugging/profiling
        uint8_t operandBytes;        // Number of operand bytes

        // Pre-resolved jump target (control-flow ops). Filled in by
        // ChunkDecoder::resolveJumpTargets once the instructions vector is
        // final; jump handlers return it directly.
        const DecodedInstruction* jumpTargetPtr = nullptr;

        // Pre-computed constant pointer (for OP_Constant/OP_LongConstant)
        // This eliminates chunk.constants[index] lookup during execution
        Value* constantPtr;

        // Pre-resolved property name (OP_Get_Property, OP_Set_Property,
        // OP_Invoke). Eliminates chunk.constantStrings[stringIndex] lookup
        // at runtime, analogous to constantPtr.
        const std::string* propertyNamePtr = nullptr;

        // Line number (for error reporting)
        int lineNumber;

        // Original bytecode offset (needed for jump target resolution)
        size_t bytecodeOffset;

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
            for (const auto& instr : instructions)
            {
                if (instr.bytecodeOffset == bytecodeOffset)
                {
                    return static_cast<size_t>(&instr - instructions.data());
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
        // Decode a single instruction at given offset
        DecodedInstruction decodeInstruction(
            const Chunk& chunk,
            size_t offset,
            VM* vm,
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