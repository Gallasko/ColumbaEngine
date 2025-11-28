# For-In Loop Tests - Added to Test Suite

## Summary

Five comprehensive test cases for the for-in loop feature have been created and added to the test suite.

## Test Files Created

All test files are located in `/test/pgcompiler/scripts/`:

### 1. Simple For-In Loop (`for_in_simple.pg` / `for_in_simple.expected`)

**Test Code:**
```pg
var table = [a: 1, b: 2, c: 3];
for (var key : table) {
    __dprint(key);
}
```

**Expected Output:**
```
a
b
c
```

**What it tests:**
- Basic for-in loop functionality
- Iteration over a table with named string keys
- Correct key ordering

---

### 2. Nested For-In Loops (`for_in_nested.pg` / `for_in_nested.expected`)

**Test Code:**
```pg
var outer = [x: 1, y: 2];
for (var k1 : outer) {
    var inner = [a: 10, b: 20];
    for (var k2 : inner) {
        __dprint(k1);
        __dprint(k2);
    }
}
```

**Expected Output:**
```
x
a
x
b
y
a
y
b
```

**What it tests:**
- Nested for-in loops work correctly
- Each loop maintains its own key variable independently
- Inner loop variable doesn't interfere with outer loop variable
- Scope management is correct

---

### 3. Empty Table (`for_in_empty.pg` / `for_in_empty.expected`)

**Test Code:**
```pg
var empty = [];
for (var key : empty) {
    __dprint(key);
}
__dprint("done");
```

**Expected Output:**
```
done
```

**What it tests:**
- Empty table handling
- Loop body never executes for empty tables
- No runtime errors with empty tables
- Code after the loop executes correctly

---

### 4. For-In with Value Access (`for_in_with_values.pg` / `for_in_with_values.expected`)

**Test Code:**
```pg
var table = [name: "Alice", age: 30, city: "Paris"];
for (var key : table) {
    __dprint(key);
    __dprint(table[key]);
}
```

**Expected Output:**
```
name
Alice
age
30
city
Paris
```

**What it tests:**
- Using the loop variable as a key to access values
- Real-world usage pattern
- String and numeric values in tables

---

### 5. Numeric Keys (Array-like) (`for_in_numeric_keys.pg` / `for_in_numeric_keys.expected`)

**Test Code:**
```pg
var array = [10, 20, 30, 40];
for (var key : array) {
    __dprint(key);
    __dprint(array[key]);
}
```

**Expected Output:**
```
0
10
1
20
2
30
3
40
```

**What it tests:**
- Array-like tables with implicit numeric keys
- Numeric keys are converted to strings for iteration
- Accessing values by numeric key works correctly

---

## Test Suite Integration

The tests have been added to the test suite in [script_testbench.cc](../test/pgcompiler/script_testbench.cc) at lines 398-425:

```cpp
// ============================================================================
// For-In Loops
// ============================================================================

TEST_F(ScriptTestBench, ForInSimple)
{
    testScript("for_in_simple");
}

TEST_F(ScriptTestBench, ForInNested)
{
    testScript("for_in_nested");
}

TEST_F(ScriptTestBench, ForInEmpty)
{
    testScript("for_in_empty");
}

TEST_F(ScriptTestBench, ForInWithValues)
{
    testScript("for_in_with_values");
}

TEST_F(ScriptTestBench, ForInNumericKeys)
{
    testScript("for_in_numeric_keys");
}
```

## Running the Tests

To build and run the tests:

```bash
# Build the test target
make test_compiler

# Run all tests
./test_compiler

# Run only for-in loop tests
./test_compiler --gtest_filter="*ForIn*"
```

## Test Coverage

These tests provide comprehensive coverage of the for-in loop feature:

✅ **Basic Functionality**: Simple iteration over table keys
✅ **Nested Loops**: Multiple levels of for-in loops
✅ **Edge Cases**: Empty tables
✅ **Value Access**: Using keys to access table values
✅ **Numeric Keys**: Array-like tables with implicit indices
✅ **Scope Management**: Each iteration gets its own key variable scope

## Files Modified

1. **Created**: 10 new test files (5 `.pg` files and 5 `.expected` files)
2. **Modified**: [test/pgcompiler/script_testbench.cc](../test/pgcompiler/script_testbench.cc) - Added 5 test cases

## Validation

All tests validate:
- The script compiles without errors
- The script executes without runtime errors
- The output matches the expected output exactly
- The compiled bytecode version also produces correct output
