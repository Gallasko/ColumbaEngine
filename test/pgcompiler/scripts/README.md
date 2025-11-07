# PgCompiler Script Testbench

This directory contains script-based tests for the PgCompiler.

## Test Format

Each test consists of two files:

1. `<test_name>.pg` - The script to compile and execute
2. `<test_name>.expected` - The expected output (optional)

### Example

**simple_addition.pg:**
```pg
__dprint(1 + 2);
__dprint(10 + 5);
```

**simple_addition.expected:**
```
3
15
```

## How It Works

- The testbench reads `.pg` script files
- Compiles and executes them using the VM
- Captures output from `__dprint()` calls (stored in `vm.testOutput`)
- Compares actual output with expected output from `.expected` files
- If no `.expected` file exists, just checks for successful execution

## Running Tests

The tests are automatically discovered and run by the `ScriptTestBench` test fixture in `script_testbench.cc`.

To run all script tests:
```bash
cd build
make test_compiler
./test_compiler --gtest_filter="ScriptTestBench*"
```

To run a specific script test:
```bash
./test_compiler --gtest_filter="ScriptTestBench.SimpleAddition"
```

## Adding New Tests

1. Create a new `.pg` file in this directory with `__dprint()` statements
2. Create a corresponding `.expected` file with the expected output
3. Add a test case in `script_testbench.cc`:

```cpp
TEST_F(ScriptTestBench, MyNewTest) {
    testScript("my_new_test");
}
```

4. Rebuild and run tests

## Test Categories

### Basic Arithmetic
- `simple_addition.pg` - Addition operations
- `simple_subtraction.pg` - Subtraction operations
- `simple_multiplication.pg` - Multiplication operations
- `simple_division.pg` - Division operations

### Boolean Operations
- `boolean_literals.pg` - Boolean true/false literals
- `boolean_not.pg` - Boolean NOT operator

### Comparison Operations
- `equality_comparison.pg` - Equality (==) operator
- `inequality_comparison.pg` - Inequality (!=) operator
- `less_than_comparison.pg` - Less than (<, <=) operators
- `greater_than_comparison.pg` - Greater than (>, >=) operators

### Complex Expressions
- `precedence.pg` - Operator precedence rules
- `parentheses.pg` - Parentheses override precedence
- `nested_expressions.pg` - Complex nested expressions

### Variables (Add when implemented)
- `variable_declaration.pg`
- `variable_assignment.pg`

### Control Flow (Add when implemented)
- `if_statement.pg`
- `while_loop.pg`
- `for_loop.pg`

### Functions (Add when implemented)
- `function_declaration.pg`
- `function_call.pg`
- `recursive_function.pg`

### Error Cases
- `syntax_error.pg` - Should produce compile error
- `runtime_error.pg` - Should produce runtime error

## Notes

- If a `.expected` file is not present, the test will only check for successful execution (no output validation)
- Output is trimmed of leading/trailing whitespace before comparison
- Tests should be deterministic and produce consistent output
- Each `__dprint()` call produces one line of output with a newline character
