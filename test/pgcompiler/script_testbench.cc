#include "gtest/gtest.h"
#include "vm.h"
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
class ScriptTestBench : public ::testing::Test {
protected:
    VM vm;

    void SetUp() override {
        // Reset VM state
        vm.stack.clear();
        for (auto& pair : vm.globals) {
            vm.releaseAndDelete(pair.second);
        }
        vm.globals.clear();
        vm.testOutput.clear();
    }

    void TearDown() override {
        // Clean up
        vm.stack.clear();
        for (auto& pair : vm.globals) {
            vm.releaseAndDelete(pair.second);
        }
        vm.globals.clear();
    }

    /**
     * Run a script and return its output from __dprint
     */
    std::string runScript(const std::string& scriptPath, InterpretResult& result) {
        // Read the script file
        std::ifstream file(scriptPath);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open script file: " + scriptPath);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();

        // Clear previous test output
        vm.testOutput.clear();

        // Use VM's built-in interpretFromText method
        result = vm.interpretFromText(source);

        // Return captured output from __dprint
        return vm.testOutput;
    }

    /**
     * Load expected output from file
     */
    std::string loadExpectedOutput(const std::string& expectedPath) {
        std::ifstream file(expectedPath);
        if (!file.is_open()) {
            return ""; // No expected file means we just check for success
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    /**
     * Test a script file
     */
    void testScript(const std::string& scriptName) {
        std::string scriptPath = getScriptPath(scriptName);
        std::string expectedPath = getExpectedPath(scriptName);

        ASSERT_TRUE(std::filesystem::exists(scriptPath))
            << "Script file not found: " << scriptPath;

        InterpretResult result;
        std::string output = runScript(scriptPath, result);

        // Check if expected file exists
        if (std::filesystem::exists(expectedPath)) {
            std::string expected = loadExpectedOutput(expectedPath);

            // Normalize line endings and trim
            output = trim(output);
            expected = trim(expected);

            EXPECT_EQ(output, expected)
                << "Script: " << scriptName << "\n"
                << "Output mismatch!\n"
                << "Expected:\n" << expected << "\n"
                << "Got:\n" << output;
        }

        // Always check for successful execution
        EXPECT_EQ(result, InterpretResult::OK)
            << "Script " << scriptName << " failed to execute";
    }

    /**
     * Test a script that should fail with a specific error
     */
    void testScriptError(const std::string& scriptName, InterpretResult expectedResult) {
        std::string scriptPath = getScriptPath(scriptName);

        ASSERT_TRUE(std::filesystem::exists(scriptPath))
            << "Script file not found: " << scriptPath;

        InterpretResult result;
        runScript(scriptPath, result);

        EXPECT_EQ(result, expectedResult)
            << "Script " << scriptName << " should have failed with error";
    }

private:
    std::string getScriptPath(const std::string& name) {
        return "test/pgcompiler/scripts/" + name + ".pg";
    }

    std::string getExpectedPath(const std::string& name) {
        return "test/pgcompiler/scripts/" + name + ".expected";
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

TEST_F(ScriptTestBench, SimpleAddition) {
    testScript("simple_addition");
}

TEST_F(ScriptTestBench, SimpleSubtraction) {
    testScript("simple_subtraction");
}

TEST_F(ScriptTestBench, SimpleMultiplication) {
    testScript("simple_multiplication");
}

TEST_F(ScriptTestBench, SimpleDivision) {
    testScript("simple_division");
}

// ============================================================================
// Boolean Operations
// ============================================================================

TEST_F(ScriptTestBench, BooleanLiterals) {
    testScript("boolean_literals");
}

TEST_F(ScriptTestBench, BooleanNot) {
    testScript("boolean_not");
}

// ============================================================================
// Comparison Operations
// ============================================================================

TEST_F(ScriptTestBench, EqualityComparison) {
    testScript("equality_comparison");
}

// ============================================================================
// Complex Expressions
// ============================================================================

TEST_F(ScriptTestBench, PrecedenceTest) {
    testScript("precedence");
}

TEST_F(ScriptTestBench, ParenthesesTest) {
    testScript("parentheses");
}

// ============================================================================
// Error Tests
// ============================================================================

TEST_F(ScriptTestBench, SyntaxError) {
    testScriptError("syntax_error", InterpretResult::COMPILE_ERROR);
}

TEST_F(ScriptTestBench, RuntimeError) {
    testScriptError("runtime_error", InterpretResult::RUNTIME_ERROR);
}

} // namespace test
} // namespace pg
