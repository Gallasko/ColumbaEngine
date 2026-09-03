#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "Compiler/ast/pass/loop_invariant_hoisting.h"

#include "ECS/entitysystem.h"

#include "Interpreter/lexer.h"
#include "Interpreter/parser.h"

#include "../../mocklogger.h"

#include <string>

namespace pg {
namespace test {

/**
 * Loop-invariant hoisting: structural checks on the transformed AST plus
 * behavioral equivalence between both front-ends with the pass active.
 */
class LoopInvariantHoisting : public ::testing::Test
{
protected:
    /** Parse source and run the hoisting pass; returns the printed AST */
    std::string transform(const std::string& source, bool& changed)
    {
        Lexer lexer;
        lexer.readFromText(source);

        Parser parser(lexer.getTokens());
        auto statements = parser.parse();

        EXPECT_FALSE(parser.hasError()) << "test source failed to parse";

        LoopInvariantHoistingPass pass;
        changed = pass.runPass(nullptr, statements);

        std::string printed;
        while (not statements.empty())
        {
            if (statements.front())
                printed += statements.front()->prettyPrint() + "\n";

            statements.pop();
        }

        return printed;
    }

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

    /** Both front-ends must produce identical results on this source */
    void expectFrontEndsAgree(const std::string& source)
    {
        InterpretResult prattResult, astResult;

        auto prattOut = runWith(source, ScriptFrontEnd::Pratt, prattResult);
        auto astOut = runWith(source, ScriptFrontEnd::Ast, astResult);

        EXPECT_EQ(prattResult, astResult);
        EXPECT_EQ(prattOut, astOut);
    }
};

TEST_F(LoopInvariantHoisting, HoistsPureExpressionFromForLoop)
{
    MockLogger<TerminalSink> logger;

    bool changed = false;
    auto printed = transform(
        "var a = 3\n"
        "var b = 7\n"
        "var total = 0\n"
        "for (var i = 0; i < 100; i = i + 1)\n"
        "{\n"
        "    total = total + (a * b + a)\n"
        "}\n", changed);

    EXPECT_TRUE(changed);
    EXPECT_NE(printed.find("@hoist0"), std::string::npos) << printed;
}

TEST_F(LoopInvariantHoisting, SkipsLoopsContainingCalls)
{
    MockLogger<TerminalSink> logger;

    bool changed = false;
    auto printed = transform(
        "fun touch()\n"
        "{\n"
        "    1;\n"
        "}\n"
        "var a = 3\n"
        "var total = 0\n"
        "for (var i = 0; i < 100; i = i + 1)\n"
        "{\n"
        "    touch()\n"
        "    total = total + (a * a)\n"
        "}\n", changed);

    EXPECT_FALSE(changed);
    EXPECT_EQ(printed.find("@hoist"), std::string::npos) << printed;
}

TEST_F(LoopInvariantHoisting, DoesNotHoistMutatedVariables)
{
    MockLogger<TerminalSink> logger;

    bool changed = false;
    auto printed = transform(
        "var a = 3\n"
        "var total = 0\n"
        "for (var i = 0; i < 100; i = i + 1)\n"
        "{\n"
        "    a = a + 1\n"
        "    total = total + (a * a)\n"
        "}\n", changed);

    EXPECT_FALSE(changed);
    EXPECT_EQ(printed.find("@hoist"), std::string::npos) << printed;
}

TEST_F(LoopInvariantHoisting, HoistsFromWhileAndNestedLoops)
{
    MockLogger<TerminalSink> logger;

    bool changed = false;
    auto printed = transform(
        "var a = 4\n"
        "var b = 9\n"
        "var i = 0\n"
        "var total = 0\n"
        "while (i < 10)\n"
        "{\n"
        "    var j = 0\n"
        "    while (j < 10)\n"
        "    {\n"
        "        total = total + (a * b)\n"
        "        j = j + 1\n"
        "    }\n"
        "    i = i + 1\n"
        "}\n", changed);

    EXPECT_TRUE(changed);
    EXPECT_NE(printed.find("@hoist"), std::string::npos) << printed;
}

TEST_F(LoopInvariantHoisting, BehaviorUnchangedWithHoisting)
{
    MockLogger<TerminalSink> logger;

    // Hoistable invariant used inside a summation loop
    expectFrontEndsAgree(
        "var a = 3\n"
        "var b = 7\n"
        "var c = 11\n"
        "var total = 0\n"
        "for (var i = 0; i < 1000; i = i + 1)\n"
        "{\n"
        "    total = total + (a * b + c * c - a)\n"
        "}\n"
        "__dprint(total)\n");

    // Invariant in the while condition
    expectFrontEndsAgree(
        "var limit = 10 * 10\n"
        "var i = 0\n"
        "var steps = 0\n"
        "while (i < limit - 1)\n"
        "{\n"
        "    steps = steps + 1\n"
        "    i = i + 1\n"
        "}\n"
        "__dprint(steps)\n");

    // Zero-iteration loop: hoisted decls run once, result must not change
    expectFrontEndsAgree(
        "var a = 5\n"
        "var total = 0\n"
        "for (var i = 0; i < 0; i = i + 1)\n"
        "{\n"
        "    total = total + (a * a)\n"
        "}\n"
        "__dprint(total)\n");

    // Mutation through the loop must block hoisting (results depend on it)
    expectFrontEndsAgree(
        "var a = 2\n"
        "var total = 0\n"
        "for (var i = 0; i < 5; i = i + 1)\n"
        "{\n"
        "    a = a + 1\n"
        "    total = total + (a * a)\n"
        "}\n"
        "__dprint(total)\n");
}

} // namespace test
} // namespace pg
