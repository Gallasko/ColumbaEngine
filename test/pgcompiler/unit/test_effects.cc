#include "gtest/gtest.h"

#include "Compiler/ast/analysis/locations.h"
#include "Compiler/ast/analysis/ssa.h"

#include "Interpreter/lexer.h"
#include "Interpreter/parser.h"

#include <algorithm>
#include <set>

namespace pg
{
    namespace test
    {
        using namespace ast::analysis;

        TEST(EffectLocations, PathsInternAndOverlap)
        {
            Locations l;
            const LocId a = l.global("a");
            const LocId ax = l.child(a, l.key("x")), axs = l.child(ax, Locations::AnyKey);
            EXPECT_EQ(l.text(axs), "g:a.x.*");
            EXPECT_EQ(l.child(a, l.key("x")), ax);  // interned
            const LocId amountAny = l.child(l.child(l.child(a, Locations::AnyKey), l.key("gains")), l.key("amount"));
            const LocId amountGains = l.child(l.child(l.child(a, l.key("gains")), l.key("gains")), l.key("amount"));
            EXPECT_TRUE(l.overlaps(amountAny, amountGains));
            EXPECT_FALSE(l.overlaps(ax, l.child(a, l.key("y"))));
            EXPECT_TRUE(l.overlaps(a, axs));  // a prefix overlaps below it
            EXPECT_TRUE(l.within(l.child(ax, l.key("y")), l.child(a, Locations::AnyKey)));
            EXPECT_FALSE(l.within(l.child(a, Locations::AnyKey), ax));
            EXPECT_FALSE(l.overlaps(ax, l.child(l.global("b"), l.key("x"))));
            LocId deepest = a;
            for (int i = 0; i < 8; ++i) deepest = l.child(deepest, l.key("k"));
            EXPECT_EQ(l.depth(deepest), MaxDepth);
            EXPECT_TRUE(l.deep(deepest));
            EXPECT_EQ(l.child(deepest, l.key("z")), deepest);
            EXPECT_EQ(l.text(l.child(l.param(0), l.paramKey(1))), "#0.[#1]");
            EXPECT_TRUE(l.overlaps(l.unknown(), ax));
        }

        TEST(EffectLocations, RootsAndText)
        {
            Locations l;
            int s1, s2;
            EXPECT_EQ(l.text(l.input("cfg")), "in:cfg");
            EXPECT_EQ(l.text(l.site(&s1)), "@0");
            EXPECT_EQ(l.text(l.site(&s2)), "@1");
            EXPECT_EQ(l.site(&s1), l.site(&s1));
            EXPECT_EQ(l.text(l.unknown()), "?");
            EXPECT_EQ(l.rootKind(l.param(2)), RootKind::Param);
            EXPECT_NE(l.global("a"), l.input("a"));
            const LocId p = l.child(l.child(l.child(l.param(0), l.paramKey(1)), Locations::AnyKey), l.key("amount"));
            EXPECT_EQ(l.text(p), "#0.[#1].*.amount");
            EXPECT_TRUE(l.isParamKey(l.keyOf(l.parent(l.parent(p)))));
            EXPECT_EQ(l.paramOfKey(l.keyOf(l.parent(l.parent(p)))), 1u);
            EXPECT_EQ(l.rootOf(p), l.param(0));
            EXPECT_TRUE(l.within(p, l.param(0)));
            EXPECT_FALSE(l.within(p, l.unknown()));
            EXPECT_FALSE(l.within(l.unknown(), l.unknown()));
        }

        TEST(EffectLocations, DeepFromCreationAndSeededAnyKey)
        {
            Locations l;
            LocId n = l.global("a");
            for (int i = 0; i < MaxDepth - 1; ++i) n = l.child(n, l.key("k"));
            EXPECT_FALSE(l.deep(n));
            n = l.child(n, l.key("k"));
            EXPECT_TRUE(l.deep(n));  // deep before any child is asked of it
            Locations fresh;
            EXPECT_EQ(fresh.text(fresh.child(fresh.param(0), Locations::AnyKey)), "#0.*");
        }

        namespace
        {
            // The top-level statements of a source
            std::vector<StatementPtr> parsed(const std::string& source)
            {
                Lexer lexer;
                lexer.readFromText(source);

                Parser parser(lexer.getTokens());
                auto statements = parser.parse();

                EXPECT_FALSE(parser.hasError()) << "test source failed to parse";

                std::vector<StatementPtr> out;

                while (not statements.empty())
                {
                    if (statements.front())
                        out.push_back(statements.front());

                    statements.pop();
                }

                return out;
            }

            // The IR of the source's first function; a name the source does not define is a native
            BodyIR firstFunction(const std::string& source, Locations& locs)
            {
                auto statements = parsed(source);
                std::set<std::string> defined;

                for (const auto& s : statements)
                {
                    if (s->kind() == AstKind::FunctionStatement)
                        defined.insert(std::static_pointer_cast<FunctionStatement>(s)->name.text);
                }

                for (const auto& s : statements)
                {
                    if (s->kind() == AstKind::FunctionStatement)
                        return buildFunction(*std::static_pointer_cast<FunctionStatement>(s), locs, [&](const std::string& n) { return defined.count(n) == 0; });
                }

                ADD_FAILURE() << "no function in the source";
                return {};
            }

            // The ops and names of the leaves of a value, in order
            std::vector<std::string> leafTexts(const BodyIR& ir, ValueId v)
            {
                std::vector<std::string> out;

                for (auto l : ir.leaves(v))
                {
                    const Node& n = ir.nodes[l];

                    switch (n.op)
                    {
                        case Op::Const: out.push_back("const " + n.name); break;
                        case Op::Pure: out.push_back("pure"); break;
                        case Op::Field: out.push_back("field"); break;
                        case Op::Param: out.push_back("param"); break;
                        case Op::Literal: out.push_back("literal"); break;
                        case Op::Call: out.push_back("call " + n.name); break;
                        case Op::Unknown: out.push_back("unknown"); break;
                        case Op::Global: out.push_back("global " + n.name); break;
                        default: out.push_back("other"); break;
                    }
                }

                return out;
            }
        }

        TEST(EffectSsa, LookupIsAPhiOfTheFieldAndTheDefault)
        {
            Locations locs;
            auto ir = firstFunction("fun f(t, k)\n{\n    var l = lookup(t, k, [])\n    return l\n}\n", locs);
            EXPECT_EQ(ir.dump(), "%0 = param 0\n%1 = param 1\n%2 = field %0 [#1]\n%3 = literal @0\n%4 = phi %2 %3\nreturn %4\n");
        }

        TEST(EffectSsa, AnIfMergesItsBranches)
        {
            Locations locs;
            auto ir = firstFunction("fun g(c)\n{\n    var x = 1\n    if (c)\n    {\n        x = 2\n    }\n    return x\n}\n", locs);
            ASSERT_EQ(ir.returns.size(), 1u);
            EXPECT_EQ(leafTexts(ir, ir.returns[0]), (std::vector<std::string>{"const 1", "const 2"}));
        }

        TEST(EffectSsa, ABreakReachesAfterTheLoop)
        {
            Locations locs;
            auto ir = firstFunction(
                "fun h(n)\n{\n    var x = 0\n    while (x < n)\n    {\n        if (x > 5)\n        {\n            x = 9\n            break\n        }\n"
                "        x = x + 1\n    }\n    return x\n}\n", locs);
            ASSERT_EQ(ir.returns.size(), 1u);
            EXPECT_EQ(leafTexts(ir, ir.returns[0]), (std::vector<std::string>{"const 0", "const 9", "pure"}));
        }

        TEST(EffectSsa, AContinueFeedsTheHeader)
        {
            Locations locs;
            auto ir = firstFunction(
                "fun c(n)\n{\n    var x = 0\n    for (var i = 0; i < n; i++)\n    {\n        if (i == 2)\n        {\n            x = 7\n            continue\n        }\n"
                "        x = 1\n    }\n    return x\n}\n", locs);
            ASSERT_EQ(ir.returns.size(), 1u);
            EXPECT_EQ(leafTexts(ir, ir.returns[0]), (std::vector<std::string>{"const 0", "const 7", "const 1"}));
        }

        TEST(EffectSsa, ALocalTableFieldIsTheValueWritten)
        {
            Locations locs;
            auto ir = firstFunction("fun t()\n{\n    var t = {x: 1}\n    t.x = 2\n    return t.x\n}\n", locs);
            ASSERT_EQ(ir.returns.size(), 1u);
            EXPECT_EQ(leafTexts(ir, ir.returns[0]), (std::vector<std::string>{"const 2"}));
            ASSERT_EQ(ir.stores.size(), 1u);
            EXPECT_EQ(ir.stores[0].key, locs.key("x"));
            EXPECT_EQ(ir.nodes[ir.stores[0].base].op, Op::Literal);
            EXPECT_EQ(ir.nodes[ir.stores[0].value].name, "2");
        }

        TEST(EffectSsa, AClosureMakesTheBodyUnknown)
        {
            Locations locs;
            auto ir = firstFunction("fun m(t)\n{\n    var g = fun()\n    {\n        t.x = 1\n    }\n    return 0\n}\n", locs);
            EXPECT_TRUE(ir.unknown);
        }

        TEST(EffectSsa, ALoopOverATableGivesItsEntries)
        {
            Locations locs;
            auto ir = firstFunction("fun e(t)\n{\n    for (var v : t)\n    {\n        push(v, 1)\n    }\n}\n", locs);
            ASSERT_EQ(ir.stores.size(), 1u);
            const Node& base = ir.nodes[ir.stores[0].base];
            EXPECT_EQ(base.op, Op::Field);
            EXPECT_EQ(base.key, Locations::AnyKey);
            EXPECT_EQ(base.in, (std::vector<ValueId>{0}));
            EXPECT_EQ(ir.stores[0].key, Locations::AnyKey);
            EXPECT_FALSE(ir.unknown);
        }

        TEST(EffectSsa, ATableHandedAwayIsReadFromItsField)
        {
            // Handed to a call, the table may be written: its field is read again, not remembered
            Locations locs;
            auto ir = firstFunction("fun a()\n{\n    var t = {x: 1}\n    t.x = 2\n    touch(t)\n    return t.x\n}\n", locs);
            ASSERT_EQ(ir.returns.size(), 1u);
            EXPECT_EQ(leafTexts(ir, ir.returns[0]), (std::vector<std::string>{"field"}));
        }

        TEST(EffectSsa, ALoopSeesWhatItsLaterCallWrote)
        {
            // The read comes first in the body, but the call of the turn before may have written it
            Locations locs;
            auto ir = firstFunction(
                "fun l(n)\n{\n    var t = {x: 1}\n    var y = 0\n    while (y < n)\n    {\n        y = t.x\n        touch(t)\n    }\n    return y\n}\n", locs);
            ASSERT_EQ(ir.returns.size(), 1u);
            EXPECT_EQ(leafTexts(ir, ir.returns[0]), (std::vector<std::string>{"const 0", "field"}));
        }

        TEST(EffectSsa, AGlobalAssignedIsARebind)
        {
            Locations locs;
            auto ir = firstFunction("fun w()\n{\n    lastAge = 4\n    count++\n}\n", locs);
            ASSERT_EQ(ir.rebinds.size(), 2u);
            EXPECT_EQ(ir.rebinds[0].first, "lastAge");
            EXPECT_EQ(ir.nodes[ir.rebinds[0].second].name, "4");
            EXPECT_EQ(ir.rebinds[1].first, "count");
            EXPECT_EQ(ir.nodes[ir.rebinds[1].second].op, Op::Pure);
        }

        TEST(EffectSsa, CallsOfNativesFunctionsAndValues)
        {
            auto statements = parsed("fun k(o, f)\n{\n    var n = len(o)\n    helper(o)\n    o.m(n)\n    f(2)\n}\n");
            ASSERT_EQ(statements.size(), 1u);
            Locations locs;
            auto kir = buildFunction(*std::static_pointer_cast<FunctionStatement>(statements[0]), locs, [](const std::string& n) { return n != "helper"; });

            std::vector<const Node*> calls;
            const Node* native = nullptr;

            for (const auto& n : kir.nodes)
            {
                if (n.op == Op::Call)
                    calls.push_back(&n);
                else if (n.op == Op::Native)
                    native = &n;
            }

            ASSERT_NE(native, nullptr);
            EXPECT_EQ(native->name, "len");
            EXPECT_EQ(nativeKind("len"), NativeKind::Fresh);
            EXPECT_EQ(nativeKind("helper"), NativeKind::None);

            ASSERT_EQ(calls.size(), 3u);
            EXPECT_EQ(calls[0]->name, "helper");
            EXPECT_EQ(calls[0]->in, (std::vector<ValueId>{0}));

            // A method: no name, its receiver handed with its arguments
            EXPECT_EQ(calls[1]->name, "");
            EXPECT_NE(std::find(calls[1]->in.begin(), calls[1]->in.end(), 0u), calls[1]->in.end());
            EXPECT_NE(calls[1]->origin, nullptr);

            // A local called: no name
            EXPECT_EQ(calls[2]->name, "");
        }

        TEST(EffectSsa, TopLevelDefinesAndRebinds)
        {
            Locations locs;
            auto statements = parsed("var a = make()\nb = 3\n{\n    var c = 1\n    c = 2\n}\n");
            ASSERT_EQ(statements.size(), 3u);
            auto native = [](const std::string&) { return true; };

            auto defined = buildTopLevel(statements[0], locs, native);
            ASSERT_EQ(defined.defines.size(), 1u);
            EXPECT_EQ(defined.defines[0].first, "a");
            EXPECT_EQ(defined.nodes[defined.defines[0].second].op, Op::Call);
            EXPECT_TRUE(defined.rebinds.empty());

            auto rebound = buildTopLevel(statements[1], locs, native);
            ASSERT_EQ(rebound.rebinds.size(), 1u);
            EXPECT_EQ(rebound.rebinds[0].first, "b");

            // A var of a block is a local, even at the top level
            auto block = buildTopLevel(statements[2], locs, native);
            EXPECT_TRUE(block.defines.empty());
            EXPECT_TRUE(block.rebinds.empty());
        }

        TEST(EffectSsa, ThisAndANestedFunAreUnknown)
        {
            Locations locs;
            auto ir = firstFunction("fun n()\n{\n    fun inner()\n    {\n        return 1\n    }\n    return inner()\n}\n", locs);
            EXPECT_TRUE(ir.unknown);
            // The nested fun is a local: calling it names no global
            ASSERT_EQ(ir.returns.size(), 1u);
            EXPECT_EQ(ir.nodes[ir.returns[0]].op, Op::Call);
            EXPECT_EQ(ir.nodes[ir.returns[0]].name, "");
        }
            TEST(EffectSsa, ThisIsUnknown)
        {
            Locations locs;
            auto ir = firstFunction("fun s()\n{\n    var a = this\n    return a\n}\n", locs);
            EXPECT_TRUE(ir.unknown);
            ASSERT_EQ(ir.returns.size(), 1u);
            EXPECT_EQ(ir.nodes[ir.returns[0]].op, Op::Unknown);
        }

        TEST(EffectSsa, TheDefaultOfALookupRunsBeforeItsField)
        {
            // touch(t) runs before the field is read and may set t.x: the field is not the 1 of the literal
            Locations locs;
            auto ir = firstFunction("fun f()\n{\n    var t = {x: 1}\n    var v = lookup(t, \"x\", touch(t))\n    return v\n}\n", locs);
            ASSERT_EQ(ir.returns.size(), 1u);
            EXPECT_EQ(leafTexts(ir, ir.returns[0]), (std::vector<std::string>{"call touch", "field"}));
        }

        TEST(EffectSsa, ANameBoundOnOneSideIsUnknownOnTheOther)
        {
            // `if (c) var x = 1` without a block: the parser takes no var there, so it is grafted
            auto statements = parsed("fun g(c)\n{\n    if (c)\n    {\n        c = 1\n    }\n    return x\n}\n");
            auto bare = parsed("var x = 1\n");
            ASSERT_EQ(statements.size(), 1u);
            ASSERT_EQ(bare.size(), 1u);

            auto f = std::static_pointer_cast<FunctionStatement>(statements[0]);
            auto body = std::static_pointer_cast<BlockStatement>(f->body);
            ASSERT_EQ(body->statements.front()->kind(), AstKind::IfStatement);
            std::static_pointer_cast<IfStatement>(body->statements.front())->thenBranch = bare[0];

            Locations locs;
            auto ir = buildFunction(*f, locs, [](const std::string&) { return true; });
            ASSERT_EQ(ir.returns.size(), 1u);
            EXPECT_EQ(leafTexts(ir, ir.returns[0]), (std::vector<std::string>{"const 1", "unknown"}));
        }

        TEST(EffectSsa, AnAssignmentInsideAndReachesTheJoin)
        {
            Locations locs;
            auto ir = firstFunction("fun a(c)\n{\n    var z = 0\n    var w = c or (z = 5)\n    return z\n}\n", locs);
            ASSERT_EQ(ir.returns.size(), 1u);
            EXPECT_EQ(leafTexts(ir, ir.returns[0]), (std::vector<std::string>{"const 0", "const 5"}));
        }

        TEST(EffectSsa, AnInnerLoopsBreakFeedsTheOuterHeader)
        {
            Locations locs;
            auto ir = firstFunction(
                "fun n(a)\n{\n    var y = 0\n    var i = 0\n    while (i < a)\n    {\n        while (true)\n        {\n            y = 3\n            break\n        }\n"
                "        i = i + 1\n    }\n    return y\n}\n", locs);
            ASSERT_EQ(ir.returns.size(), 1u);

            // y after the outer loop is its header phi: what it was before, and what the inner break left
            const Node& header = ir.nodes[ir.returns[0]];
            EXPECT_EQ(header.op, Op::Phi);
            EXPECT_EQ(header.in.size(), 2u);
            EXPECT_EQ(leafTexts(ir, ir.returns[0]), (std::vector<std::string>{"const 0", "const 3"}));
        }

        TEST(EffectSsa, InsertAndMergeStore)
        {
            Locations locs;
            auto ir = firstFunction("fun i(l, v)\n{\n    insert(l, 0, v)\n    merge(l, v)\n}\n", locs);
            ASSERT_EQ(ir.stores.size(), 2u);
            EXPECT_EQ(ir.stores[0].base, 0u);
            EXPECT_EQ(ir.stores[0].key, Locations::AnyKey);
            EXPECT_EQ(ir.stores[0].value, 1u);

            // merge puts what its second holds in its first
            EXPECT_EQ(ir.stores[1].base, 0u);
            EXPECT_EQ(ir.stores[1].key, Locations::AnyKey);
            const Node& held = ir.nodes[ir.stores[1].value];
            EXPECT_EQ(held.op, Op::Field);
            EXPECT_EQ(held.key, Locations::AnyKey);
            EXPECT_EQ(held.in, (std::vector<ValueId>{1}));
        }

        TEST(EffectSsa, AChainedSetWritesTheFieldRead)
        {
            Locations locs;
            auto ir = firstFunction("fun s(a, v)\n{\n    a.b.c = v\n    a.b.c.d = v\n}\n", locs);
            ASSERT_EQ(ir.stores.size(), 2u);

            const Node& ab = ir.nodes[ir.stores[0].base];
            EXPECT_EQ(ab.op, Op::Field);
            EXPECT_EQ(ab.key, locs.key("b"));
            EXPECT_EQ(ab.in, (std::vector<ValueId>{0}));
            EXPECT_EQ(ir.stores[0].key, locs.key("c"));

            // a.b.c: a field of a field
            const Node& abc = ir.nodes[ir.stores[1].base];
            EXPECT_EQ(abc.op, Op::Field);
            EXPECT_EQ(abc.key, locs.key("c"));
            ASSERT_EQ(abc.in.size(), 1u);
            EXPECT_EQ(ir.nodes[abc.in[0]].op, Op::Field);
            EXPECT_EQ(ir.stores[1].key, locs.key("d"));
        }
    }
}
