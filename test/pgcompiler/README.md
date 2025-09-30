# Enhanced Optimization Testing Framework

This document describes the enhanced optimization testing framework for the PgCompiler that provides comprehensive testing of optimization passes against example files.

## Overview

The framework automatically:
- Discovers `.pg` files in `examples/optimization/`
- Tests individual optimization passes
- Tests predefined pass combinations  
- Generates granular CTest cases for precise failure isolation
- Compares unoptimized vs optimized program semantics

## Key Components

### OptimizationTestBase

Enhanced base class providing:
- **Pass Registration**: `registerPass(name, pass)` for organizing passes
- **Automatic Discovery**: Finds all `.pg` files in `examples/optimization/`
- **Combination Testing**: Tests predefined pass sequences
- **Granular Testing**: Individual test methods for precise failure isolation

### Predefined Pass Combinations

The framework includes these predefined combinations:

1. **`standard`**: `ConstantPropagation` → `LongJumpOptimization`
2. **`aggressive`**: `ConstantPropagation` → `LongJumpOptimization` → `LoopSimplification`  
3. **`conservative`**: `ConstantUniformity` → `ConstantPropagation`

Add new combinations by modifying `setupPredefinedCombinations()` in `OptimizationTestBase`.

## Usage

### Basic Usage

```cpp
class MyOptimizationTest : public OptimizationTestBase {
protected:
    void SetUp() override {
        OptimizationTestBase::SetUp();
        
        // Register your passes
        registerPass("MyPass", std::make_unique<MyOptimizationPass>());
        registerPass("AnotherPass", std::make_unique<AnotherPass>());
    }
};

// Test all examples against all registered passes
TEST_F(MyOptimizationTest, AllIndividualPasses) {
    testAllIndividualPasses(false);
}

// Test all examples against all predefined combinations
TEST_F(MyOptimizationTest, AllCombinations) {
    testAllPredefinedCombinations(false);
}
```

### Macro-Generated Tests

Use the `GENERATE_OPTIMIZATION_TESTS(TestClass)` macro to automatically generate individual test cases:

```cpp
class MyOptimizationTest : public OptimizationTestBase {
    // ... setup code ...
};

GENERATE_OPTIMIZATION_TESTS(MyOptimizationTest)
```

This creates tests like:
- `IndividualTest_01_simple_constants_ConstantPropagation`
- `IndividualTest_05_control_flow_LongJumpOptimization`
- `CombinationTest_10_complex_scenario_aggressive`

### Custom Pass Combinations

```cpp
TEST_F(MyOptimizationTest, CustomCombination) {
    auto code = readExampleFile("06_simple_loops.pg");
    
    std::vector<BytecodePass*> myPipeline = {
        registeredPasses["ConstantPropagation"].get(),
        registeredPasses["LoopSimplification"].get(),
        registeredPasses["MyCustomPass"].get()
    };
    
    testPassCombination(myPipeline, code, false, "my-pipeline");
}
```

## Running Tests

### All Tests
```bash
make test_compiler
./test_compiler
```

### Specific Test Categories
```bash
# All optimization tests
./test_compiler --gtest_filter="*Optimization*"

# Individual pass tests only
./test_compiler --gtest_filter="*IndividualTest*"

# Combination tests only  
./test_compiler --gtest_filter="*CombinationTest*"

# Specific file and pass
./test_compiler --gtest_filter="*IndividualTest_05_control_flow_ConstantPropagation*"
```

### With Verbose Output
```cpp
// In your test, set verbose=true to see bytecode transformations
testSingleOptimization(pass.get(), code, false, true);
testPassCombination(passes, code, true, "debug");
```

## Adding New Components

### Adding a New Optimization Pass

1. **Create the pass** (e.g., `MyNewPass.h/cpp`)
2. **Register it** in your test `SetUp()`:
   ```cpp
   registerPass("MyNewPass", std::make_unique<MyNewPass>());
   ```
3. **Test automatically**: The framework will test it against all examples
4. **Add to combinations** (optional): Modify `setupPredefinedCombinations()`

### Adding New Example Files

1. **Create `.pg` file** in `examples/optimization/`
2. **Include test output** with `__dprint()` statements
3. **Add comments** explaining the optimization target
4. **Tests auto-generate**: Framework discovers new files automatically

### Adding New Pass Combinations

Modify `setupPredefinedCombinations()` in `OptimizationTestBase`:

```cpp
predefinedCombinations["my_combo"] = {
    "my_combo",
    {"Pass1", "Pass2", "Pass3"},
    "Description of this combination"
};
```

## Test Structure

```
test/pgcompiler/
├── fixtures/
│   ├── optimization_test_base.h         # Enhanced base class
│   ├── optimization_test_base.cpp       # (if needed)
│   └── compiler_test_base.h             # Original base
├── integration/
│   ├── test_optimization_examples.cc    # Enhanced examples test  
│   └── test_optimization_framework.cc   # Framework demonstration
└── unit/
    └── test_*_pass.cc                   # Individual pass tests
```

## Debugging Failed Tests

### 1. Identify the Failure
```bash
# Run specific failing test
./test_compiler --gtest_filter="*IndividualTest_05_control_flow_ConstantPropagation*"
```

### 2. Enable Verbose Output
```cpp
// Modify test to use verbose=true
testSpecificFileAndPass("05_control_flow.pg", "ConstantPropagation", true);
```

### 3. Examine Bytecode
Verbose output shows:
- Original bytecode
- Bytecode after each pass
- Execution outputs for comparison

### 4. Test in Isolation
```cpp
// Test just the problematic combination
TEST_F(MyTest, Debug_SpecificIssue) {
    auto code = readExampleFile("05_control_flow.pg");
    testSingleOptimization(
        registeredPasses["ConstantPropagation"].get(), 
        code, false, true  // verbose=true
    );
}
```

## Best Practices

1. **Semantics First**: All optimizations must preserve program behavior
2. **Comprehensive Testing**: Test edge cases and pass interactions
3. **Granular Isolation**: Use individual tests to pinpoint failures
4. **Document Examples**: Include clear comments in `.pg` files
5. **Test Combinations**: Verify passes work together correctly

## Example Output

```
=== Testing 05_control_flow.pg with ConstantPropagation ===
Original bytecode:
0000    OP_CONSTANT         0 '5'
0003    OP_DEFINE_GLOBAL    1 'x'
0006    OP_GET_GLOBAL       1 'x'
...

Optimized bytecode (modified: true):
0000    OP_CONSTANT         0 '5'  
0003    OP_DEFINE_GLOBAL    1 'x'
0006    OP_CONSTANT         0 '5'    # Optimized!
...

Original output: '5'
Optimized output: '5'
✓ Semantics preserved
```

This framework ensures comprehensive testing while making it easy to add new passes and debug optimization issues.