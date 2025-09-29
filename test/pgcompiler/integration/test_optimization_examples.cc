#include <gtest/gtest.h>
#include "optimization_test_base.h"
#include "constant_propagation_pass.h"
#include "constant_uniformity_pass.h"
#include "long_jump_optimization_pass.h"
#include <fstream>

using namespace pg;

class OptimizationExamplesTest : public pg::test::OptimizationTestBase {
protected:
    void SetUp() override {
        pg::test::OptimizationTestBase::SetUp();
        constantPropagationPass = std::make_unique<ConstantPropagationPass>();
        longJumpPass = std::make_unique<LongJumpOptimizationPass>();
    }

    // Helper to read example files
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

    std::unique_ptr<ConstantPropagationPass> constantPropagationPass;
    std::unique_ptr<LongJumpOptimizationPass> longJumpPass;
};

TEST_F(OptimizationExamplesTest, SimpleConstants) {
    auto code = readExampleFile("01_simple_constants.pg");
    ASSERT_FALSE(code.empty()) << "Could not read example file";

    testSingleOptimization(constantPropagationPass.get(), code, false, true);
}

TEST_F(OptimizationExamplesTest, LocalVariables) {
    auto code = readExampleFile("02_local_variables.pg");
    ASSERT_FALSE(code.empty()) << "Could not read example file";

    testSingleOptimization(constantPropagationPass.get(), code, false, true);
}

TEST_F(OptimizationExamplesTest, VariableReassignment) {
    auto code = readExampleFile("03_variable_reassignment.pg");
    ASSERT_FALSE(code.empty()) << "Could not read example file";

    testSingleOptimization(constantPropagationPass.get(), code, false, true);
}

TEST_F(OptimizationExamplesTest, MixedScopes) {
    auto code = readExampleFile("04_mixed_scopes.pg");
    ASSERT_FALSE(code.empty()) << "Could not read example file";

    testSingleOptimization(constantPropagationPass.get(), code, false, true);
}

TEST_F(OptimizationExamplesTest, ControlFlow) {
    auto code = readExampleFile("05_control_flow.pg");
    ASSERT_FALSE(code.empty()) << "Could not read example file";

    // Test with both constant propagation and long jump optimization
    testSingleOptimization(constantPropagationPass.get(), code, false, true);
    testSingleOptimization(longJumpPass.get(), code, false, true);
}

TEST_F(OptimizationExamplesTest, SimpleLoops) {
    auto code = readExampleFile("06_simple_loops.pg");
    ASSERT_FALSE(code.empty()) << "Could not read example file";

    testSingleOptimization(constantPropagationPass.get(), code, false, true);
}

TEST_F(OptimizationExamplesTest, ArithmeticExpressions) {
    auto code = readExampleFile("07_arithmetic_expressions.pg");
    ASSERT_FALSE(code.empty()) << "Could not read example file";

    testSingleOptimization(constantPropagationPass.get(), code, false, true);
}

TEST_F(OptimizationExamplesTest, NestedScopes) {
    auto code = readExampleFile("08_nested_scopes.pg");
    ASSERT_FALSE(code.empty()) << "Could not read example file";

    testSingleOptimization(constantPropagationPass.get(), code, false, true);
}

TEST_F(OptimizationExamplesTest, BooleanLogic) {
    auto code = readExampleFile("09_boolean_logic.pg");
    ASSERT_FALSE(code.empty()) << "Could not read example file";

    testSingleOptimization(constantPropagationPass.get(), code, false, true);
}

TEST_F(OptimizationExamplesTest, ComplexScenario) {
    auto code = readExampleFile("10_complex_scenario.pg");
    ASSERT_FALSE(code.empty()) << "Could not read example file";

    testSingleOptimization(constantPropagationPass.get(), code, false, true);
}

TEST_F(OptimizationExamplesTest, AllExamplesWithAllPasses) {
    // Test all examples with all optimization passes to ensure compatibility
    std::vector<std::string> exampleFiles = {
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

    for (const auto& filename : exampleFiles) {
        auto code = readExampleFile(filename);
        ASSERT_FALSE(code.empty()) << "Could not read " << filename;

        std::cout << "\n=== Testing " << filename << " with all passes ===" << std::endl;

        // Test each pass individually
        testSingleOptimization(constantPropagationPass.get(), code, false, false);
        testSingleOptimization(longJumpPass.get(), code, false, false);

        std::cout << filename << " passed all optimization tests" << std::endl;
    }
}