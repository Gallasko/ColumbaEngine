# Adding New Opcodes to the VM

This guide walks you through all the steps required to add a new opcode to the PgEngine VM.

## Overview

Adding a new opcode involves modifying several files across the compiler and VM infrastructure. This document uses the `OP_Modulo` implementation as a reference example.

## Step-by-Step Checklist

### 1. Define the Opcode (chunk.h)

**File:** `src/Engine/Compiler/chunk.h`

Add your new opcode to the `OpCode` enum:

```cpp
enum class OpCode : uint8_t
{
    // ... existing opcodes ...
    OP_Divide,
    OP_Modulo,        // Add your new opcode here
    OP_True,
    // ... rest of opcodes ...
};
```

**Location:** Add it in a logical group (arithmetic ops with arithmetic, comparison with comparison, etc.)

**Important:** The order matters! Opcodes are serialized by their numeric value, so inserting in the middle will break compatibility with existing bytecode files.

---

### 2. Update Instruction Size Function (chunk.h)

**File:** `src/Engine/Compiler/chunk.h`

Add your opcode to the `getInstructionSize()` function:

```cpp
inline int getInstructionSize(OpCode opcode)
{
    switch (opcode)
    {
        // ... existing cases ...
        case OpCode::OP_Divide:
        case OpCode::OP_Modulo:        // Add here
        case OpCode::OP_True:
            return 1; // opcode only, no operand

        // Or if your opcode has operands:
        case OpCode::OP_YourOpcode:
            return 2; // opcode + 1 byte operand
        // ... rest of cases ...
    }
}
```

**Common sizes:**
- `1 byte`: Simple operations (no operands) - most arithmetic/logic ops
- `2 bytes`: Opcode + 1 byte operand - constants, locals, calls
- `3 bytes`: Opcode + 2 byte operand - jumps, some optimized ops
- `4 bytes`: Long constants (opcode + 3 byte operand)
- `5 bytes`: Long jumps (opcode + 4 byte operand)

---

### 3. Implement the Operation Handler (vm.cpp)

**File:** `src/Engine/Compiler/vm.cpp`

#### 3a. Add Forward Declaration

At the top of the file (around line 70-80):

```cpp
// Forward declarations for operation handlers
void op_add(VM* vm);
void op_divide(VM* vm);
void op_modulo(VM* vm);        // Add your forward declaration
void op_negate(VM* vm);
```

#### 3b. Implement the Handler Function

For simple binary operations, use the `BINARY_OP_TEMPLATE` macro (around line 1323):

```cpp
BINARY_OP_TEMPLATE(op_add, addValues)
BINARY_OP_TEMPLATE(op_divide, divideValues)
BINARY_OP_TEMPLATE(op_modulo, moduloValues)    // Add your operation
```

For custom operations, implement manually:

```cpp
void op_your_opcode(VM* vm)
{
    // Pop operands from stack (in reverse order)
    Value operand2 = vm->pop();
    Value operand1 = vm->pop();

    // Perform operation
    Value result = vm->yourOperation(operand1, operand2);

    // Push result
    vm->push(result);
}
```

**Stack manipulation patterns:**
- **Nullary ops** (no operands): Just push a value
- **Unary ops** (1 operand): Pop 1, compute, push result
- **Binary ops** (2 operands): Pop 2 (reverse order!), compute, push result
- **Ternary ops** (3 operands): Pop 3 (reverse order!), compute, push result

#### 3c. Implement Value Operation Method (if needed)

If you're adding a new operation type, add the method to `VM` class (around line 928):

```cpp
Value VM::moduloValues(const Value& a, const Value& b)
{
    // Fast path for integers
    if (IS_INT(a) && IS_INT(b) && AS_INT(b) != 0)
        return INT_VAL(AS_INT(a) % AS_INT(b));

    // Fast path for floats
    if (IS_FLOAT(a) && IS_FLOAT(b) && areNotAlmostEqual(static_cast<float>(AS_FLOAT(b)), 0.0f))
        return FLOAT_VAL(std::fmod(AS_FLOAT(a), AS_FLOAT(b)));

    // Handle type conversions, error cases, etc.
    // ...

    throw std::runtime_error("Invalid types for modulo operation");
}
```

**Best practices:**
- Handle fast paths first (common types)
- Handle type conversions
- Validate inputs (division by zero, null checks, etc.)
- Throw descriptive errors

---

### 4. Declare Operation Method in VM Header (vm.h)

**File:** `src/Engine/Compiler/vm.h`

Add the method declaration (around line 408):

```cpp
// Arithmetic operations with proper reference tracking
Value addValues(const Value a, const Value b);
Value divideValues(const Value& a, const Value& b);
Value moduloValues(const Value& a, const Value& b);    // Add your declaration
Value negateValue(const Value& val);
```

---

### 5. Register the Operation (vm.cpp)

**File:** `src/Engine/Compiler/vm.cpp`

In the `register_builtin_operations()` function (around line 1158):

```cpp
void VM::register_builtin_operations() {
    register_operation(static_cast<uint8_t>(OpCode::OP_Add), op_add);
    register_operation(static_cast<uint8_t>(OpCode::OP_Divide), op_divide);
    register_operation(static_cast<uint8_t>(OpCode::OP_Modulo), op_modulo);    // Add registration
    register_operation(static_cast<uint8_t>(OpCode::OP_Negate), op_negate);
    // ... rest of registrations ...
}
```

**Important:** Every opcode MUST be registered, or it will cause a null pointer dereference when executed!

---

### 6. Add Parser Support (cparser.cpp)

**File:** `src/Engine/Compiler/cparser.cpp`

#### 6a. Add Parse Rule (CRITICAL!)

First, add the parse rule to the `rules` map (around line 614):

```cpp
std::unordered_map<TokenType, ParseRule> rules = {
    // ... existing rules ...
    {TokenType::STAR,         {NULL,        binary,     Precedence::FACTOR}},
    {TokenType::SLASH,        {NULL,        binary,     Precedence::FACTOR}},
    {TokenType::MOD,          {NULL,        binary,     Precedence::FACTOR}},  // Add here!
    // ... rest of rules ...
};
```

**Rule format:** `{TokenType, {prefix_fn, infix_fn, precedence}}`
- **prefix_fn**: Function to call when token appears at start (NULL for binary ops)
- **infix_fn**: Function to call when token appears between operands (binary, dot, call, etc.)
- **precedence**: Operator precedence level

**Common precedences (lowest to highest):**
- `NONE`: Not an operator
- `ASSIGNMENT`: `=`, `+=`, etc.
- `OR`: `||`
- `AND`: `&&`
- `EQUALITY`: `==`, `!=`
- `COMPARISON`: `<`, `>`, `<=`, `>=`
- `TERM`: `+`, `-`
- `FACTOR`: `*`, `/`, `%`
- `UNARY`: `-`, `!`
- `CALL`: `.`, `()`
- `PRIMARY`: Highest

#### 6b. Add Opcode Emission

Add handling for the token in the `binary()` function (around line 99-107):

```cpp
switch (operatorType)
{
    case TokenType::PLUS:
        parser.writeByte(OpCode::OP_Add);
        break;
    case TokenType::MOD:
        parser.writeByte(OpCode::OP_Modulo);    // Add your opcode emission
        break;
    // ... rest of cases ...
}
```

**Location depends on opcode type:**
- **Binary operators:** In `binary()` function + add parse rule
- **Unary operators:** In `unary()` function + add parse rule
- **Statements:** In relevant statement parsing function
- **Control flow:** In control flow parsing functions

---

### 7. Add Token Type (if needed) (token.h)

**File:** `src/Engine/Interpreter/token.h`

If you need a new token type (for modulo, `%` was already defined as `MOD`):

```cpp
enum class TokenType : int
{
    // Single character Operators
    STAR   = '*',
    MOD    = '%',        // Already exists for modulo
    YOUR_TOKEN = '?',    // Add new single-char token

    // Two character Operators
    YOUR_TWO_CHAR = 270, // Add two-char token with unique number
    // ...
};
```

**Token ranges:**
- Single char: Use the character's ASCII value
- Two char: Use range 270-299
- Keywords: Use range 300+

---

### 8. Add Disassembly Support (compiler_debug.cpp)

**File:** `src/Engine/Compiler/compiler_debug.cpp`

#### 8a. Add to `disassembleInstruction()` (around line 274)

```cpp
int disassembleInstruction(VM* vm, const Chunk& chunk, int offset)
{
    // ... existing code ...

    case OpCode::OP_Divide:
        return simpleInstruction("OP_Divide", offset);

    case OpCode::OP_Modulo:
        return simpleInstruction("OP_Modulo", offset);    // Add disassembly

    case OpCode::OP_True:
        return simpleInstruction("OP_True", offset);
    // ... rest of cases ...
}
```

**Helper functions:**
- `simpleInstruction()`: No operands
- `constantInstruction()`: 1-byte constant index operand
- `longConstantInstruction()`: 3-byte constant index operand
- `byteInstruction()`: 1-byte numeric operand
- `jumpInstruction()`: 2-byte jump offset
- `longJumpInstruction()`: 4-byte jump offset

#### 8b. Add to `opcodeToString()` (around line 512)

```cpp
const char* opcodeToString(OpCode opcode)
{
    switch (opcode)
    {
        // ... existing cases ...
        case OpCode::OP_Divide: return "OP_Divide";
        case OpCode::OP_Modulo: return "OP_Modulo";    // Add string conversion
        case OpCode::OP_True: return "OP_True";
        // ... rest of cases ...
    }
}
```

This is used for profiling reports and debug output.

---

### 9. Add Dependencies (if needed)

**File:** `src/Engine/stdafx.h`

If your operation requires new standard library includes:

```cpp
// C++ standard
#include <vector>
#include <string>
// ... existing includes ...
#include <cmath>        // Added for std::fmod in OP_Modulo
```

---

### 10. Test Your Opcode

Create a test script to verify your opcode works:

```javascript
// test_modulo.pg
var a = 10 % 3;
print(a);  // Should print: 1

var b = 10.5 % 3.2;
print(b);  // Should print: 1.1

var c = 7 % 2;
print(c);  // Should print: 1
```

Run with profiling to see your opcode in action:

```bash
./PgCompiler --profile test_modulo.pg
```

---

## Common Opcode Patterns

### Simple Binary Arithmetic Operation

```cpp
// 1. Add to OpCode enum
OP_YourOp,

// 2. Implement value operation
Value VM::yourOpValues(const Value& a, const Value& b) {
    if (IS_INT(a) && IS_INT(b))
        return INT_VAL(AS_INT(a) OP AS_INT(b));
    // ... handle other types ...
}

// 3. Use BINARY_OP_TEMPLATE
BINARY_OP_TEMPLATE(op_your_op, yourOpValues)

// 4. Register
register_operation(static_cast<uint8_t>(OpCode::OP_YourOp), op_your_op);

// 5. Parser
case TokenType::YOUR_TOKEN:
    parser.writeByte(OpCode::OP_YourOp);
    break;

// 6. Disassembly
case OpCode::OP_YourOp:
    return simpleInstruction("OP_YourOp", offset);
```

### Stack Manipulation Operation

```cpp
// Example: OP_PopN - pops N values from stack
void op_pop_n(VM* vm)
{
    // Read operand (how many to pop)
    uint8_t count = vm->currentFrame->closure->function->chunk.code[vm->currentFrame->ip++];

    // Pop N values
    for (int i = 0; i < count; i++)
    {
        vm->pop();
    }
}

// In disassembly, use byteInstruction for the count operand
case OpCode::OP_PopN:
    return byteInstruction("OP_PopN", chunk, offset);
```

### Control Flow Operation

```cpp
// Example: OP_Jump_If_False
void op_jump_if_false(VM* vm)
{
    // Read 2-byte jump offset
    uint16_t offset = (vm->currentFrame->closure->function->chunk.code[vm->currentFrame->ip] << 8);
    offset |= vm->currentFrame->closure->function->chunk.code[vm->currentFrame->ip + 1];
    vm->currentFrame->ip += 2;

    // Check condition on top of stack (don't pop yet)
    Value condition = vm->peek(0);

    // Jump if false
    if (!truthy(condition))
    {
        vm->currentFrame->ip += offset;
    }

    // Pop the condition
    vm->pop();
}

// Disassembly
case OpCode::OP_Jump_If_False:
    return jumpInstruction("OP_Jump_If_False", chunk, offset);
```

---

## Checklist Summary

When adding a new opcode, make changes in these files (in order):

1. ✅ **chunk.h**: Add to `OpCode` enum and `getInstructionSize()`
2. ✅ **vm.cpp**: Forward declaration, handler implementation, value operation (if needed)
3. ✅ **vm.h**: Declare value operation method (if needed)
4. ✅ **vm.cpp**: Register in `register_builtin_operations()`
5. ✅ **token.h**: Add token type (if needed)
6. ✅ **cparser.cpp**:
   - **Add parse rule** to `rules` map (precedence and parser function)
   - **Add opcode emission** in appropriate parsing function
7. ✅ **compiler_debug.cpp**: Add to `disassembleInstruction()` and `opcodeToString()`
8. ✅ **stdafx.h**: Add any required includes
9. ✅ **Test**: Write and run test script

---

## Common Pitfalls

### 1. Forgetting to Register the Operation
**Symptom:** Segmentation fault when executing bytecode with your opcode
**Solution:** Always call `register_operation()` in `register_builtin_operations()`

### 2. Wrong Stack Order
**Symptom:** Incorrect results, especially for non-commutative operations (subtract, divide)
**Solution:** Remember stack is LIFO - pop operands in reverse order:
```cpp
Value b = vm->pop();  // Second operand
Value a = vm->pop();  // First operand
vm->push(a - b);      // NOT b - a!
```

### 3. Forgetting Parse Rule
**Symptom:** Parser doesn't recognize your operator, syntax errors in valid code
**Solution:** Add entry to `rules` map in cparser.cpp with correct precedence and parser function

### 4. Forgetting Disassembly Support
**Symptom:** Bytecode dumps show "Unknown opcode" or crash
**Solution:** Add case to both `disassembleInstruction()` and `opcodeToString()`

### 5. Wrong Instruction Size
**Symptom:** Bytecode deserialization errors, offset misalignment
**Solution:** Ensure `getInstructionSize()` returns correct size for your opcode

### 6. Not Handling All Value Types
**Symptom:** Runtime errors for certain type combinations
**Solution:** Handle int, float, string, and mixed types appropriately

### 7. Breaking Bytecode Compatibility
**Symptom:** Old bytecode files fail to load
**Solution:** Always add new opcodes at the end of logical groups, never insert in the middle

---

## Performance Considerations

### Fast Paths
Always implement fast paths for common types:

```cpp
// Fast path for integers (most common)
if (IS_INT(a) && IS_INT(b))
    return INT_VAL(AS_INT(a) % AS_INT(b));

// Fast path for floats
if (IS_FLOAT(a) && IS_FLOAT(b))
    return FLOAT_VAL(std::fmod(AS_FLOAT(a), AS_FLOAT(b)));

// Slower paths for mixed types or error handling
// ...
```

### Reference Counting
The `BINARY_OP_TEMPLATE` macro handles reference counting automatically. If writing custom handlers, remember to:
- Acquire references for heap objects before pushing
- Release references for popped values

### Profiling
Your opcode will automatically appear in profiling reports if you use the profiling system correctly. The opcode name from `opcodeToString()` will be used in reports.

---

## Advanced: Optimized Opcodes

For performance-critical operations, create specialized opcodes:

```cpp
// Instead of generic OP_Add, create optimized versions:
OP_AddLL,      // Add two local variables (no stack ops)
OP_AddLC,      // Add local and constant
OP_AddCC,      // Add two constants (can be optimized away)
```

These require additional complexity but can significantly improve performance for hot loops.

---

## Example: Complete OP_Modulo Implementation

See the git commit for OP_Modulo for a complete, working example of all the steps above.

**Files modified:**
- `src/Engine/Compiler/chunk.h`
- `src/Engine/Compiler/vm.h`
- `src/Engine/Compiler/vm.cpp`
- `src/Engine/Compiler/cparser.cpp`
- `src/Engine/Compiler/compiler_debug.cpp`
- `src/Engine/stdafx.h`

---

## Questions or Issues?

If you encounter problems while adding opcodes:
1. Check this document thoroughly
2. Reference existing opcodes similar to yours
3. Use the profiler to debug execution
4. Check disassembly output to verify correct bytecode generation
5. Consult the VM architecture documentation

---

**Last Updated:** 2026-01-20
**Version:** 1.0
