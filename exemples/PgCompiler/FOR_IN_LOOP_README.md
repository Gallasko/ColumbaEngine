# For-In Loop Feature

This document describes the new `for-in` loop feature added to the PgCompiler scripting language.

## Syntax

```javascript
for (var key : table) {
    // Loop body
    // 'key' contains the current field name as a string
}
```

## Description

The for-in loop allows you to iterate over all fields in a table (object/instance). On each iteration, the loop variable contains the **key** (field name) as a string.

## Usage Examples

### Basic Table Iteration

```javascript
var player = {
    "name": "Alice",
    "health": 100,
    "score": 5000
};

for (var key : player) {
    logInfo(key + " = " + toString(player[key]));
}

// Output:
// name = Alice
// health = 100
// score = 5000
```

### Nested Table Iteration

```javascript
var entity = {
    "id": 42,
    "Transform": {
        "x": 100,
        "y": 200
    },
    "Velocity": {
        "dx": 5.0,
        "dy": -3.0
    }
};

for (var componentName : entity) {
    logInfo("Component: " + componentName);
    var component = entity[componentName];

    if (componentName != "id") {
        for (var propName : component) {
            logInfo("  " + propName + " = " + toString(component[propName]));
        }
    }
}

// Output:
// Component: id
// Component: Transform
//   x = 100
//   y = 200
// Component: Velocity
//   dx = 5.0
//   dy = -3.0
```

### Debugging Tables

The for-in loop is particularly useful for debugging and inspecting table contents:

```javascript
fun debugTable(table, indent) {
    for (var key : table) {
        logInfo(indent + key + " = " + toString(table[key]));
    }
}

debugTable(playerEntity, "  ");
```

### ECS Entity Inspection

When working with ECS entities passed from C++:

```javascript
// playerEntity is passed from C++ as a serialized entity
for (var key : playerEntity) {
    if (key == "__entityId") {
        logInfo("Entity ID: " + toString(playerEntity[key]));
    }
    else if (key == "PositionComponent") {
        var pos = playerEntity["PositionComponent"];
        logInfo("Position:");
        for (var prop : pos) {
            logInfo("  " + prop + " = " + toString(pos[prop]));
        }
    }
}
```

## Implementation Details

### Parser Changes

The for statement parser (`forStatement()` in `cparser.cpp`) was extended to detect the for-in syntax:

```cpp
if (match(TokenType::TOK_VAR)) {
    consume("Expect variable name after 'var'.", TokenType::EXPRESSION);
    Token varToken = previousToken;

    if (match(TokenType::DPOINT)) {  // : token
        // This is a for-in loop
        expression();  // Parse the table expression
        // ... emit iterator opcodes
    }
}
```

### New Opcodes

Two new opcodes were added to support iteration:

1. **`OP_Get_Iterator`**: Initializes an iterator for a table
   - Input: `[table]` on stack
   - Output: `[table, iterator_state]` on stack
   - The iterator state is an integer tracking the current position

2. **`OP_Iterator_Next`**: Advances the iterator and gets the next key
   - Input: `[table, iterator_state]` on stack
   - Output: `[table, new_iterator_state, key_or_false]` on stack
   - Returns `false` when iteration is complete

### VM Implementation

The iterator operations are implemented in `vm.cpp`:

```cpp
void op_get_iterator(VM* vm)
{
    Value tableVal = vm->peek(0);  // Keep table on stack
    // Create an integer to track iteration index
    vm->push(makeIntValue(0));
}

void op_iterator_next(VM* vm)
{
    Value iteratorState = vm->pop();
    Value tableVal = vm->peek(0);

    ObjInstance* table = vm->asInstance(tableVal);
    int64_t index = AS_INT(iteratorState);

    if (static_cast<size_t>(index) >= table->fields.size()) {
        // End of iteration
        vm->push(makeIntValue(index));
        vm->push(makeBoolValue(false));
        return;
    }

    // Get the key at current index
    auto it = table->fields.begin();
    std::advance(it, index);

    // Push updated state and key
    vm->push(makeIntValue(index + 1));
    Value keyVal = vm->createString(it->first);
    vm->push(keyVal);
}
```

### Stack Layout During Iteration

```
Initial state:
  [table]

After OP_Get_Iterator:
  [table, iterator_state(0)]

After OP_Iterator_Next (success):
  [table, iterator_state(n+1), key_string]

After OP_Iterator_Next (end):
  [table, iterator_state(n), false]
```

### Compiler-Generated Code Structure

```
1. Parse table expression
2. OP_Get_Iterator              # Initialize iterator
3. Store iterator in hidden local
4. Loop start:
5.   Get iterator from local
6.   OP_Iterator_Next           # Get next key
7.   Duplicate key for check
8.   OP_Long_Jump_If_False end  # Exit if false
9.   Pop duplicate
10.  Store key in loop variable
11.  Execute loop body
12.  Pop loop variable
13.  OP_Long_Loop loop_start
14. End:
15.  Pop false/nil
16.  Pop iterator
17.  Pop table
```

## Limitations

1. **Keys Only**: The for-in loop provides only the **key**, not the value. You must index the table to get the value.

   ```javascript
   for (var key : table) {
       var value = table[key];  // Must explicitly get the value
   }
   ```

2. **Strings**: Keys are always strings, even if the original field was set with an integer index.

3. **Iteration Order**: The iteration order matches the internal storage order of the table's fields (currently using `std::unordered_map` in C++), which is not guaranteed to be any specific order.

4. **No Modification During Iteration**: Modifying the table structure (adding/removing fields) while iterating may cause undefined behavior.

## Comparison with Traditional For Loop

### For-In Loop (New)
```javascript
for (var key : table) {
    logInfo(key + ": " + toString(table[key]));
}
```

**Pros:**
- Clean, readable syntax
- No need to manage iteration manually
- Works with any table

**Cons:**
- Keys only (must index to get values)
- Can't control iteration order
- Can't break out early (yet - break not implemented)

### Traditional For Loop
```javascript
for (var i = 0; i < count; i = i + 1) {
    // ...
}
```

**Pros:**
- Full control over iteration
- Can iterate by index
- Can break early

**Cons:**
- Requires knowing the size
- More verbose
- Doesn't work naturally with key-value tables

## Testing

A test script is provided in `test_for_in.pg`:

```bash
# Run the test
./PgCompiler test_for_in.pg
```

Or use it with the ECS serialization example:

```bash
./EcsSerializationExample
```

The ECS example passes an entity table to the script as a global variable, allowing you to iterate over the entity's components using the for-in loop.

## Future Enhancements

Potential improvements for the feature:

1. **For-In with Value**: `for (var key, value : table)` to get both key and value
2. **Break/Continue**: Add break and continue statements for early exit
3. **For-Of**: Iterate over values only: `for (var value :values: table)`
4. **Array Iteration**: Special handling for numeric indices to iterate in order
5. **Iterator Protocol**: Allow custom objects to define iteration behavior

## Files Modified

- `chunk.h` - Added `OP_Get_Iterator` and `OP_Iterator_Next` opcodes
- `cparser.cpp` - Extended `forStatement()` to handle for-in syntax
- `vm.cpp` - Implemented `op_get_iterator()` and `op_iterator_next()`
- `compiler_debug.cpp` - Added debug output for new opcodes

## Related Documentation

- [ECS Serialization README](ECS_SERIALIZATION_README.md) - How to serialize entities to tables
- PgCompiler Language Reference - General language documentation
