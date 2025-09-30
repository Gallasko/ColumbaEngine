#include <gtest/gtest.h>
#include "optimization_test_base.h"
#include "constant_propagation_pass.h"
#include "constant_uniformity_pass.h"
#include "long_jump_optimization_pass.h"
#include "loop_simplification_pass.h"

using namespace pg;

/**
 * Enhanced optimization testing framework demonstration
 * 
 * This test class shows how to use the new OptimizationTestBase to:
 * 1. Test individual passes against all example files
 * 2. Test predefined pass combinations
 * 3. Generate granular test cases for precise failure isolation
 */
class OptimizationFrameworkTest : public pg::test::OptimizationTestBase {
protected:
    void SetUp() override {
        pg::test::OptimizationTestBase::SetUp();
        
        // Register all available optimization passes
        registerPass("ConstantPropagation", std::make_unique<ConstantPropagationPass>());
        registerPass("LongJumpOptimization", std::make_unique<LongJumpOptimizationPass>());
        registerPass("LoopSimplification", std::make_unique<LoopSimplificationPass>());
        registerPass("ConstantUniformity", std::make_unique<ConstantUniformityPass>());
    }
};

// Generate all the individual test cases using the macro
GENERATE_OPTIMIZATION_TESTS(OptimizationFrameworkTest)

// Additional comprehensive tests
TEST_F(OptimizationFrameworkTest, AllExamplesAllIndividualPasses_Verbose) {
    // This test runs all examples against all individual passes with verbose output
    // Useful for debugging when developing new passes
    testAllIndividualPasses(true);
}

TEST_F(OptimizationFrameworkTest, AllExamplesAllCombinations_Verbose) {
    // This test runs all examples against all predefined combinations with verbose output
    testAllPredefinedCombinations(true);
}

// Test specific challenging examples with verbose output for debugging
TEST_F(OptimizationFrameworkTest, ComplexScenario_AllPasses_Verbose) {
    auto code = readExampleFile("10_complex_scenario.pg");
    ASSERT_FALSE(code.empty()) << "Could not read complex scenario file";

    for (const auto& [passName, pass] : registeredPasses) {
        std::cout << "\n=== Testing complex scenario with " << passName << " ===" << std::endl;
        testSingleOptimization(pass.get(), code, false, true);
    }
}

TEST_F(OptimizationFrameworkTest, ControlFlow_StandardCombination_Verbose) {
    auto code = readExampleFile("05_control_flow.pg");
    ASSERT_FALSE(code.empty()) << "Could not read control flow file";

    // Test the standard combination with verbose output
    testSpecificFileAndCombination("05_control_flow.pg", "standard", true);
}

// Test custom pass combinations
TEST_F(OptimizationFrameworkTest, CustomPassCombination_ConstantFocused) {
    auto code = readExampleFile("01_simple_constants.pg");
    ASSERT_FALSE(code.empty()) << "Could not read simple constants file";

    // Create a custom combination focusing on constant optimization
    std::vector<BytecodePass*> constantPasses = {
        registeredPasses["ConstantUniformity"].get(),
        registeredPasses["ConstantPropagation"].get()
    };

    testPassCombination(constantPasses, code, true, "constant-focused");
}

TEST_F(OptimizationFrameworkTest, CustomPassCombination_FullPipeline) {
    auto code = readExampleFile("10_complex_scenario.pg");
    ASSERT_FALSE(code.empty()) << "Could not read complex scenario file";

    // Create a full optimization pipeline
    std::vector<BytecodePass*> fullPipeline = {
        registeredPasses["ConstantUniformity"].get(),
        registeredPasses["ConstantPropagation"].get(),
        registeredPasses["LongJumpOptimization"].get(),
        registeredPasses["LoopSimplification"].get()
    };

    testPassCombination(fullPipeline, code, true, "full-pipeline");
}

// Test that demonstrates how to add new passes and combinations
TEST_F(OptimizationFrameworkTest, DemoAddingNewPass) {
    // This is how you would add a new pass in the future:
    // 1. Create the pass
    // auto newPass = std::make_unique<MyNewOptimizationPass>();
    // 
    // 2. Register it
    // registerPass("MyNewOptimization", std::move(newPass));
    // 
    // 3. Test it against all examples
    // testAllIndividualPasses();
    // 
    // 4. Add it to predefined combinations by modifying setupPredefinedCombinations()
    
    SUCCEED() << "This test demonstrates the pattern for adding new passes";
}

// Test that shows how to isolate failures
TEST_F(OptimizationFrameworkTest, DemoFailureIsolation) {
    // When a test fails, you can run specific combinations:
    // 
    // For individual pass failures:
    // ctest -R "IndividualTest_05_control_flow_ConstantPropagation"
    // 
    // For combination failures:
    // ctest -R "CombinationTest_10_complex_scenario_aggressive"
    // 
    // For verbose debugging:
    // testSpecificFileAndPass("05_control_flow.pg", "ConstantPropagation", true);
    
    SUCCEED() << "This test demonstrates failure isolation patterns";
}

// Performance and stress testing
TEST_F(OptimizationFrameworkTest, StressTest_AllCombinations) {
    // This test ensures all passes work together without crashes
    // Useful for detecting pass interaction issues
    
    auto exampleFiles = discoverExampleFiles();
    
    for (const auto& filename : exampleFiles) {
        auto code = readExampleFile(filename);
        if (code.empty()) continue;

        // Test all predefined combinations
        for (const auto& [comboName, combo] : predefinedCombinations) {
            std::vector<BytecodePass*> passPointers;
            for (const auto& passName : combo.passNames) {
                auto it = registeredPasses.find(passName);
                if (it != registeredPasses.end()) {
                    passPointers.push_back(it->second.get());
                }
            }

            if (!passPointers.empty()) {
                // This should not crash or produce different output
                testPassCombination(passPointers, code, false, combo.name + "_stress");
            }
        }
    }
}