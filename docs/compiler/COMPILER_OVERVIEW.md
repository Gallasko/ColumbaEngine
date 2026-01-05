# PgEngine Compiler Overview

## Table of Contents
- [Introduction](#introduction)
- [Architecture](#architecture)
- [Language Features](#language-features)
- [Type System](#type-system)
- [Standard Library](#standard-library)
- [Memory Management](#memory-management)
- [Performance Features](#performance-features)
- [Integration with ECS](#integration-with-ecs)

## Introduction

The PgEngine compiler is a bytecode compiler and virtual machine (VM) for the Pg scripting language. It provides a fast, memory-efficient runtime for game logic scripting with tight integration to the engine's Entity Component System (ECS).

### Key Features
- **Bytecode compilation** with caching (.pg → .pgc)
- **NaN-boxing value representation** for efficient memory usage
- **Pool-based memory allocation** with reference counting
- **First-class functions** with closures
- **Object-oriented programming** with classes and instances
- **Native module system** for C++ integration
- **ECS integration** for game entity manipulation

## Architecture

### Compilation Pipeline

```
Source Code (.pg)
    ↓
Lexer (tokens)
    ↓
Parser (AST)
    ↓
Compiler (bytecode)
    ↓
Bytecode Cache (.pgc)
    ↓
VM Execution
```

### Core Components

#### 1. Lexer (`lexer.h`)
- Tokenizes source code
- Handles keywords, operators, literals
- Tracks line numbers for error reporting

#### 2. Parser (`cparser.cpp`)
- Recursive descent parser
- Pratt parsing for expressions
- Generates bytecode directly (single-pass)

#### 3. Virtual Machine (`vm.cpp`, `vm.h`)
- Stack-based bytecode interpreter
- Call frame management
- Global and local variable storage
- Reference counting for memory management

#### 4. Memory Pools (`vmpools.h`)
- Segregated pool allocators for each object type
- O(1) allocation and deallocation
- Reduced heap fragmentation
- Better cache locality

## Language Features

### Variables and Scoping

```javascript
// Global variables
var globalVar = 42

// Local variables (block scoped)
{
    var localVar = 10
    __dprint(localVar)  // 10
}
// __dprint(localVar)  // Error: undefined

// Assignment
var x = 5
x = x + 1  // 6
```

### Data Types

#### Primitives
```javascript
// Integers (64-bit)
var integer = 42

// Floats (double precision)
var float = 3.14

// Booleans
var truth = true
var lie = false

// Strings
var str = "Hello, World!"
```

#### Composite Types

**Vectors (Arrays)**
```javascript
// Simple vector
var vec = [1, 2, 3, 4, 5]
__dprint(vec[0])  // 1

// Sparse vector with explicit indices
var sparse = [0: 10, 5: 20, 10: 30]
__dprint(sparse[5])  // 20

// Negative indexing (Python-style)
var arr = [1, 2, 3, 4, 5]
__dprint(arr[-1])  // 5 (last element)
__dprint(arr[-2])  // 4 (second to last)

// Vector operations
vec[2] = 99         // Set element
vec[vec.length()] = 6  // Append (if supported)
```

**Tables (Dictionaries/Objects)**
```javascript
// Table literal
var person = {
    name: "Alice",
    age: 30,
    city: "Paris"
}

// Access properties
__dprint(person.name)     // "Alice"
__dprint(person["age"])   // 30

// Modify properties
person.age = 31
person["city"] = "London"

// Add new properties
person.email = "alice@example.com"
```

### Control Flow

#### Conditionals
```javascript
// If-else
if (x > 0) {
    __dprint("Positive")
} else if (x < 0) {
    __dprint("Negative")
} else {
    __dprint("Zero")
}

// Ternary operator (if supported)
var result = (x > 0) ? "positive" : "non-positive"
```

#### Loops

**While Loop**
```javascript
var i = 0
while (i < 10) {
    __dprint(i)
    i = i + 1
}
```

**For Loop**
```javascript
// C-style for loop
for (var i = 0; i < 10; i = i + 1) {
    __dprint(i)
}

// With increment operators
for (var i = 0; i < 10; i++) {
    __dprint(i)
}
```

**For-In Loop**
```javascript
// Iterate over table keys
var table = {a: 1, b: 2, c: 3}
for (var key : table) {
    __dprint(key)
    __dprint(table[key])
}

// Iterate over vector indices
var vec = [10, 20, 30]
for (var i : vec) {
    __dprint(i)      // Index: 0, 1, 2
    __dprint(vec[i]) // Value: 10, 20, 30
}
```

### Functions

#### Function Declaration
```javascript
// Basic function
fun greet(name) {
    __dprint("Hello, " + name)
}

// Function with return
fun add(a, b) {
    return a + b
}

// Call functions
greet("Alice")
var sum = add(5, 3)  // 8
```

#### First-Class Functions
```javascript
// Functions as values
var myFunc = fun(x) {
    return x * 2
}

var result = myFunc(5)  // 10

// Pass functions as arguments
fun apply(func, value) {
    return func(value)
}

var doubled = apply(myFunc, 10)  // 20
```

#### Closures
```javascript
// Closure capturing outer variables
fun makeCounter() {
    var count = 0
    return fun() {
        count = count + 1
        return count
    }
}

var counter = makeCounter()
__dprint(counter())  // 1
__dprint(counter())  // 2
__dprint(counter())  // 3
```

### Classes and Objects

#### Class Definition
```javascript
// Define a class
class Vector2D {
    // Constructor
    init(x, y) {
        this.x = x
        this.y = y
    }

    // Methods
    length() {
        return sqrt(this.x * this.x + this.y * this.y)
    }

    add(other) {
        return Vector2D(this.x + other.x, this.y + other.y)
    }
}

// Create instances
var v1 = Vector2D(3, 4)
var v2 = Vector2D(1, 2)

// Call methods
__dprint(v1.length())  // 5
var v3 = v1.add(v2)
__dprint(v3.x)  // 4
__dprint(v3.y)  // 6
```

#### Inheritance
```javascript
class Animal {
    init(name) {
        this.name = name
    }

    speak() {
        __dprint(this.name + " makes a sound")
    }
}

class Dog : Animal {
    init(name, breed) {
        super.init(name)
        this.breed = breed
    }

    speak() {
        __dprint(this.name + " barks!")
    }
}

var dog = Dog("Rex", "Labrador")
dog.speak()  // "Rex barks!"
```

### Operators

#### Arithmetic
```javascript
var a = 10 + 5   // Addition: 15
var b = 10 - 5   // Subtraction: 5
var c = 10 * 5   // Multiplication: 50
var d = 10 / 5   // Division: 2
var e = -a       // Unary negation: -15
```

#### Comparison
```javascript
var eq = (5 == 5)   // Equality: true
var ne = (5 != 3)   // Inequality: true
var lt = (3 < 5)    // Less than: true
var gt = (5 > 3)    // Greater than: true
var le = (5 <= 5)   // Less or equal: true
var ge = (5 >= 5)   // Greater or equal: true
```

#### Logical
```javascript
var and = true and false   // Logical AND: false
var or = true or false     // Logical OR: true
var not = not true         // Logical NOT: false
```

#### Assignment
```javascript
var x = 10
x = x + 5   // Basic assignment: 15
x += 5      // Add-assign: 20
x -= 5      // Subtract-assign: 15
x++         // Post-increment: returns 15, x becomes 16
++x         // Pre-increment: returns 17, x becomes 17
x--         // Post-decrement: returns 17, x becomes 16
--x         // Pre-decrement: returns 15, x becomes 15
```

## Type System

### NaN-Boxing

The VM uses NaN-boxing for efficient value representation:

```cpp
// 64-bit Value encoding
// Integers: 48-bit signed integer + type tag
// Floats: IEEE 754 double (normal floats, not NaN)
// Pointers: 48-bit pointer + type tag in NaN space
// Booleans: Special NaN patterns
```

### Type Checking Macros
```cpp
IS_INT(value)      // Check if integer
IS_DOUBLE(value)   // Check if double/float
IS_BOOL(value)     // Check if boolean
IS_STRING(value)   // Check if string
IS_VECTOR(value)   // Check if vector
IS_INSTANCE(value) // Check if table/object
IS_FUNC(value)     // Check if function
```

### Type Conversion
```cpp
AS_INT(value)      // Extract integer
AS_DOUBLE(value)   // Extract double
AS_BOOL(value)     // Extract boolean
vm->asString(value)    // Get string object
vm->asVector(value)    // Get vector object
vm->asInstance(value)  // Get instance object
```

## Standard Library

### Built-in Functions

#### Debug Output
```javascript
__dprint(value)      // Debug print (newline)
__ddprint(value)     // Debug print (no newline)
```

#### String Module
```javascript
import string

// String operations
var len = string.strlen("hello")           // 5
var parts = string.split("a-b-c", "-")     // ["a", "b", "c"]
var lines = string.splitLines("a\nb\nc")   // ["a", "b", "c"]
var str = string.toString(42)              // "42"
```

#### Math Module
```javascript
import math

// Math functions
var x = math.sqrt(16)      // 4
var y = math.abs(-5)       // 5
var z = math.sin(3.14159)  // ~0
var w = math.cos(0)        // 1
var r = math.random()      // Random [0, 1)
```

#### Algorithm Module
```javascript
import algorithm

// Sorting and searching
var arr = [3, 1, 4, 1, 5]
algorithm.sort(arr)        // [1, 1, 3, 4, 5]
var idx = algorithm.find(arr, 4)  // 2 (if implemented)
```

#### File Module
```javascript
import file

// File operations
var content = file.read("data.txt")
file.write("output.txt", content)
var exists = file.exists("data.txt")
```

### ECS Integration

#### ECS Module
```javascript
import ecs

// Entity operations
var entity = ecs.getEntity(entityId)
ecs.removeEntity(entityId)

// Component attachment
ecs.attachComponent(entityId, "Health",
    "current", 100,
    "max", 100)

ecs.attachComponent(entity, "Position",
    "x", 10.0,
    "y", 20.0)

// Event sending
ecs.sendEvent("PlayerDamaged",
    "entityId", playerId,
    "damage", 25)
```

#### System Module
```javascript
import sys

// Get entities with specific component
var bullets = sys.getEntities("Bullet")

for (var bullet : bullets) {
    var bulletComp = bullet["Bullet"]
    var pos = bullet["PositionComponent"]

    // Modify components
    pos.setX(pos.x + bulletComp.velocity * deltaTime)
    bulletComp.lifetime = bulletComp.lifetime - deltaTime

    // Remove dead bullets
    if (bulletComp.lifetime <= 0) {
        ecs.removeEntity(bullet.__entityId)
    }
}
```

## Memory Management

### Reference Counting

The VM uses automatic reference counting for memory management:

```cpp
// Creating objects increments refcount to 1
Value str = vm->createString("hello");  // refcount = 1

// retainValue increments refcount
Value copy = vm->retainValue(str);      // refcount = 2

// releaseValue decrements refcount
vm->releaseValue(copy);                  // refcount = 1

// releaseAndDelete decrements and deletes if refcount reaches 0
vm->releaseAndDelete(str);               // refcount = 0, deleted
```

### Pool Allocation

Objects are allocated from type-specific pools:

```cpp
// Pools (defined in vmpools.h)
pools.stringPool      // ElementType (strings)
pools.closurePool     // Closure objects
pools.functionPool    // ObjFunction objects
pools.vectorPool      // ObjVector objects
pools.instancePool    // ObjInstance (tables/objects)
pools.classPool       // Klass (class definitions)
```

### Lifecycle
1. **Allocation**: Object allocated from pool with refcount = 1
2. **Usage**: Refcount incremented/decremented as references change
3. **Deallocation**: When refcount reaches 0, object is released back to pool
4. **Cleanup**: On VM destruction, all pools are destroyed

## Performance Features

### Bytecode Caching

```javascript
// First run: compiles .pg to .pgc
./PgCompiler script.pg

// Subsequent runs: loads cached .pgc (much faster)
./PgCompiler script.pg  // Uses script.pgc if available
```

### Optimization Passes

The compiler includes several optimization passes:

1. **Long Jump Optimization** (`long_jump_optimization_pass.cpp`)
   - Converts long jumps to short jumps when possible
   - Reduces bytecode size

2. **Basic Operator Local Indexing** (`basic_operator_local_indexing.cpp`)
   - Optimizes local variable access
   - Reduces stack operations

3. **Constant Uniformity** (`constant_uniformity_pass.cpp`)
   - Optimizes constant expressions
   - Fold constant operations at compile time

### Fast Paths

The VM includes fast paths for common operations:

```cpp
// Integer arithmetic fast path
if (IS_INT(a) && IS_INT(b))
    return INT_VAL(AS_INT(a) + AS_INT(b));

// Float arithmetic fast path
if (IS_FLOAT(a) && IS_FLOAT(b))
    return FLOAT_VAL(AS_FLOAT(a) + AS_FLOAT(b));
```

## Integration with ECS

### System Scripts

Scripts can be attached to ECS systems for game logic:

```cpp
// C++ side
auto system = builder
    .withName("BulletSystem")
    .ownComponent("Bullet")
    .ownComponent("Position")
    .onDelta("res/asteroid/update_bullets.pg")
    .build();
```

```javascript
// Script side (update_bullets.pg)
import sys

var bullets = sys.getEntities("Bullet")

for (var bullet : bullets) {
    var b = bullet["Bullet"]
    var pos = bullet["PositionComponent"]

    // Update position
    pos.setX(pos.x + b.vx * deltaTime)
    pos.setY(pos.y + b.vy * deltaTime)

    // Update lifetime
    b.lifetime = b.lifetime - deltaTime

    // Remove if dead
    if (b.lifetime <= 0) {
        ecs.removeEntity(bullet.__entityId)
    }
}
```

### Event Handlers

```cpp
// C++ side
auto system = builder
    .withName("DamageSystem")
    .onEvent("PlayerDamaged", "scripts/handle_damage.pg")
    .build();
```

```javascript
// Script side (handle_damage.pg)
import ecs

var entityId = event.entityId
var damage = event.damage

var entity = ecs.getEntity(entityId)
var health = entity["Health"]

health.current = health.current - damage

if (health.current <= 0) {
    ecs.sendEvent("PlayerDied", "entityId", entityId)
    ecs.removeEntity(entityId)
}
```

### Entity Manipulation

```javascript
import ecs

// Create entity with components
var playerId = ecs.createEntity()
ecs.attachComponent(playerId, "Player",
    "lives", 3,
    "score", 0)
ecs.attachComponent(playerId, "PositionComponent",
    "x", 0.0,
    "y", 0.0)
ecs.attachComponent(playerId, "Texture2DComponent",
    "texture", "player.png")

// Modify entity
var player = ecs.getEntity(playerId)
player["Player"].score = player["Player"].score + 100

// Remove entity
if (player["Player"].lives <= 0) {
    ecs.removeEntity(playerId)
}
```

## Error Handling

### Compile-Time Errors

```
Error at line 10: Expected ';' after statement
Error at line 15: Undefined variable 'x'
Error at line 20: Cannot use 'return' outside function
```

### Runtime Errors

```
Runtime error: Division by zero
Runtime error: Undefined property 'foo'
Runtime error: Vector index out of bounds
Runtime error: Cannot call non-function value
```

### Best Practices

1. **Check boundaries**: Always validate vector/table access
2. **Use try-catch** (if implemented): Handle errors gracefully
3. **Test incrementally**: Compile and test small pieces
4. **Use debug prints**: `__dprint()` for debugging
5. **Profile hot paths**: Use profiling tools for performance-critical code

## Debugging

### Debug Flags

```cpp
// In object.h
#define DEBUG_TRACE_EXECUTION    // Trace VM execution
#define DEBUG_RUNTIME_MEMORY     // Track allocations
#define DEBUG_CHECK_STACK        // Validate stack operations
#define DEBUG_PRINT_CODE         // Print compiled bytecode
```

### Bytecode Inspection

```bash
# Compile with bytecode output
./PgCompiler script.pg --dump-bytecode

# View bytecode
cat script.pgc | hexdump -C
```

### Memory Profiling

```bash
# Check for memory leaks
valgrind --leak-check=full ./PgCompiler script.pg

# Profile heap usage
valgrind --tool=massif ./PgCompiler script.pg
massif-visualizer massif.out.*
```

## Advanced Topics

### Custom Native Modules

```cpp
// Define custom module
struct MyModule : public NativeModule {
    MyModule() {
        addNativeFunction("myFunc", [](VM* vm, int argCount, Value* args) -> Value {
            // Implementation
            return makeIntValue(42);
        });
    }
};

// Register module
vm.addNativeModule("mymodule", MyModule());
```

```javascript
// Use in script
import mymodule

var result = mymodule.myFunc()
```

### Extending the Compiler

1. **Add new opcodes**: Define in `chunk.h`, implement in `vm.cpp`
2. **Add new passes**: Create optimization pass, register in compiler
3. **Add new types**: Define object type, add pool, implement serialization
4. **Add new syntax**: Extend parser in `cparser.cpp`

## Conclusion

The PgEngine compiler provides a powerful, efficient scripting system for game development with:
- Fast bytecode compilation and execution
- Tight ECS integration
- Flexible type system
- Comprehensive standard library
- Efficient memory management

For more information, see:
- [Language Reference](LANGUAGE_REFERENCE.md)
- [API Documentation](API_REFERENCE.md)
- [Examples](../examples/)
