#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

#include <stdexcept>

#include "object.h"

#include "Memory/elementtype.h"
#include "Memory/memorypool.h"

namespace pg
{
    enum class OpCode : uint8_t
    {
        OP_Return = 0,
        OP_Constant,
        OP_LongConstant,
        OP_Constant_String,  // Load string from constantStrings array
        OP_Negate,
        OP_Add,
        OP_Subtract,
        OP_Multiply,
        OP_Divide,
        OP_Modulo,
        OP_True,
        OP_False,
        OP_Not,
        OP_And,
        OP_Or,
        OP_Equal,
        OP_NotEqual,
        OP_Greater,
        OP_GreaterEqual,
        OP_Less,
        OP_LessEqual,
        OP_Pop,
        OP_Define_Global,
        OP_Get_Global,
        OP_Set_Global,
        OP_Get_Local,
        OP_Set_Local,
        OP_Jump_If_False,
        OP_Long_Jump_If_False,
        OP_Jump,
        OP_Long_Jump,
        OP_Loop,
        OP_Long_Loop,
        OP_Debug_Print,

        OP_Post_Incr_Global,
        OP_Incr_Global,
        OP_Post_Incr_Local,
        OP_Incr_Local,
        OP_Post_Decr_Global,
        OP_Decr_Global,
        OP_Post_Decr_Local,
        OP_Decr_Local,

        OP_Call,
        OP_Invoke,
        OP_Closure,

        OP_Get_Upvalue,
        OP_Set_Upvalue,

        OP_Close_Upvalue,

        OP_Class,

        OP_Set_Property,
        OP_Get_Property,

        OP_Method,

        OP_Short_Int,

        // Optimized opcodes can be added here

        OP_PopN,               // Pop N values from the stack (operand is number of values to pop)

        OP_Define_Constant_Global,    // Define global variable with constant operand
        OP_Get_Constant_Global,       // Get global variable with constant operand
        OP_Set_Constant_Global,       // Set global variable with constant operand

        OP_Define_Global_Non_Popping,

        OP_AddLL,      // ADD optimized for two local variables
        OP_SubtractLL, // SUBTRACT optimized for two local variables

        OP_SubtractLC, // SUBTRACT optimized for local and constant
        OP_SubtractCL, // SUBTRACT optimized for constant and local

        // Table operations
        OP_Build_Vector,  // Create vector instance from stack key-value pairs
        OP_Build_Table,   // Create table instance from stack key-value pairs
        OP_Get_Index,     // table[index] - get field by computed key
        OP_Set_Index,     // table[index] = val - set field by computed key

        // Iterator operations
        OP_Get_Iterator,  // Get iterator for a table (pushes iterator state)
        OP_Iterator_Next, // Advance iterator and push key (or nil if done), updates iterator state
        OP_Table_Size,    // Get the size of a table (number of fields)
        OP_Table_At,      // Get key at specific index (expects table and index on stack)

        // Module operations
        OP_Import,        // Import a module (expects module name string on stack)
    };

    struct Chunk
    {
        std::vector<uint8_t> code;

        std::vector<Value> constants;

        std::vector<int> lines;

        // Track which native modules were imported during compilation
        // These need to be loaded when executing cached bytecode
        // Note: Imports are always global (added to VM globals), even if written in local scopes
        std::vector<std::string> importedModules;

        // Constant strings (for interned string values) - stored per chunk
        // All compile-time string constants are stored here and referenced by index
        std::vector<std::string> constantStrings;

        // Note: With NaN-boxing and pool-based memory, constants don't need cleanup in destructor
        // The VM's pools handle all memory management via reference counting

        // Helper function to compare two values for semantic equality
        // For primitives (int, bool, double), compares bit patterns
        // For strings, compares actual string content (requires stringPool)
        bool valuesEqual(Value a, Value b, AllocatorPool<std::string, 64>* stringPool = nullptr) const
        {
            // Fast path: if bit patterns match, they're definitely equal
            if (a == b) return true;

            // For non-string types, only bit pattern equality matters
            if (not IS_STRING(a) or not IS_STRING(b))
                return false;

            // If no string pool available, can only compare bit patterns
            if (not stringPool)
                return false;

            // Compare actual string content
            uint32_t indexA = AS_STRING_INDEX(a);
            uint32_t indexB = AS_STRING_INDEX(b);

            // Bounds check
            if (indexA >= stringPool->getNbElements() or indexB >= stringPool->getNbElements())
                return false;

            auto strA = stringPool->getElement(indexA);
            auto strB = stringPool->getElement(indexB);

            if (not strA or not strB)
                return false;

            return strA == strB;
        }

        AllocatorPool<std::string, 64>* stringPool = nullptr;  // Set by compiler/VM for string comparison

        size_t addConstant(const Value& value, int line)
        {
            // Check if constant already exists
            for (size_t i = 0; i < constants.size(); i++)
            {
                if (valuesEqual(constants[i], value, stringPool))
                {
                    // Found existing constant, emit code to load it
                    auto cIndex = i;
                    if (cIndex > 255)
                    {
                        addCode(OpCode::OP_LongConstant, line);
                        addCode((cIndex >> 16) & 0xFF, line);
                        addCode((cIndex >> 8) & 0xFF, line);
                        addCode(cIndex & 0xFF, line);
                    }
                    else
                    {
                        addCode(OpCode::OP_Constant, line);
                        addCode(cIndex, line);
                    }
                    return code.size() - 1;
                }
            }

            // Constant doesn't exist, add it
            constants.push_back(value);
            auto cIndex = constants.size() - 1;

            // If more than 256 constants in one chunk, we can't store the index in one byte
            // So we store it as a 4 bytes instruction (OpLongConstant, byte1, byte2, byte3, with the 3 bytes forming the constant index)
            if (cIndex > 255)
            {
                addCode(OpCode::OP_LongConstant, line);
                addCode((cIndex >> 16) & 0xFF, line);
                addCode((cIndex >> 8) & 0xFF, line);
                addCode(cIndex & 0xFF, line);
            }
            else if (cIndex > 0xFFFFFF)
            {
                // Should never happen
                throw std::runtime_error("Too many constants in one chunk (> 16 millions)");
            }
            else
            {
                addCode(OpCode::OP_Constant, line);
                addCode(cIndex, line);
            }

            return code.size() - 1;
        }

        // Add constant to array without emitting opcodes (for instructions like OP_Closure)
        uint8_t addConstantIndex(const Value& value)
        {
            // Check if constant already exists
            for (size_t i = 0; i < constants.size(); i++)
            {
                if (valuesEqual(constants[i], value, stringPool))
                {
                    if (i > 255)
                    {
                        throw std::runtime_error("Too many constants for single-byte index");
                    }

                    return static_cast<uint8_t>(i);
                }
            }

            // Constant doesn't exist, add it
            constants.push_back(value);
            auto cIndex = constants.size() - 1;

            if (cIndex > 255)
            {
                throw std::runtime_error("Too many constants for single-byte index");
            }

            return static_cast<uint8_t>(cIndex);
        }

        size_t addCode(const OpCode& op, int line)
        {
            code.push_back(static_cast<uint8_t>(op));
            lines.push_back(line);

            return code.size() - 1;
        }

        size_t addCode(uint8_t byte, int line)
        {
            code.push_back(byte);
            lines.push_back(line);

            return code.size() - 1;
        }

        void clear()
        {
            code.clear();
            constants.clear();
            lines.clear();
        }
    };

    // Utility function to get the size of an instruction in bytes
    inline int getInstructionSize(OpCode opcode)
    {
        switch (opcode)
        {
            case OpCode::OP_Constant:
            case OpCode::OP_Get_Local:
            case OpCode::OP_Set_Local:
            case OpCode::OP_Get_Upvalue:
            case OpCode::OP_Set_Upvalue:
            case OpCode::OP_Get_Property:
            case OpCode::OP_Set_Property:
            case OpCode::OP_Call:
            case OpCode::OP_Short_Int:
                return 2; // opcode + 1 byte operand

            case OpCode::OP_Define_Global:
            case OpCode::OP_Define_Global_Non_Popping:
            case OpCode::OP_Get_Global:
            case OpCode::OP_Set_Global:
            case OpCode::OP_Return:
            case OpCode::OP_Negate:
            case OpCode::OP_Add:
            case OpCode::OP_Subtract:
            case OpCode::OP_Multiply:
            case OpCode::OP_Divide:
            case OpCode::OP_Modulo:
            case OpCode::OP_True:
            case OpCode::OP_False:
            case OpCode::OP_Not:
            case OpCode::OP_And:
            case OpCode::OP_Or:
            case OpCode::OP_Equal:
            case OpCode::OP_NotEqual:
            case OpCode::OP_Greater:
            case OpCode::OP_GreaterEqual:
            case OpCode::OP_Less:
            case OpCode::OP_LessEqual:
            case OpCode::OP_Pop:
            case OpCode::OP_Debug_Print:
            case OpCode::OP_Post_Incr_Global:
            case OpCode::OP_Incr_Global:
            case OpCode::OP_Post_Incr_Local:
            case OpCode::OP_Incr_Local:
            case OpCode::OP_Post_Decr_Global:
            case OpCode::OP_Decr_Global:
            case OpCode::OP_Post_Decr_Local:
            case OpCode::OP_Decr_Local:
            case OpCode::OP_Close_Upvalue:
                return 1; // opcode only, no operand

            case OpCode::OP_LongConstant:
                return 4; // opcode + 3 byte operand

            case OpCode::OP_Jump_If_False:
            case OpCode::OP_Jump:
            case OpCode::OP_Loop:
                return 3; // opcode + 2 byte operand

            case OpCode::OP_Long_Jump_If_False:
            case OpCode::OP_Long_Jump:
            case OpCode::OP_Long_Loop:
                return 5; // opcode + 4 byte operand

            case OpCode::OP_Closure:
            case OpCode::OP_Method:
                return 2; // opcode + 1 byte operand (constant index), plus upvalue bytes handled separately

            case OpCode::OP_Invoke:
                return 3;

            case OpCode::OP_PopN:
                return 2; // opcode + 1 byte operand (number of values to pop)

            case OpCode::OP_AddLL:
            case OpCode::OP_SubtractLL:
            case OpCode::OP_SubtractLC:
            case OpCode::OP_SubtractCL:
                return 3; // opcode + 2 byte operands (local variable indices)

            case OpCode::OP_Define_Constant_Global:
            case OpCode::OP_Set_Constant_Global:
                return 3; // opcode + 2 byte operand (constant index)

            case OpCode::OP_Get_Constant_Global:
                return 2; // opcode + 1 byte operand (constant index)

            case OpCode::OP_Class:
                return 2; // opcode + 1 byte operand (constant index for class name)

            case OpCode::OP_Build_Vector:
            case OpCode::OP_Build_Table:
                return 2; // opcode + 1 byte operand (pair count)

            case OpCode::OP_Get_Index:
            case OpCode::OP_Set_Index:
                return 1; // opcode only

            case OpCode::OP_Get_Iterator:
            case OpCode::OP_Iterator_Next:
            case OpCode::OP_Table_Size:
            case OpCode::OP_Table_At:
                return 1; // opcode only

            case OpCode::OP_Import:
                return 1; // opcode only (module name is on stack)

            default:
                return 1; // default to single byte for unknown opcodes
        }
    }

    struct ObjFunction
    {
        Chunk chunk;
        int arity; // Number of parameters
        std::string name;
        int upvalueCount = 0;
    };
}