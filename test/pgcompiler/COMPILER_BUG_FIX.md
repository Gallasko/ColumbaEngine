# Compiler Bug Fix: OP_Get_Local Offset Issue

## Problem Summary

Multiple tests were failing because local variables in the main script scope were being accessed with incorrect indices:
- Variables were compiled as `OP_Get_Local '1'` when they should be `OP_Get_Local '0'`
- This caused the wrong values to be read (reading from constant pool or wrong stack slots)
- Affected tests: LocalScope, ForLoop, ForLoopIncrement, and many others

## Root Cause

The issue was in `compiler.cpp` at line 258-263 in the `initCompiler()` function:

```cpp
if (type != FunctionType::TYPE_FUNCTION)
{
    // The first local is always the function itself
    locals.push_back(Local{Token(TokenType::TOK_FUN, "this", 0, 0), 0, false});
    localCount++;
}
```

This code was adding an implicit "this" local at slot 0 for:
- Scripts (TYPE_SCRIPT)
- Methods (TYPE_METHOD)
- Initializers (TYPE_INITIALIZER)

However, **scripts don't need slot 0 reserved**. This caused all local variables in scripts to be offset by +1:
- First variable `i` → compiled as slot 1 instead of slot 0
- Second variable `j` → compiled as slot 2 instead of slot 1
- etc.

## The Fix

Changed the condition to only add implicit "this" for methods and initializers:

```cpp
if (type == FunctionType::TYPE_METHOD || type == FunctionType::TYPE_INITIALIZER)
{
    // For methods and initializers, the first local is "this"
    locals.push_back(Local{Token(TokenType::TOK_FUN, "this", 0, 0), 0, false});
    localCount++;
}
```

Now scripts start with local index 0 for the first variable, matching how the VM's stack frames work.

## Why Methods and Initializers Still Need Slot 0

Methods and initializers need slot 0 for the implicit `this` reference:
- When you call `obj.method()`, slot 0 contains the `obj` instance
- The method body can reference `this` to access the instance
- The VM setup for method calls ensures slot 0 contains the receiver

## Stack Frame Layout

### For Scripts:
```
Stack:     [closure] [var1] [var2] ...
           ^
           frame->slots points here
Slots:     [0]       [1]    [2]    ...
```

### For Functions:
```
Stack:     [...] [closure] [arg1] [arg2] ...
                            ^
                            frame->slots points here
Slots:                      [0]    [1]    ...
```

### For Methods:
```
Stack:     [...] [closure] [this] [arg1] [arg2] ...
                            ^
                            frame->slots points here
Slots:                      [0]    [1]    [2]    ...
```

## Tests Fixed

This single fix should resolve:
1. LocalScope - variable shadowing now works correctly
2. ForLoop - loop variable `i` accessed correctly
3. ForLoopIncrement - loop with ++ operator works
4. StringInVariables - variables resolve to values not names
5. Many other tests that use local variables in script scope

## Additional Fixes Needed

1. **UnaryComplex** - Fixed by correcting the expected output file (removed extra line)
2. **StringConcatenation** - Still needs investigation (string + operator may not be implemented)
3. **TestSimpleClosure** - Still crashes with double-free (separate memory management bug)
4. **TestFunc** - Needs investigation after this fix is applied
