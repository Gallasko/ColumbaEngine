# VM Performance Optimization Plan

**Goal**: Achieve 10x performance improvement in script execution
**Current Performance**: 2000 entities @ 13.5ms (6.7 μs/entity)
**Target Performance**: 2000 entities @ 1.35ms (0.67 μs/entity)

**Date**: 2026-02-05
**Status**: Planning Phase

---

## Benchmark Results Analysis

### Current Bottlenecks (2000 iterations)

| Operation | Total Time | % of Total | Per-Op Time | Target Time | Improvement Needed |
|-----------|-----------|------------|-------------|-------------|-------------------|
| OP_Build_Table | 1713 μs | 29.54% | 0.856 μs | 0.085 μs | **10x** |
| OP_Set_Property (x2) | 518 μs | 8.93% | 0.129 μs | 0.013 μs | **10x** |
| OP_Get_Property (x2) | 366 μs | 6.32% | 0.091 μs | 0.009 μs | **10x** |
| OP_Get_Global | 331 μs | 5.72% | 0.083 μs | 0.008 μs | **10x** |
| OP_Get_Index | 62 μs | 1.07% | 0.031 μs | 0.003 μs | **10x** |
| OP_Set_Index | 84 μs | 1.45% | 0.042 μs | 0.004 μs | **10x** |

**Total Critical Path**: ~3074 μs (53% of execution time)
**Target**: ~307 μs

---

## Part 2: Critical Path Optimizations

### Optimization 1: Reduce String Allocation Overhead

**Problem**: Property names are converted from Value → ElementType → string on EVERY access

**Current Code** ([vm_struct_op.cpp:166-177](../src/Engine/Compiler/vm_struct_op.cpp#L166-L177)):
```cpp
auto nameValue = vm->currentFrame->closure->function->chunk.constants[constantIndex];
auto name = vm->valueToElement(nameValue);  // Expensive conversion
if (not name.isLitteral()) {
    vm->runtimeError("Property name must be a litteral.");
    return;
}
auto nameStr = name.toString();  // Allocates string
```

**Solution**: Cache string directly in constants pool
```cpp
// Property names are ALWAYS compile-time literals
// Store them as interned strings directly in constants
auto nameValue = vm->currentFrame->closure->function->chunk.constants[constantIndex];

// FAST PATH: Property names are always strings at compile time
if (IS_STRING(nameValue)) {
    const std::string& nameStr = vm->asStringRef(nameValue);  // No allocation!
    // ... rest of code
}
```

**Expected Gain**: 30-40% reduction in property access time

---

### Optimization 2: Fast Path for Fields (No Metamethods)

**Problem**: Every property access checks for metamethods, even when instance has no `__get`/`__set`

**Current Code** ([vm_struct_op.cpp:180-187](../src/Engine/Compiler/vm_struct_op.cpp#L180-L187)):
```cpp
// Try to find a field first
if (instance->fields.find(nameStr) != instance->fields.end()) {
    // ... found it
}

// Check for __get metamethod  (ALWAYS checks even if no metamethod exists)
auto getMetaIt = instance->klass->methods.find("__get");
```

**Solution**: Add flag to Klass to indicate if it has metamethods
```cpp
struct Klass {
    std::string name;
    std::unordered_map<std::string, Value> methods;

    // NEW: Performance flags
    bool hasGetMetamethod = false;
    bool hasSetMetamethod = false;
    // ... rest
};

// In op_get_property:
auto fieldIt = instance->fields.find(nameStr);
if (fieldIt != instance->fields.end()) {
    // FAST PATH: Field found
    auto inst = vm->pop();
    vm->releaseAndDelete(inst);
    vm->push(vm->retainValue(fieldIt->second));
    return;
}

// Only check for metamethods if the class has them
if (instance->klass->hasGetMetamethod) {
    // Metamethod path...
}
```

**Expected Gain**: 20-30% for non-metamethod classes

---

### Optimization 3: Optimize Map Lookups

**Problem**: `std::unordered_map::find()` called multiple times for same key

**Current Code**:
```cpp
// First lookup to check existence
if (instance->fields.find(nameStr) != instance->fields.end()) {
    // Second lookup to get value
    vm->push(vm->retainValue(instance->fields[nameStr]));  // Lookup #2!
}
```

**Solution**: Use iterator from first lookup
```cpp
auto fieldIt = instance->fields.find(nameStr);
if (fieldIt != instance->fields.end()) {
    auto inst = vm->pop();
    vm->releaseAndDelete(inst);
    vm->push(vm->retainValue(fieldIt->second));  // No second lookup!
    return;
}
```

**Expected Gain**: 10-15% reduction in property access

---

### Optimization 4: Reduce Retain/Release Overhead

**Problem**: Every property access involves retain + release pairs

**Current Code**:
```cpp
auto inst = vm->pop();
vm->releaseAndDelete(inst);  // Decrements refcount, checks for deletion
vm->push(vm->retainValue(fieldValue));  // Increments refcount
```

**Solution**: Use move semantics where possible
```cpp
// If we're just transferring ownership, don't retain/release
auto inst = vm->pop();
vm->push(vm->retainValue(fieldIt->second));  // Retain new value
vm->releaseAndDelete(inst);  // Release old (delayed)
```

**Better Solution**: Lazy refcounting
- Track "hot" values that don't need refcounting
- Use arena allocation for temporary values

**Expected Gain**: 15-20% reduction

---

### Optimization 5: Optimize OP_Build_Table

**Problem**: Building 2000 tables takes 1713 μs (29.54% of total time!)

**Current Bottlenecks**:
1. Map allocation for each table
2. String key allocations (8 keys × 2000 tables = 16,000 allocations)
3. Retain/release for each value insertion
4. Map rehashing as properties are added

**Current Code** ([vm_struct_op.cpp:517-585](../src/Engine/Compiler/vm_struct_op.cpp#L517-L585)):
```cpp
// Create new instance of Table
Value instanceVal = vm->createInstance(tableClass);
ObjInstance* table = vm->asInstance(instanceVal);

// Pop pairCount key-value pairs from stack (in reverse)
for (int i = 0; i < pairCount; i++) {
    Value key = vm->pop();
    Value value = vm->pop();

    std::string keyStr;
    if (IS_STRING(key)) {
        keyStr = vm->asString(key);  // STRING ALLOCATION!
    }
    // ...
    pairs.push_back({keyStr, value});
}

// Insert pairs
for (auto it = pairs.rbegin(); it != pairs.rend(); ++it) {
    table->fields[it->first] = vm->retainValue(it->second);  // MAP INSERTION + REHASH
    vm->releaseAndDelete(it->second);
}
```

**Solutions**:

**A) Pre-allocate map**:
```cpp
Value instanceVal = vm->createInstance(tableClass);
ObjInstance* table = vm->asInstance(instanceVal);

// Reserve map capacity upfront
table->fields.reserve(pairCount);  // Avoid rehashing!
```

**B) Avoid intermediate vector**:
```cpp
// Instead of collecting pairs then inserting, insert directly
std::vector<std::pair<Value, Value>> kvPairs;
kvPairs.reserve(pairCount);

for (int i = 0; i < pairCount; i++) {
    Value key = vm->pop();
    Value value = vm->pop();
    kvPairs.push_back({key, value});
}

// Insert in reverse order directly
for (auto it = kvPairs.rbegin(); it != kvPairs.rend(); ++it) {
    std::string keyStr = vm->asString(it->first);  // Only convert when needed
    table->fields[keyStr] = vm->retainValue(it->second);
    vm->releaseAndDelete(it->first);
    vm->releaseAndDelete(it->second);
}
```

**Expected Gain**: 40-50% reduction in table creation time

---

## Part 3: Architectural Changes

### Architecture 1: String Interning for Property Names

**Concept**: Property names are compile-time constants - intern them once, use integer IDs everywhere

**Implementation**:

```cpp
class StringInterner {
public:
    static StringInterner& instance() {
        static StringInterner interner;
        return interner;
    }

    // Intern a string, return its ID
    uint32_t intern(const std::string& str) {
        auto it = stringToId.find(str);
        if (it != stringToId.end()) {
            return it->second;
        }

        uint32_t id = nextId++;
        stringToId[str] = id;
        idToString[id] = str;
        return id;
    }

    const std::string& getString(uint32_t id) const {
        return idToString.at(id);
    }

private:
    std::unordered_map<std::string, uint32_t> stringToId;
    std::unordered_map<uint32_t, std::string> idToString;
    uint32_t nextId = 0;
};

// In compiler, when emitting OP_Get_Property:
uint32_t propId = StringInterner::instance().intern(propertyName);
chunk.writeConstant(makeIntValue(propId));  // Store ID instead of string!

// In op_get_property:
uint32_t propId = AS_INT(nameValue);  // No string conversion!
const std::string& nameStr = StringInterner::instance().getString(propId);
```

**Benefits**:
- ✅ No string allocations for property names
- ✅ Fast integer comparisons instead of string comparisons
- ✅ Smaller bytecode (4 bytes vs variable-length strings)

**Expected Gain**: 50-70% reduction in property access overhead

---

### Architecture 2: Property Index Cache for Classes

**Concept**: Each class knows the indices of its most commonly accessed properties

**Implementation**:

```cpp
struct Klass {
    std::string name;
    std::unordered_map<std::string, Value> methods;

    // NEW: Property index cache (maps property name ID → field index)
    std::unordered_map<uint32_t, uint32_t> propertyIndexCache;
    std::vector<std::string> propertyNames;  // Index → name mapping

    // Check if this class has a property at a specific index
    bool hasProperty(uint32_t propId) const {
        return propertyIndexCache.find(propId) != propertyIndexCache.end();
    }

    uint32_t getPropertyIndex(uint32_t propId) const {
        return propertyIndexCache.at(propId);
    }
};

// In op_get_property with property index cache:
uint32_t propId = AS_INT(nameValue);

// Fast path: Check if class has this property cached
if (instance->klass->hasProperty(propId)) {
    uint32_t fieldIndex = instance->klass->getPropertyIndex(propId);
    const std::string& fieldName = instance->klass->propertyNames[fieldIndex];

    auto fieldIt = instance->fields.find(fieldName);
    if (fieldIt != instance->fields.end()) {
        vm->push(vm->retainValue(fieldIt->second));
        return;
    }
}
```

**Expected Gain**: 30-40% for cached properties

---

### Architecture 3: Slot-Based Properties (Major Change)

**Concept**: Instead of string-keyed maps, use indexed slots for properties

**Current**:
```cpp
struct ObjInstance {
    Klass* klass;
    std::unordered_map<std::string, Value> fields;  // Slow!
};
```

**New**:
```cpp
struct Klass {
    std::string name;
    std::unordered_map<std::string, Value> methods;

    // NEW: Property slot mapping
    std::unordered_map<std::string, uint32_t> propertySlots;
    uint32_t propertyCount = 0;
};

struct ObjInstance {
    Klass* klass;

    // FAST: Array-based property storage
    std::vector<Value> slots;  // Indexed access - O(1)

    // SLOW: Map for dynamic properties added at runtime
    std::unordered_map<std::string, Value> dynamicFields;
};

// Property access becomes:
// Instead of: instance->fields["x"]
// Use: instance->slots[klass->propertySlots["x"]]
```

**Benefits**:
- ✅ O(1) array access instead of O(1) map lookup (lower constant)
- ✅ Better cache locality
- ✅ No string allocations on access
- ✅ Backward compatible (fallback to dynamicFields)

**Expected Gain**: 5-10x faster property access!

---

### Architecture 4: Inline Cache for Property Access

**Concept**: Cache the last property access location for monomorphic call sites

**Implementation**:

```cpp
// Each OP_Get_Property instruction gets an inline cache slot
struct PropertyCache {
    Klass* klass = nullptr;  // Which class this cache is for
    uint32_t fieldIndex = 0;  // Index in fields map (or slot index)
    std::string* fieldName = nullptr;  // Pointer to field name (avoid lookup)
};

// In VM:
std::vector<PropertyCache> propertyCache;  // One per property access site

// In op_get_property:
uint8_t constantIndex = *vm->currentFrame->ip++;
uint16_t cacheIndex = (vm->currentFrame->ip - vm->currentFrame->closure->function->chunk.code.data());

auto nameValue = vm->currentFrame->closure->function->chunk.constants[constantIndex];
auto* instance = vm->asInstance(vm->peek(0));

// Check inline cache first
PropertyCache& cache = vm->propertyCache[cacheIndex];
if (cache.klass == instance->klass && cache.fieldName != nullptr) {
    // CACHE HIT - super fast path!
    auto fieldIt = instance->fields.find(*cache.fieldName);
    if (fieldIt != instance->fields.end()) {
        vm->pop();  // instance
        vm->push(vm->retainValue(fieldIt->second));
        return;
    }
}

// Cache miss - slow path
std::string nameStr = vm->asString(nameValue);
auto fieldIt = instance->fields.find(nameStr);
if (fieldIt != instance->fields.end()) {
    // Update cache for next time
    cache.klass = instance->klass;
    cache.fieldName = &fieldIt->first;  // Point to string in map

    vm->pop();
    vm->push(vm->retainValue(fieldIt->second));
    return;
}
```

**Benefits**:
- ✅ Amortizes string conversion cost
- ✅ Cache locality for hot paths
- ✅ Minimal memory overhead

**Expected Gain**: 2-3x for hot loops (monomorphic access patterns)

---

### Architecture 5: Fast Property Access Bytecode

**Concept**: Add specialized opcodes for common property patterns

**New Opcodes**:
```cpp
OP_Get_Property_Fast  // Property access with no metamethods, cached name
OP_Set_Property_Fast  // Property write with no metamethods, cached name
OP_Get_Slot          // Direct slot access (if using slot-based properties)
OP_Set_Slot          // Direct slot write
```

**Compiler Optimization**:
```cpp
// When compiling: obj.x
if (canProveNoMetamethods(obj.type) && isKnownProperty(obj.type, "x")) {
    uint32_t slotIndex = getPropertySlot(obj.type, "x");
    emitByte(OP_Get_Slot);
    emitByte(slotIndex);  // Direct slot index - no constant needed!
} else {
    emitByte(OP_Get_Property);
    emitByte(makeConstant(STRING_VAL("x")));
}
```

**Expected Gain**: 5-10x for typed/known properties

---

### Architecture 6: Optimize Global Variable Access

**Problem**: OP_Get_Global takes 0.083 μs (should be ~0.01 μs)

**Current Code**:
```cpp
std::unordered_map<std::string, Value> globals;

// Access:
std::string name = getConstantString(constantIndex);
auto it = globals.find(name);  // Map lookup
```

**Solution 1**: Cache global indices
```cpp
// At compile time, assign each global an index
std::unordered_map<std::string, uint32_t> globalIndices;
std::vector<Value> globalSlots;

// Emit OP_Get_Global_Indexed instead
uint32_t globalIndex = vm->globalIndices["count"];
emitByte(OP_Get_Global_Indexed);
emitShort(globalIndex);

// Runtime:
uint16_t index = readShort();
vm->push(vm->retainValue(vm->globalSlots[index]));  // Direct array access!
```

**Expected Gain**: 5-8x faster global access

---

### Architecture 7: Reduce Object Allocation in OP_Build_Table

**Problem**: Creating 2000 tables takes 1713 μs (0.856 μs each)

**Current Overhead**:
- Instance allocation
- Map allocation + initialization
- 8 string keys converted + allocated
- 8 map insertions with potential rehashing
- 8 retain calls

**Solution A**: Reserve map capacity
```cpp
Value instanceVal = vm->createInstance(tableClass);
ObjInstance* table = vm->asInstance(instanceVal);

// Reserve capacity upfront (avoid rehashing)
table->fields.reserve(pairCount);

// Then insert pairs...
```

**Expected Gain**: 20-30% reduction

**Solution B**: Use object pools for table instances
```cpp
// Pre-allocate table instances
std::vector<ObjInstance*> tablePool;

ObjInstance* allocateTable() {
    if (!tablePool.empty()) {
        auto* table = tablePool.back();
        tablePool.pop_back();
        table->fields.clear();  // Reuse existing map
        return table;
    }
    return new ObjInstance();  // Allocate new
}
```

**Expected Gain**: 40-50% reduction

**Solution C**: Compile-time table shape optimization
```cpp
// When compiler sees table literal with known keys:
// { "x": 100, "y": 200, "z": 0 }
// It can emit:
OP_Build_Table_Shaped  // Special opcode
uint8_t shapeId        // Pre-registered shape

// At runtime:
TableShape& shape = vm->tableShapes[shapeId];
ObjInstance* table = vm->allocateTableWithShape(shape);
// Shape contains: pre-sized map, expected keys, etc.
```

**Expected Gain**: 50-70% reduction for table literals

---

## Part 3 Summary: Architectural Changes

### Recommended Priority:

1. **High Impact, Low Effort**:
   - ✅ Reserve map capacity in OP_Build_Table
   - ✅ Fix double map lookups (use iterator)
   - ✅ Cache metamethod flags in Klass
   - ✅ Direct string reference (avoid conversion)

2. **High Impact, Medium Effort**:
   - ⭐ String interning for property names
   - ⭐ Inline caching for property access
   - ⭐ Global variable indexing

3. **High Impact, High Effort**:
   - 🔧 Slot-based properties (major refactor)
   - 🔧 Specialized fast-path opcodes
   - 🔧 Table shape optimization

---

## Implementation Roadmap

### Phase 1: Quick Wins (Target: 3-4x improvement)

1. Fix double map lookups in property access
2. Add `fields.reserve()` to OP_Build_Table
3. Add metamethod flags to Klass
4. Use string references instead of conversions

**Estimated Time**: 2-4 hours
**Expected Improvement**: 3-4x faster

### Phase 2: Medium Optimizations (Target: 6-7x improvement)

1. Implement string interning for property names
2. Add inline caching for property access
3. Optimize global variable access with indices

**Estimated Time**: 1-2 days
**Expected Improvement**: 6-7x faster

### Phase 3: Major Refactoring (Target: 10x+ improvement)

1. Implement slot-based property storage
2. Add specialized fast-path opcodes
3. Table shape optimization for literals

**Estimated Time**: 3-5 days
**Expected Improvement**: 10x+ faster

---

## Expected Performance After Optimizations

### Current:
- 2000 entities: 13.5ms (6.75 μs/entity)
- Property access: 0.44 μs per access
- Table creation: 0.856 μs per table

### After Phase 1 (Quick Wins):
- 2000 entities: ~4ms (2 μs/entity)
- Property access: ~0.15 μs per access
- Table creation: ~0.3 μs per table

### After Phase 2 (Medium):
- 2000 entities: ~2ms (1 μs/entity)
- Property access: ~0.07 μs per access
- Table creation: ~0.15 μs per table

### After Phase 3 (Major):
- 2000 entities: ~1-1.5ms (0.5-0.75 μs/entity) ✓ **10x faster!**
- Property access: ~0.04 μs per access
- Table creation: ~0.08 μs per table

---

## Metrics to Track

- [ ] OP_Get_Property time per operation
- [ ] OP_Set_Property time per operation
- [ ] OP_Build_Table time per operation
- [ ] Total script execution time
- [ ] Memory allocations per frame
- [ ] Cache hit rate (for inline caches)

---

## Related Documentation

- [VM Internals](source/script/vm_internals.rst) - VM architecture
- [Component Proxy System](COMPONENT_PROXY_SYSTEM.md) - Zero-copy proxies
- [Script Performance Benchmarks](../benchmark/script_performance.cc) - Benchmark suite

---

**End of Plan**
