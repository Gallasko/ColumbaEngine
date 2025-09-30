#pragma once

#include "compiler_test_base.h"
#include "bytecode_pass.h"
#include "bytecode_rewriter.h"
#include "constant_uniformity_pass.h"
#include "compiler_debug.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <filesystem>
#include <fstream>

namespace pg {
namespace test {

/**
 * Pass combination configuration for testing
 */
struct PassCombination {
    std::string name;
    std::vector<std::string> passNames;
    std::string description;
};

/**
 * Enhanced base test fixture for all optimization pass tests
 * Provides:
 * - Automatic test discovery from examples/optimization/
 * - Individual pass testing
 * - Predefined pass combination testing
 * - Dynamic CTest generation for granular failure isolation
 */
class OptimizationTestBase : public CompilerTestBase {
protected:
    void SetUp() override {
        CompilerTestBase::SetUp();
        rewriter = std::make_unique<BytecodeRewriter>();
        uniformityPass = std::make_unique<ConstantUniformityPass>();
        setupPredefinedCombinations();
    }

    /**
     * Setup predefined pass combinations for testing
     * Add your common pass sequences here
     */
    void setupPredefinedCombinations() {
        // Common optimization pipeline
        predefinedCombinations["standard"] = {
            "standard", 
            {"ConstantPropagation", "LongJumpOptimization"}, 
            "Standard optimization pipeline"
        };
        
        // Aggressive optimization
        predefinedCombinations["aggressive"] = {
            "aggressive", 
            {"ConstantPropagation", "LongJumpOptimization", "LoopSimplification"}, 
            "Aggressive optimization with loop passes"
        };
        
        // Conservative optimization (safe passes only)
        predefinedCombinations["conservative"] = {
            "conservative", 
            {"ConstantUniformity", "ConstantPropagation"}, 
            "Conservative optimization focusing on constants"
        };
    }

    /**
     * Register a pass for testing - call this in derived test SetUp()
     */
    void registerPass(const std::string& name, std::unique_ptr<BytecodePass> pass) {
        registeredPasses[name] = std::move(pass);
    }

    /**
     * Get all example files from examples/optimization/ directory
     */
    std::vector<std::string> discoverExampleFiles() {
        std::vector<std::string> files;
        std::string examplesPath = "examples/optimization";
        
        try {
            if (std::filesystem::exists(examplesPath)) {
                for (const auto& entry : std::filesystem::directory_iterator(examplesPath)) {
                    if (entry.path().extension() == ".pg") {
                        files.push_back(entry.path().filename().string());
                    }
                }
                std::sort(files.begin(), files.end());
            }
        } catch (const std::exception& e) {
            // Fallback to hardcoded list if filesystem access fails
            files = {
                "01_simple_constants.pg",
                "02_local_variables.pg", 
                "03_variable_reassignment.pg",
                "04_mixed_scopes.pg",
                "05_control_flow.pg",
                "06_simple_loops.pg",
                "07_arithmetic_expressions.pg",
                "08_nested_scopes.pg",
                "09_boolean_logic.pg",
                "10_complex_scenario.pg"
            };
        }
        
        return files;
    }

    /**
     * Read content from an example file
     */
    std::string readExampleFile(const std::string& filename) {
        std::ifstream file("examples/optimization/" + filename);
        if (!file.is_open()) {
            return "";
        }

        std::string content;
        std::string line;
        while (std::getline(file, line)) {
            // Skip comment lines for cleaner execution
            if (line.empty() || line.substr(0, 2) == "//") {
                continue;
            }
            content += line + "\n";
        }
        return content;
    }

    /**
     * Standard optimization test pattern:
     * 1. Compile code without optimization
     * 2. Apply optimization passes
     * 3. Verify program semantics are preserved (same output)
     * 4. Optionally verify optimization occurred
     */
    void testOptimizationPreservesSemantics(
        BytecodePass* pass,
        const std::vector<std::string>& testCases,
        bool expectModifications = true,
        bool verbose = false
    ) {
        for (const auto& code : testCases) {
            if (verbose) {
                std::cout << "\n=== Testing: " << code << " ===" << std::endl;
            }

            // Compile without optimization
            auto originalChunk = compileExpression(code);

            if (verbose) {
                std::cout << "Original bytecode:" << std::endl;
                disassembleChunk(originalChunk, "Original");
            }

            // Execute original and capture output
            vm.testOutput.clear();
            auto originalResult = executeChunk(originalChunk);
            std::string originalOutput = vm.testOutput;

            // Create copy for optimization
            auto optimizedChunk = originalChunk;

            // Apply uniformity pass first (standard preprocessing)
            uniformityPass->runPass(optimizedChunk, rewriter.get());

            // Apply the specific optimization pass
            bool modified = pass->runPass(optimizedChunk, rewriter.get());

            if (verbose) {
                std::cout << "Optimized bytecode (modified: " << modified << "):" << std::endl;
                disassembleChunk(optimizedChunk, "Optimized");
            }

            // Execute optimized and capture output
            vm.testOutput.clear();
            auto optimizedResult = executeChunk(optimizedChunk);
            std::string optimizedOutput = vm.testOutput;

            // Verify execution success
            EXPECT_EQ(originalResult, InterpretResult::OK)
                << "Original code should execute successfully: " << code;
            EXPECT_EQ(optimizedResult, InterpretResult::OK)
                << "Optimized code should execute successfully: " << code;

            // MOST IMPORTANT: Verify semantics are preserved
            EXPECT_EQ(originalOutput, optimizedOutput)
                << "Optimization must preserve program semantics!\n"
                << "Code: " << code << "\n"
                << "Original output: '" << originalOutput << "'\n"
                << "Optimized output: '" << optimizedOutput << "'";

            // Optionally verify optimization occurred
            if (expectModifications) {
                // Note: We don't require modification for every test case,
                // as some may not be optimizable
            }
        }
    }

    /**
     * Test optimization with single code string
     */
    void testSingleOptimization(
        BytecodePass* pass,
        const std::string& code,
        bool expectModification = true,
        bool verbose = false
    ) {
        testOptimizationPreservesSemantics(pass, {code}, expectModification, verbose);
    }

    /**
     * Test with a sequence of passes (pass combination)
     */
    void testPassCombination(
        const std::vector<BytecodePass*>& passes,
        const std::string& code,
        bool verbose = false,
        const std::string& combinationName = "custom"
    ) {
        if (verbose) {
            std::cout << "\n=== Testing " << combinationName << " combination: " << code << " ===" << std::endl;
        }

        // Compile without optimization
        auto originalChunk = compileExpression(code);

        if (verbose) {
            std::cout << "Original bytecode:" << std::endl;
            disassembleChunk(originalChunk, "Original");
        }

        // Execute original and capture output
        vm.testOutput.clear();
        auto originalResult = executeChunk(originalChunk);
        std::string originalOutput = vm.testOutput;

        // Create copy for optimization
        auto optimizedChunk = originalChunk;

        // Apply uniformity pass first (standard preprocessing)
        uniformityPass->runPass(optimizedChunk, rewriter.get());

        // Apply all passes in sequence
        for (size_t i = 0; i < passes.size(); ++i) {
            bool modified = passes[i]->runPass(optimizedChunk, rewriter.get());
            if (verbose) {
                std::cout << "After pass " << (i+1) << " (" << passes[i]->getName() 
                          << "), modified: " << modified << std::endl;
                disassembleChunk(optimizedChunk, "After " + passes[i]->getName());
            }
        }

        // Execute optimized and capture output
        vm.testOutput.clear();
        auto optimizedResult = executeChunk(optimizedChunk);
        std::string optimizedOutput = vm.testOutput;

        // Verify execution success
        EXPECT_EQ(originalResult, InterpretResult::OK)
            << "Original code should execute successfully: " << code;
        EXPECT_EQ(optimizedResult, InterpretResult::OK)
            << "Optimized code should execute successfully with " << combinationName << " combination: " << code;

        // MOST IMPORTANT: Verify semantics are preserved
        EXPECT_EQ(originalOutput, optimizedOutput)
            << "Pass combination '" << combinationName << "' must preserve program semantics!\n"
            << "Code: " << code << "\n"
            << "Original output: '" << originalOutput << "'\n"
            << "Optimized output: '" << optimizedOutput << "'";
    }

    /**
     * Test all individual passes against all example files
     */
    void testAllIndividualPasses(bool verbose = false) {
        auto exampleFiles = discoverExampleFiles();
        
        for (const auto& filename : exampleFiles) {
            auto code = readExampleFile(filename);
            if (code.empty()) continue;

            if (verbose) {
                std::cout << "\n=== Testing " << filename << " with individual passes ===" << std::endl;
            }

            for (const auto& [passName, pass] : registeredPasses) {
                if (verbose) {
                    std::cout << "Testing " << filename << " with " << passName << std::endl;
                }
                testSingleOptimization(pass.get(), code, false, false);
            }
        }
    }

    /**
     * Test all predefined pass combinations against all example files
     */
    void testAllPredefinedCombinations(bool verbose = false) {
        auto exampleFiles = discoverExampleFiles();
        
        for (const auto& filename : exampleFiles) {
            auto code = readExampleFile(filename);
            if (code.empty()) continue;

            for (const auto& [comboName, combo] : predefinedCombinations) {
                if (verbose) {
                    std::cout << "\n=== Testing " << filename << " with " << comboName << " combination ===" << std::endl;
                }

                // Convert pass names to pass pointers
                std::vector<BytecodePass*> passPointers;
                for (const auto& passName : combo.passNames) {
                    auto it = registeredPasses.find(passName);
                    if (it != registeredPasses.end()) {
                        passPointers.push_back(it->second.get());
                    } else if (verbose) {
                        std::cout << "Warning: Pass '" << passName << "' not found in registered passes" << std::endl;
                    }
                }

                if (!passPointers.empty()) {
                    testPassCombination(passPointers, code, false, combo.name);
                }
            }
        }
    }

    /**
     * Generate individual test for specific example file and pass
     * Used by macro-generated tests for granular failure isolation
     */
    void testSpecificFileAndPass(const std::string& filename, const std::string& passName, bool verbose = false) {
        auto code = readExampleFile(filename);
        ASSERT_FALSE(code.empty()) << "Could not read example file: " << filename;

        auto it = registeredPasses.find(passName);
        ASSERT_NE(it, registeredPasses.end()) << "Pass not found: " << passName;

        testSingleOptimization(it->second.get(), code, false, verbose);
    }

    /**
     * Generate individual test for specific example file and pass combination
     */
    void testSpecificFileAndCombination(const std::string& filename, const std::string& combinationName, bool verbose = false) {
        auto code = readExampleFile(filename);
        ASSERT_FALSE(code.empty()) << "Could not read example file: " << filename;

        auto it = predefinedCombinations.find(combinationName);
        ASSERT_NE(it, predefinedCombinations.end()) << "Combination not found: " << combinationName;

        // Convert pass names to pass pointers
        std::vector<BytecodePass*> passPointers;
        for (const auto& passName : it->second.passNames) {
            auto passIt = registeredPasses.find(passName);
            if (passIt != registeredPasses.end()) {
                passPointers.push_back(passIt->second.get());
            }
        }

        ASSERT_FALSE(passPointers.empty()) << "No valid passes found for combination: " << combinationName;
        testPassCombination(passPointers, code, verbose, it->second.name);
    }

    /**
     * Standard test cases for most optimization passes
     */
    std::vector<std::string> getStandardTestCases() {
        return {
            // Basic constant tests
            "var x = 42; __dprint(x)",
            "var flag = true; __dprint(flag)",
            "var flag = false; __dprint(flag)",
            "var pi = 3.14; __dprint(pi)",

            // Local variable tests
            "{ var x = 42; __dprint(x); }",
            "{ var flag = true; __dprint(flag); }",

            // Arithmetic with constants
            "var x = 5; __dprint(x + 1)",
            "var y = 10; __dprint(y * 2)",

            // Multiple variables
            "var a = 10; var b = 20; __dprint(a); __dprint(b)",

            // Control flow
            "var x = 5; if (true) { __dprint(x); }",
            "var x = 5; if (false) { __dprint(x); } else { __dprint(x + 1); }",

            // Loops
            "var i = 0; while (i < 3) { __dprint(i); i++; }",
            "for (var j = 0; j < 2; j++) { __dprint(j); }"
        };
    }

    std::unique_ptr<BytecodeRewriter> rewriter;
    std::unique_ptr<ConstantUniformityPass> uniformityPass;
    
    // Registry of optimization passes for testing
    std::map<std::string, std::unique_ptr<BytecodePass>> registeredPasses;
    
    // Predefined pass combinations for testing
    std::map<std::string, PassCombination> predefinedCombinations;
};

/**
 * Macro to generate individual test cases for each file/pass combination
 * Usage in test files:
 * 
 * class MyOptimizationTest : public OptimizationTestBase { ... };
 * GENERATE_OPTIMIZATION_TESTS(MyOptimizationTest)
 */
#define GENERATE_OPTIMIZATION_TESTS(TestClass) \
    TEST_F(TestClass, AllIndividualPasses) { \
        testAllIndividualPasses(false); \
    } \
    \
    TEST_F(TestClass, AllPredefinedCombinations) { \
        testAllPredefinedCombinations(false); \
    } \
    \
    TEST_F(TestClass, IndividualTest_01_simple_constants_ConstantPropagation) { \
        testSpecificFileAndPass("01_simple_constants.pg", "ConstantPropagation", false); \
    } \
    \
    TEST_F(TestClass, IndividualTest_01_simple_constants_LongJumpOptimization) { \
        testSpecificFileAndPass("01_simple_constants.pg", "LongJumpOptimization", false); \
    } \
    \
    TEST_F(TestClass, IndividualTest_02_local_variables_ConstantPropagation) { \
        testSpecificFileAndPass("02_local_variables.pg", "ConstantPropagation", false); \
    } \
    \
    TEST_F(TestClass, IndividualTest_02_local_variables_LongJumpOptimization) { \
        testSpecificFileAndPass("02_local_variables.pg", "LongJumpOptimization", false); \
    } \
    \
    TEST_F(TestClass, IndividualTest_03_variable_reassignment_ConstantPropagation) { \
        testSpecificFileAndPass("03_variable_reassignment.pg", "ConstantPropagation", false); \
    } \
    \
    TEST_F(TestClass, IndividualTest_03_variable_reassignment_LongJumpOptimization) { \
        testSpecificFileAndPass("03_variable_reassignment.pg", "LongJumpOptimization", false); \
    } \
    \
    TEST_F(TestClass, IndividualTest_04_mixed_scopes_ConstantPropagation) { \
        testSpecificFileAndPass("04_mixed_scopes.pg", "ConstantPropagation", false); \
    } \
    \
    TEST_F(TestClass, IndividualTest_04_mixed_scopes_LongJumpOptimization) { \
        testSpecificFileAndPass("04_mixed_scopes.pg", "LongJumpOptimization", false); \
    } \
    \
    TEST_F(TestClass, IndividualTest_05_control_flow_ConstantPropagation) { \
        testSpecificFileAndPass("05_control_flow.pg", "ConstantPropagation", false); \
    } \
    \
    TEST_F(TestClass, IndividualTest_05_control_flow_LongJumpOptimization) { \
        testSpecificFileAndPass("05_control_flow.pg", "LongJumpOptimization", false); \
    } \
    \
    TEST_F(TestClass, CombinationTest_01_simple_constants_standard) { \
        testSpecificFileAndCombination("01_simple_constants.pg", "standard", false); \
    } \
    \
    TEST_F(TestClass, CombinationTest_05_control_flow_standard) { \
        testSpecificFileAndCombination("05_control_flow.pg", "standard", false); \
    } \
    \
    TEST_F(TestClass, CombinationTest_10_complex_scenario_aggressive) { \
        testSpecificFileAndCombination("10_complex_scenario.pg", "aggressive", false); \
    }

} // namespace test
} // namespace pg