#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "math_module.h"
#include "Files/filemodule.h"
#include "Helpers/stringmodule.h"
#include "Helpers/algorithmmodule.h"

#include "ECS/entitysystem.h"

#include "../mocklogger.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

namespace pg {
namespace test {

/**
 * Simple script-based testbench
 *
 * Test scripts are stored in test/pgcompiler/scripts/
 * Each test consists of:
 * - <name>.pg - The script to compile and run
 * - <name>.expected - The expected output (optional)
 *
 * If .expected file exists, output is validated.
 * If .expected file doesn't exist, we just check for successful execution.
 */
class ScriptTestBench : public ::testing::Test
{
protected:
    VM *vm;

    void registerNativeFunctions(VM* vmInstance)
    {
        // Register native modules for testing
        vmInstance->addNativeModule("math", MathModule());
        vmInstance->addNativeModule("mathNative", MathModule());

        // Todo add test for those standard helpers modules
        vmInstance->addNativeModule("file", FileModule());
        vmInstance->addNativeModule("string", StringModule());
        vmInstance->addNativeModule("algorithm", AlgorithmModule());

        // Register toString native function
        vmInstance->registerNative("__toString", [](VM *vm, int argCount, Value* args) -> Value {
            if (argCount != 1) return makeBoolValue(false);

            std::string str;

            if (IS_STRING(args[0]))
            {
                str = vm->asString(args[0]);
            }
            else if (IS_INT(args[0]))
            {
                str = std::to_string(AS_INT(args[0]));
            }
            else if (IS_DOUBLE(args[0]))
            {
                str = std::to_string(AS_DOUBLE(args[0]));
            }
            else if (IS_BOOL(args[0]))
            {
                str = AS_BOOL(args[0]) ? "true" : "false";
            }
            else
            {
                str = "<unknown>";
            }

            return vm->createString(str);
        });
    }

    void SetUp() override
    {
        // Reset VM state
        vm = new VM();
        registerNativeFunctions(vm);
    }

    void TearDown() override
    {
        // Clean up
        delete vm;
    }

    /**
     * Run a script and return its output from __dprint
     */
    std::string runScript(const std::string& scriptPath, InterpretResult& result)
    {
        // Read the script file
        std::ifstream file(scriptPath);
        if (not file.is_open())
        {
            throw std::runtime_error("Failed to open script file: " + scriptPath);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();

        // Clear previous test output
        if (vm)
        {
            delete vm;
            vm = new VM();
            registerNativeFunctions(vm);
        }

        if (scriptPath.find(".pgc") != std::string::npos)
            result = vm->interpretFromBytecodeFile(scriptPath);
        else
            result = vm->interpretFromText(source, false, scriptPath + ".compiled.pgc");

        // Return captured output from __dprint
        return vm->testOutput;
    }

     /**
     * Run a script and return its output from __dprint
     */
    std::string runOptScript(const std::string& scriptPath, InterpretResult& result)
    {
        // Read the script file
        std::ifstream file(scriptPath);
        if (not file.is_open())
        {
            throw std::runtime_error("Failed to open script file: " + scriptPath);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();

        EntitySystem ecs;
        VM optimizedVM;

        ecs.setVMOptimizationLevel(VmOptimizationLevel::O3);
        ecs.setupVm(optimizedVM);

        // Clear previous test output
        if (scriptPath.find(".pgc") != std::string::npos)
            result = optimizedVM.interpretFromBytecodeFile(scriptPath);
        else
            result = optimizedVM.interpretFromText(source, false, scriptPath + ".compiled.pgc");

        // Return captured output from __dprint
        return optimizedVM.testOutput;
    }

    /**
     * Load expected output from file
     */
    std::string loadExpectedOutput(const std::string& expectedPath)
    {
        std::ifstream file(expectedPath);
        if (not file.is_open())
        {
            return ""; // No expected file means we just check for success
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    void executeScript(const std::string& scriptPath, const std::string& expectedPath, const std::string& scriptName)
    {
        InterpretResult result, compiledResult;
        std::string output = runScript(scriptPath, result);

        // Check if expected file exists
        if (std::filesystem::exists(expectedPath))
        {
            std::string expected = loadExpectedOutput(expectedPath);

            // Normalize line endings and trim
            output = trim(output);
            expected = trim(expected);

            EXPECT_EQ(output, expected)
                << "Script: " << scriptName << "\n"
                << "Output mismatch!\n"
                << "Expected:\n" << expected << "\n"
                << "Got:\n" << output;

            // Try to run the compiled bytecode version as well
            output = runScript(scriptPath + ".compiled.pgc", compiledResult);

            output = trim(output);

            EXPECT_EQ(output, expected)
                << "Script (compiled): " << scriptName << "\n"
                << "Output mismatch!\n"
                << "Expected:\n" << expected << "\n"
                << "Got:\n" << output;

            EXPECT_EQ(compiledResult, InterpretResult::OK) <<
                "Script (compiled):" << scriptName << " failed to execute";
        }

        // Always check for successful execution
        EXPECT_EQ(result, InterpretResult::OK) << "Script " << scriptName << " failed to execute";

#ifndef _WIN32
        // Todo: fix this
        // O3 optimization path is skipped on Windows/MinGW due to a known interaction
        // between longjmp and Windows SEH (RtlUnwindEx) when C++ objects are on the
        // stack above the setjmp site. Works correctly on Linux/macOS.
        output = runOptScript(scriptPath, result);

        // Always check for successful execution
        EXPECT_EQ(result, InterpretResult::OK) << "Script [O3]: " << scriptName << " failed to execute";

        // Check if expected file exists
        if (std::filesystem::exists(expectedPath))
        {
            std::string expected = loadExpectedOutput(expectedPath);

            // Normalize line endings and trim
            output = trim(output);
            expected = trim(expected);

            EXPECT_EQ(output, expected)
                << "Script [O3]: " << scriptName << "\n"
                << "Output mismatch!\n"
                << "Expected:\n" << expected << "\n"
                << "Got:\n" << output;

            // Try to run the compiled bytecode version as well
            output = runOptScript(scriptPath + ".compiled.pgc", compiledResult);

            output = trim(output);

            EXPECT_EQ(output, expected)
                << "Script [O3] (compiled): " << scriptName << "\n"
                << "Output mismatch!\n"
                << "Expected:\n" << expected << "\n"
                << "Got:\n" << output;

            EXPECT_EQ(compiledResult, InterpretResult::OK) <<
                "Script [O3] (compiled):" << scriptName << " failed to execute";
        }

        // Always check for successful execution
        EXPECT_EQ(result, InterpretResult::OK) << "Script [O3] " << scriptName << " failed to execute";
#endif
    }

    /**
     * Test a script file
     */
    void testScript(const std::string& scriptName)
    {
        std::string scriptPath = getScriptPath(scriptName);
        std::string expectedPath = getExpectedPath(scriptName);

        ASSERT_TRUE(std::filesystem::exists(scriptPath)) << "Script file not found: " << scriptPath;

        executeScript(scriptPath, expectedPath, scriptName);
    }

    /**
     * Test an example file
     */
    void testExample(const std::string& exampleFolder, const std::string& scriptName)
    {
        std::string scriptPath = getScriptPath(exampleFolder, scriptName);
        std::string expectedPath = getExpectedPath(exampleFolder, scriptName);

        ASSERT_TRUE(std::filesystem::exists(scriptPath))
            << "Script file not found: " << scriptPath;

        executeScript(scriptPath, expectedPath, scriptName);
    }

    /**
     * Test a script that should fail with a specific error
     */
    void testScriptError(const std::string& scriptName, InterpretResult expectedResult)
    {
        std::string scriptPath = getScriptPath(scriptName);

        ASSERT_TRUE(std::filesystem::exists(scriptPath))
            << "Script file not found: " << scriptPath;

        InterpretResult result;
        runScript(scriptPath, result);

        EXPECT_EQ(result, expectedResult)
            << "Script " << scriptName << " should have failed with error";
    }

private:
    std::string getScriptPath(const std::string& name)
    {
        return "test/pgcompiler/scripts/" + name + ".pg";
    }

    std::string getExpectedPath(const std::string& name)
    {
        return "test/pgcompiler/scripts/" + name + ".expected";
    }

    std::string getScriptPath(const std::string& dir, const std::string& name)
    {
        return "test/pgcompiler/examples/" + dir + "/" + name + ".pg";
    }

    std::string getExpectedPath(const std::string& dir, const std::string& name)
    {
        return "test/pgcompiler/examples/" + dir + "/" + name + ".expected";
    }

    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, last - first + 1);
    }
};

// ============================================================================
// Basic Arithmetic Tests
// ============================================================================

TEST_F(ScriptTestBench, SimpleAddition)
{
    MockLogger<TerminalSink> logger;

    testScript("simple_addition");

    LOG_INFO("Test", "Completed simple addition test");
}

TEST_F(ScriptTestBench, SimpleSubtraction)
{
    testScript("simple_subtraction");
}

TEST_F(ScriptTestBench, SimpleMultiplication)
{
    testScript("simple_multiplication");
}

TEST_F(ScriptTestBench, SimpleDivision)
{
    testScript("simple_division");
}

// ============================================================================
// Boolean Operations
// ============================================================================

TEST_F(ScriptTestBench, BooleanLiterals)
{
    testScript("boolean_literals");
}

TEST_F(ScriptTestBench, BooleanNot)
{
    testScript("boolean_not");
}

// ============================================================================
// Comparison Operations
// ============================================================================

TEST_F(ScriptTestBench, EqualityComparison)
{
    testScript("equality_comparison");
}

TEST_F(ScriptTestBench, InequalityComparison)
{
    testScript("inequality_comparison");
}

TEST_F(ScriptTestBench, LessThanComparison)
{
    testScript("less_than_comparison");
}

TEST_F(ScriptTestBench, GreaterThanComparison)
{
    testScript("greater_than_comparison");
}

TEST_F(ScriptTestBench, ComparisonMixed)
{
    testScript("comparison_mixed");
}

// ============================================================================
// Logical Operators
// ============================================================================

TEST_F(ScriptTestBench, LogicalAnd)
{
    testScript("logical_and");
}

TEST_F(ScriptTestBench, LogicalOr)
{
    testScript("logical_or");
}

TEST_F(ScriptTestBench, LogicalCombinations)
{
    testScript("logical_combinations");
}

// ============================================================================
// Unary Operators
// ============================================================================

TEST_F(ScriptTestBench, UnaryNegation)
{
    testScript("unary_negation");
}

TEST_F(ScriptTestBench, UnaryComplex)
{
    testScript("unary_complex");
}

// ============================================================================
// Variables
// ============================================================================

TEST_F(ScriptTestBench, VariableDeclaration)
{
    testScript("variable_declaration");
}

TEST_F(ScriptTestBench, VariableAssignment)
{
    testScript("variable_assignment");
}

TEST_F(ScriptTestBench, MultipleVariables)
{
    testScript("multiple_variables");
}

TEST_F(ScriptTestBench, LocalScope)
{
    testScript("local_scope");
}

TEST_F(ScriptTestBench, PrefixIncrement)
{
    testScript("prefix_increment");
}

TEST_F(ScriptTestBench, PostfixIncrement)
{
    testScript("postfix_increment");
}

TEST_F(ScriptTestBench, PrefixDecrement)
{
    testScript("prefix_decrement");
}

TEST_F(ScriptTestBench, PostfixDecrement)
{
    testScript("postfix_decrement");
}

TEST_F(ScriptTestBench, PlusMinusEqual)
{
    testScript("testPlusMinusEqual");
}

// ============================================================================
// Control Flow
// ============================================================================

TEST_F(ScriptTestBench, IfStatement)
{
    testScript("if_statement");
}

TEST_F(ScriptTestBench, IfElseStatement)
{
    testScript("if_else_statement");
}

TEST_F(ScriptTestBench, IfElseIfChain)
{
    testScript("if_else_if_chain");
}

TEST_F(ScriptTestBench, WhileLoop)
{
    testScript("while_loop");
}

TEST_F(ScriptTestBench, WhileLoopZero)
{
    testScript("while_loop_zero");
}

TEST_F(ScriptTestBench, WhileAndCondition)
{
    testScript("while_and_condition");
}

TEST_F(ScriptTestBench, WhileNested)
{
    testScript("while_nested");
}

TEST_F(ScriptTestBench, ForLoop)
{
    testScript("for_loop");
}

TEST_F(ScriptTestBench, ForLoopIncrement)
{
    testScript("for_loop_increment");
}

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

TEST_F(ScriptTestBench, ForInNestedLocal)
{
    testScript("for_in_nested_local");
}

TEST_F(ScriptTestBench, ForInWithOuterLocal)
{
    testScript("for_in_with_outer_local");
}

// ============================================================================
// Break Statement
// ============================================================================

TEST_F(ScriptTestBench, BreakWhileSimple)
{
    testScript("break_while_simple");
}

TEST_F(ScriptTestBench, BreakForSimple)
{
    testScript("break_for_simple");
}

TEST_F(ScriptTestBench, BreakImmediate)
{
    testScript("break_immediate");
}

TEST_F(ScriptTestBench, BreakNestedLoops)
{
    testScript("break_nested_loops");
}

TEST_F(ScriptTestBench, BreakWithLocals)
{
    testScript("break_with_locals");
}

TEST_F(ScriptTestBench, BreakMultipleInLoop)
{
    testScript("break_multiple_in_loop");
}

TEST_F(ScriptTestBench, BreakForIn)
{
    testScript("break_for_in");
}

TEST_F(ScriptTestBench, BreakWithReturnValue)
{
    testScript("break_with_return_value");
}

TEST_F(ScriptTestBench, BreakDeeplyNested)
{
    testScript("break_deeply_nested");
}

// ============================================================================
// Vectors
// ============================================================================

TEST_F(ScriptTestBench, VectorLeak)
{
    testScript("vector_leak");
}

// ============================================================================
// Strings
// ============================================================================

TEST_F(ScriptTestBench, StringLiterals)
{
    testScript("string_literals");
}

TEST_F(ScriptTestBench, StringIndexing)
{
    testScript("string_indexing");
}

TEST_F(ScriptTestBench, StringConcatenation)
{
    testScript("string_concatenation");
}

TEST_F(ScriptTestBench, StringInVariables)
{
    testScript("string_in_variables");
}

// ============================================================================
// Complex Expressions
// ============================================================================

TEST_F(ScriptTestBench, PrecedenceTest)
{
    testScript("precedence");
}

TEST_F(ScriptTestBench, ParenthesesTest)
{
    testScript("parentheses");
}

// ============================================================================
// Functions & Closures
// ============================================================================

TEST_F(ScriptTestBench, TestSimpleReturn)
{
    testScript("testSimpleReturn");
}

TEST_F(ScriptTestBench, TestFunc)
{
    testScript("testFunc");
}

TEST_F(ScriptTestBench, TestIncr)
{
    testScript("testIncr");
}

TEST_F(ScriptTestBench, TestFib)
{
    testScript("testFib");
}

TEST_F(ScriptTestBench, TestSimpleClosure)
{
    testScript("testSimpleClosure");
}

TEST_F(ScriptTestBench, TestClosedClosure)
{
    testScript("testClosedClosure");
}

TEST_F(ScriptTestBench, TestClosure)
{
    testScript("testClosure");
}

TEST_F(ScriptTestBench, TestClosureUpvaluePayload)
{
    testScript("testClosureUpvaluePayload");
}

// ============================================================================
// Classes & Objects
// ============================================================================

TEST_F(ScriptTestBench, TestClass)
{
    testScript("testClass");
}

TEST_F(ScriptTestBench, TestClassLocal)
{
    testScript("testClassLocal");
}

TEST_F(ScriptTestBench, TestClassPostIncr)
{
    testScript("testClassPostIncr");
}

TEST_F(ScriptTestBench, TestInstance)
{
    testScript("testInstance");
}

TEST_F(ScriptTestBench, TestMethods)
{
    testScript("testMethods");
}

TEST_F(ScriptTestBench, TestBound)
{
    testScript("testBound");
}

TEST_F(ScriptTestBench, TestProperties)
{
    testScript("testProperties");
}

TEST_F(ScriptTestBench, TestCoffee)
{
    testScript("testCoffee");
}

TEST_F(ScriptTestBench, TestOops)
{
    testScript("testOops");
}

// ============================================================================
// Function and Method Calls with Locals Tests
// ============================================================================

TEST_F(ScriptTestBench, TestFunctionWithLocals)
{
    testScript("testFunctionWithLocals");
}

TEST_F(ScriptTestBench, TestBoundMethodWithLocals)
{
    testScript("testBoundMethodWithLocals");
}

TEST_F(ScriptTestBench, TestMixedCallsWithLocals)
{
    testScript("testMixedCallsWithLocals");
}

// ============================================================================
// Loop Tests
// ============================================================================

TEST_F(ScriptTestBench, TestLoop)
{
    testScript("testLoop");
}

TEST_F(ScriptTestBench, TestLoop2)
{
    testScript("testLoop2");
}

TEST_F(ScriptTestBench, TestLoopLocal)
{
    testScript("testLoopLocal");
}

TEST_F(ScriptTestBench, TestModulo)
{
    testScript("testModulo");
}

// Performance tests (no output expected)
TEST_F(ScriptTestBench, TestBasicIncr)
{
    testScript("testBasicIncr");
}

TEST_F(ScriptTestBench, TestIncrLoop)
{
    testScript("testIncrLoop");
}

TEST_F(ScriptTestBench, TestLoopGlobal)
{
    testScript("testLoopGlobal");
}

TEST_F(ScriptTestBench, TestLoopString)
{
    testScript("testLoopString");
}

TEST_F(ScriptTestBench, TestStringReleaseRetrack)
{
    testScript("testStringReleaseRetrack");
}

// ============================================================================
// Tables Tests
// ============================================================================

//Todo correct them and add them back
// TEST_F(ScriptTestBench, TestTables)
// {
//     testScript("testTables");
// }

// TEST_F(ScriptTestBench, TestTables2)
// {
//     testScript("testTables2");
// }

TEST_F(ScriptTestBench, TestTable)
{
    testScript("test_table");
}

TEST_F(ScriptTestBench, TestLoopTable)
{
    testScript("test_loop_table");
}

TEST_F(ScriptTestBench, TestResetTable)
{
    testScript("test_reset_table");
}

// ============================================================================
// Import Tests
// ============================================================================

TEST_F(ScriptTestBench, ImportSingle)
{
    testScript("import_single");
}

TEST_F(ScriptTestBench, ImportMultiple)
{
    testScript("import_multiple");
}

TEST_F(ScriptTestBench, ImportWithVars)
{
    testScript("import_with_vars");
}

TEST_F(ScriptTestBench, ImportChained)
{
    testScript("import_chained");
}

TEST_F(ScriptTestBench, ImportNested)
{
    testScript("import_nested");
}

TEST_F(ScriptTestBench, ImportDuplicate)
{
    testScript("import_duplicate");
}

TEST_F(ScriptTestBench, ImportLocalScope)
{
    testScript("import_local_scope");
}

// Todo correct them (expected output files are wrong) and add them back
// TEST_F(ScriptTestBench, ImportCompiled)
// {
//     testScript("compiled_import");
// }

// TEST_F(ScriptTestBench, ImportNativeModule)
// {
//     testScript("test_native_math_module");
// }

// TEST_F(ScriptTestBench, ImportFileOverridesNative)
// {
//     testScript("test_file_overrides_native");
// }

// ============================================================================
// Error Tests
// ============================================================================

// Todo those make the rest of the tests fails because of the way the VM handles errors (longjmp). Need to refactor error handling to be more test-friendly before re-enabling those.

// TEST_F(ScriptTestBench, SyntaxError)
// {
//     testScriptError("syntax_error", InterpretResult::COMPILE_ERROR);
// }

// TEST_F(ScriptTestBench, RuntimeError)
// {
//     testScriptError("runtime_error", InterpretResult::RUNTIME_ERROR);
// }

// ============================================================================
// Advent of Code 2025 Tests
// ============================================================================

TEST_F(ScriptTestBench, AdventOfCode2025Day1Part1)
{
    testExample("adventofcode2025/day1", "part1");
}

TEST_F(ScriptTestBench, AdventOfCode2025Day1Part2)
{
    // MockLogger<TerminalSink> logger;

    testExample("adventofcode2025/day1", "part2");
}

TEST_F(ScriptTestBench, AdventOfCode2025Day3Part1)
{
    testExample("adventofcode2025/day3", "part1");
}

TEST_F(ScriptTestBench, AdventOfCode2025Day3Part2)
{
    testExample("adventofcode2025/day3", "part2");
}

// TEST_F(ScriptTestBench, TestFuncFailed)
// {
//     testScriptError("testFuncFailed", InterpretResult::RUNTIME_ERROR);
// }

// ============================================================================
// Metamethod Tests (__get and __set)
// ============================================================================

TEST_F(ScriptTestBench, MetamethodClassGetBasic)
{
    testScript("metamethod_class_get_basic");
}

TEST_F(ScriptTestBench, MetamethodClassSetBasic)
{
    testScript("metamethod_class_set_basic");
}

TEST_F(ScriptTestBench, MetamethodGetBasic)
{
    testScript("metamethod_get_basic");
}

TEST_F(ScriptTestBench, MetamethodSetBasic)
{
    testScript("metamethod_set_basic");
}

TEST_F(ScriptTestBench, MetamethodClassGetIndex)
{
    testScript("metamethod_class_get_index");
}

TEST_F(ScriptTestBench, MetamethodGetIndex)
{
    testScript("metamethod_get_index");
}

TEST_F(ScriptTestBench, MetamethodClassSetIndex)
{
    testScript("metamethod_class_set_index");
}

TEST_F(ScriptTestBench, MetamethodSetIndex)
{
    testScript("metamethod_set_index");
}

TEST_F(ScriptTestBench, MetamethodFieldsPriority)
{
    testScript("metamethod_fields_priority");
}

TEST_F(ScriptTestBench, MetamethodProxySimulation)
{
    testScript("metamethod_proxy_simulation");
}

// Memory Management Tests
TEST_F(ScriptTestBench, VMMemoryPoolReserve)
{
    // Test that VM can be created and destroyed without crashes
    // This specifically tests the pools.reserve() issue
    VM* testVm = new VM();
    delete testVm;

    // Create another VM to ensure pools are properly cleaned up
    testVm = new VM();
    delete testVm;
}

TEST_F(ScriptTestBench, VMMultipleCreationDestruction)
{
    // Test multiple VM creation/destruction cycles
    for (int i = 0; i < 10; i++)
    {
        VM* testVm = new VM();
        // Run a simple script
        testVm->interpretFromText("var x = 1 + 2;", false);
        delete testVm;
    }
}

TEST_F(ScriptTestBench, VMStringMemoryHandling)
{
    // Test that string creation and destruction works correctly
    VM* testVm = new VM();

    // Create various string types
    testVm->interpretFromText(R"(
        var smallStr = "abc";          // Small string (inline)
        var longStr = "this is a very long string";  // Heap string
        var str1 = "test";
        var str2 = "test";  // Should reuse interned string
    )", false);

    delete testVm;
}

// ============================================================================
// Popping Jump Optimization Pass Tests
// ============================================================================

TEST_F(ScriptTestBench, PoppingJump_BasicIf)
{
    testScript("popping_jump_tests/01_basic_if");
}

TEST_F(ScriptTestBench, PoppingJump_IfElse)
{
    testScript("popping_jump_tests/02_if_else");
}

TEST_F(ScriptTestBench, PoppingJump_NestedIf)
{
    testScript("popping_jump_tests/03_nested_if");
}

TEST_F(ScriptTestBench, PoppingJump_MultipleConditions)
{
    testScript("popping_jump_tests/04_multiple_conditions");
}

TEST_F(ScriptTestBench, PoppingJump_IfWithLoop)
{
    testScript("popping_jump_tests/05_if_with_loop");
}

TEST_F(ScriptTestBench, PoppingJump_IfWithReturn)
{
    testScript("popping_jump_tests/06_if_with_return");
}

TEST_F(ScriptTestBench, PoppingJump_ComplexExpression)
{
    testScript("popping_jump_tests/07_complex_expression");
}

TEST_F(ScriptTestBench, PoppingJump_StringBooleanOp)
{
    testScript("popping_jump_tests/08_string_boolean_op");
}

TEST_F(ScriptTestBench, PoppingJump_IfElseChain)
{
    testScript("popping_jump_tests/09_if_else_chain");
}

TEST_F(ScriptTestBench, PoppingJump_ConditionAsExpression)
{
    testScript("popping_jump_tests/10_condition_as_expression");
}

TEST_F(ScriptTestBench, PoppingJump_BreakInLoop)
{
    testScript("popping_jump_tests/11_break_in_loop");
}

TEST_F(ScriptTestBench, PoppingJump_ContinueInLoop)
{
    testScript("popping_jump_tests/12_continue_in_loop");
}

TEST_F(ScriptTestBench, PoppingJump_ComponentGeneratorPattern)
{
    testScript("popping_jump_tests/13_component_generator_pattern");
}

TEST_F(ScriptTestBench, PoppingJump_NestedStringConcat)
{
    testScript("popping_jump_tests/14_nested_string_concat");
}

TEST_F(ScriptTestBench, PoppingJump_BooleanValueFlow)
{
    testScript("popping_jump_tests/15_boolean_value_flow");
}

TEST_F(ScriptTestBench, PoppingJump_NestedIfWithCounter)
{
    testScript("popping_jump_tests/16_nested_if_with_counter");
}

// Component Generator Pattern Tests
TEST_F(ScriptTestBench, ComponentGen_CounterWithCommaInsertion)
{
    testScript("component_gen_tests/01_counter_with_comma_insertion");
}

TEST_F(ScriptTestBench, ComponentGen_LoopStringConcatWithIndex)
{
    testScript("component_gen_tests/02_loop_string_concat_with_index");
}

TEST_F(ScriptTestBench, ComponentGen_NestedLoopFieldMatching)
{
    testScript("component_gen_tests/03_nested_loop_field_matching");
}

TEST_F(ScriptTestBench, ComponentGen_SubstringQuoteStripping)
{
    testScript("component_gen_tests/04_substring_quote_stripping");
}

TEST_F(ScriptTestBench, ComponentGen_ConditionalDefaultValue)
{
    testScript("component_gen_tests/05_conditional_default_value");
}

TEST_F(ScriptTestBench, ComponentGen_TypeBasedExtractionDispatch)
{
    testScript("component_gen_tests/06_type_based_extraction_dispatch");
}

TEST_F(ScriptTestBench, ComponentGen_SetterConditionalGeneration)
{
    testScript("component_gen_tests/07_setter_conditional_generation");
}

TEST_F(ScriptTestBench, ComponentGen_MacroTypeMapping)
{
    testScript("component_gen_tests/08_macro_type_mapping");
}

TEST_F(ScriptTestBench, ComponentGen_ShortnameExtraction)
{
    testScript("component_gen_tests/09_shortname_extraction");
}

TEST_F(ScriptTestBench, ComponentGen_CompleteConstructorGeneration)
{
    testScript("component_gen_tests/10_complete_constructor_generation");
}

// YAML Parser Pattern Tests
TEST_F(ScriptTestBench, YamlParser_IndentCountingEarlyExit)
{
    testScript("yaml_parser_tests/01_indent_counting_early_exit");
}

TEST_F(ScriptTestBench, YamlParser_SplitFirstWithSubstring)
{
    testScript("yaml_parser_tests/02_split_first_with_substring");
}

TEST_F(ScriptTestBench, YamlParser_InlineArrayParsing)
{
    testScript("yaml_parser_tests/03_inline_array_parsing");
}

TEST_F(ScriptTestBench, YamlParser_InlineObjectWithQuoteTracking)
{
    testScript("yaml_parser_tests/04_inline_object_with_quote_tracking");
}

TEST_F(ScriptTestBench, YamlParser_QuoteStrippingConditional)
{
    testScript("yaml_parser_tests/05_quote_stripping_conditional");
}

TEST_F(ScriptTestBench, YamlParser_MultilineStringAccumulation)
{
    testScript("yaml_parser_tests/06_multiline_string_accumulation");
}

TEST_F(ScriptTestBench, YamlParser_BackwardQuoteSearch)
{
    testScript("yaml_parser_tests/07_backward_quote_search");
}

TEST_F(ScriptTestBench, YamlParser_EscapeSequenceHandling)
{
    testScript("yaml_parser_tests/08_escape_sequence_handling");
}

TEST_F(ScriptTestBench, YamlParser_NestedContainChecks)
{
    testScript("yaml_parser_tests/09_nested_contain_checks");
}

TEST_F(ScriptTestBench, YamlParser_StatefulCharIteration)
{
    testScript("yaml_parser_tests/10_stateful_char_iteration");
}

} // namespace test
} // namespace pg

/**
 * Entry point for the test
 */
int main(int argc, char **argv)
{
   std::cout << "Start all the tests" << std::endl;
   ::testing::InitGoogleTest( &argc, argv );
   return RUN_ALL_TESTS();
}
