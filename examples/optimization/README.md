# Optimization Examples and Test Suite

This directory contains a comprehensive set of examples demonstrating bytecode optimization in the PgCompiler. Each example serves dual purposes:

1. **Documentation** - Shows how different language constructs are optimized
2. **Testing** - Validates that optimizations preserve program semantics

## Examples Overview

| File | Description | Key Optimization Features |
|------|-------------|---------------------------|
| `01_simple_constants.pg` | Basic constant declarations | Global constant uniformity |
| `02_local_variables.pg` | Local scope constants | Local variable optimization |
| `03_variable_reassignment.pg` | Variable reassignment patterns | OP_Set_Local generation |
| `04_mixed_scopes.pg` | Global + local interaction | Cross-scope optimization |
| `05_control_flow.pg` | Conditionals with constants | Jump optimization |
| `06_simple_loops.pg` | Basic loop constructs | Loop preparation |
| `07_arithmetic_expressions.pg` | Mathematical operations | Constant folding potential |
| `08_nested_scopes.pg` | Complex scoping | Variable shadowing |
| `09_boolean_logic.pg` | Boolean constants and logic | Boolean optimization |
| `10_complex_scenario.pg` | Real-world-like scenario | Multiple optimization types |

## Testing Framework

### OptimizationTestBase

The `OptimizationTestBase` class provides a standardized testing pattern:

```cpp
// Standard test pattern
testOptimizationPreservesSemantics(pass, testCases, expectModifications, verbose);
```

**Key Features:**
- **Semantics Preservation**: Compares output before/after optimization using `__dprint`
- **Cross-Pass Compatibility**: Tests work with any `BytecodePass`
- **Comprehensive Coverage**: Includes standard test cases for common patterns
- **Debugging Support**: Verbose mode shows bytecode before/after optimization

### Usage Example

```cpp
class MyOptimizationTest : public OptimizationTestBase {
    void testMyPass() {
        auto testCases = getStandardTestCases();
        testCases.push_back("my_specific_case();");

        testOptimizationPreservesSemantics(myPass.get(), testCases);
    }
};
```

## Running Tests

```bash
# Build and run optimization tests
make test_compiler
./test_compiler --gtest_filter="*Optimization*"

# Run example-based integration tests
./test_compiler --gtest_filter="*OptimizationExamples*"

# Run with verbose output to see bytecode transformations
./test_compiler --gtest_filter="*OptimizationExamples*.SimpleConstants"
```

## Optimization Passes Covered

1. **ConstantUniformityPass** - Deduplicates identical constants
2. **ConstantPropagationPass** - Replaces variable loads with constants
3. **LongJumpOptimizationPass** - Converts long jumps to short jumps

## Key Testing Principles

1. **Semantics First**: Every optimization must preserve program behavior
2. **Real-world Patterns**: Examples reflect actual usage scenarios
3. **Comprehensive Coverage**: Tests cover edge cases and interactions
4. **Standardization**: Consistent testing pattern across all passes
5. **Documentation**: Examples serve as living documentation

## Adding New Examples

1. Create a new `.pg` file with clear comments
2. Include `__dprint` statements to verify behavior
3. Add expected output in comments
4. Update the integration test to include the new example
5. Test with all optimization passes to ensure compatibility

## Debugging Optimization Issues

When an optimization test fails:

1. **Check semantics**: Are the outputs identical?
2. **Examine bytecode**: Use verbose mode to see transformations
3. **Verify patterns**: Ensure the optimization pattern exists in the code
4. **Test isolation**: Run the pass individually to isolate issues

## Future Extensions

This framework is designed to be extended for:
- Loop optimization passes
- Dead code elimination
- Constant folding
- Inline function optimization
- Register allocation optimization

Each new pass should:
1. Extend `OptimizationTestBase`
2. Add specific test cases to examples
3. Validate semantics preservation
4. Document optimization behavior