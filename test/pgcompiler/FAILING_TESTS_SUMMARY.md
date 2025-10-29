# Failing Tests Summary

**Test Run Date:** 2025-10-29
**Total Tests:** 62
**Tests Run Before Crash:** 43
**Passed:** 36
**Failed:** 7
**Crashed:** 1 (TestSimpleClosure - double free)

## Failed Tests

### 1. UnaryComplex
**Status:** Output mismatch (missing one line)
**Expected:**
```
-5
true
false
-10
true
true
```
**Got:**
```
-5
true
false
-10
true
```
**Issue:** Missing last `true` output - likely missing a `__dprint()` call or execution stopping early

---

### 2. LocalScope
**Status:** Output mismatch (variable shadowing issue)
**Expected:**
```
1
2
1
```
**Got:**
```
1
x
1
```
**Issue:** Variable `x` is being printed as the string "x" instead of its value `2`. This indicates variable shadowing or scope resolution is not working correctly.

---

### 3. ForLoop
**Status:** No output + OK result
**Expected:**
```
0
1
2
3
4
```
**Got:** (empty)
**Issue:** For loop body never executes. Possible compilation or control flow issue with for statements.

---

### 4. ForLoopIncrement
**Status:** No output + RUNTIME_ERROR
**Expected:**
```
0
1
2
3
4
```
**Got:** (empty)
**Result:** RUNTIME_ERROR (code 02-00-00-00 instead of OK 00-00-00-00)
**Issue:** For loop with ++ operator fails at runtime. Critical error.

---

### 5. StringConcatenation
**Status:** Partial output + RUNTIME_ERROR
**Expected:**
```
hello world
ab
test!
```
**Got:**
```
hello world
s
```
**Result:** RUNTIME_ERROR
**Issue:** String concatenation partially works for first expression but crashes on subsequent concatenations. The variable name "s" is being printed instead of the concatenated value.

---

### 6. StringInVariables
**Status:** Output mismatch (variable name instead of value)
**Expected:**
```
Hello, Alice
Hello, Bob
```
**Got:**
```
Hello, Alice
name
```
**Issue:** Variable `name` is being printed as the string "name" instead of its value "Bob". Similar to LocalScope issue - variable resolution problem.

---

### 7. TestFunc
**Status:** No output + RUNTIME_ERROR
**Expected:**
```
<test>
1
0
```
**Got:** (empty)
**Result:** RUNTIME_ERROR
**Issue:** Function test fails completely - no output produced. Likely function call or parameter passing issue.

---

## Crash

### TestSimpleClosure
**Status:** CRASH - Double free detected
**Error:** `free(): double free detected in tcache 2`
**Issue:** Memory management bug in closure implementation. Reference counting or cleanup code is freeing memory twice. This is a critical memory safety issue that needs investigation with stack traces and memory debugging.

---

## Tests Not Run (after crash)

The following tests were not executed due to the crash:
- TestClosedClosure
- TestClosure
- TestClass
- TestClassLocal
- TestClassPostIncr
- TestInstance
- TestMethods
- TestBound
- TestProperties
- TestCoffee
- TestOops
- TestLoop
- TestLoop2
- TestLoopLocal
- TestModulo
- TestBasicIncr
- TestIncrLoop
- TestLoopGlobal
- TestFuncFailed

Total: 19 tests not executed

---

## Pattern Analysis

### Common Issues:
1. **Variable resolution bug:** Multiple tests (LocalScope, StringInVariables) print variable names instead of values
2. **For loop broken:** Both for loop tests fail (one with runtime error)
3. **String operations:** String concatenation causes runtime errors
4. **Memory safety:** Critical double-free crash in closure test

### Categories of Failures:
- **Scoping/Variable Resolution:** 2 tests
- **Control Flow (for loops):** 2 tests
- **String Operations:** 2 tests
- **Function Calls:** 1 test
- **Memory Crash:** 1 test

---

## Next Steps

1. **Enable stack logging and bytecode printing** to investigate:
   - Variable resolution issues (LocalScope, StringInVariables)
   - For loop compilation/execution
   - String concatenation runtime errors
   - Function parameter passing (TestFunc)

2. **Debug memory crash** with valgrind or AddressSanitizer:
   - TestSimpleClosure double-free issue
   - Reference counting logic in closure cleanup

3. **Investigate compiler vs runtime issues:**
   - Are for loops being compiled correctly?
   - Is string concatenation implemented?
   - Why are variable names being treated as strings?
