#include "stdafx.h"

#include "chunk_serializer.h"
#include "vm.h"

namespace pg
{
    // Implementation of serializeValue that needs VM definition
    bool ChunkSerializer::serializeValueImpl(std::ostream& out, const Value& value, VM* vm)
    {
        if (IS_INT(value))
        {
            writeUint8(out, static_cast<uint8_t>(ValueType::INT));
            writeInt64(out, AS_INT(value));
            return true;
        }
        else if (IS_DOUBLE(value))
        {
            writeUint8(out, static_cast<uint8_t>(ValueType::DOUBLE));
            writeDouble(out, AS_DOUBLE(value));
            return true;
        }
        else if (IS_BOOL(value))
        {
            writeUint8(out, static_cast<uint8_t>(ValueType::BOOL));
            writeUint8(out, AS_BOOL(value) ? 1 : 0);
            return true;
        }
        else if (IS_STRING(value))
        {
            writeUint8(out, static_cast<uint8_t>(ValueType::STRING));

            // Get the actual string content from the VM's string pool
            if (vm != nullptr)
            {
                std::string strContent = vm->asString(value);
                writeString(out, strContent);
            }
            else
            {
                // Fallback: serialize empty string if no VM provided
                writeString(out, "");
            }
            return true;
        }
        else if (IS_FUNC(value))
        {
            // For functions in constants (nested functions), we need to serialize recursively
            writeUint8(out, static_cast<uint8_t>(ValueType::FUNCTION));
            if (vm != nullptr)
            {
                ObjFunction* func = vm->asFunction(value);
                return serializeFunctionImpl(func, out, vm);
            }
            return false;
        }
        else
        {
            LOG_ERROR("ChunkSerializer", "Unsupported value type for serialization: " << valueTypeName(value));
            // Unsupported type for serialization
            return false;
        }
    }

    bool ChunkSerializer::deserializeValueImpl(std::istream& in, Value& value, VM* vm)
    {
        uint8_t typeTag = readUint8(in);
        ValueType type = static_cast<ValueType>(typeTag);

        switch (type)
        {
            case ValueType::INT:
            {
                int64_t intVal = readInt64(in);
                value = makeIntValue(intVal);
                return true;
            }
            case ValueType::DOUBLE:
            {
                double doubleVal = readDouble(in);
                value = makeDoubleValue(doubleVal);
                return true;
            }
            case ValueType::BOOL:
            {
                uint8_t boolVal = readUint8(in);
                value = makeBoolValue(boolVal != 0);
                return true;
            }
            case ValueType::STRING:
            {
                // Read string content and recreate it in the VM's string pool
                std::string strContent = readString(in);
                if (vm != nullptr)
                {
                    ElementType elem(strContent);
                    value = vm->createString(elem);
                }
                else
                {
                    // Without VM, we can't properly deserialize strings
                    value = makeBoolValue(false);
                }
                return true;
            }
            case ValueType::FUNCTION:
            {
                // Deserialize nested function
                ObjFunction* func = deserializeFunctionImpl(in, vm);
                if (func != nullptr && vm != nullptr)
                {
                    value = vm->createFunction();
                    *vm->asFunction(value) = *func;
                    delete func;
                    return true;
                }
                return false;
            }
            default:
                return false;
        }
    }

    bool ChunkSerializer::serializeFunctionImpl(const ObjFunction* function, std::ostream& out, VM* vm)
    {
        // Don't write magic/version for nested functions (already written at top level)
        // Write function metadata
        writeInt32(out, function->arity);
        writeString(out, function->name);
        writeInt32(out, function->upvalueCount);

        // Write the chunk (without magic/version)
        return serializeChunkImpl(function->chunk, out, vm);
    }

    ObjFunction* ChunkSerializer::deserializeFunctionImpl(std::istream& in, VM* vm)
    {
        // Create new function
        ObjFunction* function = new ObjFunction();

        // Read function metadata
        function->arity = readInt32(in);
        function->name = readString(in);
        function->upvalueCount = readInt32(in);

        // Read the chunk
        if (!deserializeChunkImpl(function->chunk, in, vm))
        {
            delete function;
            return nullptr;
        }

        return function;
    }

    bool ChunkSerializer::serializeChunkImpl(const Chunk& chunk, std::ostream& out, VM* vm)
    {
        // Write code section
        writeUint32(out, static_cast<uint32_t>(chunk.code.size()));
        out.write(reinterpret_cast<const char*>(chunk.code.data()), chunk.code.size());

        // Write line information
        writeUint32(out, static_cast<uint32_t>(chunk.lines.size()));
        for (int line : chunk.lines)
        {
            writeInt32(out, line);
        }

        // Write constants section
        writeUint32(out, static_cast<uint32_t>(chunk.constants.size()));
        for (const Value& value : chunk.constants)
        {
            if (!serializeValueImpl(out, value, vm))
                return false;
        }

        // Write constantStrings section (for interned property names)
        writeUint32(out, static_cast<uint32_t>(chunk.constantStrings.size()));
        for (const std::string& str : chunk.constantStrings)
        {
            writeString(out, str);
        }

        return out.good();
    }

    bool ChunkSerializer::deserializeChunkImpl(Chunk& chunk, std::istream& in, VM* vm)
    {
        // Read code section
        uint32_t codeSize = readUint32(in);
        chunk.code.resize(codeSize);
        in.read(reinterpret_cast<char*>(chunk.code.data()), codeSize);

        // Read line information
        uint32_t linesSize = readUint32(in);
        chunk.lines.resize(linesSize);
        for (uint32_t i = 0; i < linesSize; i++)
        {
            chunk.lines[i] = readInt32(in);
        }

        // Read constants section
        uint32_t constantsCount = readUint32(in);
        chunk.constants.reserve(constantsCount);
        for (uint32_t i = 0; i < constantsCount; i++)
        {
            Value value;
            if (!deserializeValueImpl(in, value, vm))
                return false;
            chunk.constants.push_back(value);
        }

        // Read constantStrings section (for interned property names)
        uint32_t constantStringsCount = readUint32(in);
        chunk.constantStrings.reserve(constantStringsCount);
        for (uint32_t i = 0; i < constantStringsCount; i++)
        {
            std::string str = readString(in);
            chunk.constantStrings.push_back(str);
        }

        return in.good();
    }
}