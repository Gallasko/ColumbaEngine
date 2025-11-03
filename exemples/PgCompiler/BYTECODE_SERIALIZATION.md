# Bytecode Serialization System

This system allows you to serialize compiled bytecode to binary files and deserialize them later for faster loading.

## Features

- **Binary Format**: Compact binary format with magic number (0x50474243 = "PGBC") and version checking
- **Value Serialization**: Supports integers, doubles, booleans, strings, and nested functions
- **String Pool Integration**: Properly handles strings from the VM's string pool
- **Nested Functions**: Recursively serializes function constants
- **Line Information**: Preserves line numbers for debugging

## File Format

```
[Header]
- Magic Number (4 bytes): 0x50474243 ("PGBC")
- Version (4 bytes): 1

[Code Section]
- Code Size (4 bytes)
- Bytecode (variable length)

[Line Information]
- Line Count (4 bytes)
- Line Numbers (4 bytes each)

[Constants Section]
- Constant Count (4 bytes)
- For each constant:
  - Type Tag (1 byte): INT=0, DOUBLE=1, BOOL=2, STRING=3, FUNCTION=4
  - Value Data (variable length depending on type)
```

## Usage Examples

### 1. Serialize a Chunk

```cpp
#include "chunk_serializer.h"
#include "vm.h"

// After compiling your code to a chunk
Chunk myChunk = /* ... compiled bytecode ... */;
VM* vm = /* ... your VM instance ... */;

// Serialize to file
bool success = pg::ChunkSerializer::serializeToFile(myChunk, "output.pgbc", vm);
if (success) {
    std::cout << "Bytecode saved successfully!" << std::endl;
}
```

### 2. Deserialize a Chunk

```cpp
#include "chunk_serializer.h"
#include "vm.h"

VM* vm = /* ... your VM instance ... */;
Chunk loadedChunk;

// Deserialize from file
bool success = pg::ChunkSerializer::deserializeFromFile(loadedChunk, "output.pgbc", vm);
if (success) {
    std::cout << "Bytecode loaded successfully!" << std::endl;
    // Now you can execute the chunk
    vm->interpret(&loadedChunk);
}
```

### 3. Serialize a Complete Function

```cpp
// For functions with nested closures
ObjFunction* function = /* ... compiled function ... */;

bool success = pg::ChunkSerializer::serializeFunctionToFile(function, "function.pgbc", vm);
```

### 4. Deserialize a Function

```cpp
ObjFunction* function = pg::ChunkSerializer::deserializeFunctionFromFile("function.pgbc", vm);
if (function) {
    // Use the function
}
```

### 5. Stream-based Serialization

```cpp
#include <sstream>

// Serialize to a string stream (for network transmission, etc.)
std::ostringstream oss;
pg::ChunkSerializer::serialize(myChunk, oss, vm);
std::string bytecode = oss.str();

// Deserialize from a string stream
std::istringstream iss(bytecode);
Chunk loadedChunk;
pg::ChunkSerializer::deserialize(loadedChunk, iss, vm);
```

## Integration with Compiler

You can integrate this into your compiler to cache compiled scripts:

```cpp
// In your application.cpp or main compilation loop
bool compileScript(const std::string& sourceFile, VM* vm)
{
    // Check if compiled bytecode exists
    std::string bytecodeFile = sourceFile + ".pgbc";

    // Try to load cached bytecode
    Chunk chunk;
    if (pg::ChunkSerializer::deserializeFromFile(chunk, bytecodeFile, vm))
    {
        std::cout << "Loaded cached bytecode from " << bytecodeFile << std::endl;
        return vm->interpret(&chunk) == InterpretResult::OK;
    }

    // Compile from source
    std::cout << "Compiling " << sourceFile << "..." << std::endl;
    // ... your compilation code ...

    // Save compiled bytecode for next time
    pg::ChunkSerializer::serializeToFile(chunk, bytecodeFile, vm);

    return vm->interpret(&chunk) == InterpretResult::OK;
}
```

## Command-Line Options

You could add command-line flags to your compiler:

```cpp
// Add these options to your argument parser:
--save-bytecode <file>    // Save compiled bytecode to file
--load-bytecode <file>    // Load and execute bytecode from file
--compile-only            // Compile and save bytecode without executing
```

Example implementation:

```cpp
if (args.has("--save-bytecode"))
{
    std::string outputFile = args.get("--save-bytecode");
    if (pg::ChunkSerializer::serializeToFile(chunk, outputFile, &vm))
    {
        std::cout << "Bytecode saved to " << outputFile << std::endl;
    }
}

if (args.has("--load-bytecode"))
{
    std::string inputFile = args.get("--load-bytecode");
    Chunk chunk;
    if (pg::ChunkSerializer::deserializeFromFile(chunk, inputFile, &vm))
    {
        vm.interpret(&chunk);
    }
}
```

## Performance Benefits

- **Faster Startup**: Skip parsing and compilation phases
- **Reduced Memory**: No need to keep source code in memory
- **Distribution**: Ship bytecode instead of source code
- **Caching**: Cache compiled code between runs

## Notes

- **VM Required**: The VM instance is needed for deserializing strings (to recreate them in the string pool)
- **Version Checking**: Files with mismatched version numbers will fail to load
- **Platform Independent**: Binary format uses fixed-size types (uint32_t, etc.)
- **Not Human-Readable**: Use your disassembler tool to inspect bytecode files

## Testing

You can test the serialization with a simple script:

```bash
# Compile and save
./PgCompiler my_script.pg --save-bytecode my_script.pgbc

# Load and execute
./PgCompiler --load-bytecode my_script.pgbc

# Compare output (should be identical)
```

## Future Enhancements

Potential improvements:
- Compression (zlib, lz4)
- Cryptographic signing for tamper detection
- Metadata (source file, compilation date, optimization level)
- Delta encoding for similar bytecode files
- Memory-mapped file loading for large programs