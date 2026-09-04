#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "Compiler/ast/pass/entity_loop_lowering.h"

#include "ECS/entitysystem.h"

#include "Interpreter/lexer.h"
#include "Interpreter/parser.h"

#include "../../mocklogger.h"

#include <string>

namespace pg {
namespace test {

/**
 * Entity-loop lowering: transform-level assertions for the match and every
 * bail rule, plus behavioral Pratt-vs-Ast comparisons over script-driven
 * standard components.
 */
class EntityLoopLowering : public ::testing::Test
{
protected:
    /** Parse source, run ONLY the lowering pass, return the printed AST */
    std::string transform(const std::string& source, bool& changed)
    {
        Lexer lexer;
        lexer.readFromText(source);

        Parser parser(lexer.getTokens());
        auto statements = parser.parse();

        EXPECT_FALSE(parser.hasError()) << "test source failed to parse";

        EntityLoopLoweringPass pass;
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

    void expectLowered(const std::string& source)
    {
        bool changed = false;
        auto printed = transform(source, changed);

        EXPECT_TRUE(changed) << printed;
        EXPECT_NE(printed.find("__ecsEntityIds"), std::string::npos) << printed;
        EXPECT_NE(printed.find("__ecsEntityView"), std::string::npos) << printed;
        EXPECT_NE(printed.find("@eid0"), std::string::npos) << printed;
    }

    void expectNotLowered(const std::string& source)
    {
        bool changed = false;
        auto printed = transform(source, changed);

        EXPECT_FALSE(changed) << printed;
        EXPECT_EQ(printed.find("__ecsEntityIds"), std::string::npos) << printed;
    }

    /** Run source at O3 with the ecs module available; returns __dprint output */
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

    /**
     * Both front-ends must produce this exact output. Comparing against a
     * literal (not just each other) guards against vacuous agreement if the
     * ECS setup silently produced nothing.
     */
    void expectBothFrontEndsOutput(const std::string& source, const std::string& expected)
    {
        InterpretResult prattResult, astResult;

        auto prattOut = trim(runWith(source, ScriptFrontEnd::Pratt, prattResult));
        auto astOut = trim(runWith(source, ScriptFrontEnd::Ast, astResult));

        EXPECT_EQ(prattResult, InterpretResult::OK);
        EXPECT_EQ(astResult, InterpretResult::OK);
        EXPECT_EQ(prattOut, expected);
        EXPECT_EQ(astOut, expected);
    }

    static std::string trim(const std::string& str)
    {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, last - first + 1);
    }

    /** Shared ECS setup: a script-defined Health component on 3 entities */
    static std::string ecsSetup()
    {
        return
            "import \"ecs\"\n"
            "var sys = createSystem(\"TestSys\")\n"
            "sys.ownComponent(\"Health\").build()\n"
            "var e1 = createEntity()\n"
            "e1.attachComp(\"Health\", \"hp\", 10)\n"
            "var e2 = createEntity()\n"
            "e2.attachComp(\"Health\", \"hp\", 20)\n"
            "var e3 = createEntity()\n"
            "e3.attachComp(\"Health\", \"hp\", 30)\n";
    }
};

// ---------------------------------------------------------------------------
// Transform-level: the match
// ---------------------------------------------------------------------------

TEST_F(EntityLoopLowering, LowersCanonicalLoop)
{
    MockLogger<TerminalSink> logger;

    expectLowered(
        "for (var e : getEntities(\"Position\"))\n"
        "{\n"
        "    var p = e.PositionComponent\n"
        "    p.x = p.x + 1\n"
        "}\n");
}

TEST_F(EntityLoopLowering, LowersBracketMemberForm)
{
    MockLogger<TerminalSink> logger;

    // The common scripting form: e["PositionComponent"]
    bool changed = false;
    auto printed = transform(
        "for (var e : getEntities(\"Asteroid\"))\n"
        "{\n"
        "    var pos = e[\"PositionComponent\"]\n"
        "    var data = e[\"Asteroid\"]\n"
        "    pos.x = pos.x + data.vx\n"
        "}\n", changed);

    EXPECT_TRUE(changed) << printed;
    EXPECT_NE(printed.find("__ecsEntityView"), std::string::npos) << printed;
    // Both accessed members are requested from the view
    EXPECT_NE(printed.find("PositionComponent"), std::string::npos) << printed;
    EXPECT_NE(printed.find("Asteroid"), std::string::npos) << printed;
}

TEST_F(EntityLoopLowering, LowersNestedLoopsIndependently)
{
    MockLogger<TerminalSink> logger;

    bool changed = false;
    auto printed = transform(
        "for (var a : getEntities(\"A\"))\n"
        "{\n"
        "    for (var b : getEntities(\"B\"))\n"
        "    {\n"
        "        var x = a.A\n"
        "        var y = b.B\n"
        "    }\n"
        "}\n", changed);

    EXPECT_TRUE(changed) << printed;
    EXPECT_NE(printed.find("@eid0"), std::string::npos) << printed;
    EXPECT_NE(printed.find("@eid1"), std::string::npos) << printed;
}

// ---------------------------------------------------------------------------
// Transform-level: every bail rule
// ---------------------------------------------------------------------------

TEST_F(EntityLoopLowering, BailsOnShadowedGetEntities)
{
    MockLogger<TerminalSink> logger;

    // Script-defined getEntities: the whole program must be left alone
    expectNotLowered(
        "fun getEntities(name)\n"
        "{\n"
        "    return [1, 2]\n"
        "}\n"
        "for (var e : getEntities(\"Position\"))\n"
        "{\n"
        "    var p = e.PositionComponent\n"
        "}\n");

    // Assignment counts as shadowing too
    expectNotLowered(
        "getEntities = 5\n"
        "for (var e : getEntities(\"Position\"))\n"
        "{\n"
        "    var p = e.PositionComponent\n"
        "}\n");
}

TEST_F(EntityLoopLowering, BailsOnEscapingLoopVar)
{
    MockLogger<TerminalSink> logger;

    // Passed bare to a call
    expectNotLowered(
        "for (var e : getEntities(\"P\"))\n"
        "{\n"
        "    __dprint(e)\n"
        "}\n");

    // Stored in a variable
    expectNotLowered(
        "for (var e : getEntities(\"P\"))\n"
        "{\n"
        "    var copy = e\n"
        "}\n");

    // Captured by a nested function
    expectNotLowered(
        "for (var e : getEntities(\"P\"))\n"
        "{\n"
        "    fun grab()\n"
        "    {\n"
        "        return e.P\n"
        "    }\n"
        "}\n");

    // Dynamic index key
    expectNotLowered(
        "var key = \"P\"\n"
        "for (var e : getEntities(\"P\"))\n"
        "{\n"
        "    var p = e[key]\n"
        "}\n");
}

TEST_F(EntityLoopLowering, BailsOnMutatedLoopVarAndSpecialMembers)
{
    MockLogger<TerminalSink> logger;

    // Reassigned
    expectNotLowered(
        "for (var e : getEntities(\"P\"))\n"
        "{\n"
        "    e = 0\n"
        "}\n");

    // has() needs the eager table's native closure
    expectNotLowered(
        "for (var e : getEntities(\"P\"))\n"
        "{\n"
        "    if (e.has(\"Q\")) { var q = e.Q }\n"
        "}\n");

    // attachComp() likewise
    expectNotLowered(
        "for (var e : getEntities(\"P\"))\n"
        "{\n"
        "    e.attachComp(\"Q\", \"v\", 1)\n"
        "}\n");
}

TEST_F(EntityLoopLowering, DoesNotTouchNonGetEntitiesLoops)
{
    MockLogger<TerminalSink> logger;

    // Method-call form is conservatively not matched
    expectNotLowered(
        "for (var e : ecs.getEntities(\"P\"))\n"
        "{\n"
        "    var p = e.P\n"
        "}\n");

    // Plain table iteration
    expectNotLowered(
        "var t = {1, 2, 3}\n"
        "for (var k : t)\n"
        "{\n"
        "    __dprint(k)\n"
        "}\n");

    // Intermediate variable form is out of v1 scope
    expectNotLowered(
        "var entities = getEntities(\"P\")\n"
        "for (var e : entities)\n"
        "{\n"
        "    var p = e.P\n"
        "}\n");
}

// ---------------------------------------------------------------------------
// Behavioral: Pratt and Ast agree on real ECS iteration
// ---------------------------------------------------------------------------

TEST_F(EntityLoopLowering, BehaviorMatchesOnRealComponents)
{
    MockLogger<TerminalSink> logger;

    // Read loop: sum of hp
    expectBothFrontEndsOutput(ecsSetup() +
        "var total = 0\n"
        "for (var e : getEntities(\"Health\"))\n"
        "{\n"
        "    var h = e.Health\n"
        "    total = total + h.hp\n"
        "}\n"
        "__dprint(total)\n",
        "60");

    // Write loop: mutate through the proxy, then re-read
    expectBothFrontEndsOutput(ecsSetup() +
        "for (var e : getEntities(\"Health\"))\n"
        "{\n"
        "    var h = e.Health\n"
        "    h.hp = h.hp + 1\n"
        "}\n"
        "var total = 0\n"
        "for (var e : getEntities(\"Health\"))\n"
        "{\n"
        "    var h = e.Health\n"
        "    total = total + h.hp\n"
        "}\n"
        "__dprint(total)\n",
        "63");

    // break/continue inside the lowered loop
    expectBothFrontEndsOutput(ecsSetup() +
        "var total = 0\n"
        "for (var e : getEntities(\"Health\"))\n"
        "{\n"
        "    var h = e.Health\n"
        "    if (h.hp == 20) { continue; }\n"
        "    if (h.hp == 30) { break; }\n"
        "    total = total + h.hp\n"
        "}\n"
        "__dprint(total)\n",
        "10");

    // Empty component set
    expectBothFrontEndsOutput(
        "import \"ecs\"\n"
        "var sys = createSystem(\"EmptySys\")\n"
        "sys.ownComponent(\"Ghost\").build()\n"
        "var n = 0\n"
        "for (var e : getEntities(\"Ghost\"))\n"
        "{\n"
        "    n = n + 1\n"
        "}\n"
        "__dprint(n)\n",
        "0");

    // __entityId access (always present on the lazy view)
    expectBothFrontEndsOutput(ecsSetup() +
        "var n = 0\n"
        "for (var e : getEntities(\"Health\"))\n"
        "{\n"
        "    if (e.__entityId > 0) { n = n + 1 }\n"
        "}\n"
        "__dprint(n)\n",
        "3");
}

TEST_F(EntityLoopLowering, ShadowedGetEntitiesBehaviorAgrees)
{
    MockLogger<TerminalSink> logger;

    // A script-defined getEntities must behave identically on both
    // front-ends (the pass must not mis-lower it)
    expectBothFrontEndsOutput(
        "fun getEntities(name)\n"
        "{\n"
        "    return [5, 6, 7]\n"
        "}\n"
        "var total = 0\n"
        "for (var v : getEntities(\"whatever\"))\n"
        "{\n"
        "    total = total + v\n"
        "}\n"
        "__dprint(total)\n",
        "18");
}

// ---------------------------------------------------------------------------
// Documented divergence, pinned AST-only: entity deleted mid-loop yields an
// id-only view (deterministic), instead of the eager path's stale table
// ---------------------------------------------------------------------------

TEST_F(EntityLoopLowering, AstOnlyMidLoopDeleteKeepsSnapshotIteration)
{
    MockLogger<TerminalSink> logger;

    InterpretResult result;

    // Deleting an entity mid-loop must not shrink the iteration: the id
    // snapshot still visits it, and its view (id-only) is safe as long as
    // the body doesn't dereference the missing component
    auto out = trim(runWith(ecsSetup() +
        "var id2 = e2.__entityId\n"
        "var visited = 0\n"
        "for (var e : getEntities(\"Health\"))\n"
        "{\n"
        "    removeEntity(id2)\n"
        "    visited = visited + 1\n"
        "}\n"
        "__dprint(visited)\n", ScriptFrontEnd::Ast, result));

    EXPECT_EQ(result, InterpretResult::OK);
    EXPECT_EQ(out, "3");
}

} // namespace test
} // namespace pg
