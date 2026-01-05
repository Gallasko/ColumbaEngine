# PgEngine Compiler Quick Start Guide

## Getting Started in 5 Minutes

### 1. Hello World

Create a file `hello.pg`:

```javascript
__dprint("Hello, World!")
```

Run it:
```bash
./PgCompiler hello.pg
```

Output:
```
Hello, World!
```

### 2. Variables and Math

```javascript
// Variables
var x = 10
var y = 20
var sum = x + y

__dprint("Sum: " + sum)  // "Sum: 30"

// Math operations
var result = (x + y) * 2 - 5
__dprint(result)  // 55
```

### 3. Functions

```javascript
// Define a function
fun factorial(n) {
    if (n <= 1) {
        return 1
    }
    return n * factorial(n - 1)
}

// Call it
var result = factorial(5)
__dprint(result)  // 120
```

### 4. Vectors (Arrays)

```javascript
// Create a vector
var numbers = [1, 2, 3, 4, 5]

// Access elements
__dprint(numbers[0])   // 1
__dprint(numbers[-1])  // 5 (last element)

// Iterate
for (var i : numbers) {
    __dprint(numbers[i])
}
```

### 5. Tables (Objects)

```javascript
// Create a table
var player = {
    name: "Alice",
    health: 100,
    score: 0
}

// Access properties
__dprint(player.name)    // "Alice"
__dprint(player["health"])  // 100

// Modify
player.score = player.score + 10
__dprint(player.score)  // 10
```

### 6. Classes

```javascript
class Player {
    init(name) {
        this.name = name
        this.health = 100
        this.score = 0
    }

    damage(amount) {
        this.health = this.health - amount
        if (this.health <= 0) {
            __dprint(this.name + " died!")
        }
    }

    addScore(points) {
        this.score = this.score + points
    }
}

// Create instance
var player = Player("Alice")
player.addScore(50)
player.damage(30)
__dprint(player.health)  // 70
```

### 7. Loops

```javascript
// While loop
var i = 0
while (i < 5) {
    __dprint(i)
    i = i + 1
}

// For loop
for (var j = 0; j < 5; j++) {
    __dprint(j)
}

// For-in loop
var items = ["apple", "banana", "cherry"]
for (var idx : items) {
    __dprint(items[idx])
}
```

### 8. Using Modules

```javascript
// Import standard library
import string
import math

// Use string functions
var text = "hello-world"
var parts = string.split(text, "-")
__dprint(parts[0])  // "hello"

// Use math functions
var x = math.sqrt(16)
__dprint(x)  // 4.0
```

## Game Development Features

### Working with Entities (ECS)

```javascript
import ecs
import sys

// Get an entity
var player = ecs.getEntity(playerId)

// Add a component
ecs.attachComponent(playerId, "Health",
    "current", 100,
    "max", 100)

// Access component properties
__dprint(player["Health"].current)  // 100

// Modify components
player["Health"].current = player["Health"].current - 10

// Remove entity
if (player["Health"].current <= 0) {
    ecs.removeEntity(playerId)
}
```

### System Scripts (Game Loop)

In your game system script (e.g., `update_bullets.pg`):

```javascript
import sys
import ecs

// Get all entities with Bullet component
var bullets = sys.getEntities("Bullet")

for (var bullet : bullets) {
    // Each bullet has all its components
    var bulletComp = bullet["Bullet"]
    var pos = bullet["PositionComponent"]

    // Update position
    pos.setX(pos.x + bulletComp.vx * deltaTime)
    pos.setY(pos.y + bulletComp.vy * deltaTime)

    // Update lifetime
    bulletComp.lifetime = bulletComp.lifetime - deltaTime

    // Remove if expired
    if (bulletComp.lifetime <= 0) {
        ecs.removeEntity(bullet.__entityId)
    }
}
```

### Sending Events

```javascript
import ecs

// Send a custom event
ecs.sendEvent("PlayerScored",
    "playerId", playerId,
    "points", 100)

// Send collision event
ecs.sendEvent("BulletHit",
    "bulletId", bulletId,
    "targetId", targetId,
    "damage", 25)
```

## Common Patterns

### Counter Pattern

```javascript
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

### Timer Pattern

```javascript
class Timer {
    init(duration) {
        this.duration = duration
        this.elapsed = 0
        this.active = true
    }

    update(dt) {
        if (this.active) {
            this.elapsed = this.elapsed + dt
            if (this.elapsed >= this.duration) {
                this.active = false
                return true  // Timer finished
            }
        }
        return false
    }

    reset() {
        this.elapsed = 0
        this.active = true
    }
}

// Usage in game loop
var timer = Timer(5.0)  // 5 second timer

// In update function
if (timer.update(deltaTime)) {
    __dprint("Timer finished!")
}
```

### State Machine Pattern

```javascript
class StateMachine {
    init(initialState) {
        this.state = initialState
        this.states = {}
    }

    addState(name, onEnter, onUpdate, onExit) {
        this.states[name] = {
            onEnter: onEnter,
            onUpdate: onUpdate,
            onExit: onExit
        }
    }

    changeState(newState) {
        if (this.states[this.state].onExit) {
            this.states[this.state].onExit()
        }

        this.state = newState

        if (this.states[this.state].onEnter) {
            this.states[this.state].onEnter()
        }
    }

    update() {
        if (this.states[this.state].onUpdate) {
            this.states[this.state].onUpdate()
        }
    }
}
```

### Object Pool Pattern

```javascript
class ObjectPool {
    init(createFunc, initialSize) {
        this.createFunc = createFunc
        this.available = []
        this.active = []

        // Pre-allocate objects
        for (var i = 0; i < initialSize; i++) {
            this.available.push(createFunc())
        }
    }

    acquire() {
        var obj
        if (this.available.length() > 0) {
            obj = this.available.pop()
        } else {
            obj = this.createFunc()
        }
        this.active.push(obj)
        return obj
    }

    release(obj) {
        // Remove from active
        var idx = this.active.indexOf(obj)
        if (idx >= 0) {
            this.active.remove(idx)
            this.available.push(obj)
        }
    }
}
```

## Tips and Tricks

### 1. Use Local Variables
```javascript
// Good - local variable
fun calculate(x) {
    var result = x * x + 2 * x + 1
    return result
}

// Avoid - global variable
var globalResult
fun calculate(x) {
    globalResult = x * x + 2 * x + 1
    return globalResult
}
```

### 2. Cache Frequently Used Values
```javascript
// Good - cache component reference
var bullets = sys.getEntities("Bullet")
for (var bullet : bullets) {
    var b = bullet["Bullet"]  // Cache reference
    b.x = b.x + b.vx
    b.y = b.y + b.vy
}

// Avoid - repeated lookups
for (var bullet : bullets) {
    bullet["Bullet"].x = bullet["Bullet"].x + bullet["Bullet"].vx
    bullet["Bullet"].y = bullet["Bullet"].y + bullet["Bullet"].vy
}
```

### 3. Use Setters for Component Updates
```javascript
// Components have auto-generated setters that trigger events
var pos = entity["PositionComponent"]

// Good - triggers PositionComponentChangedEvent
pos.setX(100)
pos.setY(200)

// Also works - direct assignment
pos.x = 100
pos.y = 200
```

### 4. Batch Operations
```javascript
// Good - single event
ecs.sendEvent("BulkUpdate",
    "count", entityCount,
    "timestamp", currentTime)

// Avoid - event spam
for (var entity : entities) {
    ecs.sendEvent("Update", "id", entity.id)
}
```

### 5. Use String Interpolation (if supported)
```javascript
var name = "Alice"
var score = 100

// If string interpolation is available
__dprint("Player: ${name}, Score: ${score}")

// Otherwise use concatenation
__dprint("Player: " + name + ", Score: " + score)
```

## Debugging

### Debug Prints
```javascript
__dprint("Value: " + x)
__ddprint(x)  // No newline

// Print multiple values
__dprint("x=" + x + ", y=" + y + ", z=" + z)
```

### Validate Assumptions
```javascript
fun divide(a, b) {
    if (b == 0) {
        __dprint("ERROR: Division by zero!")
        return 0
    }
    return a / b
}
```

### Check Boundaries
```javascript
fun getItem(items, index) {
    if (index < 0 or index >= items.length()) {
        __dprint("ERROR: Index out of bounds")
        return nil
    }
    return items[index]
}
```

## Performance Tips

1. **Minimize allocations**: Reuse objects when possible
2. **Cache lookups**: Store frequently accessed values in local variables
3. **Use appropriate data structures**: Vectors for indexed access, tables for key-value
4. **Avoid deep recursion**: Use iterative solutions when possible
5. **Profile your code**: Focus optimization on hot paths only

## Common Errors and Solutions

### Error: "Undefined variable 'x'"
**Solution**: Declare the variable before use
```javascript
var x = 10  // Declare first
__dprint(x)
```

### Error: "Vector index out of bounds"
**Solution**: Check vector size before accessing
```javascript
if (index >= 0 and index < vec.length()) {
    __dprint(vec[index])
}
```

### Error: "Cannot use 'return' outside function"
**Solution**: Only use return inside functions
```javascript
fun myFunc() {
    return 42  // OK
}
```

### Error: "Expected ';' after statement"
**Solution**: Add semicolons (or newlines) after statements
```javascript
var x = 10  // OK
var y = 20  // OK
```

## Next Steps

- Read the [Full Compiler Documentation](COMPILER_OVERVIEW.md)
- Check out [Example Scripts](../../examples/)
- Learn about [ECS Integration](../ecs/ECS_SCRIPTING.md)
- Explore [Advanced Features](ADVANCED_FEATURES.md)

## Getting Help

- Check the examples directory: `examples/`
- Run tests: `./test_compiler`
- Read error messages carefully - they include line numbers
- Use `__dprint()` liberally for debugging
- Start small and test incrementally
