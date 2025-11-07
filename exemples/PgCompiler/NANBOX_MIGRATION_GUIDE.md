# NaN-Boxing Migration Guide

This document provides a complete guide for migrating the PgCompiler VM to use NaN-boxed values with pool-based memory management.

## Overview

**What Changed:**
- `Value` is now `uint64_t` (8 bytes) instead of a 16-byte struct
- Values are encoded using NaN-boxing (doubles + tagged indices)
- Pool-based allocation replaces `new`/`delete` for heap objects
- Vector-based reference counting replaces `unordered_map<void*, int>`

**Performance Benefits:**
- 50% smaller stack (2.1 MB vs 4.2 MB)
- 2x more values per cache line
- ~10-20x faster refcounting (vector vs hashmap)
- Expected 3-4x overall speedup

---

## 1. VM Method Signature Updates

### vm.h - Line 335-338

**OLD:**
```cpp
inline Value retainValue(const Value& value);
inline bool releaseValue(const Value& value);
inline Value trackNewValue(const Value& value);
```

**NEW:**
```cpp
inline Value retainValue(Value value);  // Pass by value (64-bit)
inline bool releaseValue(Value value);
inline Value trackNewValue(Value value);
```

**Why:** 64-bit values fit in registers, no need for references

---

## 2. Value Access Pattern Changes

### OLD Pattern: Direct Pointer Access
```cpp
ElementType* obj = AS_OBJ(value);
Closure* closure = AS_CLOSURE(value);
ObjFunction* func = AS_FUNC(value);
```

### NEW Pattern: Pool Lookup
```cpp
// Strings (formerly OBJ)
ElementType* obj = vm->pools.getString(value);

// Closures
Closure* closure = vm->pools.getClosure(value);

// Functions
ObjFunction* func = vm->pools.getFunction(value);

// Upvalues
ObjUpvalue* upvalue = vm->pools.getUpvalue(value);

// Classes
Klass* klass = vm->pools.getClass(value);

// Native Functions
NativeFunction* native = vm->pools.getNativeFunc(value);

// Instances
ObjInstance* instance = vm->pools.getInstance(value);

// Bound Methods
ObjBoundMethod* bound = vm->pools.getBoundMethod(value);
```

---

## 3. Value Creation Pattern Changes

### OLD Pattern: new + make*Value + track
```cpp
auto* obj = new ElementType(str);
Value val = makeObjValue(obj);
return trackNewValue(val);
```

### NEW Pattern: Pool Allocation
```cpp
ElementType* obj = pools.stringPool.allocate(str);
uint32_t index = pools.stringPool.getNbElements() - 1;
pools.ensureRefCountCapacity(makeStringValue(index), index);
pools.stringRefCounts[index] = 1;
return makeStringValue(index);
```

**Or use helper method (recommended):**
```cpp
// Add to VM class:
Value createString(const std::string& str) {
    ElementType* obj = pools.stringPool.allocate(str);
    uint32_t index = pools.stringPool.getNbElements() - 1;
    Value val = makeStringValue(index);
    pools.ensureRefCountCapacity(val, index);
    pools.stringRefCounts[index] = 1;
    return val;
}

// Usage:
return vm->createString(str);
```

---

## 4. File-by-File Migration Checklist

### ✅ COMPLETED:
- [x] `value_nanbox.h` - Created
- [x] `vmpools.h` - Created
- [x] `object.h` - Old Value struct removed
- [x] `vm.h` - Headers included, VMPools added
- [x] `vm.h` - IndexableStack updated to 8-byte values
- [x] `vm.h` - retainValue/releaseValue/trackNewValue rewritten

### ❌ NEEDS FIXING:

#### **vm.h**

**Line 340:** `getTotalTrackedObjects()`
```cpp
// OLD:
size_t getTotalTrackedObjects() const { return refCounts.size(); }

// NEW:
size_t getTotalTrackedObjects() const {
    return pools.stringRefCounts.size() +
           pools.closureRefCounts.size() +
           pools.functionRefCounts.size() +
           pools.upvalueRefCounts.size() +
           pools.classRefCounts.size() +
           pools.nativeFuncRefCounts.size() +
           pools.instanceRefCounts.size() +
           pools.boundMethodRefCounts.size();
}
```

**Line 431-432:** `defineNative()`
```cpp
// OLD:
Value val;
val.type = COMPILER_VAL_NATIVE;
val.as.nativeFunc = nativeFunc;

// NEW:
NativeFunction* native = pools.nativeFuncPool.allocate();
native->function = fn;
uint32_t index = pools.nativeFuncPool.getNbElements() - 1;
Value val = makeNativeFuncValue(index);
pools.ensureRefCountCapacity(val, index);
pools.nativeFuncRefCounts[index] = 1;
```

**Lines 103-114:** `freeValue()` - This function is obsolete with pools
```cpp
// DELETE THIS FUNCTION - pools handle cleanup
// Or stub it:
inline void freeValue(Value& value) {
    // No-op: pools handle cleanup via releaseValue/releaseAndDelete
}
```

---

#### **object.h**

**Lines 133-151:** `elementToValue()` function
```cpp
// This function needs major rewrite - it currently tries to create Values
// from ElementType, but now needs VM pool access

// TEMPORARY SOLUTION: Add VM* parameter
inline Value elementToValue(const ElementType& element, VM* vm)
{
    if (element.isInt())
        return makeIntValue(element.get<int>());
    else if (element.isFloat())
        return makeDoubleValue(element.get<float>());
    else if (element.isBool())
        return makeBoolValue(element.get<bool>());
    else if (element.isString())
    {
        // Need pool allocation!
        return vm->createString(element.get<std::string>());
    }
    // ... handle other cases
}
```

**Lines 156-165:** `getValueAsInt()` function
```cpp
// OLD:
if (IS_INT(value))
    return static_cast<int>(AS_INT(value));
else if (IS_FLOAT(value))
    return static_cast<int>(AS_FLOAT(value));

// NEW:
if (IS_INT(value))
    return static_cast<int>(AS_INT(value));  // This still works!
else if (IS_DOUBLE(value))
    return static_cast<int>(AS_DOUBLE(value));
```

**Lines 171-188:** `valueToElement()` function
```cpp
// OLD: switch(value.type)

// NEW:
inline ElementType valueToElement(const Value& value, VM* vm)
{
    if (IS_BOOL(value))
        return ElementType(AS_BOOL(value));
    else if (IS_INT(value))
        return ElementType(static_cast<int>(AS_INT(value)));
    else if (IS_DOUBLE(value))
        return ElementType(AS_DOUBLE(value));
    else if (IS_STRING(value))
        return *vm->pools.getString(value);
    // ... handle other types with pool lookups

    return ElementType();  // Default
}
```

---

#### **chunk.h**

**Lines 97-100:** Destructor cleanup
```cpp
// OLD:
if (constant.type == COMPILER_VAL_OBJ && constant.as.obj != nullptr)
{
    delete constant.as.obj;
}

// NEW:
// Constants are now indices - no cleanup needed in Chunk
// VM pools will handle cleanup when VM is destroyed
// Just clear the vector:
constants.clear();
```

**Lines 139, 156:** `addConstant()` methods
```cpp
// OLD:
return addConstant(FUNC_VAL(value), line);

// NEW:
// Functions need to be allocated in pool first
// This should be done by compiler, not Chunk
// Just store the Value directly:
return addConstant(value, line);  // value is already a pool index
```

---

#### **vm.cpp**

**Line 92:** `interpret()`
```cpp
// OLD:
push(trackNewValue(FUNC_VAL(function)));

// NEW:
// Function is already in pool (allocated by compiler)
// Just need to get its index
uint32_t funcIndex = /* get from compiler */;
Value funcVal = makeFunctionValue(funcIndex);
push(trackNewValue(funcVal));
```

**Line 95:** `interpret()`
```cpp
// OLD:
push(trackNewValue(CLOSURE_VAL(closure)));

// NEW:
Closure* closure = pools.closurePool.allocate(function);
uint32_t index = pools.closurePool.getNbElements() - 1;
Value closureVal = makeClosureValue(index);
pools.ensureRefCountCapacity(closureVal, index);
pools.closureRefCounts[index] = 1;
push(closureVal);
```

**Line 237:** `captureUpvalue()`
```cpp
// OLD:
trackNewValue(UPVALUE_VAL(newUpvalue));

// NEW:
ObjUpvalue* upvalue = pools.upvaluePool.allocate(local);
uint32_t index = pools.upvaluePool.getNbElements() - 1;
Value upvalueVal = makeUpvalueValue(index);
pools.ensureRefCountCapacity(upvalueVal, index);
pools.upvalueRefCounts[index] = 1;
return upvalue;
```

**Lines 265-318:** `callValue()` - Switch statement
```cpp
// OLD:
switch (callee.type)
{
    case CompilerValueType::COMPILER_VAL_CLOSURE:
        return call(AS_CLOSURE(callee), argCount);
    // ...
}

// NEW:
if (IS_CLOSURE(callee)) {
    return call(pools.getClosure(callee), argCount);
}
else if (IS_NAT_FUNC(callee)) {
    auto* native = pools.getNativeFunc(callee);
    // ...
}
else if (IS_CLASS(callee)) {
    Klass* klass = pools.getClass(callee);
    // ...
}
else if (IS_BOUND_METHOD(callee)) {
    ObjBoundMethod* boundMethod = pools.getBoundMethod(callee);
    // ...
}
```

**Line 294:** `callValue()` - Instance creation
```cpp
// OLD:
stack[stack.size() - argCount - 1] = trackNewValue(INSTANCE_VAL(instance));

// NEW:
ObjInstance* inst = pools.instancePool.allocate(klass);
uint32_t index = pools.instancePool.getNbElements() - 1;
Value instVal = makeInstanceValue(index);
pools.ensureRefCountCapacity(instVal, index);
pools.instanceRefCounts[index] = 1;
stack[stack.size() - argCount - 1] = instVal;
```

**Line 352:** `callMethod()`
```cpp
// OLD:
Closure* method = AS_CLOSURE(it->second);

// NEW:
Closure* method = pools.getClosure(it->second);
```

**Lines 412-424:** `getValueRefCount()`
```cpp
// OLD:
switch(value.type) {
    case COMPILER_VAL_OBJ: ptr = value.as.obj; break;
    // ...
}
auto it = refCounts.find(ptr);

// NEW:
if (!requiresRefCount(value)) return 0;

uint32_t index = GET_INDEX(value);
auto& refCounts = pools.getRefCountVector(value);

if (index < refCounts.size())
    return refCounts[index];
return 0;
```

**Lines 435-473:** `deleteValue()`
```cpp
// This entire function is now handled by pools.releaseToPool()
// Can be deleted or stubbed
void deleteValue(const Value& value) {
    // No-op: pools handle deletion
}
```

**Lines 507-644:** Arithmetic functions (`addValues`, `subtractValues`, etc.)
```cpp
// OLD:
if (IS_INT(a) and IS_OBJ(b) and AS_OBJ(b)->isNumber())
{
    double floatB = (*AS_OBJ(b)).get<float>();
    // ...
}

// NEW:
if (IS_INT(a) and IS_STRING(b))
{
    ElementType* obj = pools.getString(b);
    if (obj->isNumber()) {
        double floatB = obj->get<float>();
        // ...
    }
}
```

**Line 1241:** `invoke()`
```cpp
// OLD:
ObjInstance* instance = AS_INSTANCE(receiverValue);

// NEW:
ObjInstance* instance = pools.getInstance(receiverValue);
```

**Line 1302:** `op_closure()`
```cpp
// OLD:
ObjFunction* function = AS_FUNC(functionValue);
vm->push(vm->trackNewValue(CLOSURE_VAL(closure)));

// NEW:
ObjFunction* function = vm->pools.getFunction(functionValue);
Closure* closure = vm->pools.closurePool.allocate(function);
uint32_t index = vm->pools.closurePool.getNbElements() - 1;
Value closureVal = makeClosureValue(index);
vm->pools.ensureRefCountCapacity(closureVal, index);
vm->pools.closureRefCounts[index] = 1;
vm->push(closureVal);
```

**Lines 1335-1382:** `op_debug_print()`
```cpp
// OLD:
if (IS_FUNC(value)) {
    ObjFunction* func = AS_FUNC(value);
    // ...
}

// NEW:
if (IS_FUNC(value)) {
    ObjFunction* func = vm->pools.getFunction(value);
    // ...
}
else if (IS_CLASS(value)) {
    Klass* klass = vm->pools.getClass(value);
    // ...
}
else if (IS_INSTANCE(value)) {
    ObjInstance* instance = vm->pools.getInstance(value);
    // ...
}
else if (IS_BOUND_METHOD(value)) {
    ObjBoundMethod* boundMethod = vm->pools.getBoundMethod(value);
    // ...
}
```

**Line 1924:** `op_class()`
```cpp
// OLD:
vm->push(vm->trackNewValue(CLASS_VAL(newClass)));

// NEW:
Klass* klass = vm->pools.classPool.allocate(className);
uint32_t index = vm->pools.classPool.getNbElements() - 1;
Value klassVal = makeClassValue(index);
vm->pools.ensureRefCountCapacity(klassVal, index);
vm->pools.classRefCounts[index] = 1;
vm->push(klassVal);
```

**Line 1938:** `bindMethod()`
```cpp
// OLD:
auto* bound = new ObjBoundMethod(vm->peek(0), AS_CLOSURE(methodValue));
vm->push(vm->trackNewValue(BOUND_METHOD_VAL(bound)));

// NEW:
Closure* closure = vm->pools.getClosure(methodValue);
ObjBoundMethod* bound = vm->pools.boundMethodPool.allocate(vm->peek(0), closure);
uint32_t index = vm->pools.boundMethodPool.getNbElements() - 1;
Value boundVal = makeBoundMethodValue(index);
vm->pools.ensureRefCountCapacity(boundVal, index);
vm->pools.boundMethodRefCounts[index] = 1;
vm->push(boundVal);
```

**Lines 1957, 2001:** `op_get_property()`, `op_set_property()`
```cpp
// OLD:
auto* instance = AS_INSTANCE(vm->peek(0));

// NEW:
auto* instance = vm->pools.getInstance(vm->peek(0));
```

**Line 2057:** `op_method()`
```cpp
// OLD:
Klass* klass = AS_CLASS(classValue);

// NEW:
Klass* klass = vm->pools.getClass(classValue);
```

---

#### **compiler_debug.cpp**

**Lines 10-70:** `printValue()`
```cpp
// Replace all AS_* macros with pool lookups
// OLD:
if (IS_FUNC(value)) {
    ObjFunction* func = AS_FUNC(value);
    // ...
}

// NEW:
if (IS_FUNC(value)) {
    // Need VM* parameter!
    // This is a design issue - printValue needs VM access
    // TEMPORARY: Make it a VM method
}
```

**Lines 354-360:** `disassembleInstruction()`
```cpp
// OLD:
if (not IS_FUNC(chunk.constants[cIndex])) { ... }
ObjFunction* function = AS_FUNC(chunk.constants[cIndex]);

// NEW:
Value constVal = chunk.constants[cIndex];
if (not IS_FUNC(constVal)) { ... }
// Need VM* to get function from pool!
```

---

#### **application.cpp**

**Line 30:** `nativeLogInfo()`
```cpp
// OLD:
LOG_INFO("DOM", *AS_OBJ(args[0]));

// NEW:
// Need VM* parameter!
// Native functions need access to pools
// Update NativeFn signature:
typedef Value (*NativeFn)(VM* vm, int argCount, Value* args);

// Then:
ElementType* obj = vm->pools.getString(args[0]);
LOG_INFO("DOM", *obj);
```

---

## 5. Design Decisions Needed

### Issue 1: Functions That Need VM Access

Many helper functions now need VM* parameter to access pools:
- `printValue()`
- `valueToElement()`
- `elementToValue()`
- All debugging functions
- Native functions

**Recommendation:**
- Make these VM member functions, OR
- Pass VM* as first parameter consistently

### Issue 2: Compiler Integration

The compiler creates ObjFunction objects. With pools, these need to be allocated in VM pools:

**Option A:** Compiler gets VM* and allocates directly in pools
**Option B:** Compiler creates objects, VM copies them into pools during interpret()
**Option C:** Two-phase: compiler creates, VM adopts into pools

**Recommendation:** Option A - Compiler needs VM reference anyway for error reporting

### Issue 3: Constant Pool in Chunk

Chunk stores constants as Values. With NaN-boxing:
- Constants are now indices, not pointers
- Need VM to resolve indices to objects
- Chunk destructor can't free objects anymore (good - VM pools own them)

**No changes needed** - this actually simplifies Chunk!

---

## 6. Testing Strategy

### Phase 1: Compilation
1. Fix all compilation errors file by file
2. Start with core files (vm.cpp, object.h)
3. Move to periphery (debug, application)

### Phase 2: Runtime Testing
1. Simple integer arithmetic (computePi with integers only)
2. Add float/double support
3. Add string support
4. Add function calls
5. Add closures and classes

### Phase 3: Performance Verification
1. Run `perf stat` on computePi.pg
2. Verify cache miss rate improvement
3. Verify execution time improvement
4. Compare against baseline

---

## 7. Estimated Timeline

- **Core VM files (vm.h, vm.cpp, object.h):** 4-6 hours
- **Compiler integration:** 2-3 hours
- **Debug and utility files:** 2-3 hours
- **Testing and debugging:** 3-4 hours
- **Performance tuning:** 1-2 hours

**Total:** 12-18 hours of focused work

---

## 8. Quick Reference: Common Replacements

| OLD | NEW |
|-----|-----|
| `AS_OBJ(v)` | `vm->pools.getString(v)` |
| `AS_FUNC(v)` | `vm->pools.getFunction(v)` |
| `AS_CLOSURE(v)` | `vm->pools.getClosure(v)` |
| `AS_UPVALUE(v)` | `vm->pools.getUpvalue(v)` |
| `AS_CLASS(v)` | `vm->pools.getClass(v)` |
| `AS_NAT_FUNC(v)` | `vm->pools.getNativeFunc(v)` |
| `AS_INSTANCE(v)` | `vm->pools.getInstance(v)` |
| `AS_BOUND_METHOD(v)` | `vm->pools.getBoundMethod(v)` |
| `AS_FLOAT(v)` | `AS_DOUBLE(v)` |
| `IS_FLOAT(v)` | `IS_DOUBLE(v)` |
| `makeFloatValue(d)` | `makeDoubleValue(d)` |
| `FLOAT_VAL(d)` | `DOUBLE_VAL(d)` |
| `new ElementType(...)` | `pools.stringPool.allocate(...)` + index creation |
| `new Closure(...)` | `pools.closurePool.allocate(...)` + index creation |
| `delete ptr` | `pools.releaseToPool(value)` (automatic) |
| `refCounts[ptr]` | `pools.getRefCountVector(v)[GET_INDEX(v)]` |
| `value.type` | `IS_INT(value)`, `IS_STRING(value)`, etc. |
| `value.as.xxx` | `AS_INT(value)`, `AS_DOUBLE(value)`, or pool lookup |

---

## 9. Helper Functions to Add to VM

Add these to VM class to simplify migration:

```cpp
// In VM class:
public:
    // String creation
    Value createString(const std::string& str) {
        ElementType* obj = pools.stringPool.allocate(str);
        uint32_t index = pools.stringPool.getNbElements() - 1;
        Value val = makeStringValue(index);
        pools.ensureRefCountCapacity(val, index);
        pools.stringRefCounts[index] = 1;
        return val;
    }

    // Closure creation
    Value createClosure(ObjFunction* function) {
        Closure* closure = pools.closurePool.allocate(function);
        uint32_t index = pools.closurePool.getNbElements() - 1;
        Value val = makeClosureValue(index);
        pools.ensureRefCountCapacity(val, index);
        pools.closureRefCounts[index] = 1;
        return val;
    }

    // Class creation
    Value createClass(const std::string& name) {
        Klass* klass = pools.classPool.allocate(name);
        uint32_t index = pools.classPool.getNbElements() - 1;
        Value val = makeClassValue(index);
        pools.ensureRefCountCapacity(val, index);
        pools.classRefCounts[index] = 1;
        return val;
    }

    // Instance creation
    Value createInstance(Klass* klass) {
        ObjInstance* instance = pools.instancePool.allocate(klass);
        uint32_t index = pools.instancePool.getNbElements() - 1;
        Value val = makeInstanceValue(index);
        pools.ensureRefCountCapacity(val, index);
        pools.instanceRefCounts[index] = 1;
        return val;
    }

    // Bound method creation
    Value createBoundMethod(Value receiver, Closure* method) {
        ObjBoundMethod* bound = pools.boundMethodPool.allocate(receiver, method);
        uint32_t index = pools.boundMethodPool.getNbElements() - 1;
        Value val = makeBoundMethodValue(index);
        pools.ensureRefCountCapacity(val, index);
        pools.boundMethodRefCounts[index] = 1;
        return val;
    }

    // Upvalue creation
    Value createUpvalue(Value* slot) {
        ObjUpvalue* upvalue = pools.upvaluePool.allocate(slot);
        uint32_t index = pools.upvaluePool.getNbElements() - 1;
        Value val = makeUpvalueValue(index);
        pools.ensureRefCountCapacity(val, index);
        pools.upvalueRefCounts[index] = 1;
        return val;
    }
```

---

## 10. When You're Stuck

**Common Issues:**

1. **"Cannot access pools"** → Function needs VM* parameter
2. **"Segfault on value access"** → Using AS_* macro instead of pool lookup
3. **"Refcount not found"** → Forgot to call ensureRefCountCapacity
4. **"Type not matching"** → Using IS_FLOAT instead of IS_DOUBLE
5. **"Pool index out of bounds"** → Object not allocated in pool yet

**Debugging Tips:**
- Enable `DEBUG_RUNTIME_MEMORY` to see refcount operations
- Add logging in pool allocate/release
- Check pool sizes: `pools.stringPool.getNbElements()`
- Verify indices are valid before lookup

---

Good luck with the migration! The performance gains will be worth it! 🚀
