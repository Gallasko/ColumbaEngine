#pragma once

#include "compiler_test_base.h"
#include "bytecode_pass.h"
#include "bytecode_rewriter.h"
#include "constant_uniformity_pass.h"
#include "compiler_debug.h"
#include <memory>
#include <vector>
#include <string>

namespace pg {
namespace test {

/**
 * Base test fixture for all optimization pass tests
 * Provides standardized testing pattern: compare original vs optimized output
 */
class OptimizationTestBase : public CompilerTestBase {
protected:
    void SetUp() override {
        CompilerTestBase::SetUp();
        rewriter = std::make_unique<BytecodeRewriter>();
        uniformityPass = std::make_unique<ConstantUniformityPass>();
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
};

} // namespace test
} // namespace pg