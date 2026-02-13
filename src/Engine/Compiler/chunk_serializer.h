#pragma once

#include "chunk.h"
#include "value_nanbox.h"
#include <fstream>
#include <vector>
#include <cstring>

namespace pg
{
    // Forward declaration
    struct VM;
    // Magic number to identify bytecode files
    constexpr uint32_t BYTECODE_MAGIC = 0x50474243;  // "PGBC" (Pg ByteCode)
    constexpr uint32_t BYTECODE_VERSION = 1;

    class ChunkSerializer
    {
    public:
        // Serialize a chunk to a binary file
        static bool serializeToFile(const Chunk& chunk, const std::string& filename, VM* vm = nullptr)
        {
            std::ofstream file(filename, std::ios::binary);
            if (!file.is_open())
                return false;

            return serialize(chunk, file, vm);
        }

        // Deserialize a chunk from a binary file
        static bool deserializeFromFile(Chunk& chunk, const std::string& filename, VM* vm)
        {
            std::ifstream file(filename, std::ios::binary);
            if (!file.is_open())
                return false;

            return deserialize(chunk, file, vm);
        }

        // Serialize a chunk to an output stream
        static bool serialize(const Chunk& chunk, std::ostream& out, VM* vm = nullptr);

        // Deserialize a chunk from an input stream
        static bool deserialize(Chunk& chunk, std::istream& in, VM* vm);

        // Serialize a complete function (including nested functions)
        static bool serializeFunctionToFile(const ObjFunction* function, const std::string& filename, VM* vm);

        // Deserialize a complete function (including nested functions)
        static ObjFunction* deserializeFunctionFromFile(const std::string& filename, VM* vm);

    private:
        // Value type tags for serialization
        enum class ValueType : uint8_t
        {
            INT = 0,
            DOUBLE = 1,
            BOOL = 2,
            STRING = 3,
            FUNCTION = 4,
            INTERNED_STRING = 5,  // Index into VM's constantStrings (compile-time constants)
            // Other types are not serializable (closures, native functions, etc.)
        };

        // Implementation functions (defined in .cpp)
        static bool serializeValueImpl(std::ostream& out, const Value& value, VM* vm);
        static bool deserializeValueImpl(std::istream& in, Value& value, VM* vm);
        static bool serializeFunctionImpl(const ObjFunction* function, std::ostream& out, VM* vm);
        static ObjFunction* deserializeFunctionImpl(std::istream& in, VM* vm);
        static bool serializeChunkImpl(const Chunk& chunk, std::ostream& out, VM* vm);
        static bool deserializeChunkImpl(Chunk& chunk, std::istream& in, VM* vm);

        // Primitive write helpers
        static void writeUint8(std::ostream& out, uint8_t value)
        {
            out.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        static void writeInt32(std::ostream& out, int32_t value)
        {
            out.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        static void writeUint32(std::ostream& out, uint32_t value)
        {
            out.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        static void writeInt64(std::ostream& out, int64_t value)
        {
            out.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        static void writeUint64(std::ostream& out, uint64_t value)
        {
            out.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        static void writeDouble(std::ostream& out, double value)
        {
            out.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        static void writeString(std::ostream& out, const std::string& str)
        {
            writeUint32(out, static_cast<uint32_t>(str.size()));
            out.write(str.data(), str.size());
        }

        // Primitive read helpers
        static uint8_t readUint8(std::istream& in)
        {
            uint8_t value;
            in.read(reinterpret_cast<char*>(&value), sizeof(value));
            return value;
        }

        static int32_t readInt32(std::istream& in)
        {
            int32_t value;
            in.read(reinterpret_cast<char*>(&value), sizeof(value));
            return value;
        }

        static uint32_t readUint32(std::istream& in)
        {
            uint32_t value;
            in.read(reinterpret_cast<char*>(&value), sizeof(value));
            return value;
        }

        static int64_t readInt64(std::istream& in)
        {
            int64_t value;
            in.read(reinterpret_cast<char*>(&value), sizeof(value));
            return value;
        }

        static uint64_t readUint64(std::istream& in)
        {
            uint64_t value;
            in.read(reinterpret_cast<char*>(&value), sizeof(value));
            return value;
        }

        static double readDouble(std::istream& in)
        {
            double value;
            in.read(reinterpret_cast<char*>(&value), sizeof(value));
            return value;
        }

        static std::string readString(std::istream& in)
        {
            uint32_t size = readUint32(in);
            std::string str(size, '\0');
            in.read(&str[0], size);
            return str;
        }
    };
}