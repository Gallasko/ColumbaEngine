# PgCompiler Test Data

This directory contains test data files for the PgCompiler testing framework.

## Directory Structure

- `valid_programs/` - Valid .pg program files that should compile and execute successfully
- `invalid_programs/` - Invalid .pg program files that should produce compile or runtime errors
- `expected_bytecode/` - Human-readable representations of expected bytecode output

## File Types

### Valid Programs (.pg)
These files contain valid PgCompiler language expressions that should:
- Compile without errors
- Execute without runtime errors
- Produce deterministic results

### Invalid Programs (.pg) 
These files contain invalid syntax or semantics that should:
- Produce compile-time errors (syntax_errors.pg)
- Produce runtime errors (runtime_errors.pg)

### Expected Bytecode (.txt)
These files document the expected bytecode generation for specific expressions:
- Human-readable format
- Include opcode sequences
- List constant pool contents
- Used for regression testing

## Usage

Test files are automatically copied to the build directory by CMake and can be used by the test framework to:
- Load test programs from disk
- Verify compilation results
- Test error handling
- Perform regression testing
- Validate bytecode generation

## Adding New Tests

To add new test cases:
1. Create new .pg files in appropriate subdirectories
2. Add corresponding expected output files if needed
3. Reference the files in test cases using relative paths from the test data directory