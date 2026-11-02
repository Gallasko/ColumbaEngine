#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "Compiler/ast/pass/length_hoisting.h"
#include "Compiler/ast/pass/loop_invariant_hoisting.h"
#include "Compiler/ast/pass/static_loop_evaluation.h"

#include "ECS/entitysystem.h"

#include "Interpreter/lexer.h"
#include "Interpreter/parser.h"

#include "../../mocklogger.h"

#include <string>
#include <vector>

namespace pg {
namespace test {

/**
 * AST pass tests: structural checks on the transformed AST plus behavioral
 * equivalence between both front-ends with the passes active.
 */
class AstPassBase : public ::testing::Test
{
protected:
    /** Parse source, run one pass over it, and return the printed AST */
    std::string transformWith(AstPass& pass, const std::string& source, bool& changed)
    {
        Lexer lexer;
        lexer.readFromText(source);

        Parser parser(lexer.getTokens());
        auto statements = parser.parse();

        EXPECT_FALSE(parser.hasError()) << "test source failed to parse";

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

class LoopInvariantHoisting : public AstPassBase
{
protected:
    std::string transform(const std::string& source, bool& changed)
    {
        LoopInvariantHoistingPass pass;
        return transformWith(pass, source, changed);
    }
};

class LengthHoisting : public AstPassBase
{
protected:
    // Every script here runs with the functions it needs
    static std::string withImports(const std::string& source)
    {
        return "import \"algorithm\"\nimport \"string\"\n" + source;
    }

    // How many len() reads were hoisted out of the source
    long hoisted(const std::string& source)
    {
        LengthHoistingPass pass;
        bool changed = false;
        transformWith(pass, source, changed);

        return pass.hoistedReads();
    }

    // What the source prints with the passes and without them
    void expectSameRun(const std::string& source)
    {
        const std::string script = withImports(source);

        std::string outputs[2];

        for (int level = 0; level < 2; ++level)
        {
            EntitySystem ecs;
            ecs.setVMOptimizationLevel(level == 0 ? VmOptimizationLevel::O0 : VmOptimizationLevel::O3);
            ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

            VM vm;
            ecs.setupVm(vm);

            EXPECT_EQ(vm.interpretFromText(script), InterpretResult::OK) << script;

            outputs[level] = vm.testOutput;
        }

        EXPECT_EQ(outputs[0], outputs[1]) << script;
    }
};

class StaticLoopEvaluation : public AstPassBase
{
protected:
    std::string transform(const std::string& source, bool& changed)
    {
        StaticLoopEvaluationPass pass;
        return transformWith(pass, source, changed);
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

TEST_F(LengthHoisting, MeasuresARowThatDoesNotChange)
{
    MockLogger<TerminalSink> logger;

    const std::string source =
        "fun count(table)\n"
        "{\n"
        "    var res = []\n"
        "    for (var i = 0; i < len(table); i++)\n"
        "    {\n"
        "        res[i] = []\n"
        "        for (var j = 0; j < len(table[i]); j++)\n"
        "        {\n"
        "            res[i][j] = -1\n"
        "            if (j + 1 < len(table[i]) and table[i][j] == 1)\n"
        "            {\n"
        "                res[i][j] += 1\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "    return len(res)\n"
        "}\n"
        "__dprint(count([[1, 0], [1, 1, 1]]))\n";

    EXPECT_EQ(hoisted(source), 2);
    expectSameRun(source);
}

TEST_F(LengthHoisting, AnOwnedListIsBeyondOtherWrites)
{
    MockLogger<TerminalSink> logger;

    const std::string source =
        "fun clear(table)\n"
        "{\n"
        "    var res = [[1, 1], [1]]\n"
        "    for (var i = 0; i < len(res); i++)\n"
        "    {\n"
        "        for (var j = 0; j < len(res[i]); j++)\n"
        "        {\n"
        "            if (res[i][j] < 4)\n"
        "            {\n"
        "                table[i][j] = 0\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}\n"
        "var t = [[5, 5], [5]]\n"
        "clear(t)\n"
        "__dprint(t[0][1])\n";

    EXPECT_EQ(hoisted(source), 2);
    expectSameRun(source);
}

TEST_F(LengthHoisting, AStringIsBeyondWrites)
{
    MockLogger<TerminalSink> logger;

    const std::string source =
        "for (var file : [\"ab\", \"abc\"])\n"
        "{\n"
        "    var f = file + \"d\"\n"
        "    var table = [[]]\n"
        "    for (var i = 0; i < len(f); i++)\n"
        "    {\n"
        "        table[0][i] = 1\n"
        "    }\n"
        "    __dprint(len(table[0]))\n"
        "}\n";

    EXPECT_EQ(hoisted(source), 1);
    expectSameRun(source);
}

TEST_F(LengthHoisting, LeavesWhatMayChange)
{
    MockLogger<TerminalSink> logger;

    const std::vector<std::string> sources = {
        // The row measured is written
        "fun f(t, i)\n{\n    for (var j = 0; j < len(t[i]); j++)\n    {\n        if (j < 3)\n        {\n            t[i][j + 1] = 0\n        }\n    }\n    return len(t[i])\n}\n__dprint(f([[1]], 0))\n",

        // The row measured changes with the loop
        "fun f(t)\n{\n    var n = 0\n    for (var i = 0; n < len(t[i]); i++)\n    {\n        n = n + 1\n    }\n    return n\n}\n__dprint(f([[1, 2], [1, 2, 3], [1]]))\n",

        // The measured index moves
        "fun f(t)\n{\n    var k = 0\n    var n = 0\n    for (var i = 0; i < len(t[k]); i++)\n    {\n        n = n + 1\n        if (n < 2)\n        {\n            k = 1\n        }\n    }\n    return n\n}\n__dprint(f([[1, 2], [1, 2, 3, 4]]))\n",

        // A row of an owned list that is another's
        "fun f(t)\n{\n    var res = []\n    res[0] = t\n    for (var i = 0; i < len(t); i++)\n    {\n        if (i < 3)\n        {\n            res[0][i + 1] = 0\n        }\n    }\n    return len(t)\n}\n__dprint(f([1]))\n",

        // A row of an owned list given away
        "fun f()\n{\n    var res = [[1]]\n    var row = res[0]\n    for (var i = 0; i < len(res[0]); i++)\n    {\n        if (i < 3)\n        {\n            row[i + 1] = 0\n        }\n    }\n    return len(res[0])\n}\n__dprint(f())\n",

        // Iterated: the loop's variable is a row
        "fun f()\n{\n    var res = [[1]]\n    for (var row : res)\n    {\n        for (var i = 0; i < len(res[0]); i++)\n        {\n            if (i < 3)\n            {\n                row[i + 1] = 0\n            }\n        }\n    }\n    return len(res[0])\n}\n__dprint(f())\n",

        // Popped: the row left the list
        "fun f()\n{\n    var res = [[1], [2]]\n    var row = pop(res)\n    var k = 0\n    for (var i = 0; i < len(res[k]); i++)\n    {\n        if (i < 3)\n        {\n            row[i + 1] = 0\n        }\n    }\n    return len(row)\n}\n__dprint(f())\n",

        // An owned list written by name
        "fun f(t)\n{\n    var res = [1]\n    for (var i = 0; i < len(res); i++)\n    {\n        t[0] = 2\n        if (i < 3)\n        {\n            push(res, 1)\n        }\n    }\n    return len(res)\n}\n__dprint(f([1]))\n",

        // A name that holds a list on some path
        "fun f(c)\n{\n    var s = \"ab\"\n    if (c)\n    {\n        s = [1]\n    }\n    for (var i = 0; i < len(s); i++)\n    {\n        if (i < 3)\n        {\n            s[i + 1] = 0\n        }\n    }\n    return len(s)\n}\n__dprint(f(true))\n",

        // A script function behind a native's name
        "fun trim(x)\n{\n    return [1]\n}\nfun f()\n{\n    var s = trim(\"a\")\n    var o = s\n    for (var i = 0; i < len(s); i++)\n    {\n        if (i < 3)\n        {\n            o[i + 1] = 0\n        }\n    }\n    return len(s)\n}\n__dprint(f())\n",
    };

    for (const auto& source : sources)
    {
        EXPECT_EQ(hoisted(source), 0) << source;
        expectSameRun(source);
    }
}

TEST_F(StaticLoopEvaluation, FoldsFullyStaticForLoop)
{
    MockLogger<TerminalSink> logger;

    bool changed = false;
    auto printed = transform(
        "var total = 0\n"
        "for (var i = 0; i < 10; i++) total += i;\n", changed);

    EXPECT_TRUE(changed);
    // Loop gone, final value assigned (sum 0..9 = 45)
    EXPECT_EQ(printed.find("For statement"), std::string::npos) << printed;
    EXPECT_NE(printed.find("45"), std::string::npos) << printed;
}

TEST_F(StaticLoopEvaluation, FoldsWhileWithOuterCounter)
{
    MockLogger<TerminalSink> logger;

    bool changed = false;
    auto printed = transform(
        "var i = 0\n"
        "var total = 0\n"
        "while (i < 5)\n"
        "{\n"
        "    total = total + i * i\n"
        "    i = i + 1\n"
        "}\n", changed);

    EXPECT_TRUE(changed);
    EXPECT_EQ(printed.find("while statement"), std::string::npos) << printed;
    // total = 0+1+4+9+16 = 30 and i = 5 both assigned
    EXPECT_NE(printed.find("30"), std::string::npos) << printed;
    EXPECT_NE(printed.find("Assign: 5"), std::string::npos) << printed;
}

TEST_F(StaticLoopEvaluation, BailsOnCallsAndUnknownVariables)
{
    MockLogger<TerminalSink> logger;

    bool changed = false;

    // Call in body
    auto printed = transform(
        "var total = 0\n"
        "for (var i = 0; i < 10; i++) total += touch();\n", changed);

    EXPECT_FALSE(changed);
    EXPECT_NE(printed.find("For statement"), std::string::npos) << printed;

    // Unknown loop bound (e.g. engine-provided global)
    printed = transform(
        "var total = 0\n"
        "for (var i = 0; i < count; i++) total += i;\n", changed);

    EXPECT_FALSE(changed);
    EXPECT_NE(printed.find("For statement"), std::string::npos) << printed;

    // __dprint inside the loop is an output side effect per iteration
    printed = transform(
        "var total = 0\n"
        "for (var i = 0; i < 3; i++)\n"
        "{\n"
        "    __dprint(i)\n"
        "    total += i\n"
        "}\n", changed);

    EXPECT_FALSE(changed);
    EXPECT_NE(printed.find("For statement"), std::string::npos) << printed;
}

TEST_F(StaticLoopEvaluation, BailsOnRunawayLoops)
{
    MockLogger<TerminalSink> logger;

    bool changed = false;
    auto printed = transform(
        "var i = 0\n"
        "while (i < 100000000) i += 1;\n", changed);

    // Over the step budget: left for runtime
    EXPECT_FALSE(changed);
    EXPECT_NE(printed.find("while statement"), std::string::npos) << printed;
}

TEST_F(StaticLoopEvaluation, BehaviorUnchangedWithFolding)
{
    MockLogger<TerminalSink> logger;

    // The motivating example
    expectFrontEndsAgree(
        "var total = 0\n"
        "for (var i = 0; i < 10; i++) total += i;\n"
        "__dprint(total)\n");

    // Nested static loops with break/continue and integer division
    expectFrontEndsAgree(
        "var total = 0\n"
        "for (var i = 0; i < 20; i++)\n"
        "{\n"
        "    if (i % 3 == 0) { continue; }\n"
        "    if (i > 14) { break; }\n"
        "    total = total + i / 2\n"
        "}\n"
        "__dprint(total)\n");

    // Folded loop feeding a later dynamic use
    expectFrontEndsAgree(
        "var acc = 1\n"
        "var n = 0\n"
        "while (n < 12)\n"
        "{\n"
        "    acc = acc * 2\n"
        "    n = n + 1\n"
        "}\n"
        "__dprint(acc)\n"
        "__dprint(n)\n");
}

} // namespace test
} // namespace pg
