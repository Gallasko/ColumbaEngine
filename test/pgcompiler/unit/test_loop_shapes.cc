#include "gtest/gtest.h"

#include "Compiler/vm.h"

#include "ECS/entitysystem.h"

#include "../../mocklogger.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace pg {
namespace test {

/**
 * What a script runs at O3, by the name the profiler gives each instruction: the shape of the
 * code, which the scripts of the testbench cannot see (they only see what it prints).
 */
class LoopShapes : public ::testing::Test
{
protected:
    std::map<std::string, uint64_t> run(const std::string& source, std::string& output)
    {
        EntitySystem ecs;
        ecs.setVMOptimizationLevel(VmOptimizationLevel::O3);
        ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

        VM vm;
        ecs.setupVm(vm);

        vm.profiler.setEnabled(true);

        EXPECT_EQ(vm.interpretFromText(source), InterpretResult::OK);

        output = vm.testOutput;

        std::map<std::string, uint64_t> counts;

        for (const auto& instruction : vm.profiler.getResultsSortedByCount())
            counts[instruction.opcodeName] += instruction.executionCount;

        return counts;
    }

    // With the decoder's fusion or without: what it fuses must not show
    InterpretResult runScript(const std::string& source, bool fusion, std::string& output)
    {
        EntitySystem ecs;
        ecs.setVMOptimizationLevel(VmOptimizationLevel::O3);
        ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

        VM vm;
        ecs.setupVm(vm);

        vm.enableDecodeFusion = fusion;

        const InterpretResult result = vm.interpretFromText(source);

        output = vm.testOutput;

        return result;
    }

    static uint64_t runs(const std::map<std::string, uint64_t>& counts, const std::string& name)
    {
        const auto it = counts.find(name);

        return it == counts.end() ? 0 : it->second;
    }

    static uint64_t runsOfElided(const std::map<std::string, uint64_t>& counts)
    {
        uint64_t total = 0;

        for (const auto& [name, count] : counts)
        {
            if (name.rfind("ELIDED", 0) == 0)
                total += count;
        }

        return total;
    }
};

TEST_F(LoopShapes, ForLoopTestsAtTheBottom)
{
    MockLogger<TerminalSink> logger;

    std::string output;
    const auto counts = run(
        "for (var i = 0; i < 5; i++)\n"
        "{\n"
        "    __dprint(i)\n"
        "}\n", output);

    EXPECT_EQ(output, "0\n1\n2\n3\n4\n");

    EXPECT_EQ(runs(counts, "OP_Loop"), 0u);
    EXPECT_EQ(runs(counts, "OP_Long_Loop"), 0u);

    // Only the jump into the loop, once
    EXPECT_EQ(runs(counts, "OP_Jump"), 1u);
}

TEST_F(LoopShapes, ForLoopWithContinueTestsAtTheBottom)
{
    MockLogger<TerminalSink> logger;

    std::string output;
    const auto counts = run(
        "for (var i = 0; i < 4; i++)\n"
        "{\n"
        "    if (i == 1)\n"
        "    {\n"
        "        continue\n"
        "    }\n"
        "\n"
        "    __dprint(i)\n"
        "}\n", output);

    EXPECT_EQ(output, "0\n2\n3\n");

    EXPECT_EQ(runs(counts, "OP_Loop"), 0u);
    EXPECT_EQ(runs(counts, "OP_Long_Loop"), 0u);
}

TEST_F(LoopShapes, NestedForLoopsTestAtTheBottom)
{
    MockLogger<TerminalSink> logger;

    std::string output;
    const auto counts = run(
        "for (var i = 0; i < 2; i++)\n"
        "{\n"
        "    for (var j = 0; j < 2; j++)\n"
        "    {\n"
        "        __dprint(i * 10 + j)\n"
        "    }\n"
        "}\n", output);

    EXPECT_EQ(output, "0\n1\n10\n11\n");

    EXPECT_EQ(runs(counts, "OP_Loop"), 0u);
    EXPECT_EQ(runs(counts, "OP_Long_Loop"), 0u);
}

TEST_F(LoopShapes, WhileLoopTestsAtTheBottom)
{
    MockLogger<TerminalSink> logger;

    std::string output;
    const auto counts = run(
        "fun run(n)\n"
        "{\n"
        "    var i = 0\n"
        "    while (i < n)\n"
        "    {\n"
        "        __dprint(i)\n"
        "        i = i + 1\n"
        "    }\n"
        "}\n"
        "run(2)\n", output);

    EXPECT_EQ(output, "0\n1\n");

    EXPECT_EQ(runs(counts, "OP_Loop"), 0u);
    EXPECT_EQ(runs(counts, "OP_Long_Loop"), 0u);
}

TEST_F(LoopShapes, ElidedInstructionsNeverRun)
{
    MockLogger<TerminalSink> logger;

    std::string output;
    const auto counts = run(
        "fun run()\n"
        "{\n"
        "    var values = [1, 2, 3]\n"
        "    var seen = [0, 0, 0]\n"
        "\n"
        "    for (var i = 0; i < 3; i++)\n"
        "    {\n"
        "        seen[i] = values[i] + 1\n"
        "    }\n"
        "\n"
        "    __dprint(seen[0])\n"
        "    __dprint(seen[2])\n"
        "}\n"
        "run()\n", output);

    EXPECT_EQ(output, "2\n4\n");

    EXPECT_GT(runs(counts, "FUSED_GetIndex(L,S)"), 0u);
    EXPECT_GT(runs(counts, "FUSED_SetIndex(L,S,S)"), 0u);
    EXPECT_EQ(runsOfElided(counts), 0u);
}

// A branch can land on a read whose producer is gone: it reaches what follows it
TEST_F(LoopShapes, BranchToAnElidedInstructionLandsOnTheNextOne)
{
    MockLogger<TerminalSink> logger;

    std::string output;
    const auto counts = run(
        "fun run()\n"
        "{\n"
        "    var values = [\"a\", \"b\"]\n"
        "\n"
        "    for (var i = 0; i < 2; i++)\n"
        "    {\n"
        "        if (i == 1)\n"
        "        {\n"
        "            __dprint(\"one\")\n"
        "        }\n"
        "\n"
        "        __dprint(values[i])\n"
        "    }\n"
        "}\n"
        "run()\n", output);

    EXPECT_EQ(output, "a\none\nb\n");
    EXPECT_EQ(runsOfElided(counts), 0u);
}

// A fused branch on a constant keeps its target as a distance: the code it crosses got shorter
TEST_F(LoopShapes, FusedConstantBranchesCrossingElidedInstructions)
{
    MockLogger<TerminalSink> logger;

    std::string output;
    const auto counts = run(
        "fun run()\n"
        "{\n"
        "    var words = [\"a\", \"b\", \"end\"]\n"
        "    var seen = [0, 0, 0]\n"
        "    var i = 0\n"
        "\n"
        "    while (words[i] != \"end\")\n"
        "    {\n"
        "        if (words[i] == \"a\")\n"
        "        {\n"
        "            seen[0] = words[i]\n"
        "            __dprint(seen[0])\n"
        "        }\n"
        "        else\n"
        "        {\n"
        "            seen[1] = words[i]\n"
        "            __dprint(seen[1])\n"
        "        }\n"
        "\n"
        "        i = i + 1\n"
        "    }\n"
        "\n"
        "    __dprint(i)\n"
        "}\n"
        "run()\n", output);

    EXPECT_EQ(output, "a\nb\n2\n");
    EXPECT_EQ(runsOfElided(counts), 0u);
}

TEST_F(LoopShapes, AndInAnIfDropsNothingAfterItsTests)
{
    MockLogger<TerminalSink> logger;

    std::string output;
    const auto counts = run(
        "fun run(a, b, c)\n"
        "{\n"
        "    if (a < 1 and b < 2 and c < 3)\n"
        "    {\n"
        "        __dprint(\"then\")\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        __dprint(\"else\")\n"
        "    }\n"
        "}\n"
        "run(0, 1, 2)\n"
        "run(0, 2, 2)\n"
        "run(1, 1, 2)\n", output);

    EXPECT_EQ(output, "then\nelse\nelse\n");

    EXPECT_EQ(runs(counts, "OP_Jump_If_False"), 0u);
    EXPECT_EQ(runs(counts, "OP_Long_Jump_If_False"), 0u);
}

TEST_F(LoopShapes, AFieldOfALocalIsOneInstruction)
{
    MockLogger<TerminalSink> logger;

    std::string output;
    const auto counts = run(
        "class Point\n"
        "{\n"
        "    init(x, y)\n"
        "    {\n"
        "        this.x = x\n"
        "        this.y = y\n"
        "    }\n"
        "\n"
        "    sum()\n"
        "    {\n"
        "        return this.x + this.y\n"
        "    }\n"
        "}\n"
        "fun run()\n"
        "{\n"
        "    var p = Point(1, 2)\n"
        "    var total = 0\n"
        "\n"
        "    for (var i = 0; i < 3; i++)\n"
        "    {\n"
        "        total = total + p.x + p.sum()\n"
        "    }\n"
        "\n"
        "    __dprint(total)\n"
        "}\n"
        "run()\n", output);

    EXPECT_EQ(output, "12\n");

    EXPECT_GE(runs(counts, "FUSED_GetProperty(L)"), 9u);
    EXPECT_EQ(runs(counts, "OP_Get_Property"), 0u);
}

// What the fused read leaves to the generic one, the same with it and without it
TEST_F(LoopShapes, AFieldOfALocalReadsLikeAnyField)
{
    MockLogger<TerminalSink> logger;

    const std::vector<std::pair<std::string, std::string>> cases = {
        // One read, tables of two shapes in turn, then one that grows
        {"fun x(t)\n{\n    return t.x\n}\n"
         "fun run()\n{\n    var a = {x: 1}\n    var b = {y: 0, x: 2}\n    var c = {x: 3}\n"
         "    for (var i = 0; i < 3; i++)\n    {\n        __dprint(x(a) + x(b))\n    }\n"
         "    c.y = 1\n    __dprint(x(c))\n    __dprint(x(a))\n}\nrun()\n",
         "3\n3\n3\n3\n1\n"},

        // No such field
        {"fun x(t)\n{\n    return t.x == none\n}\nfun run()\n{\n    var a = {x: 1}\n    __dprint(x(a))\n    __dprint(x({y: 1}))\n}\nrun()\n",
         "false\ntrue\n"},

        // A method read as a value, kept past its instance
        {"class C\n{\n    init()\n    {\n        this.v = 5\n    }\n\n    get()\n    {\n        return this.v\n    }\n}\n"
         "fun bind()\n{\n    var c = C()\n    var m = c.get\n    return m\n}\n__dprint(bind()())\n",
         "5\n"},

        // A field that is its instance's last reference on the slot written over
        {"fun run()\n{\n    var n = {v: 1, next: {v: 2, next: {v: 3, next: none}}}\n    while (n != none)\n    {\n        __dprint(n.v)\n        n = n.next\n    }\n}\nrun()\n",
         "1\n2\n3\n"},

        // A string kept after what held it is gone
        {"fun run()\n{\n    var t = {s: \"a string long enough for the heap, not to be small\"}\n    var s = t.s\n    t = none\n    __dprint(s)\n}\nrun()\n",
         "a string long enough for the heap, not to be small\n"},

        // __get of a class and of a table, with the local alive after
        {"class G\n{\n    init()\n    {\n        this.real = 1\n    }\n\n    __get(name)\n    {\n        return name + \"!\"\n    }\n}\n"
         "fun run()\n{\n    var g = G()\n    __dprint(g.real)\n    __dprint(g.other)\n    __dprint(g.other)\n    __dprint(g.real)\n}\nrun()\n",
         "1\nother!\nother!\n1\n"},
        {"fun get(self, name)\n{\n    return name + \"?\"\n}\n"
         "fun run()\n{\n    var t = {real: 1}\n    t.__get = get\n    __dprint(t.real)\n    __dprint(t.other)\n    __dprint(t.real)\n}\nrun()\n",
         "1\nother?\n1\n"},

        // __get gives the list another value: the one indexed is the one read before it ran
        {"class K\n{\n    init(f)\n    {\n        this.f = f\n    }\n\n    __get(name)\n    {\n        var f = this.f\n        f()\n        return 0\n    }\n}\n"
         "fun run()\n{\n    var v = [1]\n    var r = K(fun()\n    {\n        v = [2]\n        return 0\n    })\n    __dprint(v[r.k])\n    __dprint(v[0])\n}\nrun()\n",
         "1\n2\n"},
    };

    for (const auto& [source, expected] : cases)
    {
        for (const bool fusion : {true, false})
        {
            std::string output;
            EXPECT_EQ(runScript(source, fusion, output), InterpretResult::OK) << source;
            EXPECT_EQ(output, expected) << source;
        }
    }
}

TEST_F(LoopShapes, AFieldOfWhatIsNoInstanceStops)
{
    MockLogger<TerminalSink> logger;

    for (const bool fusion : {true, false})
    {
        std::string output;
        EXPECT_EQ(runScript("fun run()\n{\n    var n = 1\n    return n.x\n}\nrun()\n", fusion, output), InterpretResult::RUNTIME_ERROR);
    }
}

TEST_F(LoopShapes, ALoopsLastLocalGoesWithItsIncrement)
{
    MockLogger<TerminalSink> logger;

    std::string output;
    const auto counts = run(
        "fun run()\n"
        "{\n"
        "    var rows = [{v: 1}, {v: 2}, {v: 3}]\n"
        "    var total = 0\n"
        "\n"
        "    for (var i = 0; i < 3; i++)\n"
        "    {\n"
        "        var r = rows[i]\n"
        "        total = total + r.v\n"
        "    }\n"
        "\n"
        "    for (var j = 2; j >= 0; j--)\n"
        "    {\n"
        "        var r = rows[j]\n"
        "        total = total * r.v\n"
        "    }\n"
        "\n"
        "    __dprint(total)\n"
        "}\n"
        "run()\n", output);

    EXPECT_EQ(output, "36\n");

    EXPECT_EQ(runs(counts, "FUSED_Pop+IncrLocal"), 3u);
    EXPECT_EQ(runs(counts, "FUSED_Pop+DecrLocal"), 3u);
    EXPECT_EQ(runs(counts, "OP_PopN"), 0u);
    EXPECT_EQ(runs(counts, "FUSED_IncrLocal"), 0u);
    EXPECT_EQ(runs(counts, "FUSED_DecrLocal"), 0u);
}

// One local and nothing else to drop, the shape that stays a plain Pop
TEST_F(LoopShapes, ALoopsOnlyPopGoesWithItsIncrement)
{
    MockLogger<TerminalSink> logger;

    std::string output;
    const auto counts = run(
        "fun run()\n"
        "{\n"
        "    var rows = [{v: 1}, {v: 2}, {v: 3}]\n"
        "\n"
        "    for (var i = 0; i < 3; i++)\n"
        "    {\n"
        "        var r = rows[i]\n"
        "\n"
        "        if (r.v == 2)\n"
        "        {\n"
        "            __dprint(r.v)\n"
        "        }\n"
        "    }\n"
        "}\n"
        "run()\n", output);

    EXPECT_EQ(output, "2\n");

    EXPECT_EQ(runs(counts, "FUSED_Pop+IncrLocal"), 3u);
    EXPECT_EQ(runs(counts, "FUSED_IncrLocal"), 0u);
}

// The Pop of a statement merges with those of the locals that end its body: what the statement
// stores, and how, must not depend on being the last
TEST_F(LoopShapes, AWriteThatEndsABodyKeepsItsFusion)
{
    MockLogger<TerminalSink> logger;

    std::string output;
    const auto counts = run(
        "fun run()\n"
        "{\n"
        "    var rows = [{v: 1}, {v: 2}, {v: 3}]\n"
        "    var seen = [0, 0, 0]\n"
        "    var total = 0\n"
        "\n"
        "    for (var i = 0; i < 3; i++)\n"
        "    {\n"
        "        var r = rows[i]\n"
        "        var d = r.v\n"
        "        total = total + d\n"
        "    }\n"
        "\n"
        "    for (var j = 0; j < 3; j++)\n"
        "    {\n"
        "        var r = rows[j]\n"
        "        var d = r.v\n"
        "        seen[j] = d * 2\n"
        "    }\n"
        "\n"
        "    var k = 0\n"
        "    while (k < 3)\n"
        "    {\n"
        "        var r = rows[k]\n"
        "        var d = r.v\n"
        "        k = k + d\n"
        "    }\n"
        "\n"
        "    __dprint(total)\n"
        "    __dprint(seen[2])\n"
        "    __dprint(k)\n"
        "}\n"
        "run()\n", output);

    EXPECT_EQ(output, "6\n6\n3\n");

    EXPECT_EQ(runs(counts, "OP_Set_Local"), 0u);
    EXPECT_EQ(runs(counts, "OP_Set_Index"), 0u);
    EXPECT_EQ(runs(counts, "FUSED_SetIndex(L,S,S)"), 3u);
    EXPECT_EQ(runs(counts, "FUSED_Pop+IncrLocal"), 6u);
    EXPECT_EQ(runsOfElided(counts), 0u);
}

// A jump landing on the PopN comes with all of it to pop: it stays whole. `and` lands there with
// the value of the statement, as the assignment does
TEST_F(LoopShapes, APopNAJumpLandsOnIsNotReduced)
{
    MockLogger<TerminalSink> logger;

    const std::vector<std::pair<std::string, std::string>> cases = {
        {"fun run()\n{\n    var x = 0\n    for (var i = 0; i < 3; i++)\n    {\n        var k = i\n        k == 1 and (x = k + 4)\n    }\n    __dprint(x)\n}\nrun()\n",
         "5\n"},
        {"fun run()\n{\n    var x = 0\n    for (var i = 0; i < 3; i++)\n    {\n        var k = i\n        var m = k\n        k == 1 or (x = x + m)\n    }\n    __dprint(x)\n}\nrun()\n",
         "2\n"},
        {"fun run()\n{\n    var v = [0, 0, 0]\n    for (var i = 0; i < 3; i++)\n    {\n        var k = i\n        k == 1 and (v[k] = 9)\n    }\n    __dprint(v[1])\n}\nrun()\n",
         "9\n"},

        // A value that comes through no fused store, a heap value left in the slot
        {"fun id(x)\n{\n    return x\n}\nfun run()\n{\n    var s = \"\"\n    var i = 0\n    while (i < 2)\n    {\n        var k = i\n        var t = {a: k}\n        s = id(\"a string long enough to live on the heap\")\n        i++\n    }\n    __dprint(s)\n}\nrun()\n",
         "a string long enough to live on the heap\n"},
    };

    for (const auto& [source, expected] : cases)
    {
        for (const bool fusion : {true, false})
        {
            std::string output;
            EXPECT_EQ(runScript(source, fusion, output), InterpretResult::OK) << source;
            EXPECT_EQ(output, expected) << source;
        }
    }
}

// The increment is where `continue` lands, after what it popped itself: the Pop must stay on its own
TEST_F(LoopShapes, AContinueLandsOnTheIncrementAlone)
{
    MockLogger<TerminalSink> logger;

    const std::string source =
        "fun run()\n"
        "{\n"
        "    var rows = [{v: 1}, {v: 2}, {v: 3}, {v: 4}]\n"
        "    var total = 0\n"
        "\n"
        "    for (var i = 0; i < 4; i++)\n"
        "    {\n"
        "        var r = rows[i]\n"
        "\n"
        "        if (r.v == 2)\n"
        "            continue\n"
        "\n"
        "        total = total + r.v\n"
        "    }\n"
        "\n"
        "    __dprint(total)\n"
        "}\n"
        "run()\n";

    for (const bool fusion : {true, false})
    {
        std::string output;
        EXPECT_EQ(runScript(source, fusion, output), InterpretResult::OK);
        EXPECT_EQ(output, "8\n");
    }
}

TEST_F(LoopShapes, AnIncrementOfWhatIsNoNumberStopsAfterThePop)
{
    MockLogger<TerminalSink> logger;

    const std::string source =
        "fun run()\n"
        "{\n"
        "    var i = \"text\"\n"
        "    while (true)\n"
        "    {\n"
        "        var kept = {v: 1}\n"
        "        __dprint(kept.v)\n"
        "        i++\n"
        "    }\n"
        "}\n"
        "run()\n";

    for (const bool fusion : {true, false})
    {
        std::string output;
        EXPECT_EQ(runScript(source, fusion, output), InterpretResult::RUNTIME_ERROR);
        EXPECT_EQ(output, "1\n");
    }
}

} // namespace test
} // namespace pg
