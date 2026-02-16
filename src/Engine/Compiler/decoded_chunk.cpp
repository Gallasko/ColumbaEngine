#include "stdafx.h"
#include "decoded_chunk.h"
#include "vm.h"
#include "logger.h"
#include "compiler_debug.h"

namespace pg
{
    ObjFunction::~ObjFunction()
    {
        if (decodedChunk != nullptr)
        {
            delete decodedChunk;
            decodedChunk = nullptr;
        }
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
            DecodedInstruction instr = decodeInstruction(chunk, offset, vm, nextOffset);

            decoded->instructions.push_back(instr);
            offset = nextOffset;
            instructionIndex++;
        }

        decoded->totalInstructions = instructionIndex;

        // Optimize: pre-resolve constant pointers
        resolveConstantPointers(decoded, chunk);

        resolveJumpTargets(decoded);

        // TODO Future: analyze pure batches for instruction fusion
        // This will allow us to detect patterns like GET_LOCAL + GET_LOCAL + ADD
        // and replace them with fused instructions like ADD_LL
        // analyzePureBatches(decoded);

        LOG_INFO("ChunkDecoder", "Decoded " << decoded->instructions.size() << " instructions");

        return decoded;
    }

    DecodedInstruction ChunkDecoder::decodeInstruction(
        const Chunk& chunk,
        size_t offset,
        VM* vm,
        size_t& nextOffset)
    {
        DecodedInstruction instr;

        uint8_t opcode = chunk.code[offset];
        instr.originalOpcode = opcode;
        instr.bytecodeOffset = offset;

        // Look up handler and metadata
        const OpCodeInfo& info = vm->operations[opcode];
        instr.handler = info.handler;
        instr.decodedHandler = info.decodedHandler;
        instr.flags = info.flags;
        instr.operandBytes = info.operandBytes;
        instr.constantPtr = nullptr;

        // Get line number for error reporting
        if (offset < chunk.lines.size())
        {
            instr.lineNumber = chunk.lines[offset];
        }
        else
        {
            instr.lineNumber = -1;
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
            instr.operandBytes = operandBytes;
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

        nextOffset = offset + 1 + operandBytes;
        return instr;
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

        for (auto& instr : decoded->instructions)
        {
            if (instr.originalOpcode == static_cast<uint8_t>(OpCode::OP_Constant))
            {
                uint8_t constantIndex = instr.operands.byte;
                if (constantIndex < chunk.constants.size())
                {
                    instr.constantPtr = const_cast<Value*>(&chunk.constants[constantIndex]);
                }
            }
            else if (instr.originalOpcode == static_cast<uint8_t>(OpCode::OP_LongConstant))
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

    void ChunkDecoder::resolveJumpTargets(DecodedChunk* decoded)
    {
        // For control flow instructions (jumps and loops), resolve target instruction index
        // This allows us to jump directly to the correct instruction in the decoded array
        // without needing to map bytecode offsets at runtime

        for (auto& instr : decoded->instructions)
        {
            if (instr.hasControlFlow())
            {
                size_t targetBytecodeOffset = 0;

                // Determine target bytecode offset based on opcode and operands
                // Jump offsets are stored relative to the end of the instruction
                // So target = bytecodeOffset + instructionSize + signedOffset
                switch (static_cast<OpCode>(instr.originalOpcode))
                {
                    case OpCode::OP_Jump:
                    case OpCode::OP_Jump_If_False:
                        // Regular jumps use 2-byte operands (big-endian, signed)
                        targetBytecodeOffset = instr.bytecodeOffset + 1 + instr.operandBytes +
                                               static_cast<int16_t>((instr.operands.indexed.byte1 << 8) |
                                                                     instr.operands.indexed.byte2);
                        break;
                    case OpCode::OP_Loop:
                        // Regular loops use 2-byte operands (big-endian, signed)
                        targetBytecodeOffset = instr.bytecodeOffset + 1 + instr.operandBytes -
                                               static_cast<uint16_t>((instr.operands.indexed.byte1 << 8) |
                                                                     instr.operands.indexed.byte2);
                        break;

                    case OpCode::OP_Long_Jump:
                    case OpCode::OP_Long_Jump_If_False:
                        // Long jumps use 4-byte operands (big-endian, signed)
                        targetBytecodeOffset = instr.bytecodeOffset + 1 + instr.operandBytes +
                        static_cast<int32_t>((instr.operands.indexed.byte1 << 24) |
                                             (instr.operands.indexed.byte2 << 16) |
                                             (instr.operands.indexed.byte3 << 8)  |
                                              instr.operands.indexed.byte4);
                        break;

                    case OpCode::OP_Long_Loop:
                        // Long loops use 4-byte operands (big-endian, signed)
                        targetBytecodeOffset = instr.bytecodeOffset + 1 + instr.operandBytes -
                                               static_cast<uint32_t>((instr.operands.indexed.byte1 << 24) |
                                                                     (instr.operands.indexed.byte2 << 16) |
                                                                     (instr.operands.indexed.byte3 << 8)  |
                                                                      instr.operands.indexed.byte4);
                        break;

                    default:
                        continue;  // Not a control flow instruction
                }

                // Find corresponding instruction index in decoded chunk
                auto it = decoded->jumpTargets.find(targetBytecodeOffset);
                if (it != decoded->jumpTargets.end())
                {
                    instr.nextInstuctionIndex = it->second;
                }
            }
        }
    }

} // namespace pg
