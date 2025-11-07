# Tables Implementation - Clean Plan

## Core Concept
**Tables are just instances of a built-in "Table" class with everything stored in the `fields` map.**

- Integer indices → stored as string keys: `"0"`, `"1"`, `"2"`, etc.
- String keys → stored directly
- Auto-indexing handles mixed syntax seamlessly

## Example Behavior

```pg
var t = ["a": 1, 2, 3, "b": 4, 5]
```

**Parsed as:**
```pg
var t = ["a": 1, "1": 2, "2": 3, "b": 4, "4": 5]
```

**Stored in instance.fields map:**
```
{
  "a" -> INT_VAL(1),
  "1" -> INT_VAL(2),
  "2" -> INT_VAL(3),
  "b" -> INT_VAL(4),
  "4" -> INT_VAL(5)
}
```

---

## Implementation Steps

### 1. No Struct Changes Needed! ✓

Use existing `ObjInstance` as-is:
```cpp
struct ObjInstance {
    Klass* klass;
    std::unordered_map<std::string, Value> fields;
};
```

### 2. Initialize Built-in Table Class

**File: `exemples/PgCompiler/vm.cpp`**

```cpp
void VM::initializeBuiltins() {
    // Create the built-in Table class
    Klass* tableClass = pools.allocateClass("Table");
    globals["Table"] = CLASS_VAL(tableClass);
}
```

### 3. Add Single Opcode

**File: `exemples/PgCompiler/opcode.h`**

```cpp
enum class OpCode : uint8_t {
    // ... existing opcodes ...

    OP_Build_Table,    // Create table instance from stack key-value pairs
    OP_Get_Index,      // table[index] - get field by computed key
    OP_Set_Index,      // table[index] = val - set field by computed key
};
```

### 4. Parser with Auto-Indexing

**File: `exemples/PgCompiler/parser.cpp`**

```cpp
void tableExpression() {
    // Parse: [val, val, "key": val, val, ...]
    // Auto-index elements without explicit keys

    consume("Expect '['", TokenType::CENTER);

    int autoIndex = 0;  // Running counter
    int pairCount = 0;  // Count of key-value pairs

    if (!check(TokenType::CCLOSE)) {
        do {
            // Try to detect explicit key syntax
            bool hasExplicitKey = false;

            // Lookahead: check if next pattern is "something :"
            if ((check(TokenType::STRING) || check(TokenType::EXPRESSION) ||
                 check(TokenType::NUMBER)) && peekNext() == TokenType::DPOINT) {

                // Explicit key!
                Token keyToken = currentToken();
                advance();  // consume key
                advance();  // consume ':'

                // Emit key as constant string
                writeConstant(keyToken.text);

                // Parse value expression
                expression();

                // Update autoIndex if key is numeric
                if (keyToken.type == TokenType::NUMBER) {
                    int keyNum = std::stoi(keyToken.text);
                    if (keyNum >= autoIndex) {
                        autoIndex = keyNum + 1;
                    }
                }

                pairCount++;
            } else {
                // No explicit key - use autoIndex

                // Emit auto-index as key
                writeConstant(std::to_string(autoIndex));

                // Parse value expression
                expression();

                autoIndex++;
                pairCount++;
            }

        } while (match(TokenType::COMMA));
    }

    consume("Expect ']'", TokenType::CCLOSE);

    // Emit build table with pair count
    emitByte(OpCode::OP_Build_Table);
    emitByte(pairCount);
}

// Index access: table[index]
void indexAccess() {
    consume("Expect '['", TokenType::CENTER);

    // Parse index expression
    expression();

    consume("Expect ']'", TokenType::CCLOSE);

    // Check if this is assignment or access
    if (match(TokenType::EQUAL)) {
        // table[index] = value
        expression();
        emitByte(OpCode::OP_Set_Index);
    } else {
        // value = table[index]
        emitByte(OpCode::OP_Get_Index);
    }
}
```

**Add to parse rules:**
```cpp
ParseRule rules[] = {
    // ...
    [TokenType::CENTER] = {tableExpression, indexAccess, Precedence::CALL},
    // ...
};
```

### 5. VM Operations (Simple!)

**File: `exemples/PgCompiler/vm.cpp`**

```cpp
void op_build_table(VM* vm) {
    uint8_t pairCount = *vm->currentFrame->ip++;

    // Get the built-in Table class
    Value tableClassVal = vm->globals["Table"];
    if (!IS_CLASS(tableClassVal)) {
        runtimeError(vm, "Table class not found");
        return;
    }
    Klass* tableClass = AS_CLASS(tableClassVal);

    // Create new instance of Table
    ObjInstance* table = vm->pools.allocateInstance(tableClass);

    // Pop pairCount key-value pairs from stack (in reverse)
    std::vector<std::pair<std::string, Value>> pairs;
    for (int i = 0; i < pairCount; i++) {
        Value value = vm->pop();
        Value key = vm->pop();

        // Convert key to string
        std::string keyStr;
        if (IS_STRING(key)) {
            keyStr = AS_STRING(key);
        } else if (IS_INT(key)) {
            keyStr = std::to_string(AS_INT(key));
        } else {
            runtimeError(vm, "Table key must be string or integer");
            return;
        }

        pairs.push_back({keyStr, value});
    }

    // Insert pairs in correct order (we popped in reverse)
    for (auto it = pairs.rbegin(); it != pairs.rend(); ++it) {
        table->fields[it->first] = it->second;
    }

    vm->push(INSTANCE_VAL(table));
}

void op_get_index(VM* vm) {
    Value index = vm->pop();
    Value instance = vm->pop();

    if (!IS_INSTANCE(instance)) {
        runtimeError(vm, "Can only index tables/instances");
        return;
    }

    ObjInstance* inst = AS_INSTANCE(instance);

    // Convert index to string key
    std::string key;
    if (IS_INT(index)) {
        key = std::to_string(AS_INT(index));
    } else if (IS_STRING(index)) {
        key = AS_STRING(index);
    } else {
        runtimeError(vm, "Index must be integer or string");
        return;
    }

    // Look up in fields map
    auto it = inst->fields.find(key);
    if (it == inst->fields.end()) {
        vm->push(NIL_VAL);  // Or error, your choice
    } else {
        vm->push(it->second);
    }
}

void op_set_index(VM* vm) {
    Value value = vm->pop();
    Value index = vm->pop();
    Value instance = vm->peek(0); // Keep instance on stack

    if (!IS_INSTANCE(instance)) {
        runtimeError(vm, "Can only index tables/instances");
        return;
    }

    ObjInstance* inst = AS_INSTANCE(instance);

    // Convert index to string key
    std::string key;
    if (IS_INT(index)) {
        key = std::to_string(AS_INT(index));
    } else if (IS_STRING(index)) {
        key = AS_STRING(index);
    } else {
        runtimeError(vm, "Index must be integer or string");
        return;
    }

    // Store in fields map
    inst->fields[key] = value;
}
```

### 6. Register Operations in VM

**File: `exemples/PgCompiler/vm.cpp` - operation table**

```cpp
// In VM initialization
operations[OP_Build_Table] = {op_build_table, "OP_Build_Table", 1};
operations[OP_Get_Index] = {op_get_index, "OP_Get_Index", 0};
operations[OP_Set_Index] = {op_set_index, "OP_Set_Index", 0};
```

---

## Testing

```pg
// Test 1: Pure array
var arr = [10, 20, 30, 40]
__dprint(arr[0])   // 10
__dprint(arr[3])   // 40
arr[1] = 99
__dprint(arr[1])   // 99

// Test 2: Pure map
var map = ["name": "Alice", "age": 25]
__dprint(map["name"])  // "Alice"
map["age"] = 26
__dprint(map["age"])   // 26

// Test 3: Mixed with auto-indexing
var mixed = ["a": 1, 2, 3, "b": 4, 5]
__dprint(mixed["a"])   // 1
__dprint(mixed["1"])   // 2  (auto-indexed)
__dprint(mixed["2"])   // 3  (auto-indexed)
__dprint(mixed["b"])   // 4
__dprint(mixed["4"])   // 5  (auto-indexed, skipped 3)

// Test 4: Gap handling
var gaps = [100, "x": 200, 300]
__dprint(gaps["0"])    // 100
__dprint(gaps["x"])    // 200
__dprint(gaps["1"])    // 300 (autoIndex continued from 0)
```

---

## Files to Modify

1. `exemples/PgCompiler/vm.cpp`
   - Add `initializeBuiltins()` to create Table class
   - Add `op_build_table()`, `op_get_index()`, `op_set_index()`
   - Register operations in dispatch table

2. `exemples/PgCompiler/opcode.h`
   - Add `OP_Build_Table`, `OP_Get_Index`, `OP_Set_Index`

3. `exemples/PgCompiler/parser.h`
   - Declare `tableExpression()` and `indexAccess()`

4. `exemples/PgCompiler/parser.cpp`
   - Implement `tableExpression()` with auto-indexing
   - Implement `indexAccess()`
   - Add to parse rules for `TokenType::CENTER`

---

## Advantages of This Approach

✅ **No new types** - reuses existing infrastructure
✅ **Simple** - everything is just a map
✅ **Flexible** - handles arrays, maps, and mixed seamlessly
✅ **No memory overhead** - no separate array storage
✅ **Consistent** - all indexing goes through same path
✅ **Future-proof** - easy to add methods like `.length()` or `.push()`

---

**Status:** Ready for Implementation