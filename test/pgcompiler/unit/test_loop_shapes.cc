#include "gtest/gtest.h"

#include "Compiler/vm.h"

#include "ECS/entitysystem.h"

#include "../../mocklogger.h"

#include <map>
#include <string>

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

} // namespace test
} // namespace pg
