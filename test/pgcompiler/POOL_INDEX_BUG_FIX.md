# Pool Index Reuse Bug Fix

## Problem Summary

String concatenation and other heap object operations were producing incorrect results because pool indices were being calculated incorrectly when freed slots were reused.

### Symptoms:
- String `"a" + "b"` produced `"s"` (a different string from the pool)
- The pattern: index(a) + index(b) = index(result) → 3 + 4 = 7, and pool[7] = "s"
- This proved the pool was reusing slot 7, but `createString()` was using the wrong index

## Root Cause

The `AllocatorPool` class has a **free list** that reuses previously released slots. When `allocate()` is called:
1. If the free list has slots, it reuses one (could be any index)
2. Otherwise, it allocates at the end with index = `nbElements++`

The bug was in all `VM::create*()` methods:

```cpp
Value VM::createString(const ElementType& element)
{
    uint32_t index = pools.stringPool.getNbElements();  // BUG: assumes no reuse
    pools.stringPool.allocate(element);
    Value val = makeStringValue(index);
    return trackNewValue(val);
}
```

**The problem:** `getNbElements()` returns the current count, which assumes the new element will be at the end. But if `allocate()` reuses a freed slot (e.g., slot 7), then `index` is wrong!

### Example Scenario:
1. Strings are allocated: "hello"(0), " "(1), "world"(2), ... "s"(7), ...
2. Some strings get released, adding slots to the free list (e.g., slot 7)
3. `"a"` is allocated at slot 3, `"b"` at slot 4
4. `"a" + "b"` calls `createString("ab")`
5. `getNbElements()` returns 7 (current count)
6. `allocate()` reuses slot 7 from the free list
7. **Mismatch!** We think the new string is at index 7, but pool[7] still contains "s"
8. Later, `"ab"` is stored at slot 7, overwriting "s"
9. Result: Any reference to index 7 now points to the wrong string

## The Solution

### Step 1: Add `allocateWithIndex()` to MemoryPool

Added a new method to [`memorypool.h`](../../src/Engine/Memory/memorypool.h) that returns both the pointer AND the correct index:

```cpp
template <typename... Args>
std::pair<T*, size_t> allocateWithIndex(Args&&... args)
{
    LOG_THIS_MEMBER("Memory Pool");

    if (freeList)
    {
        auto chunk = freeList;
        freeList = chunk->next;
        ::new(&(chunk->element)) T(std::forward<Args>(args)...);
        nbElements++;

        // Find the actual index by searching
        T* ptr = reinterpret_cast<T*>(chunk);
        size_t index = 0;
        for (size_t i = 0; i < size; i++)
        {
            if (getElement(i) == ptr)
            {
                index = i;
                break;
            }
        }

        return {ptr, index};
    }

    const size_t index = nbElements++;
    if (index >= size) reserve(index);

    PGMemChunk<T>* chunk = getChunk(index);
    T* ptr = ::new(&(chunk->element)) T(std::forward<Args>(args)...);

    return {ptr, index};
}
```

**Key insight:** When reusing from the free list, we must search through the pool to find which index matches the returned pointer.

### Step 2: Update All `VM::create*()` Methods

Updated all methods in [`vm.cpp`](../../exemples/PgCompiler/vm.cpp) to use `allocateWithIndex()`:

```cpp
Value VM::createString(const ElementType& element)
{
    auto [ptr, index] = pools.stringPool.allocateWithIndex(element);
    Value val = makeStringValue(static_cast<uint32_t>(index));
    return trackNewValue(val);
}

Value VM::createClosure(ObjFunction* function)
{
    auto [ptr, index] = pools.closurePool.allocateWithIndex(function);
    Value val = makeClosureValue(static_cast<uint32_t>(index));
    return trackNewValue(val);
}

// Similarly for createFunction, createUpvalue, createClass,
// createInstance, createBoundMethod
```

## Impact

This fix applies to **all heap-allocated object types**:
- ✅ Strings
- ✅ Closures
- ✅ Functions
- ✅ Upvalues
- ✅ Classes
- ✅ Instances
- ✅ Bound methods

Any of these could have had the same index-reuse bug, causing wrong values to be accessed or memory corruption.

## Tests Fixed

This single fix resolves:
1. **StringConcatenation** - String + operator now works correctly
2. **StringInVariables** - String variables resolve to correct values
3. Potentially fixes memory issues in closure/class tests (to be verified)

## Performance Consideration

The linear search in `allocateWithIndex()` (when reusing free slots) has O(n) complexity where n = pool size. For typical workloads:
- Most allocations happen at the end (no search needed)
- Free list reuse only happens after objects are released
- Pool sizes are usually small (< 1000 objects per type)

If profiling shows this is a bottleneck, optimization options:
1. Store index in the chunk structure
2. Use a hash map from pointer to index
3. Calculate index mathematically (complex due to exponential growth)

For now, correctness is prioritized over micro-optimization.

## Verification

To verify the fix:
1. Run `./release/PgCompiler test/pgcompiler/scripts/string_concatenation.pg`
2. Output should be:
   ```
   hello world
   ab
   test!
   ```
3. No "Global variable name must be a literal" errors
4. No wrong string values

## Related Files Modified

1. [`src/Engine/Memory/memorypool.h`](../../src/Engine/Memory/memorypool.h) - Added `allocateWithIndex()`
2. [`exemples/PgCompiler/vm.cpp`](../../exemples/PgCompiler/vm.cpp) - Updated all `create*()` methods
