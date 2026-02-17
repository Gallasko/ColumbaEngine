# Popping Jump Optimization Pass Test Suite

These test scripts verify the correctness of the popping jump optimization pass.

## Test Files

1. **01_basic_if.pg** - Basic if statement, should optimize
2. **02_if_else.pg** - If-else statement
3. **03_nested_if.pg** - Nested if statements
4. **04_multiple_conditions.pg** - Multiple sequential if statements
5. **05_if_with_loop.pg** - If inside a loop
6. **06_if_with_return.pg** - If with return statement
7. **07_complex_expression.pg** - Complex boolean expressions (and/or)
8. **08_string_boolean_op.pg** - String + boolean operations (error case)
9. **09_if_else_chain.pg** - If-else-if chains
10. **10_condition_as_expression.pg** - Condition result used as value
11. **11_break_in_loop.pg** - Break statement with condition
12. **12_continue_in_loop.pg** - Continue statement with condition
13. **13_component_generator_pattern.pg** - Complex if-else-if with string ops
14. **14_nested_string_concat.pg** - Nested conditions with string concatenation
15. **15_boolean_value_flow.pg** - Boolean value flow through conditionals

## How to Run

Compile and run each test with the test_compiler:

```bash
./build/test_compiler test/pgcompiler/scripts/popping_jump_tests/01_basic_if.pg
```

## Expected Results

All tests should execute without errors. If any test fails with a type error (e.g., "Operator + between two incompatible operand"), this indicates a bug in the popping jump optimization pass where:

1. A value is being popped incorrectly
2. The stack state is corrupted
3. The optimization is being applied to a non-optimizable pattern

## Debugging Tips

To debug a failing test:

1. Look at the bytecode dump before and after optimization
2. Check if `OP_Jump_If_False_Popping` was created
3. Verify the jump distances are correct
4. Ensure the two pops being removed are actually for the same value
5. Check if there's code between the jump and its target that affects the stack

## Known Issue

The component_generator.pg fails with "Operator + between string and boolean" error when processing Texture2DComponent.pgcomp. Tests 13-15 attempt to reproduce this pattern.
