#include "compiler_debug.h"
#include "vm.h"

#include <iostream>
#include <iomanip>

namespace pg
{
    void printValue(VM* vm, const Value& value)
    {
        // For disassembly (vm == nullptr), just print type info
        if (vm == nullptr)
        {
            if (IS_FUNC(value))
                std::cout << "<func #" << AS_FUNCTION_INDEX(value) << ">";
            else if (IS_NAT_FUNC(value))
                std::cout << "<native fn>";
            else if (IS_CLOSURE(value))
                std::cout << "<closure #" << AS_CLOSURE_INDEX(value) << ">";
            else if (IS_UPVALUE(value))
                std::cout << "<upvalue>";
            else if (IS_CLASS(value))
                std::cout << "<class #" << AS_CLASS_INDEX(value) << ">";
            else if (IS_INSTANCE(value))
                std::cout << "<instance #" << AS_INSTANCE_INDEX(value) << ">";
            else if (IS_BOUND_METHOD(value))
                std::cout << "<bound method #" << AS_BOUND_METHOD_INDEX(value) << ">";
            else if (IS_STRING(value))
                std::cout << "<string #" << AS_STRING_INDEX(value) << ">";
            else if (IS_INT(value))
                std::cout << AS_INT(value);
            else if (IS_BOOL(value))
                std::cout << (AS_BOOL(value) ? "true" : "false");
            else if (IS_DOUBLE(value))
                std::cout << AS_DOUBLE(value);
            else
                std::cout << "<unknown>";
            return;
        }

        // Runtime printing with full VM access
        if (IS_FUNC(value))
        {
            ObjFunction* func = vm->asFunction(value);
            if (func != nullptr)
            {
                std::cout << "<" << func->name << ">";
            }
            else
            {
                std::cout << "<script>";
            }
        }
        else if (IS_NAT_FUNC(value))
        {
            std::cout << "<native fn>";
        }
        else if (IS_CLOSURE(value))
        {
            Closure* closure = vm->asClosure(value);
            if (closure != nullptr && closure->function != nullptr)
            {
                std::cout << "<closure " << closure->function->name << ">";
            }
            else
            {
                std::cout << "<null closure>";
            }
        }
        else if (IS_UPVALUE(value))
        {
            std::cout << "<upvalue>";
        }
        else if (IS_CLASS(value))
        {
            Klass* klass = vm->asClass(value);
            if (klass != nullptr)
            {
                std::cout << "<class " << klass->name << ">";
            }
            else
            {
                std::cout << "<null class>";
            }
        }
        else if (IS_INSTANCE(value))
        {
            ObjInstance* instance = vm->asInstance(value);

            if (instance != nullptr && instance->klass != nullptr)
            {
                std::cout << "<instance of " << instance->klass->name << ">";
            }
            else
            {
                std::cout << "<null instance>";
            }
        }
        else if (IS_BOUND_METHOD(value))
        {
            ObjBoundMethod* bound = vm->asBoundMethod(value);
            std::cout << "<bound method: ";
            if (bound && bound->method && bound->method->function)
            {
                std::cout << "<" << bound->method->function->name << ">";
            }
            std::cout << " >";
        }
        else
        {
            std::cout << vm->valueToElement(value).toString();
        }
    }

    namespace
    {
        int simpleInstruction(const std::string& name, int offset)
        {
            std::cout << name << std::endl;
            return offset + 1;
        }

        int constantInstruction(const std::string& name, const Chunk& chunk, int offset)
        {
            uint8_t cIndex = chunk.code[offset + 1];

            std::cout << std::left << std::setw(16) << name << " " << static_cast<int>(cIndex) << " '";

            if (cIndex >= chunk.constants.size())
            {
                std::cout << "<invalid constant index>";
            }
            else
            {
                const Value& constant = chunk.constants[cIndex];
                printValue(nullptr, constant);
            }

            std::cout << "'" << std::endl;

            return offset + 2;
        }

        int longConstantInstruction(const std::string& name, const Chunk& chunk, int offset)
        {
            uint32_t cIndex = (static_cast<uint32_t>(chunk.code[offset + 1]) << 16) |
                              (static_cast<uint32_t>(chunk.code[offset + 2]) << 8) |
                              (static_cast<uint32_t>(chunk.code[offset + 3]));

            std::cout << std::left << std::setw(16) << name << " " << cIndex << " '";

            if (cIndex >= chunk.constants.size())
            {
                std::cout << "<invalid constant index>";
            }
            else
            {
                const Value& constant = chunk.constants[cIndex];
                printValue(nullptr, constant);
            }

            std::cout << "'" << std::endl;

            return offset + 4;
        }

        int byteInstruction(const std::string& name, const Chunk& chunk, int offset)
        {
            uint8_t value = chunk.code[offset + 1];

            std::cout << std::left << std::setw(16) << name << " '" << static_cast<int>(value) << "'" << std::endl;

            return offset + 2;
        }

        int twoBytesInstruction(const std::string& name, const Chunk& chunk, int offset)
        {
            uint8_t byte1 = chunk.code[offset + 1];
            uint8_t byte2 = chunk.code[offset + 2];

            std::cout << std::left << std::setw(16) << name << " '" << static_cast<int>(byte1) << "' '" << static_cast<int>(byte2) << "'" << std::endl;

            return offset + 3;
        }

        int invokeInstruction(const std::string& name, const Chunk& chunk, int offset)
        {
            uint8_t constant = chunk.code[offset + 1];
            uint8_t argCount = chunk.code[offset + 2];

            std::cout << std::left << std::setw(16) << name << " (" << static_cast<int>(argCount) << " args) "
                      << static_cast<int>(constant) << " '";

            printValue(nullptr, chunk.constants[constant]);

            std::cout << "'" << std::endl;

            return offset + 3;
        }

        int jumpInstruction(const std::string& name, const Chunk& chunk, int offset, int sign = 1)
        {
            uint16_t jump = (static_cast<uint16_t>(chunk.code[offset + 1]) << 8) |
                            (static_cast<uint16_t>(chunk.code[offset + 2]));

            std::cout << std::left << std::setw(16) << name << " " << offset << " -> " << (offset + 3 + sign * jump) << std::endl;

            return offset + 3;
        }

        int longJumpInstruction(const std::string& name, const Chunk& chunk, int offset, int sign = 1)
        {
            uint32_t jump = (static_cast<uint32_t>(chunk.code[offset + 1]) << 24) |
                            (static_cast<uint32_t>(chunk.code[offset + 2]) << 16) |
                            (static_cast<uint32_t>(chunk.code[offset + 3]) << 8) |
                            (static_cast<uint32_t>(chunk.code[offset + 4]));

            std::cout << std::left << std::setw(16) << name << " " << offset << " -> " << (offset + 5 + sign * jump) << std::endl;

            return offset + 5;
        }
    }

    void disassembleChunk(VM* vm, const Chunk& chunk, const std::string& name)
    {
        std::cout << "== " << name << " ==" << std::endl;

        for (size_t offset = 0; offset < chunk.code.size();)
        {
            offset = disassembleInstruction(vm, chunk, offset);
        }
    }

    int disassembleInstruction(VM* vm, const Chunk& chunk, int offset)
    {
        std::cout << std::setw(4) << offset << " ";

        if (offset > 0 and chunk.lines[offset] == chunk.lines[offset - 1])
            std::cout << "   | ";
        else
            std::cout << std::setw(4) << chunk.lines[offset] << " ";

        OpCode instruction = static_cast<OpCode>(chunk.code[offset]);

        switch (instruction)
        {
            case OpCode::OP_Return:
                return simpleInstruction("OP_Return", offset);

            case OpCode::OP_Constant:
                return constantInstruction("OP_Constant", chunk, offset);

            case OpCode::OP_LongConstant:
                return longConstantInstruction("OP_LongConstant", chunk, offset);

            case OpCode::OP_Negate:
                return simpleInstruction("OP_Negate", offset);

            case OpCode::OP_Add:
                return simpleInstruction("OP_Add", offset);

            case OpCode::OP_Subtract:
                return simpleInstruction("OP_Subtract", offset);

            case OpCode::OP_Multiply:
                return simpleInstruction("OP_Multiply", offset);

            case OpCode::OP_Divide:
                return simpleInstruction("OP_Divide", offset);

            case OpCode::OP_True:
                return simpleInstruction("OP_True", offset);

            case OpCode::OP_False:
                return simpleInstruction("OP_False", offset);

            case OpCode::OP_Not:
                return simpleInstruction("OP_Not", offset);

            case OpCode::OP_And:
                return simpleInstruction("OP_And", offset);

            case OpCode::OP_Or:
                return simpleInstruction("OP_Or", offset);

            case OpCode::OP_Equal:
                return simpleInstruction("OP_Equal", offset);

            case OpCode::OP_NotEqual:
                return simpleInstruction("OP_NotEqual", offset);

            case OpCode::OP_Greater:
                return simpleInstruction("OP_Greater", offset);

            case OpCode::OP_GreaterEqual:
                return simpleInstruction("OP_GreaterEqual", offset);

            case OpCode::OP_Less:
                return simpleInstruction("OP_Less", offset);

            case OpCode::OP_LessEqual:
                return simpleInstruction("OP_LessEqual", offset);

            case OpCode::OP_Pop:
                return simpleInstruction("OP_Pop", offset);

            case OpCode::OP_Define_Global:
                return simpleInstruction("OP_Define_Global", offset);

            case OpCode::OP_Get_Global:
                return simpleInstruction("OP_Get_Global", offset);

            case OpCode::OP_Set_Global:
                return simpleInstruction("OP_Set_Global", offset);

            case OpCode::OP_Get_Local:
                return byteInstruction("OP_Get_Local", chunk, offset);

            case OpCode::OP_Set_Local:
                return byteInstruction("OP_Set_Local", chunk, offset);

            case OpCode::OP_Jump_If_False:
                return jumpInstruction("OP_Jump_If_False", chunk, offset);

            case OpCode::OP_Long_Jump_If_False:
                return longJumpInstruction("OP_Long_Jump_If_False", chunk, offset);

            case OpCode::OP_Jump:
                return jumpInstruction("OP_Jump", chunk, offset);

            case OpCode::OP_Long_Jump:
                return longJumpInstruction("OP_Long_Jump", chunk, offset);

            case OpCode::OP_Loop:
                return jumpInstruction("OP_Loop", chunk, offset, -1);

            case OpCode::OP_Long_Loop:
                return longJumpInstruction("OP_Long_Loop", chunk, offset, -1);

            case OpCode::OP_Debug_Print:
                return simpleInstruction("OP_Debug_Print", offset);

            case OpCode::OP_Post_Incr_Global:
                return simpleInstruction("OP_Post_Incr_Global", offset);

            case OpCode::OP_Incr_Global:
                return simpleInstruction("OP_Incr_Global", offset);

            case OpCode::OP_Post_Incr_Local:
                return simpleInstruction("OP_Post_Incr_Local", offset);

            case OpCode::OP_Incr_Local:
                return simpleInstruction("OP_Incr_Local", offset);

            case OpCode::OP_Post_Decr_Global:
                return simpleInstruction("OP_Post_Decr_Global", offset);

            case OpCode::OP_Decr_Global:
                return simpleInstruction("OP_Decr_Global", offset);

            case OpCode::OP_Post_Decr_Local:
                return simpleInstruction("OP_Post_Decr_Local", offset);

            case OpCode::OP_Decr_Local:
                return simpleInstruction("OP_Decr_Local", offset);

            case OpCode::OP_Call:
                return byteInstruction("OP_Call", chunk, offset);

            case OpCode::OP_Invoke:
                return invokeInstruction("OP_Invoke", chunk, offset);

            case OpCode::OP_Closure:
            {
                offset++;
                uint8_t cIndex = chunk.code[offset++];
                std::cout << std::left << std::setw(16) << "OP_Closure" << " " << static_cast<int>(cIndex) << " '";
                if (cIndex >= chunk.constants.size())
                {
                    std::cout << "<invalid constant index>";
                }
                else
                {
                    const Value& constant = chunk.constants[cIndex];
                    printValue(nullptr, constant);
                }

                std::cout << "'" << std::endl;

                if (not IS_FUNC(chunk.constants[cIndex]))
                {
                    std::cout << "Error: OP_Closure constant is not a function." << std::endl;
                    return offset;
                }

                ObjFunction* function = vm->asFunction(chunk.constants[cIndex]);

                for (int i = 0; i < function->upvalueCount; i++)
                {
                    uint8_t isLocal = chunk.code[offset++];
                    uint8_t index = chunk.code[offset++];
                    std::cout << std::left << std::setw(16) << "    |-- upvalue" << " " << static_cast<int>(i) << " isLocal=" << static_cast<int>(isLocal) << " index=" << static_cast<int>(index) << std::endl;
                }

                return offset;
            }

            case OpCode::OP_Get_Upvalue:
                return byteInstruction("OP_Get_Upvalue", chunk, offset);

            case OpCode::OP_Set_Upvalue:
                return byteInstruction("OP_Set_Upvalue", chunk, offset);

            case OpCode::OP_Close_Upvalue:
                return simpleInstruction("OP_Close_Upvalue", offset);

            case OpCode::OP_Class:
                return constantInstruction("OP_Class", chunk, offset);

            case OpCode::OP_Set_Property:
                return constantInstruction("OP_Set_Property", chunk, offset);

            case OpCode::OP_Get_Property:
                return constantInstruction("OP_Get_Property", chunk, offset);

            case OpCode::OP_Method:
                return constantInstruction("OP_Method", chunk, offset);

            case OpCode::OP_AddLL:
                return twoBytesInstruction("OP_AddLL", chunk, offset);

            case OpCode::OP_SubtractLL:
                return twoBytesInstruction("OP_SubtractLL", chunk, offset);

            case OpCode::OP_SubtractLC:
                return twoBytesInstruction("OP_SubtractLC", chunk, offset);

            case OpCode::OP_SubtractCL:
                return twoBytesInstruction("OP_SubtractCL", chunk, offset);

            default:
                std::cout << "Unknown opcode " << static_cast<uint8_t>(instruction) << std::endl;
                return offset + 1;
        }
    }
}