#include "gtest/gtest.h"

#include "Compiler/vm.h"

#include "ECS/entitysystem.h"

#include "../../mocklogger.h"

#include <string>

namespace pg {
namespace test {

/**
 * Control-flow inside for-in loops.
 *
 * Regression guard for the for-in desugaring bug: both front-ends emitted the
 * hidden counter increment *after* the body, but 'continue' jumped back to the
 * condition check, skipping the increment entirely. The counter never advanced
 * and any for-in loop containing 'continue' spun forever. Both the Pratt
 * (CParser) and Ast (AstCompiler) front-ends were affected; the getEntities
 * lowering pass rewrites in place to a for-in, so it inherited the bug too.
 *
 * These cases use plain table iteration so they exercise the fix independently
 * of the ECS. If the increment is skipped again, they hang instead of failing,
 * so each is kept small and every path is bounded by the table size.
 */
class LoopControlFlow : public ::testing::Test
{
protected:
    /** Run source through one front-end at O3 and return the __dprint output */
    std::string runWith(const std::string& source, ScriptFrontEnd frontEnd, InterpretResult& result)
    {
        EntitySystem ecs;
        ecs.setVMOptimizationLevel(VmOptimizationLevel::O3);
        ecs.setVMFrontEnd(frontEnd);

        VM vm;
        ecs.setupVm(vm);

        result = vm.interpretFromText(source);

        return vm.testOutput;
    }

    static std::string trim(const std::string& str)
    {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, last - first + 1);
    }

    /** Both front-ends must produce this exact output */
    void expectBothFrontEndsOutput(const std::string& source, const std::string& expected)
    {
        MockLogger<TerminalSink> logger;

        InterpretResult prattResult, astResult;

        auto prattOut = trim(runWith(source, ScriptFrontEnd::Pratt, prattResult));
        auto astOut = trim(runWith(source, ScriptFrontEnd::Ast, astResult));

        EXPECT_EQ(prattResult, InterpretResult::OK);
        EXPECT_EQ(astResult, InterpretResult::OK);
        EXPECT_EQ(prattOut, expected) << "Pratt front-end";
        EXPECT_EQ(astOut, expected) << "Ast front-end";
    }
};

// 'continue' must still advance the iterator, not re-enter the same element.
TEST_F(LoopControlFlow, ContinueAdvancesForInLoop)
{
    expectBothFrontEndsOutput(
        "var total = 0\n"
        "for (var v : [1, 2, 3, 4])\n"
        "{\n"
        "    if (v == 2) { continue; }\n"
        "    total = total + v\n"
        "}\n"
        "__dprint(total)\n",
        "8"); // 1 + 3 + 4
}

// 'continue' on the very first element still advances.
TEST_F(LoopControlFlow, ContinueOnFirstElement)
{
    expectBothFrontEndsOutput(
        "var total = 0\n"
        "for (var v : [10, 20, 30])\n"
        "{\n"
        "    if (v == 10) { continue; }\n"
        "    total = total + v\n"
        "}\n"
        "__dprint(total)\n",
        "50"); // 20 + 30
}

// 'break' still exits early.
TEST_F(LoopControlFlow, BreakExitsForInLoop)
{
    expectBothFrontEndsOutput(
        "var total = 0\n"
        "for (var v : [1, 2, 3, 4])\n"
        "{\n"
        "    if (v == 3) { break; }\n"
        "    total = total + v\n"
        "}\n"
        "__dprint(total)\n",
        "3"); // 1 + 2
}

// 'continue' and 'break' together in one loop.
TEST_F(LoopControlFlow, ContinueAndBreakForInLoop)
{
    expectBothFrontEndsOutput(
        "var total = 0\n"
        "for (var v : [1, 2, 3, 4, 5])\n"
        "{\n"
        "    if (v == 2) { continue; }\n"
        "    if (v == 4) { break; }\n"
        "    total = total + v\n"
        "}\n"
        "__dprint(total)\n",
        "4"); // 1 + 3
}

// 'continue' in an inner for-in must not disturb the outer loop's counter.
TEST_F(LoopControlFlow, ContinueInNestedForInLoops)
{
    expectBothFrontEndsOutput(
        "var total = 0\n"
        "for (var a : [1, 2])\n"
        "{\n"
        "    for (var b : [10, 20, 30])\n"
        "    {\n"
        "        if (b == 20) { continue; }\n"
        "        total = total + a + b\n"
        "    }\n"
        "}\n"
        "__dprint(total)\n",
        "86"); // a=1: 11+31=42 ; a=2: 12+32=44 ; total 86
}

} // namespace test
} // namespace pg
