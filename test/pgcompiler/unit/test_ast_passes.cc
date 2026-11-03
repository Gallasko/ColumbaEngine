#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "Compiler/ast/pass/length_hoisting.h"
#include "Compiler/ast/pass/list_methods.h"
#include "Compiler/ast/pass/loop_invariant_hoisting.h"
#include "Compiler/ast/pass/row_hoisting.h"
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

class RowHoisting : public LengthHoisting
{
protected:
    // How many row reads were hoisted out of the source, by the pass alone
    long rows(const std::string& source)
    {
        RowHoistingPass pass;
        bool changed = false;
        transformWith(pass, source, changed);

        return pass.hoistedReads();
    }

    // How it ends and what it prints, at every level: a script that stops must stop just the same
    void expectSameEnd(const std::string& source)
    {
        const std::string script = withImports(source);

        InterpretResult results[3];
        std::string outputs[3];
        const VmOptimizationLevel levels[3] = {VmOptimizationLevel::O0, VmOptimizationLevel::O2, VmOptimizationLevel::O3};

        for (int level = 0; level < 3; ++level)
        {
            EntitySystem ecs;
            ecs.setVMOptimizationLevel(levels[level]);
            ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

            VM vm;
            ecs.setupVm(vm);

            results[level] = vm.interpretFromText(script);
            outputs[level] = vm.testOutput;
        }

        EXPECT_EQ(results[0], results[1]) << script;
        EXPECT_EQ(results[0], results[2]) << script;
        EXPECT_EQ(outputs[0], outputs[1]) << script;
        EXPECT_EQ(outputs[0], outputs[2]) << script;
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

TEST_F(LoopInvariantHoisting, LeavesConstantsWhereTheyAre)
{
    MockLogger<TerminalSink> logger;

    bool changed = false;
    transform(
        "var total = 0\n"
        "for (var i = 0; i < 100; i = i + 1)\n"
        "{\n"
        "    total = total + (-1)\n"
        "}\n", changed);

    EXPECT_FALSE(changed);
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

TEST_F(LengthHoisting, MeasuresAStringOnceWhileItsNameStays)
{
    MockLogger<TerminalSink> logger;

    const std::string source =
        "fun count(line, out)\n"
        "{\n"
        "    for (var i = 0; i < strlen(line); i++)\n"
        "    {\n"
        "        push(out, line[i])\n"
        "    }\n"
        "    return len(out)\n"
        "}\n"
        "__dprint(count(\"abcdefghij\", []))\n"
        "for (var line : [\"ab\", \"abcdefgh\"])\n"
        "{\n"
        "    var out = []\n"
        "    var seen = [line]\n"
        "    for (var i = 0; i < strlen(line); i++)\n"
        "    {\n"
        "        push(out, line[i])\n"
        "        seen[0] = out\n"
        "    }\n"
        "    __dprint(len(out))\n"
        "}\n";

    EXPECT_EQ(hoisted(source), 2);
    expectSameRun(source);
}

TEST_F(LengthHoisting, LeavesAStringThatMayChange)
{
    MockLogger<TerminalSink> logger;

    const std::vector<std::string> sources = {
        // Given another string in the loop
        "fun f(s)\n{\n    var n = 0\n    for (var i = 0; i < strlen(s); i++)\n    {\n        if (i == 1)\n        {\n            s = s + \"xyz\"\n        }\n        n++\n    }\n    return n\n}\n__dprint(f(\"abc\"))\n",

        // A script function may give a global another string
        "var G = \"abc\"\nfun grow()\n{\n    G = G + \"d\"\n}\nfun f()\n{\n    var n = 0\n    for (var i = 0; i < strlen(G) and i < 10; i++)\n    {\n        grow()\n        n++\n    }\n    return n\n}\n__dprint(f())\n",

        // Not a name: the row may be replaced
        "fun f(t)\n{\n    var n = 0\n    var k = 0\n    for (var i = 0; i < strlen(t[k]); i++)\n    {\n        t[0] = \"abcdef\"\n        n++\n    }\n    return n\n}\n__dprint(f([\"ab\"]))\n",
    };

    for (const auto& source : sources)
    {
        EXPECT_EQ(hoisted(source), 0) << source;
        expectSameRun(source);
    }

    // `strlen` the script's: not moved (the run itself differs at O2 and up before this pass)
    EXPECT_EQ(hoisted("fun strlen(s)\n{\n    return 3\n}\nfun f(s)\n{\n    var n = 0\n    for (var i = 0; i < strlen(s); i++)\n    {\n        n++\n    }\n    return n\n}\n__dprint(f(\"ab\"))\n"), 0);
}

TEST_F(LengthHoisting, ABlockAtTopLevelOwnsItsListsAndNotTheGlobals)
{
    MockLogger<TerminalSink> logger;

    // The lists of a block are the script's: a write into one cannot reach another
    const std::string block =
        "for (var file : [1])\n"
        "{\n"
        "    var t = []\n"
        "    var sizes = [1, 2, 3]\n"
        "    for (var r = 0; r < 3; r++)\n"
        "    {\n"
        "        t[r] = []\n"
        "        for (var k = 0; k < len(sizes); k++)\n"
        "        {\n"
        "            t[r][k] = sizes[k]\n"
        "        }\n"
        "    }\n"
        "    __dprint(len(t[2]))\n"
        "}\n";

    EXPECT_EQ(hoisted(block), 1);
    expectSameRun(block);

    // A global may be another's: the same loop on globals stays as it is
    const std::string globals =
        "var t = []\n"
        "var sizes = [1, 2, 3]\n"
        "for (var r = 0; r < 3; r++)\n"
        "{\n"
        "    t[r] = []\n"
        "    for (var k = 0; k < len(sizes); k++)\n"
        "    {\n"
        "        t[r][k] = sizes[k]\n"
        "    }\n"
        "}\n"
        "__dprint(len(t[2]))\n";

    EXPECT_EQ(hoisted(globals), 0);
    expectSameRun(globals);

    // A block's list a global holds is no longer the block's alone
    const std::string escaped =
        "var keep = 0\n"
        "for (var file : [1])\n"
        "{\n"
        "    var t = [[0]]\n"
        "    var sizes = [1, 2, 3]\n"
        "    keep = sizes\n"
        "    var n = 0\n"
        "    for (var k = 0; k < len(sizes) and k < 8; k++)\n"
        "    {\n"
        "        push(keep, 4)\n"
        "        n++\n"
        "    }\n"
        "    __dprint(n)\n"
        "}\n";

    EXPECT_EQ(hoisted(escaped), 0);
    expectSameRun(escaped);
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

TEST_F(LengthHoisting, ALocalHoldingARowLeavesItInTheList)
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
        "            var row = res[i]\n"
        "            row[j] = table[i][j] + 1\n"
        "        }\n"
        "    }\n"
        "    return res[1][2]\n"
        "}\n"
        "__dprint(count([[1, 0], [1, 1, 7]]))\n";

    EXPECT_EQ(hoisted(source), 2);
    expectSameRun(source);
}

TEST_F(LengthHoisting, ALocalHoldingARowIsThatRow)
{
    MockLogger<TerminalSink> logger;

    const std::vector<std::string> sources = {
        // Measured by the local, written by the list
        "fun f()\n{\n    var res = [[1]]\n    var row = res[0]\n    for (var i = 0; i < len(row); i++)\n    {\n        if (i < 3)\n        {\n            res[0][i + 1] = 0\n        }\n    }\n    return len(row)\n}\n__dprint(f())\n",

        // The local leaves: what it holds is another's
        "fun f(out)\n{\n    var res = [[1]]\n    var row = res[0]\n    push(out, row)\n    for (var i = 0; i < len(res[0]); i++)\n    {\n        if (i < 3)\n        {\n            out[0][i + 1] = 0\n        }\n    }\n    return len(res[0])\n}\n__dprint(f([]))\n",

        // The local holds something else on some path
        "fun f(c, t)\n{\n    var res = [[1]]\n    var row = res[0]\n    if (c)\n    {\n        row = t\n    }\n    for (var i = 0; i < len(t); i++)\n    {\n        if (i < 3)\n        {\n            row[i + 1] = 0\n        }\n    }\n    return len(t)\n}\n__dprint(f(true, [1]))\n",
    };

    for (const auto& source : sources)
    {
        EXPECT_EQ(hoisted(source), 0) << source;
        expectSameRun(source);
    }
}

TEST_F(RowHoisting, ReadsARowOnceForAllOfTheLoop)
{
    MockLogger<TerminalSink> logger;

    const std::string source =
        "fun twice(table)\n"
        "{\n"
        "    var res = []\n"
        "    for (var i = 0; i < len(table); i++)\n"
        "    {\n"
        "        res[i] = []\n"
        "        for (var j = 0; j < 2; j++)\n"
        "        {\n"
        "            res[i][j] = table[i][j] * 2\n"
        "        }\n"
        "    }\n"
        "    return res\n"
        "}\n"
        "var out = twice([[1, 2], [3, 4]])\n"
        "__dprint(out[1][1])\n";

    EXPECT_EQ(rows(source), 2);
    expectSameEnd(source);
}

TEST_F(RowHoisting, GoesToTheOutermostLoopItHoldsFor)
{
    MockLogger<TerminalSink> logger;

    const std::string source =
        "fun f(t)\n"
        "{\n"
        "    var total = 0\n"
        "    for (var i = 0; i < len(t); i++)\n"
        "    {\n"
        "        for (var a = 0; a < 2; a++)\n"
        "        {\n"
        "            for (var b = 0; b < 2; b++)\n"
        "            {\n"
        "                total = total + t[i][b]\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "    return total\n"
        "}\n"
        "__dprint(f([[1, 2], [3, 4]]))\n";

    EXPECT_EQ(rows(source), 1);
    expectSameEnd(source);
}

TEST_F(RowHoisting, MovesNoReadThatMayGoWrong)
{
    MockLogger<TerminalSink> logger;

    const std::vector<std::string> sources = {
        // The loop never turns, and t is none
        "fun f(t, i)\n{\n    var x = 0\n    for (var j = 0; j < 0; j++)\n    {\n        x = t[i]\n    }\n    return x\n}\n__dprint(f(none, 0))\n",

        // Measured, but not on the way here
        "fun f(t, c)\n{\n    if (c)\n    {\n        __dprint(len(t))\n    }\n    var x = 0\n    var k = 0\n    for (var j = 0; j < 0; j++)\n    {\n        x = t[k]\n    }\n    return x\n}\n__dprint(f(none, false))\n",

        // Measured on the side of an `and` that may not run
        "fun f(t, c)\n{\n    var x = 0\n    var k = 0\n    var seen = c and len(t) > 0\n    for (var j = 0; j < 0; j++)\n    {\n        x = t[k]\n    }\n    return seen\n}\n__dprint(f(none, false))\n",

        // A list is not read by a string, and the loop never turns
        "fun f(t)\n{\n    var n = len(t)\n    var k = \"a\"\n    var x = 0\n    for (var j = 0; j < 0; j++)\n    {\n        x = t[k]\n    }\n    return x + n\n}\n__dprint(f([1]))\n",

        // Nor by a float
        "fun f(t)\n{\n    var n = len(t)\n    var k = 1.5\n    var x = 0\n    for (var j = 0; j < 0; j++)\n    {\n        x = t[k]\n    }\n    return x + n\n}\n__dprint(f([1]))\n",

        // A key that is an integer only until it is not
        "fun f(t)\n{\n    var n = len(t)\n    var k = 0\n    k = \"a\"\n    var x = 0\n    for (var j = 0; j < 0; j++)\n    {\n        x = t[k]\n    }\n    return x + n\n}\n__dprint(f([1]))\n",

        // A key a parameter gives
        "fun f(t, k)\n{\n    var n = len(t)\n    var x = 0\n    for (var j = 0; j < 0; j++)\n    {\n        x = t[k]\n    }\n    return x + n\n}\n__dprint(f([1], \"a\"))\n",

        // Measured, then given something else
        "fun f(t)\n{\n    var n = len(t)\n    var k = 0\n    t = none\n    var x = 0\n    for (var j = 0; j < 0; j++)\n    {\n        x = t[k]\n    }\n    return x + n\n}\n__dprint(f([1]))\n",

        // A global measured: anything may give it something else
        "var G = [1]\nfun clear()\n{\n    G = none\n}\nfun f()\n{\n    var n = len(G)\n    clear()\n    var k = 0\n    var x = 0\n    for (var j = 0; j < 0; j++)\n    {\n        x = G[k]\n    }\n    return x + n\n}\n__dprint(f())\n",

        // `len` is the script's: it measured nothing
        "fun len(x)\n{\n    return 0\n}\nfun f(t)\n{\n    var n = len(t)\n    var k = 0\n    var x = 0\n    for (var j = 0; j < 0; j++)\n    {\n        x = t[k]\n    }\n    return x + n\n}\n__dprint(f(none))\n",
    };

    for (const auto& source : sources)
    {
        EXPECT_EQ(rows(source), 0) << source;
        expectSameEnd(source);
    }
}

TEST_F(RowHoisting, LeavesWhatMayChange)
{
    MockLogger<TerminalSink> logger;

    const std::vector<std::string> sources = {
        // The row is replaced
        "fun f(t)\n{\n    var n = len(t)\n    var i = 0\n    var total = 0\n    for (var j = 0; j < 2; j++)\n    {\n        total = total + t[i][0]\n        t[i] = [10]\n    }\n    return total + n\n}\n__dprint(f([[1]]))\n",

        // The key moves
        "fun f(t)\n{\n    var n = len(t)\n    var i = 0\n    var total = 0\n    for (var j = 0; j < 2; j++)\n    {\n        total = total + t[i][0]\n        i = 1\n    }\n    return total + n\n}\n__dprint(f([[1], [2]]))\n",

        // A list that holds itself: a write into a row is a write into the list
        "fun f(t)\n{\n    var n = len(t)\n    var i = 0\n    var total = 0\n    for (var j = 0; j < 2; j++)\n    {\n        total = total + len(t[i])\n        t[i][0] = [1, 2, 3]\n    }\n    return total + n\n}\nvar l = [1]\nl[0] = l\n__dprint(f(l))\n",

        // A local holding the list's row is written, and the list is a parameter
        "fun f(t, o)\n{\n    var n = len(t)\n    var i = 0\n    var total = 0\n    var r = o[0]\n    for (var j = 0; j < 2; j++)\n    {\n        total = total + len(t[i])\n        r[0] = [1, 2, 3]\n    }\n    return total + n\n}\nvar l = [1]\nl[0] = l\n__dprint(f(l, l))\n",

        // A list in another's hands
        "fun f(t, o)\n{\n    var n = len(t)\n    var i = 0\n    var total = 0\n    for (var j = 0; j < 2; j++)\n    {\n        total = total + t[i]\n        o[0] = 5\n    }\n    return total + n\n}\nvar l = [1]\n__dprint(f(l, l))\n",

        // A script function may do anything
        "var keep = [1]\nfun change()\n{\n    keep[0] = 7\n}\nfun f(t)\n{\n    var n = len(t)\n    var i = 0\n    var total = 0\n    for (var j = 0; j < 2; j++)\n    {\n        total = total + t[i]\n        change()\n    }\n    return total + n\n}\n__dprint(f(keep))\n",

        // Written by a native
        "fun f(t)\n{\n    var n = len(t)\n    var i = 0\n    var total = 0\n    for (var j = 0; j < 2; j++)\n    {\n        total = total + t[i]\n        insert(t, 0, 9)\n    }\n    return total + n\n}\n__dprint(f([1]))\n",

        // An owned list popped
        "fun f()\n{\n    var t = [1, 2]\n    var i = 0\n    var total = 0\n    for (var j = 0; j < 2; j++)\n    {\n        total = total + t[i]\n        pop(t)\n    }\n    return total\n}\n__dprint(f())\n",

        // A field of a table
        "fun f()\n{\n    var t = {a: 1}\n    var total = 0\n    for (var j = 0; j < 2; j++)\n    {\n        total = total + t[\"a\"]\n        t.a = 5\n    }\n    return total\n}\n__dprint(f())\n",
    };

    for (const auto& source : sources)
    {
        EXPECT_EQ(rows(source), 0) << source;
        expectSameEnd(source);
    }
}

TEST_F(RowHoisting, AWriteIntoARowLeavesTheRowWhereItIs)
{
    MockLogger<TerminalSink> logger;

    const std::vector<std::string> sources = {
        // Through the list
        "fun f()\n{\n    var t = [[1, 2], [3, 4]]\n    var i = 1\n    for (var j = 0; j < 2; j++)\n    {\n        t[i][j] = t[i][j] * 2\n        push(t[i], 1)\n    }\n    return len(t[1])\n}\n__dprint(f())\n",

        // Through a local holding it
        "fun f()\n{\n    var t = [[1, 2], [3, 4]]\n    var i = 1\n    var r = t[1]\n    for (var j = 0; j < 2; j++)\n    {\n        r[j] = t[i][j] * 2\n    }\n    return t[1][1]\n}\n__dprint(f())\n",
    };

    for (const auto& source : sources)
    {
        EXPECT_EQ(rows(source), 1) << source;
        expectSameEnd(source);
    }
}

TEST_F(RowHoisting, AWriteBackReadsItsPathOnce)
{
    MockLogger<TerminalSink> logger;

    const std::string source =
        "var t = [[1, 2], [3, 4]]\n"
        "var i = 1\n"
        "t[i][0] += 5\n"
        "t[i][1] *= 2\n"
        "t[i][1] = t[i][0] + t[i][1]\n"
        "__dprint(t[1][0])\n"
        "__dprint(t[1][1])\n";

    bool changed = false;
    RowHoistingPass pass;
    transformWith(pass, source, changed);

    EXPECT_TRUE(changed);
    expectSameEnd(source);

    // What is on the left only is not read back; what runs code is not read once
    expectSameEnd(
        "var calls = 0\n"
        "var t = [[1, 2], [3, 4]]\n"
        "fun row()\n"
        "{\n"
        "    calls = calls + 1\n"
        "    return t[0]\n"
        "}\n"
        "row()[0] += 5\n"
        "__dprint(calls)\n"
        "__dprint(t[0][0])\n");
}

TEST_F(RowHoisting, APathAConditionReadsFirstIsReadOnce)
{
    MockLogger<TerminalSink> logger;

    const std::string source =
        "fun f(t, j, i)\n"
        "{\n"
        "    var s = \"\"\n"
        "    if (t[j][i] != none)\n"
        "    {\n"
        "        s += t[j][i]\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        s = \"-\" + toString(t[j][i])\n"
        "    }\n"
        "    return s\n"
        "}\n"
        "__dprint(f([\"ab\", \"cd\"], 1, 0))\n"
        "__dprint(f([\"ab\", \"cd\"], 1, 5))\n";

    EXPECT_EQ(rows(source), 1);
    expectSameEnd(source);

    // A path past the end stops the script where it did: none has no rows
    expectSameEnd(
        "fun f(t, j, i)\n"
        "{\n"
        "    var s = \"\"\n"
        "    if (t[j][i] != none)\n"
        "    {\n"
        "        s += t[j][i]\n"
        "    }\n"
        "    return s\n"
        "}\n"
        "__dprint(f([\"ab\"], 0, 1))\n"
        "__dprint(f([\"ab\"], 3, 0))\n"
        "__dprint(\"unreachable\")\n");
}

TEST_F(RowHoisting, AnIndexIsAnIntegerByTheDeclarationItReads)
{
    MockLogger<TerminalSink> logger;

    // The second `k` holds no integer: the first still does
    const std::string source =
        "fun f(n)\n"
        "{\n"
        "    var lens = [2, 3, 1]\n"
        "    var total = 0\n"
        "    for (var k = 0; k < 3; k++)\n"
        "    {\n"
        "        for (var j = 0; j < lens[k]; j++)\n"
        "        {\n"
        "            total = total + j\n"
        "        }\n"
        "    }\n"
        "    for (var k = n - 0.5; k >= 0; k--)\n"
        "    {\n"
        "        total = total + k\n"
        "    }\n"
        "    return total\n"
        "}\n"
        "__dprint(f(2))\n";

    EXPECT_EQ(rows(source), 1);
    expectSameEnd(source);

    const std::vector<std::string> sources = {
        // The `k` the read reads is the one with no integer
        "fun f(n)\n{\n    var lens = [2, 3, 1]\n    var total = 0\n    for (var k = n - 0.5; k < 3; k++)\n    {\n        for (var j = 0; j < lens[k]; j++)\n        {\n            total = total + j\n        }\n    }\n    for (var k = 0; k < 3; k++)\n    {\n        total = total + k\n    }\n    return total\n}\n__dprint(f(1))\n",

        // A `k` of a block within hides the integer one
        "fun f(n)\n{\n    var lens = [2, 3, 1]\n    var total = 0\n    var k = 0\n    {\n        var k = n * 0.5\n        for (var j = 0; j < lens[k]; j++)\n        {\n            total = total + j\n        }\n    }\n    return total + k\n}\n__dprint(f(1))\n",

    };

    for (const auto& s : sources)
    {
        EXPECT_EQ(rows(s), 0) << s;
        expectSameEnd(s);
    }
}

TEST_F(RowHoisting, ReadingOnceLeavesWhatMayChange)
{
    MockLogger<TerminalSink> logger;

    const std::vector<std::string> sources = {
        // The row is replaced before it is read again
        "fun f(t)\n{\n    var s = 0\n    var j = 0\n    if (t[j][0] > 0)\n    {\n        t[j] = [5]\n        s = t[j][0]\n    }\n    return s\n}\n__dprint(f([[1]]))\n",

        // The key moves
        "fun f(t)\n{\n    var s = 0\n    var j = 0\n    if (t[j][0] > 0)\n    {\n        j = 1\n        s = t[j][0]\n    }\n    return s\n}\n__dprint(f([[1], [2]]))\n",

        // Written by a native
        "fun f(t)\n{\n    var s = 0\n    var j = 0\n    if (t[j][0] > 0)\n    {\n        insert(t[j], 0, 9)\n        s = t[j][0]\n    }\n    return s\n}\n__dprint(f([[1]]))\n",

        // A script function may do anything
        "var keep = [[1]]\nfun change()\n{\n    keep[0][0] = 7\n}\nfun f(t)\n{\n    var s = 0\n    var j = 0\n    if (t[j][0] > 0)\n    {\n        change()\n        s = t[j][0]\n    }\n    return s\n}\n__dprint(f(keep))\n",

        // Something runs before the path is read
        "var calls = 0\nfun g()\n{\n    calls++\n    return 1\n}\nfun f(t)\n{\n    var j = 0\n    var s = 0\n    if (g() + t[j][0] > 0)\n    {\n        s = t[j][0]\n    }\n    return s + calls\n}\n__dprint(f([[1]]))\n",

        // Read on the side of an `and` that may not run
        "fun f(t, c)\n{\n    var j = 3\n    var s = 0\n    if (c and t[j][0] > 0)\n    {\n        s = t[j][0]\n    }\n    return s\n}\n__dprint(f([[1]], false))\n",
    };

    for (const auto& source : sources)
    {
        EXPECT_EQ(rows(source), 0) << source;
        expectSameEnd(source);
    }
}

class CompoundAssignment : public RowHoisting
{
protected:
    // What the source prints, the same at every level
    void expectPrints(const std::string& source, const std::string& expected)
    {
        for (auto level : {VmOptimizationLevel::O0, VmOptimizationLevel::O2, VmOptimizationLevel::O3})
        {
            EntitySystem ecs;
            ecs.setVMOptimizationLevel(level);
            ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

            VM vm;
            ecs.setupVm(vm);

            EXPECT_EQ(vm.interpretFromText(withImports(source)), InterpretResult::OK) << source;
            EXPECT_EQ(vm.testOutput, expected) << source;
        }
    }
};

TEST_F(CompoundAssignment, LenCountsATable)
{
    MockLogger<TerminalSink> logger;

    expectPrints("var t = {}\n__dprint(len(t))\nt.a = 1\nt[\"b\"] = 2\nt.a = 3\n__dprint(len(t))\n__dprint(len({x: 1, y: 2, z: 3}))\n", "0\n2\n3\n");

    // Counted the way a loop over it counts
    expectPrints(
        "var t = {}\n"
        "for (var i = 0; i < 20; i++)\n"
        "{\n"
        "    t[toString(i)] = true\n"
        "}\n"
        "var n = 0\n"
        "for (var k : t)\n"
        "{\n"
        "    n++\n"
        "}\n"
        "__dprint(n)\n"
        "__dprint(len(t))\n", "20\n20\n");

    // Lists and strings are what they were, an instance of a class is no table
    expectPrints("__dprint(len([1, 2]))\n__dprint(len(\"abc\"))\n", "2\n3\n");

    EntitySystem ecs;
    ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

    VM vm;
    ecs.setupVm(vm);

    EXPECT_EQ(vm.interpretFromText(withImports("class Box\n{\n    size()\n    {\n        return 1\n    }\n}\n__dprint(len(Box()))\n")), InterpretResult::RUNTIME_ERROR);
}

TEST_F(LengthHoisting, ATableGrowsByItsFields)
{
    MockLogger<TerminalSink> logger;

    const std::vector<std::string> sources = {
        "fun f(t)\n{\n    var n = 0\n    for (var i = 0; i < len(t); i++)\n    {\n        n++\n        if (i < 3)\n        {\n            t.a = 1\n            t.b = 2\n        }\n    }\n    return n\n}\n__dprint(f({z: 1}))\n",

        // By another name
        "fun f(t, o)\n{\n    var n = 0\n    for (var i = 0; i < len(t); i++)\n    {\n        n++\n        if (i < 3)\n        {\n            o.a = 1\n            o.b = 2\n        }\n    }\n    return n\n}\nvar l = {z: 1}\n__dprint(f(l, l))\n",

        // A row that is a table
        "fun f(t, o)\n{\n    var n = 0\n    for (var i = 0; i < len(t[0]); i++)\n    {\n        n++\n        if (i < 3)\n        {\n            o.a = 1\n            o.b = 2\n        }\n    }\n    return n\n}\nvar l = {z: 1}\n__dprint(f([l], l))\n",
    };

    for (const auto& source : sources)
    {
        EXPECT_EQ(hoisted(source), 0) << source;
        expectSameRun(source);
    }
}

TEST_F(LengthHoisting, AFieldWriteCannotLengthenAList)
{
    MockLogger<TerminalSink> logger;

    const std::vector<std::string> measured = {
        "fun f()\n{\n    var rows = [{a: 1}, {a: 2}]\n    var n = 0\n    for (var i = 0; i < len(rows); i++)\n    {\n        var r = rows[i]\n        r.a = i\n        r.b = i\n        n++\n    }\n    return n\n}\n__dprint(f())\n",

        // In a block of the script, as a list that is filled first
        "for (var k = 0; k < 2; k++)\n{\n    var rows = []\n    push(rows, {a: 1})\n    push(rows, {a: 2})\n    var n = 0\n    for (var i = 0; i < len(rows); i++)\n    {\n        var r = rows[i]\n        r.a = i\n        n++\n    }\n    __dprint(n)\n}\n",

        // Given another list later
        "fun f()\n{\n    var rows = [{a: 1}]\n    rows = [{a: 1}, {a: 2}]\n    var n = 0\n    for (var i = 0; i < len(rows); i++)\n    {\n        var r = rows[i]\n        r.a = i\n        n++\n    }\n    return n\n}\n__dprint(f())\n",
    };

    for (const auto& source : measured)
    {
        EXPECT_EQ(hoisted(source), 1) << source;
        expectSameRun(source);
    }

    // All of these may be a table
    const std::vector<std::string> tables = {
        // Given one later, with no key written, or with one
        "fun f()\n{\n    var t = []\n    t = {z: 1}\n    var n = 0\n    for (var i = 0; i < len(t); i++)\n    {\n        n++\n        if (i < 3)\n        {\n            t.a = 1\n            t.b = 2\n        }\n    }\n    return n\n}\n__dprint(f())\n",
        "fun f()\n{\n    var t = [z: 1]\n    var n = 0\n    for (var i = 0; i < len(t); i++)\n    {\n        n++\n        if (i < 3)\n        {\n            t.a = 1\n            t.b = 2\n        }\n    }\n    return n\n}\n__dprint(f())\n",

        // Given one by a function it defines
        "fun f()\n{\n    var t = []\n    var g = fun()\n    {\n        t = {z: 1}\n        return 1\n    }\n    g()\n    var n = 0\n    for (var i = 0; i < len(t); i++)\n    {\n        n++\n        if (i < 3)\n        {\n            t.a = 1\n            t.b = 2\n        }\n    }\n    return n\n}\n__dprint(f())\n",

        // A global of that name, a list by its other declaration only
        "var t = {z: 1}\nif (true)\n{\n    var t = []\n}\nvar n = 0\nfor (var i = 0; i < len(t); i++)\n{\n    n++\n    if (i < 3)\n    {\n        t.a = 1\n        t.b = 2\n    }\n}\n__dprint(n)\n",

        // A row: nothing tells what it holds
        "fun f()\n{\n    var l = {z: 1}\n    var t = [l]\n    var n = 0\n    for (var i = 0; i < len(t[0]); i++)\n    {\n        n++\n        if (i < 3)\n        {\n            l.a = 1\n            l.b = 2\n        }\n    }\n    return n\n}\n__dprint(f())\n",
    };

    for (const auto& source : tables)
    {
        EXPECT_EQ(hoisted(source), 0) << source;
        expectSameRun(source);
    }
}

TEST_F(RowHoisting, ATableMeasuredIsReadAsOne)
{
    MockLogger<TerminalSink> logger;

    const std::string source =
        "fun f(t)\n"
        "{\n"
        "    var n = len(t)\n"
        "    var k = 1\n"
        "    var total = 0\n"
        "    for (var j = 0; j < 3; j++)\n"
        "    {\n"
        "        total = total + t[k]\n"
        "    }\n"
        "    return total + n\n"
        "}\n"
        "__dprint(f({a: 1}))\n";

    expectSameEnd(source);
}

TEST_F(CompoundAssignment, AProgramNamingAMetamethodIsLeftAlone)
{
    MockLogger<TerminalSink> logger;

    const std::string loop =
        "fun f(t)\n"
        "{\n"
        "    var n = len(t)\n"
        "    var k = 5\n"
        "    var total = 0\n"
        "    for (var j = 0; j < 3; j++)\n"
        "    {\n"
        "        total = total + t[k]\n"
        "    }\n"
        "    return total + n\n"
        "}\n"
        "var calls = [0]\n"
        "fun get(self, key)\n"
        "{\n"
        "    calls[0] = calls[0] + 1\n"
        "    return 7\n"
        "}\n"
        "var t = {a: 1}\n";

    const std::vector<std::string> sources = {
        loop + "t.__get = get\n__dprint(f(t))\n__dprint(calls[0])\n",
        loop + "t[\"__get\"] = get\n__dprint(f(t))\n__dprint(calls[0])\n",
    };

    for (const auto& source : sources)
    {
        EXPECT_EQ(rows(source), 0) << source;
        expectSameEnd(source);
    }

    // Three reads at every level
    expectPrints(sources[0], "23\n3\n");
}

TEST_F(CompoundAssignment, ModuloTakesTheRemainder)
{
    MockLogger<TerminalSink> logger;

    expectPrints("var a = 7\na %= 4\n__dprint(a)\n", "3\n");
    expectPrints("var t = [7]\nt[0] %= 4\n__dprint(t[0])\n", "3\n");
    expectPrints("var t = {a: 7}\nt.a %= 4\n__dprint(t.a)\n", "3\n");
    expectPrints("fun f()\n{\n    var a = 7\n    a %= 4\n    return a\n}\n__dprint(f())\n", "3\n");

    // The others are what they were
    expectPrints("var a = 8\na /= 4\na += 1\na -= 2\na *= 5\n__dprint(a)\n", "5\n");
}

TEST_F(CompoundAssignment, TheObjectIsEvaluatedOnce)
{
    MockLogger<TerminalSink> logger;

    const std::string counted =
        "var calls = 0\n"
        "var t = [[1, 2], [3, 4]]\n"
        "var o = {a: 1, row: [5]}\n"
        "fun row()\n{\n    calls = calls + 1\n    return t[0]\n}\n"
        "fun obj()\n{\n    calls = calls + 1\n    return o\n}\n"
        "fun key()\n{\n    calls = calls + 1\n    return 1\n}\n";

    expectPrints(counted + "row()[0] += 5\n__dprint(calls)\n__dprint(t[0][0])\n", "1\n6\n");
    expectPrints(counted + "t[key()][0] -= 1\n__dprint(calls)\n__dprint(t[1][0])\n", "1\n2\n");
    expectPrints(counted + "row()[key()] *= 3\n__dprint(calls)\n__dprint(t[0][1])\n", "2\n6\n");
    expectPrints(counted + "obj().a += 2\n__dprint(calls)\n__dprint(o.a)\n", "1\n3\n");
    expectPrints(counted + "obj().row[0] += 2\n__dprint(calls)\n__dprint(o.row[0])\n", "1\n7\n");

    // In a function, and when what the write leaves is used
    expectPrints(counted + "fun f()\n{\n    row()[0] += 5\n    obj().a += 2\n    return calls\n}\n__dprint(f())\n", "2\n");
    expectPrints(counted + "var left = (obj().a += 2)\n__dprint(calls)\n__dprint(left)\n", "1\n3\n");
}

TEST_F(CompoundAssignment, TheIndexIsEvaluatedOnce)
{
    MockLogger<TerminalSink> logger;

    expectPrints(
        "fun f()\n"
        "{\n"
        "    var t = [1, 2, 3]\n"
        "    var i = 0\n"
        "    t[i++] += 10\n"
        "    t[i++] += 10\n"
        "    __dprint(i)\n"
        "    __dprint(t[0])\n"
        "    __dprint(t[1])\n"
        "    __dprint(t[2])\n"
        "}\n"
        "f()\n", "2\n11\n12\n3\n");
}

TEST_F(CompoundAssignment, AGetterRunsOnceAtEveryLevel)
{
    MockLogger<TerminalSink> logger;

    expectPrints(
        "var reads = 0\n"
        "var stored = [1]\n"
        "class Box\n"
        "{\n"
        "    __get(key)\n"
        "    {\n"
        "        reads = reads + 1\n"
        "        return stored\n"
        "    }\n"
        "}\n"
        "var b = Box()\n"
        "var k = \"a\"\n"
        "var i = 0\n"
        "b[k][i] += 4\n"
        "b.a[i] += 4\n"
        "__dprint(reads)\n"
        "__dprint(stored[0])\n", "2\n9\n");
}

TEST_F(CompoundAssignment, ASetterGetsWhatWasRead)
{
    MockLogger<TerminalSink> logger;

    expectPrints(
        "var calls = 0\n"
        "var kept = {}\n"
        "class Box\n"
        "{\n"
        "    __get(key)\n"
        "    {\n"
        "        return 1\n"
        "    }\n"
        "    __set(key, value)\n"
        "    {\n"
        "        kept.last = value\n"
        "    }\n"
        "}\n"
        "var b = Box()\n"
        "fun box()\n{\n    calls = calls + 1\n    return b\n}\n"
        "box().x += 4\n"
        "__dprint(calls)\n"
        "__dprint(kept.last)\n"
        "box()[\"y\"] += 5\n"
        "__dprint(calls)\n"
        "__dprint(kept.last)\n", "1\n5\n2\n6\n");
}

TEST_F(RowHoisting, AnUnreadIncrementCountsTheSame)
{
    MockLogger<TerminalSink> logger;

    expectSameEnd(
        "fun f()\n"
        "{\n"
        "    var a = 1\n"
        "    var b = 1.5\n"
        "    var c = 0\n"
        "    a++\n"
        "    b++\n"
        "    c--\n"
        "    var d = a++\n"
        "    var g = fun()\n"
        "    {\n"
        "        return a\n"
        "    }\n"
        "    a++\n"
        "    return toString(a) + \" \" + toString(b) + \" \" + toString(c) + \" \" + toString(d) + \" \" + toString(g())\n"
        "}\n"
        "__dprint(f())\n");

    expectSameEnd(
        "fun f()\n"
        "{\n"
        "    var s = \"a\"\n"
        "    s++\n"
        "    return s\n"
        "}\n"
        "__dprint(f())\n");
}

TEST_F(RowHoisting, AProgramWithAClassRunsTheSame)
{
    MockLogger<TerminalSink> logger;

    expectSameEnd(
        "class Counter\n"
        "{\n"
        "    init(rows)\n"
        "    {\n"
        "        this.rows = rows\n"
        "        this.n = 0\n"
        "    }\n"
        "    add(k)\n"
        "    {\n"
        "        this.rows[0][k] += 1\n"
        "        this.n++\n"
        "    }\n"
        "}\n"
        "var rows = [[0, 0]]\n"
        "var fills = {a: 1}\n"
        "fills.b = 2\n"
        "var c = Counter(rows)\n"
        "for (var i = 0; i < 4; i++)\n"
        "{\n"
        "    c.add(i % 2)\n"
        "}\n"
        "__dprint(rows[0][0])\n"
        "__dprint(rows[0][1])\n"
        "__dprint(c.n)\n"
        "__dprint(fills.b)\n");
}

TEST_F(RowHoisting, ReadingFromTwoLocalsReadsAnything)
{
    MockLogger<TerminalSink> logger;

    expectSameEnd(
        "class Box\n"
        "{\n"
        "    __get(key)\n"
        "    {\n"
        "        return key + key\n"
        "    }\n"
        "}\n"
        "fun f()\n"
        "{\n"
        "    var v = [1, 2]\n"
        "    var s = \"a long enough string, not a small one\"\n"
        "    var small = \"ab\"\n"
        "    var t = {a: 1}\n"
        "    var b = Box()\n"
        "    var i = 1\n"
        "    var n = -1\n"
        "    var far = 10\n"
        "    var k = \"a\"\n"
        "    __dprint(v[i])\n"
        "    __dprint(v[n])\n"
        "    __dprint(v[far])\n"
        "    __dprint(s[i])\n"
        "    __dprint(s[n])\n"
        "    __dprint(s[far])\n"
        "    __dprint(small[i])\n"
        "    __dprint(t[k])\n"
        "    __dprint(t[i])\n"
        "    __dprint(b[k])\n"
        "    var row = [[5]]\n"
        "    var zero = 0\n"
        "    row = row[zero]\n"
        "    return row[zero]\n"
        "}\n"
        "__dprint(f())\n");

    expectSameEnd("fun f()\n{\n    var v = [1]\n    var k = \"a\"\n    return v[k]\n}\n__dprint(f())\n");
    expectSameEnd("fun f()\n{\n    var v = 1\n    var k = 0\n    return v[k]\n}\n__dprint(f())\n");
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

class ListMethods : public CompoundAssignment
{
protected:
    // How many list method calls the pass alone rewrites in the source
    long rewritten(const std::string& source)
    {
        ListMethodsPass pass;
        bool changed = false;
        transformWith(pass, source, changed);

        return pass.rewrittenCalls();
    }

    // How many len() reads LengthHoisting hoists from the source once its list methods are rewritten
    long hoistedThroughMethods(const std::string& source)
    {
        Lexer lexer;
        lexer.readFromText(source);

        Parser parser(lexer.getTokens());
        auto statements = parser.parse();

        EXPECT_FALSE(parser.hasError()) << "test source failed to parse";

        ListMethodsPass methods;
        methods.runPass(nullptr, statements);

        LengthHoistingPass lengths;
        lengths.runPass(nullptr, statements);

        return lengths.hoistedReads();
    }

    // What the source prints with no import, the same at every level
    void expectPrintsAlone(const std::string& source, const std::string& expected)
    {
        for (auto level : {VmOptimizationLevel::O0, VmOptimizationLevel::O2, VmOptimizationLevel::O3})
        {
            EntitySystem ecs;
            ecs.setVMOptimizationLevel(level);
            ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

            VM vm;
            ecs.setupVm(vm);

            EXPECT_EQ(vm.interpretFromText(source), InterpretResult::OK) << source;
            EXPECT_EQ(vm.testOutput, expected) << source;
        }
    }
};

TEST_F(ListMethods, AListAnswersItsMethods)
{
    MockLogger<TerminalSink> logger;

    const std::string source =
        "var v = []\n"
        "v.push(1)\n"
        "v.push(2)\n"
        "v.insert(0, 7)\n"
        "__dprint(v.len())\n"
        "__dprint(v.removeAt(1))\n"
        "__dprint(v.pop())\n"
        "__dprint(v[0])\n"
        "var w = [5]\n"
        "w.push(v.pop())\n"
        "__dprint(w[1])\n"
        "__dprint(v.len())\n"
        "var s = \"abcd\"\n"
        "__dprint(s.len())\n"
        "var t = {a: 1, b: 2}\n"
        "__dprint(t.len())\n";

    // No import is needed for them, at any level
    expectPrintsAlone(source, "3\n1\n2\n7\n7\n0\n4\n2\n");
    expectPrints(source, "3\n1\n2\n7\n7\n0\n4\n2\n");
    expectFrontEndsAgree(source);

    EXPECT_EQ(rewritten(source), 11);
}

TEST_F(ListMethods, ACallThatMayRunSomethingElseStays)
{
    MockLogger<TerminalSink> logger;

    // A class's method of the name: an instance runs it, a list its own
    const std::string withClass =
        "class Stack\n"
        "{\n"
        "    init()\n"
        "    {\n"
        "        this.items = []\n"
        "    }\n"
        "    push(x)\n"
        "    {\n"
        "        this.items = [x, x]\n"
        "    }\n"
        "}\n"
        "var rows = []\n"
        "fun fill(n)\n"
        "{\n"
        "    for (var i = 0; i < n; i++)\n"
        "    {\n"
        "        rows.push(i)\n"
        "    }\n"
        "}\n"
        "fill(3)\n"
        "var st = Stack()\n"
        "st.push(9)\n"
        "__dprint(len(rows))\n"
        "__dprint(len(st.items))\n"
        "__dprint(rows.len())\n";

    EXPECT_EQ(rewritten(withClass), 1);  // the len() alone
    expectPrints(withClass, "3\n2\n3\n");

    // A field of the name: a table runs what it holds
    const std::string withField =
        "fun twice(x)\n"
        "{\n"
        "    return x * 2\n"
        "}\n"
        "var t = {}\n"
        "t.push = twice\n"
        "__dprint(t.push(4))\n"
        "var v = []\n"
        "v.push(3)\n"
        "__dprint(v.len())\n";

    EXPECT_EQ(rewritten(withField), 1);
    expectPrints(withField, "8\n1\n");

    // A key or a string of the name is a field it may be
    EXPECT_EQ(rewritten("var t = {pop: 1}\nvar v = [1]\nv.pop()\nv.push(2)\n"), 1);
    EXPECT_EQ(rewritten("var k = \"len\"\nvar v = [1]\n__dprint(v.len())\nv.push(2)\n"), 1);

    // One that gives the method what it does not take stops the script, and stays
    EXPECT_EQ(rewritten("var v = []\nv.push()\nv.pop(1)\nv.len(2)\n"), 0);

    for (auto level : {VmOptimizationLevel::O0, VmOptimizationLevel::O3})
    {
        EntitySystem ecs;
        ecs.setVMOptimizationLevel(level);
        ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

        VM vm;
        ecs.setupVm(vm);

        EXPECT_EQ(vm.interpretFromText("var v = []\nv.push()\n"), InterpretResult::RUNTIME_ERROR);
    }
}

TEST_F(ListMethods, LoopsAreHoistedAsWithTheFunctions)
{
    MockLogger<TerminalSink> logger;

    const std::string functions =
        "fun count(table)\n"
        "{\n"
        "    var res = []\n"
        "    for (var i = 0; i < len(table); i++)\n"
        "    {\n"
        "        push(res, [])\n"
        "        for (var j = 0; j < len(table[i]); j++)\n"
        "        {\n"
        "            push(res[i], -1)\n"
        "        }\n"
        "    }\n"
        "    return len(res)\n"
        "}\n"
        "__dprint(count([[1, 0], [1, 1, 1]]))\n";

    const std::string methods =
        "fun count(table)\n"
        "{\n"
        "    var res = []\n"
        "    for (var i = 0; i < table.len(); i++)\n"
        "    {\n"
        "        res.push([])\n"
        "        for (var j = 0; j < table[i].len(); j++)\n"
        "        {\n"
        "            res[i].push(-1)\n"
        "        }\n"
        "    }\n"
        "    return res.len()\n"
        "}\n"
        "__dprint(count([[1, 0], [1, 1, 1]]))\n";

    EXPECT_GT(hoisted(functions), 0);
    EXPECT_EQ(hoistedThroughMethods(methods), hoisted(functions));

    expectPrints(methods, "2\n");
    expectSameEnd(methods);
}

} // namespace test
} // namespace pg
