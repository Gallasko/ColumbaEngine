#include "gtest/gtest.h"

#include "Compiler/ast/analysis/locations.h"
#include "Compiler/ast/analysis/ssa.h"
#include "Compiler/ast/analysis/summaries.h"
#include "Compiler/ast/pass/effects.h"

#include "Interpreter/lexer.h"
#include "Interpreter/parser.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>

namespace pg
{
    namespace test
    {
        using namespace ast::analysis;
        using ast::StatementEffects;

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

        namespace
        {
            // Every function of a source summed up, each one followed; the statements live as long as the IR
            struct Summed
            {
                std::vector<StatementPtr> statements;
                Locations locs;
                std::map<std::string, BodyIR> bodies;
                std::map<std::string, Summary> summaries;
                size_t collapsed = 0;

                const Summary& of(const std::string& name) const { return summaries.at(name); }

                std::set<std::string> writeTexts(const std::string& name) const
                {
                    std::set<std::string> out;

                    for (const auto& w : of(name).writes)
                        out.insert(locs.text(w.at));

                    return out;
                }

                const Write* write(const std::string& name, const std::string& text) const
                {
                    for (const auto& w : of(name).writes)
                    {
                        if (locs.text(w.at) == text)
                            return &w;
                    }

                    return nullptr;
                }

                bool hasEdge(const std::string& name, const std::string& at, const std::string& value) const
                {
                    for (const auto& e : of(name).edges)
                    {
                        if (locs.text(e.at) == at and locs.text(e.value) == value)
                            return true;
                    }

                    return false;
                }
            };

            std::unique_ptr<Summed> summed(const std::string& source)
            {
                auto out = std::make_unique<Summed>();
                out->statements = parsed(source);
                std::map<std::string, int> defined;

                for (const auto& s : out->statements)
                {
                    if (s->kind() == AstKind::FunctionStatement)
                        ++defined[std::static_pointer_cast<FunctionStatement>(s)->name.text];
                }

                auto isNative = [&defined](const std::string& n) { return defined.count(n) == 0; };

                for (const auto& s : out->statements)
                {
                    if (s->kind() == AstKind::FunctionStatement)
                    {
                        auto f = std::static_pointer_cast<FunctionStatement>(s);
                        out->bodies[f->name.text] = buildFunction(*f, out->locs, isNative);
                    }
                }

                auto once = [&defined](const std::string& n) { auto it = defined.find(n); return it != defined.end() and it->second == 1; };
                auto isDefined = [&defined](const std::string& n) { return defined.count(n) > 0; };
                out->summaries = summarize(out->bodies, out->locs, once, isDefined, out->collapsed);
                return out;
            }

            const std::string amountsOfSource =
                "fun amountsOf(table, key)\n"
                "{\n"
                "    var list = lookup(table, key, [])\n"
                "\n"
                "    for (var i = 0; i < len(list); i++)\n"
                "    {\n"
                "        list[i].amount = lookup(list[i], \"amount\", 1)\n"
                "    }\n"
                "\n"
                "    return list\n"
                "}\n";
        }

        TEST(EffectSummaries, AmountsOfWritesTheAmountOfItsListEntriesIdempotently)
        {
            auto s = summed(amountsOfSource);
            const Summary& a = s->of("amountsOf");

            // The rows' entries, and the fresh default, which escapes through the return
            EXPECT_EQ(s->writeTexts("amountsOf"), (std::set<std::string>{"#0.[#1].*.amount", "@0.*.amount"}));

            for (const auto& w : a.writes)
            {
                EXPECT_TRUE(w.idempotence.shape) << s->locs.text(w.at);
                EXPECT_TRUE(w.idempotence.needsParams.empty()) << s->locs.text(w.at);
                EXPECT_TRUE(w.idempotence.needsGlobals.empty()) << s->locs.text(w.at);
            }

            std::set<std::string> returns;

            for (auto r : a.returns)
                returns.insert(s->locs.text(r));

            EXPECT_TRUE(returns.count("#0.[#1]"));
            EXPECT_TRUE(a.edges.empty());  // the amount written back is the one already there
            EXPECT_FALSE(a.unknown);
            EXPECT_EQ(a.arity, 2u);
            EXPECT_EQ(s->collapsed, 0u);
        }

        TEST(EffectSummaries, ACounterIsNotIdempotent)
        {
            auto s = summed("fun bump(t)\n{\n    t.count = t.count + 1\n}\n");
            const Write* w = s->write("bump", "#0.count");
            ASSERT_NE(w, nullptr);
            EXPECT_FALSE(w->idempotence.shape);
            EXPECT_EQ(s->collapsed, 0u);
        }

        TEST(EffectSummaries, AFlagIsIdempotent)
        {
            auto s = summed("fun mark(t)\n{\n    t.done = true\n}\n");
            const Write* w = s->write("mark", "#0.done");
            ASSERT_NE(w, nullptr);
            EXPECT_TRUE(w->idempotence.shape);
            EXPECT_TRUE(w->idempotence.needsParams.empty());
            EXPECT_EQ(s->collapsed, 0u);
        }

        TEST(EffectSummaries, AParameterStoredNeedsItsArgument)
        {
            auto s = summed("fun put(t, v)\n{\n    t.x = v\n}\n");
            const Write* w = s->write("put", "#0.x");
            ASSERT_NE(w, nullptr);
            EXPECT_TRUE(w->idempotence.shape);
            EXPECT_EQ(w->idempotence.needsParams, (std::set<uint32_t>{1}));
            EXPECT_TRUE(s->hasEdge("put", "#0.x", "#1"));
            EXPECT_EQ(s->collapsed, 0u);
        }

        TEST(EffectSummaries, AGlobalStoredNeedsTheGlobal)
        {
            auto s = summed("fun put(t)\n{\n    t.x = level\n}\n");
            const Write* w = s->write("put", "#0.x");
            ASSERT_NE(w, nullptr);
            EXPECT_TRUE(w->idempotence.shape);
            EXPECT_EQ(w->idempotence.needsGlobals, (std::set<std::string>{"level"}));
            EXPECT_TRUE(s->of("put").reads.count("level"));
            EXPECT_EQ(s->collapsed, 0u);
        }

        TEST(EffectSummaries, APushIsNotIdempotent)
        {
            auto s = summed("fun add(l, v)\n{\n    push(l, v)\n}\n");
            const Summary& a = s->of("add");
            ASSERT_FALSE(a.writes.empty());
            bool within = false;

            for (const auto& w : a.writes)
            {
                within = within or s->locs.within(w.at, s->locs.param(0));
                EXPECT_FALSE(w.idempotence.shape) << s->locs.text(w.at);
            }

            EXPECT_TRUE(within);
            EXPECT_TRUE(s->hasEdge("add", "#0.*", "#1"));
            EXPECT_EQ(s->collapsed, 0u);
        }

        TEST(EffectSummaries, ACallSubstitutesTheKey)
        {
            auto s = summed("fun g(a)\n{\n    amountsOf(a, \"gains\")\n}\n\n" + amountsOfSource);
            EXPECT_TRUE(s->writeTexts("g").count("#0.gains.*.amount"));
            const Write* w = s->write("g", "#0.gains.*.amount");
            ASSERT_NE(w, nullptr);
            EXPECT_TRUE(w->idempotence.shape);
            EXPECT_FALSE(s->of("g").unknown);
            EXPECT_EQ(s->collapsed, 0u);
        }

        TEST(EffectSummaries, RecursionConverges)
        {
            auto s = summed("fun walk(t)\n{\n    if (t.next)\n    {\n        walk(t.next)\n    }\n\n    t.seen = 1\n}\n");
            const Summary& w = s->of("walk");
            EXPECT_TRUE(s->writeTexts("walk").count("#0.seen"));
            const LocId next = s->locs.child(s->locs.param(0), s->locs.key("next"));
            bool below = false;

            for (const auto& write : w.writes)
                below = below or s->locs.within(write.at, next);

            EXPECT_TRUE(below);
            EXPECT_FALSE(w.unknown);
            EXPECT_EQ(s->collapsed, 0u);
        }

        TEST(EffectSummaries, AValueCalledIsUnknown)
        {
            auto s = summed("fun cb(f, t)\n{\n    f(t)\n}\n");
            EXPECT_TRUE(s->of("cb").unknown);
            const Write* w = s->write("cb", "#1.*");
            ASSERT_NE(w, nullptr);
            EXPECT_FALSE(w->idempotence.shape);
            EXPECT_EQ(s->collapsed, 0u);
        }

        TEST(EffectSummaries, AClosureIsUnknown)
        {
            auto s = summed("fun mk(t)\n{\n    var g = fun()\n    {\n        t.x = 1\n    }\n\n    return 0\n}\n");
            EXPECT_TRUE(s->of("mk").unknown);
            EXPECT_TRUE(s->writeTexts("mk").count("?"));
            EXPECT_EQ(s->collapsed, 0u);
        }

        TEST(EffectSummaries, AnUnknownCallMayStoreOneOperandInAnother)
        {
            auto s = summed("fun h(a, b)\n{\n    helper(a, b)\n}\n");
            EXPECT_TRUE(s->of("h").unknown);
            EXPECT_TRUE(s->writeTexts("h").count("#0.*"));
            EXPECT_TRUE(s->writeTexts("h").count("#1.*"));
            EXPECT_TRUE(s->hasEdge("h", "#0.*", "#1"));
            EXPECT_TRUE(s->hasEdge("h", "#1.*", "#0.*"));
        }

        TEST(EffectSummaries, IdempotenceThroughACall)
        {
            // put's write needs its argument 1: a constant at one call, a counter at the other
            auto s = summed("fun put(t, v)\n{\n    t.x = v\n}\n\n"
                            "fun good(t)\n{\n    put(t, 3)\n}\n\n"
                            "fun bad(t)\n{\n    put(t, t.x + 1)\n}\n\n"
                            "fun passes(t, w)\n{\n    put(t, w)\n}\n");
            const Write* good = s->write("good", "#0.x");
            ASSERT_NE(good, nullptr);
            EXPECT_TRUE(good->idempotence.shape);
            const Write* bad = s->write("bad", "#0.x");
            ASSERT_NE(bad, nullptr);
            EXPECT_FALSE(bad->idempotence.shape);
            const Write* passes = s->write("passes", "#0.x");
            ASSERT_NE(passes, nullptr);
            EXPECT_TRUE(passes->idempotence.shape);
            EXPECT_EQ(passes->idempotence.needsParams, (std::set<uint32_t>{1}));
            EXPECT_TRUE(s->hasEdge("passes", "#0.x", "#1"));
        }

        TEST(EffectSummaries, AParamKeyMadeAnyAtTheCallIsNotIdempotent)
        {
            auto s = summed("fun g(a, i)\n{\n    amountsOf(a, i + 1)\n}\n\n" + amountsOfSource);
            const Write* w = s->write("g", "#0.*.*.amount");
            ASSERT_NE(w, nullptr);
            EXPECT_TRUE(w->idempotence.shape);  // the last key is still `amount`
            auto t = summed("fun set(t, k)\n{\n    t[k] = 1\n}\n\nfun g(a, i)\n{\n    set(a, i + 1)\n}\n");
            const Write* named = t->write("set", "#0.[#1]");
            ASSERT_NE(named, nullptr);
            EXPECT_TRUE(named->idempotence.shape);
            const Write* any = t->write("g", "#0.*");
            ASSERT_NE(any, nullptr);
            EXPECT_FALSE(any->idempotence.shape);
        }

        TEST(EffectSummaries, ACopyOfAnotherEntryIsNotTheOwnValue)
        {
            auto s = summed("fun shift(l)\n{\n    for (var i = 0; i < len(l); i++)\n    {\n        l[i].v = l[i + 1].v\n    }\n}\n");
            const Write* w = s->write("shift", "#0.*.v");
            ASSERT_NE(w, nullptr);
            EXPECT_FALSE(w->idempotence.shape);
        }

        TEST(EffectSummaries, AValueCarriedRoundALoopIsNotTheOwnValue)
        {
            auto s = summed("fun rot(l)\n{\n    var prev = 0\n\n    for (var i = 0; i < len(l); i++)\n    {\n"
                            "        var o = l[i].x\n        l[i].x = prev\n        prev = o\n    }\n}\n");
            const Write* w = s->write("rot", "#0.*.x");
            ASSERT_NE(w, nullptr);
            EXPECT_FALSE(w->idempotence.shape);
        }

        TEST(EffectSummaries, AValueCarriedToAnotherObjectIsAnEdge)
        {
            auto s = summed("fun mv(a, b)\n{\n    var cur = a\n    var prev = 0\n\n    for (var i = 0; i < 2; i++)\n    {\n"
                            "        var o = cur.x\n        cur.x = prev\n        prev = o\n        cur = b\n    }\n}\n");
            EXPECT_TRUE(s->hasEdge("mv", "#1.x", "#0.x"));
        }

        TEST(EffectSummaries, ACallOfAFunctionNotFollowedMayWriteAnything)
        {
            auto s = summed("fun dupWrite()\n{\n    rows.x = 1\n}\n\nfun dupWrite()\n{\n    rows.y = 2\n}\n\nfun top1()\n{\n    dupWrite()\n}\n");
            EXPECT_EQ(s->summaries.count("dupWrite"), 0u);
            EXPECT_TRUE(s->of("top1").unknown);
            EXPECT_TRUE(s->writeTexts("top1").count("?"));
        }

        TEST(EffectSummaries, AValueCalledMayWriteAnythingAndReturnAnything)
        {
            auto s = summed("fun cb(f, t)\n{\n    return f(t)\n}\n");
            EXPECT_TRUE(s->writeTexts("cb").count("?"));
            ASSERT_EQ(s->of("cb").returns.size(), 1u);
            EXPECT_EQ(s->locs.text(s->of("cb").returns[0]), "?");
        }

        TEST(EffectSummaries, AnEngineNativeSeesOnlyWhatItIsHanded)
        {
            auto s = summed("fun t(a)\n{\n    return someNative(a)\n}\n");
            EXPECT_TRUE(s->of("t").unknown);
            EXPECT_EQ(s->writeTexts("t"), (std::set<std::string>{"#0.*"}));
            std::set<std::string> returns;

            for (auto r : s->of("t").returns)
                returns.insert(s->locs.text(r));

            // What it is handed, and a table of its own
            EXPECT_EQ(returns, (std::set<std::string>{"#0", "#0.*", "in:@0"}));
        }

        TEST(EffectSummaries, ADefineIsAnEdgeOfItsGlobal)
        {
            Locations locs;
            auto statements = parsed("var index = rows[0]\n");
            ASSERT_EQ(statements.size(), 1u);
            auto ir = buildTopLevel(statements[0], locs, [](const std::string&) { return true; });

            const std::map<std::string, Summary> none;
            const std::set<std::string> nothing;
            Evaluator evaluator(locs, none, [nothing](const std::string& n) { return nothing.count(n) > 0; },
                                [nothing](const std::string& n) { return nothing.count(n) > 0; });

            const Summary s = evaluator.evaluate(ir);
            ASSERT_EQ(s.edges.size(), 1u);
            EXPECT_EQ(locs.text(s.edges[0].at), "g:index");
            EXPECT_EQ(locs.text(s.edges[0].value), "g:rows.*");
            EXPECT_TRUE(s.rebinds.empty());
        }

        TEST(EffectSummaries, AnArgumentThisBodyWritesIsNotInvariant)
        {
            auto s = summed("fun selfLen(t)\n{\n    t.n = len(t)\n}\n");
            const Write* w = s->write("selfLen", "#0.n");
            ASSERT_NE(w, nullptr);
            EXPECT_FALSE(w->idempotence.shape);

            auto c = summed("fun add(a, b)\n{\n    a.n = b.n + 1\n}\n\nfun twice(t)\n{\n    add(t, t)\n}\n");
            const Write* inner = c->write("add", "#0.n");
            ASSERT_NE(inner, nullptr);
            EXPECT_TRUE(inner->idempotence.shape);
            EXPECT_EQ(inner->idempotence.needsParams, (std::set<uint32_t>{1}));
            const Write* twice = c->write("twice", "#0.n");
            ASSERT_NE(twice, nullptr);
            EXPECT_FALSE(twice->idempotence.shape);
        }

        TEST(EffectSummaries, AGlobalNeededAndWrittenIsNotInvariant)
        {
            auto s = summed("fun put(t)\n{\n    t.x = level\n}\n\nfun both()\n{\n    put(level)\n}\n");
            const Write* w = s->write("both", "g:level.x");
            ASSERT_NE(w, nullptr);
            EXPECT_FALSE(w->idempotence.shape);
        }

        TEST(EffectSummaries, AFieldOfAnObjectWrittenElsewhereStaysInvariant)
        {
            auto s = summed("fun copyField(t)\n{\n    t.y = t.x\n}\n");
            const Write* w = s->write("copyField", "#0.y");
            ASSERT_NE(w, nullptr);
            EXPECT_TRUE(w->idempotence.shape);
            EXPECT_EQ(w->idempotence.needsParams, (std::set<uint32_t>{0}));
        }

        TEST(EffectSummaries, ASplitListIsATableThatCanBeWritten)
        {
            auto s = summed("fun viaFresh(s, r)\n{\n    var l = split(s, \",\")\n    push(l, r)\n    l[1].x = 5\n}\n");
            EXPECT_TRUE(s->writeTexts("viaFresh").count("@0.*"));
            EXPECT_TRUE(s->writeTexts("viaFresh").count("@0.*.x"));
            EXPECT_TRUE(s->hasEdge("viaFresh", "@0.*", "#1"));

            // A new list on each run, as a literal is
            auto k = summed("fun keep(t, s)\n{\n    t.parts = split(s, \",\")\n}\n");
            const Write* w = k->write("keep", "#0.parts");
            ASSERT_NE(w, nullptr);
            EXPECT_FALSE(w->idempotence.shape);
        }

        TEST(EffectSummaries, AnEntryMovedToAnotherEntryIsASelfEdge)
        {
            auto s = summed("fun shift(l)\n{\n    for (var i = 0; i < len(l); i++)\n    {\n        l[i].v = l[i + 1].v\n    }\n}\n");
            EXPECT_TRUE(s->hasEdge("shift", "#0.*.v", "#0.*.v"));

            // An exact path too: the value carried round the loop is not the slot's own
            auto e = summed("fun same(t)\n{\n    var prev = 0\n\n    for (var i = 0; i < 2; i++)\n    {\n"
                            "        var o = t.a\n        t.a = prev\n        prev = o\n    }\n}\n");
            EXPECT_TRUE(e->hasEdge("same", "#0.a", "#0.a"));
        }

        TEST(EffectSummaries, ASlotRewrittenBetweenTwoReadsIsAnEdgeOfItself)
        {
            // `a` is the old object of t.b, `c` is u: one path, two objects
            auto s = summed("fun f(t, u)\n{\n    var a = t.b\n    t.b = u\n    var c = t.b\n    c.x = a.x\n}\n");
            EXPECT_TRUE(s->hasEdge("f", "#0.b.x", "#0.b.x"));
            EXPECT_TRUE(s->hasEdge("f", "#0.b", "#1"));

            // Rewritten by a callee
            auto called = summed("fun reset(t, u)\n{\n    t.b = u\n}\n\n"
                                 "fun f(t, u)\n{\n    var a = t.b\n    reset(t, u)\n    var c = t.b\n    c.x = a.x\n}\n");
            EXPECT_TRUE(called->hasEdge("f", "#0.b.x", "#0.b.x"));

            // The slot not rewritten: the same object, its own value written back
            auto own = summed("fun g(t)\n{\n    t.b.x = t.b.x\n}\n");
            EXPECT_TRUE(own->of("g").edges.empty());
            const Write* w = own->write("g", "#0.b.x");
            ASSERT_NE(w, nullptr);
            EXPECT_TRUE(w->idempotence.shape);
        }

        TEST(EffectSummaries, AStoreOfAKeyNotKnownIsNeverTheOwnValue)
        {
            auto whole = summed("fun wholeShift(l)\n{\n    for (var i = 0; i < len(l); i++)\n    {\n        l[i] = l[i + 1]\n    }\n}\n");
            EXPECT_TRUE(whole->hasEdge("wholeShift", "#0.*", "#0.*"));

            auto gap = summed("fun gapShift(l, n)\n{\n    l[n + 1] = l[n + 2]\n}\n");
            EXPECT_TRUE(gap->hasEdge("gapShift", "#0.*", "#0.*"));

            // The entry is made before the loop's phis, so no phi of the loop has an operand above it
            auto carry = summed("fun forInCarry(l)\n{\n    var i = 0\n    var prev = 0\n\n    for (var e : l)\n    {\n"
                                "        l[i + 1] = prev\n        prev = e\n        i = i + 1\n    }\n}\n");
            EXPECT_TRUE(carry->hasEdge("forInCarry", "#0.*", "#0.*"));
        }

        TEST(EffectSummaries, ALiteralKeepsTheNeedsOfWhatFillsIt)
        {
            Locations locs;
            auto statements = parsed("var cfg = {a: level}\n");
            ASSERT_EQ(statements.size(), 1u);
            auto ir = buildTopLevel(statements[0], locs, [](const std::string&) { return true; });

            const std::map<std::string, Summary> none;
            auto no = [](const std::string&) { return false; };
            const Summary s = Evaluator(locs, none, no, no).evaluate(ir);

            ASSERT_EQ(s.writes.size(), 1u);
            EXPECT_EQ(locs.text(s.writes[0].at), "@0.a");
            EXPECT_TRUE(s.writes[0].idempotence.shape);
            EXPECT_EQ(s.writes[0].idempotence.needsGlobals, (std::set<std::string>{"level"}));
        }

        TEST(EffectSummaries, MergeWritesItsFirstWithWhatTheSecondHolds)
        {
            auto s = summed("fun m(a, b)\n{\n    return merge(a, b)\n}\n");
            EXPECT_EQ(s->writeTexts("m"), (std::set<std::string>{"#0.*"}));
            EXPECT_FALSE(s->write("m", "#0.*")->idempotence.shape);
            EXPECT_TRUE(s->hasEdge("m", "#0.*", "#1.*"));
            ASSERT_EQ(s->of("m").returns.size(), 1u);
            EXPECT_EQ(s->locs.text(s->of("m").returns[0]), "#0");
            EXPECT_FALSE(s->of("m").unknown);
        }

        TEST(EffectSummaries, CopyAndIndexByMakeATableOfTheEntries)
        {
            for (const std::string call : {"copy(t)", "indexBy(t, \"id\")"})
            {
                auto s = summed("fun c(t)\n{\n    return " + call + "\n}\n");
                EXPECT_TRUE(s->of("c").writes.empty()) << call;
                ASSERT_EQ(s->of("c").returns.size(), 1u) << call;
                EXPECT_EQ(s->locs.text(s->of("c").returns[0]), "@0") << call;
                EXPECT_TRUE(s->hasEdge("c", "@0.*", "#0.*")) << call;
                EXPECT_FALSE(s->of("c").unknown) << call;
            }
        }

        TEST(EffectSummaries, TakeWritesItsFirstAndReturnsAnEntry)
        {
            for (const std::string call : {"pop(l)", "removeAt(l, 0)"})
            {
                auto s = summed("fun p(l)\n{\n    return " + call + "\n}\n");
                EXPECT_EQ(s->writeTexts("p"), (std::set<std::string>{"#0.*"})) << call;
                EXPECT_FALSE(s->write("p", "#0.*")->idempotence.shape) << call;
                ASSERT_EQ(s->of("p").returns.size(), 1u) << call;
                EXPECT_EQ(s->locs.text(s->of("p").returns[0]), "#0.*") << call;
            }
        }

        TEST(EffectSummaries, ARebindIsAnEdgeOfItsGlobalThroughACall)
        {
            auto s = summed("fun keep(t)\n{\n    cache = t\n}\n\nfun c(a)\n{\n    keep(a.x)\n}\n");
            EXPECT_TRUE(s->of("keep").rebinds.count("cache"));
            EXPECT_TRUE(s->hasEdge("keep", "g:cache", "#0"));
            EXPECT_TRUE(s->of("c").rebinds.count("cache"));
            EXPECT_TRUE(s->hasEdge("c", "g:cache", "#0.x"));
        }

        TEST(EffectSummaries, DigestIsStableAndNamesTheWrites)
        {
            auto s = summed(amountsOfSource);
            const std::string d = s->of("amountsOf").digest(s->locs);
            EXPECT_EQ(d, s->of("amountsOf").digest(s->locs));
            EXPECT_NE(d.find("#0.[#1].*.amount"), std::string::npos);
        }

        namespace
        {
            using Texts = std::vector<std::string>;

            bool holds(const Texts& texts, const std::string& text)
            {
                return std::find(texts.begin(), texts.end(), text) != texts.end();
            }

            bool anyStartsWith(const Texts& texts, const std::string& prefix)
            {
                return std::any_of(texts.begin(), texts.end(), [&](const std::string& t) { return t.compare(0, prefix.size(), prefix) == 0; });
            }

            bool share(const Texts& a, const Texts& b)
            {
                return std::any_of(a.begin(), a.end(), [&](const std::string& t) { return holds(b, t); });
            }

            // A slot written is a slot of one of the objects of a region
            bool writesInto(const Texts& written, const Texts& region)
            {
                return std::any_of(region.begin(), region.end(), [&](const std::string& object) { return anyStartsWith(written, object + "."); });
            }

            // A program of one file, its statements kept for the questions asked of them
            struct Analyzed
            {
                std::vector<StatementPtr> statements;
                ast::ProgramEffects effects;
                bool usable = false;

                explicit Analyzed(const std::string& source) : statements(parsed(source))
                {
                    usable = effects.analyze({statements});
                }

                std::set<std::string> consts() const { return effects.inferredConsts(); }
                bool inferred(const std::string& name) const { return effects.inferredConsts().count(name) > 0; }
            };

            const std::string activitiesSource =
                amountsOfSource +
                "var activities = [{id: \"a\", gains: [{stat: \"s\"}]}, {id: \"b\", gains: [{stat: \"t\", amount: 2}]}]\n"
                "var other = {x: [1]}\n"
                "for (var i = 0; i < len(activities); i++)\n"
                "{\n"
                "    amountsOf(activities[i], \"gains\")\n"
                "}\n";

            const std::string rewrittenSlotSource =
                "fun f(t, u)\n{\n    var a = t.b\n    t.b = u\n    var c = t.b\n    c.x = a.x\n}\n"
                "var inner = {y: 1}\n"
                "var t = {b: {x: inner}}\n"
                "var u = {}\n"
                "f(t, u)\n";
        }

        TEST(ProgramEffects, TheActivityLoopWritesOnlyTheAmounts)
        {
            Analyzed p(activitiesSource);
            ASSERT_TRUE(p.usable);

            const StatementPtr& loop = p.statements.back();
            const Texts written = p.effects.writtenTexts(loop);

            EXPECT_TRUE(holds(written, "g:activities.*.gains.*.amount"));
            EXPECT_FALSE(anyStartsWith(written, "g:other"));
            EXPECT_TRUE(std::is_sorted(written.begin(), written.end()));

            EXPECT_FALSE(share(p.effects.regionsOf("activities"), p.effects.regionsOf("other")));
            EXPECT_TRUE(p.inferred("other"));
            EXPECT_FALSE(p.inferred("activities"));
            EXPECT_TRUE(p.effects.idempotent(loop));
            EXPECT_EQ(p.effects.collapsed(), 0u);
        }

        TEST(ProgramEffects, AWriteThroughAnAliasWritesTheOriginal)
        {
            Analyzed p("var rows = [{a: 1}, {a: 2}]\nvar index = {}\nindex[\"x\"] = rows[0]\nindex[\"x\"].a = 5\n");
            EXPECT_FALSE(p.inferred("rows"));
            EXPECT_FALSE(p.inferred("index"));
        }

        TEST(ProgramEffects, AGlobalAssignedInAFunctionIsNoConst)
        {
            Analyzed p("var lastAge = 3\nfun f()\n{\n    lastAge = 4\n}\nf()\n");
            EXPECT_FALSE(p.inferred("lastAge"));
        }

        TEST(ProgramEffects, AFunctionDefinedTwiceIsNotFollowed)
        {
            Analyzed p("fun g(t)\n{\n    return 0\n}\nfun g(t)\n{\n    t.x = 1\n}\nvar rows = {x: 0}\ng(rows)\n");
            EXPECT_EQ(p.effects.summaryOf("g"), nullptr);
            EXPECT_FALSE(p.inferred("rows"));
        }

        TEST(ProgramEffects, AFunctionDefinedInTwoFilesIsNotFollowed)
        {
            auto first = parsed("var rows = {x: 0}\nfun g(t)\n{\n    return 0\n}\ng(rows)\n");
            auto second = parsed("fun g(t)\n{\n    t.x = 1\n}\n");

            ast::ProgramEffects effects;
            ASSERT_TRUE(effects.analyze({first, second}));
            EXPECT_EQ(effects.summaryOf("g"), nullptr);
            EXPECT_EQ(effects.definitions("g"), 2);
            EXPECT_FALSE(effects.invariant("g"));
            EXPECT_EQ(effects.inferredConsts().count("rows"), 0u);
        }

        TEST(ProgramEffects, AConstantVarIsInferred)
        {
            Analyzed p("var lastAge = 30\nvar noBreak = {a: 1}\nfun f()\n{\n    return lastAge\n}\n");
            EXPECT_EQ(p.consts(), (std::set<std::string>{"lastAge", "noBreak"}));
            ASSERT_NE(p.effects.summaryOf("f"), nullptr);
            EXPECT_TRUE(p.effects.summaryOf("f")->reads.count("lastAge"));
            EXPECT_TRUE(p.effects.invariant("lastAge"));
            EXPECT_TRUE(p.effects.invariant("f"));
            EXPECT_TRUE(p.effects.invariant("len"));
            EXPECT_EQ(p.effects.definitions("lastAge"), 1);
        }

        TEST(ProgramEffects, AValueMadeOfConstsIsInferredInTurn)
        {
            Analyzed p("var base = {a: 1}\nvar derived = base.a\nconst fixed = 2\nvar sum = fixed + 1\n");
            EXPECT_EQ(p.consts(), (std::set<std::string>{"base", "derived", "sum"}));
        }

        // Code runs behind what the IR shows: nothing the analysis says may be used
        TEST(ProgramEffects, ACodeBehindAFieldOrALoopGivesUp)
        {
            const std::vector<std::string> sources = {
                "class Box\n{\n    __get(key)\n    {\n        return 1\n    }\n}\nvar x = 1\nx\n",
                "class Box\n{\n    __iter()\n    {\n        return 1\n    }\n}\nvar x = 1\nx\n",
                "fun __get(t, k)\n{\n    return 1\n}\nvar x = 1\nx\n",
                "fun __set(t, k, v)\n{\n    return 1\n}\nvar x = 1\nx\n",
                "fun __iter(t)\n{\n    return 1\n}\nvar x = 1\nx\n",
                "fun __next(t)\n{\n    return 1\n}\nvar x = 1\nx\n",
            };

            for (const auto& source : sources)
            {
                Analyzed p(source);
                EXPECT_FALSE(p.usable) << source;
                EXPECT_TRUE(p.consts().empty()) << source;
                EXPECT_FALSE(p.effects.invariant("x")) << source;
                EXPECT_FALSE(p.effects.invariant("len")) << source;
                EXPECT_FALSE(p.effects.onlyMakes(p.effects.effectsOf(p.statements.back()), {"x"})) << source;
                EXPECT_FALSE(p.effects.idempotent(p.statements.back())) << source;
            }
        }

        TEST(ProgramEffects, ACollapseIsOnlyReported)
        {
            // A value that may be any of 300 fields: past the guard, it is the unknown
            std::string body = "fun many(t, c)\n{\n    var v = t.k0\n";

            for (int i = 1; i < 300; ++i)
                body += "    if (c == " + std::to_string(i) + ")\n    {\n        v = t.k" + std::to_string(i) + "\n    }\n";

            body += "    return v\n}\nvar kept = 1\n";

            Analyzed p(body);
            EXPECT_TRUE(p.usable);
            EXPECT_GT(p.effects.collapsed(), 0u);
            EXPECT_TRUE(p.inferred("kept"));

            // What the value names is written: that is any location
            Analyzed q(body + "var rows = {x: 1}\nfun put(t, c)\n{\n    var v = many(t, c)\n    v.x = 1\n}\n");
            EXPECT_TRUE(q.usable);
            EXPECT_TRUE(q.consts().empty());
        }

        TEST(ProgramEffects, AGlobalNotDefinedIsAnInput)
        {
            Analyzed p("var copyOf = character\nvar made = {}\nmade.a = 1\ncharacter.x = 1\nvar fine = 1\n");
            EXPECT_FALSE(p.inferred("copyOf"));
            EXPECT_TRUE(p.inferred("fine"));
            EXPECT_FALSE(p.effects.invariant("character"));

            // Refused, even named as a target or held by one
            const StatementPtr& write = p.statements[3];
            EXPECT_FALSE(p.effects.onlyMakes(p.effects.effectsOf(write), {"character"}));

            Analyzed held("var holder = {c: character}\nfun w()\n{\n    return 1\n}\nholder.c.x = 1\n");
            EXPECT_FALSE(held.effects.onlyMakes(held.effects.effectsOf(held.statements.back()), {"holder", "character"}));
        }

        TEST(ProgramEffects, WhatOnlyMakesAccepts)
        {
            Analyzed p(
                "const places = {a: 1}\n"
                "const byId = {}\n"
                "const plain = {n: 1}\n"
                "byId.a = 1\n"
                "plain.n = places.a\n"
                "for (var i = 0; i < 2; i++)\n"
                "{\n"
                "    var mine = {}\n"
                "    mine.x = i\n"
                "}\n"
                "var count = 0\n"
                "count = 1\n"
                "const later = {}\n"
                "later.b = character\n");

            const auto& s = p.statements;
            EXPECT_TRUE(p.effects.onlyMakes(p.effects.effectsOf(s[3]), {"byId"}));
            EXPECT_FALSE(p.effects.onlyMakes(p.effects.effectsOf(s[3]), {"places"}));

            // Reads a const, writes another's
            EXPECT_TRUE(p.effects.onlyMakes(p.effects.effectsOf(s[4]), {"plain"}));
            EXPECT_FALSE(p.effects.onlyMakes(p.effects.effectsOf(s[4]), {"byId"}));

            // A table made by the statement itself, that no global holds
            EXPECT_TRUE(p.effects.onlyMakes(p.effects.effectsOf(s[5]), {"byId"}));

            // A rebind; an input read
            EXPECT_FALSE(p.effects.onlyMakes(p.effects.effectsOf(s[7]), {"count"}));
            EXPECT_FALSE(p.effects.onlyMakes(p.effects.effectsOf(s[9]), {"later"}));
        }

        TEST(ProgramEffects, ANativeNotKnownIsRefusedByOnlyMakes)
        {
            Analyzed p("const a = {}\nengineCall(1)\n");
            const auto e = p.effects.effectsOf(p.statements.back());
            EXPECT_TRUE(e.unknown);
            EXPECT_FALSE(p.effects.onlyMakes(e, {"a"}));
            EXPECT_FALSE(p.effects.idempotent(p.statements.back()));
        }

        TEST(ProgramEffects, AModulesOwnParseIsFoundByItsStatements)
        {
            const std::string module = "const places = {a: 1}\nconst byId = {}\nbyId.a = places.a\n";
            auto script = parsed("var out = 1\n");
            auto analyzed = parsed(module);

            ast::ProgramEffects effects;
            ASSERT_TRUE(effects.analyze({script, analyzed}));

            EXPECT_EQ(effects.fileOf(parsed(module)), 1);
            EXPECT_TRUE(effects.joinsInFile(1)[2].joined);
            EXPECT_EQ(effects.joinsInFile(1)[2].target, 1u);

            // Any statement of another kind or name at an index: not that file
            EXPECT_EQ(effects.fileOf(parsed("const places = {a: 1}\nconst byId = {}\n{\n    var k = 1\n}\n")), ast::ProgramEffects::NoFile);
            EXPECT_EQ(effects.fileOf(parsed("const places = {a: 1}\nconst other = {}\nbyId.a = places.a\n")), ast::ProgramEffects::NoFile);
            EXPECT_EQ(effects.fileOf(parsed("const places = {a: 1}\nconst byId = {}\n")), ast::ProgramEffects::NoFile);
        }

        TEST(ProgramEffects, AStatementNotOfTheProgramIsRefused)
        {
            const std::string source = "const places = {a: 1}\nconst byId = {}\nbyId.a = places.a\n";
            Analyzed p(source);

            const auto again = parsed(source);
            const auto e = p.effects.effectsOf(again.back());
            EXPECT_TRUE(e.unknown);
            EXPECT_FALSE(p.effects.onlyMakes(e, {"byId"}));
            EXPECT_FALSE(p.effects.idempotent(again.back()));

            EXPECT_EQ(p.effects.fileOf(parsed("const other = 1\n")), ast::ProgramEffects::NoFile);
        }

        TEST(ProgramEffects, FilesThatCannotBeToldApartJoinNothing)
        {
            const std::string twin = "const t = {}\nt.a = 1\n";
            const std::string other = "const u = {}\nu.a = 1\nconst t = {}\nt.b = 1\n";
            const std::string same = "for (var i = 0; i < 2; i++)\n{\n    i\n}\n";

            ast::ProgramEffects effects;
            ASSERT_TRUE(effects.analyze({parsed(twin), parsed(twin), parsed(other)}));
            EXPECT_EQ(effects.fileOf(parsed(twin)), ast::ProgramEffects::TwinFiles);
            EXPECT_FALSE(effects.joinsInFile(0)[1].joined);
            EXPECT_FALSE(effects.joinsInFile(1)[1].joined);
            EXPECT_TRUE(effects.joinsInFile(2)[1].joined);
            EXPECT_FALSE(effects.decided(0));
            EXPECT_FALSE(effects.decided(1));
            EXPECT_TRUE(effects.decided(2));
            EXPECT_FALSE(effects.invariant("t"));

            ast::ProgramEffects two;
            ASSERT_TRUE(two.analyze({parsed("var out = 1\n"), parsed(same), parsed(same)}));
            EXPECT_EQ(two.fileOf(parsed(same)), ast::ProgramEffects::TwinFiles);
        }

        TEST(ProgramEffects, ARegionHoldsWhatItsGlobalIsGiven)
        {
            Analyzed p("var rows = [{a: 1}]\nvar index = {}\nvar apart = {b: {}}\nindex.x = rows\n");
            EXPECT_TRUE(holds(p.effects.regionsOf("index"), "g:index"));
            EXPECT_TRUE(holds(p.effects.regionsOf("index"), "g:rows"));
            EXPECT_TRUE(share(p.effects.regionsOf("index"), p.effects.regionsOf("rows")));
            EXPECT_FALSE(share(p.effects.regionsOf("index"), p.effects.regionsOf("apart")));
            EXPECT_TRUE(p.effects.regionsOf("nothing").empty());
        }

        TEST(ProgramEffects, ATableStoredElsewhereIsNotWrittenByThatStore)
        {
            Analyzed p("var rows = {a: 1}\nvar index = {}\nindex.x = rows\n");
            EXPECT_TRUE(p.inferred("rows"));
            EXPECT_FALSE(p.inferred("index"));
        }

        TEST(ProgramEffects, AnEdgeAtAnythingReachesEveryGlobal)
        {
            // `v` is the unknown: rows may be stored anywhere
            Analyzed p(
                "var cb = 1\n"
                "var rows = {a: 1}\n"
                "var apart = {b: 1}\n"
                "fun put()\n"
                "{\n"
                "    var v = cb()\n"
                "    v.x = rows\n"
                "}\n"
                "put()\n");

            EXPECT_TRUE(holds(p.effects.regionsOf("apart"), "g:rows"));
            EXPECT_TRUE(holds(p.effects.regionsOf("cb"), "g:rows"));
            EXPECT_TRUE(p.consts().empty());
        }

        TEST(ProgramEffects, AGlobalHoldingAnythingIsWrittenByEverything)
        {
            Analyzed p("var cb = 1\nvar held = cb()\nvar rows = {a: 1}\nrows.a = 2\n");
            EXPECT_TRUE(holds(p.effects.regionsOf("held"), "?"));

            // Any table may be in it: none is made by a statement alone
            Analyzed fresh("var cb = 1\nvar held = cb()\nfor (var i = 0; i < 2; i++)\n{\n    var mine = {}\n    mine.x = i\n}\n");
            EXPECT_FALSE(fresh.effects.onlyMakes(fresh.effects.effectsOf(fresh.statements.back()), {"held"}));
        }

        TEST(ProgramEffects, AnEdgeHoldsAtEveryAliasOfItsLocation)
        {
            // c.x = a.x with c = u and a = the old t.b: u's field holds what t.b.x held
            Analyzed p(rewrittenSlotSource);
            EXPECT_TRUE(holds(p.effects.regionsOf("u"), "g:inner"));
            EXPECT_TRUE(share(p.effects.regionsOf("u"), p.effects.regionsOf("inner")));
            EXPECT_FALSE(p.inferred("t"));
            EXPECT_FALSE(p.inferred("u"));
        }

        TEST(ProgramEffects, AWriteThroughAnAliasOfASlotReachesWhatItHolds)
        {
            // t.b holds u, so t.b.x = rows is u.x: what is written through u is the row
            Analyzed p(
                "var rows = [{a: 1}]\n"
                "var t = {b: {}}\n"
                "var u = {}\n"
                "t.b = u\n"
                "t.b.x = rows[0]\n"
                "u.x.a = 5\n");

            EXPECT_TRUE(writesInto(p.effects.writtenTexts(p.statements.back()), p.effects.regionsOf("rows")));
            EXPECT_FALSE(p.inferred("rows"));
        }

        TEST(ProgramEffects, AnEdgesAliasesComeWithNoEndOnASelfEdge)
        {
            Analyzed p(
                "var l = [{v: 1}, {v: 2}]\n"
                "for (var i = 0; i < len(l); i++)\n"
                "{\n"
                "    l[i].v = l[i + 1].v\n"
                "}\n"
                "var next = {n: {n: {}}}\n"
                "next = next.n\n");

            EXPECT_FALSE(p.inferred("l"));
            EXPECT_FALSE(p.inferred("next"));
            EXPECT_EQ(p.effects.collapsed(), 0u);
        }

        TEST(ProgramEffects, AStoreInACallReachesTheAliasOfItsArgument)
        {
            // `index.x` is a row: what is written through the call is the row
            Analyzed p(
                "fun mark(t)\n{\n    t.seen = true\n}\n"
                "var rows = [{a: 1}]\n"
                "var index = {}\n"
                "var quiet = {}\n"
                "index[\"x\"] = rows[0]\n"
                "mark(index.x)\n");

            EXPECT_TRUE(holds(p.effects.writtenTexts(p.statements.back()), "g:index.x.seen"));
            EXPECT_TRUE(writesInto(p.effects.writtenTexts(p.statements.back()), p.effects.regionsOf("rows")));
            EXPECT_FALSE(p.inferred("rows"));
            EXPECT_TRUE(p.inferred("quiet"));
        }

        TEST(ProgramEffects, AnEngineNativeMayWriteAllItIsHanded)
        {
            Analyzed p("var rows = {a: {b: 1}}\nvar inner = rows.a\nvar apart = {c: 1}\nengineCall(rows)\n");
            EXPECT_FALSE(p.inferred("rows"));
            EXPECT_FALSE(p.inferred("inner"));
            EXPECT_TRUE(p.inferred("apart"));
        }

        TEST(ProgramEffects, AnEngineNativeReachesAllTheWayDown)
        {
            Analyzed p("var inner = {y: 1}\nvar mid = {i: inner}\nvar rows = {m: mid}\nvar apart = {c: 1}\nengineCall(rows)\n");
            EXPECT_FALSE(p.inferred("rows"));
            EXPECT_FALSE(p.inferred("mid"));
            EXPECT_FALSE(p.inferred("inner"));
            EXPECT_TRUE(p.inferred("apart"));
            EXPECT_TRUE(holds(p.effects.writtenTexts(p.statements.back()), "g:rows.*"));
            EXPECT_TRUE(writesInto(p.effects.writtenTexts(p.statements.back()), p.effects.regionsOf("inner")));
        }

        TEST(ProgramEffects, AnEngineNativeMayStoreOneArgumentInsideAnother)
        {
            // handed `rows` and `extra`: extra may now be in the table inner holds, so what writes inner's
            // fields may write extra
            Analyzed p(
                "var inner = {y: {}}\n"
                "var rows = {m: inner}\n"
                "var extra = {z: 1}\n"
                "engineCall(rows, extra)\n"
                "const after = {}\n"
                "inner.y.z = 2\n");

            EXPECT_TRUE(holds(p.effects.regionsOf("inner"), "g:extra"));
            EXPECT_TRUE(writesInto(p.effects.writtenTexts(p.statements.back()), p.effects.regionsOf("extra")));
        }

        // A host may call any function by name
        TEST(ProgramEffects, AFunctionNoStatementCallsCounts)
        {
            Analyzed writes("var rows = {x: 0}\nvar apart = {y: 0}\nfun hostOnly()\n{\n    rows.x = 1\n}\n");
            EXPECT_FALSE(writes.inferred("rows"));
            EXPECT_TRUE(writes.inferred("apart"));

            Analyzed rebinds("var age = 1\nfun hostOnly()\n{\n    age = 2\n}\n");
            EXPECT_FALSE(rebinds.inferred("age"));

            Analyzed stores("var rows = {x: 0}\nvar holder = {}\nfun hostOnly()\n{\n    holder.r = rows\n}\n");
            EXPECT_TRUE(share(stores.effects.regionsOf("holder"), stores.effects.regionsOf("rows")));
            EXPECT_FALSE(stores.inferred("holder"));

            // Writing its parameter says nothing of what it may be handed
            Analyzed param("var rows = {x: 0}\nfun set(t, v)\n{\n    t.x = v\n}\n");
            EXPECT_TRUE(param.inferred("rows"));

            // The alias of what it writes
            Analyzed aliased("var rows = {x: 0}\nvar holder = {}\nholder.r = rows\nfun hostOnly()\n{\n    holder.r.x = 1\n}\n");
            EXPECT_FALSE(aliased.inferred("rows"));
        }

        TEST(ProgramEffects, WhatAFunctionMakesAndFillsIsNoWriteOfTheProgram)
        {
            // As testbuilds.pg's withBag: each call makes its own table
            Analyzed p(
                "fun withBag(s)\n{\n    var held = merge({a: 1}, s)\n    held.b = 1\n    return held\n}\n"
                "var rows = [{s: withBag({c: 1})}, {s: withBag({d: 2})}]\n");

            EXPECT_TRUE(p.inferred("rows"));

            // Stored in a table of the program, it is a write of that table
            Analyzed stored("var cache = {}\nfun f()\n{\n    var t = {}\n    cache.t = t\n    t.x = 1\n}\n");
            EXPECT_FALSE(stored.inferred("cache"));

            // A statement that runs it to write a table that exists writes it
            Analyzed written(
                "fun fill(t)\n{\n    var made = {n: 1}\n    t.made = made\n    made.n = 2\n}\n"
                "var rows = {}\n"
                "fill(rows)\n");

            EXPECT_FALSE(written.inferred("rows"));
        }

        TEST(ProgramEffects, AFieldNamedByAParameterMayBeAnyField)
        {
            Analyzed p("var inner = {y: 1}\nvar rows = {x: {}}\nfun host(k)\n{\n    rows[k] = inner\n}\nrows.x.y = 2\n");
            EXPECT_TRUE(writesInto(p.effects.writtenTexts(p.statements.back()), p.effects.regionsOf("inner")));
            EXPECT_FALSE(p.inferred("inner"));
        }

        // The table a call makes may hold one of the program's: only its own slots are nobody else's
        TEST(ProgramEffects, WhatAFunctionsTableHoldsIsTheProgramsStill)
        {
            const std::vector<std::string> sources = {
                "var rows = [{x: 0}]\nfun touch()\n{\n    var c = copy(rows)\n    c[0].x = 1\n}\n",
                "var rows = [{id: \"a\", x: 0}]\nfun touch()\n{\n    var c = indexBy(rows, \"id\")\n    c[\"a\"].x = 1\n}\n",
                "var rows = {x: 0}\nfun touch()\n{\n    var c = [rows]\n    c[0].x = 1\n}\n",
                "var rows = {x: 0}\nfun touch()\n{\n    var c = {a: rows}\n    c.a.x = 1\n}\n",
                "var rows = {x: 0}\nfun touch()\n{\n    var c = {a: rows}\n    engineCall(c)\n}\n",
            };

            for (const auto& source : sources)
            {
                Analyzed p(source);
                EXPECT_FALSE(p.inferred("rows")) << source;
            }
        }

        // An engine native's table: nothing is known of it, what is stored in it stays there
        TEST(ProgramEffects, WhatAnEngineTableHoldsIsReachedThroughIt)
        {
            Analyzed local("var base = {n: 0}\nfun f()\n{\n    var l = makeTable()\n    l.r = base\n    l.r.n = 1\n}\n");
            EXPECT_FALSE(local.inferred("base"));

            Analyzed held(
                "var base = {n: 0}\n"
                "var t = {}\n"
                "fun f()\n{\n    t.a = makeTable()\n    t.a.b = base\n    t.a.b.n = 1\n}\n");

            EXPECT_FALSE(held.inferred("base"));
            EXPECT_TRUE(share(held.effects.regionsOf("t"), held.effects.regionsOf("base")));

            // A write into it is no statement making a const
            Analyzed made("const a = {}\nvar l = makeTable()\nl.x = 1\n");
            EXPECT_TRUE(anyStartsWith(made.effects.writtenTexts(made.statements.back()), "in:"));
            StatementEffects known = made.effects.effectsOf(made.statements.back());
            known.unknown = false;
            EXPECT_FALSE(made.effects.onlyMakes(known, {"a", "l"}));
        }

        // A const counts as the same on every run only when all that writes it joins it
        TEST(ProgramEffects, AConstFilledOnEveryRunIsNotInvariant)
        {
            Analyzed filled(
                "const tick = [0]\n"
                "var bump = 0\n"
                "bump = 1\n"
                "push(tick, bump)\n"
                "const table = {v: 0}\n"
                "var age = 0\n"
                "age = len(tick)\n"
                "table.v = age\n"
                "var derived = table.v\n"
                "var out = derived\n");

            EXPECT_FALSE(filled.effects.invariant("tick"));
            EXPECT_FALSE(filled.effects.invariant("table"));
            EXPECT_FALSE(filled.inferred("derived"));
            EXPECT_FALSE(filled.inferred("out"));

            Analyzed passed(
                "const tick = [0]\n"
                "var bump = 0\n"
                "bump = 1\n"
                "push(tick, bump)\n"
                "const table = {v: 0}\n"
                "var age = 0\n"
                "age = len(tick)\n"
                "table.v = age\n"
                "const u = {z: 0}\n"
                "u.z = table.v\n"
                "var out = u.z\n");

            const auto& joined = passed.effects.joinsInFile(0);
            ASSERT_EQ(joined.size(), passed.statements.size());
            EXPECT_FALSE(joined[9].joined);
            EXPECT_FALSE(passed.inferred("out"));
        }

        TEST(ProgramEffects, AConstWrittenByADefinitionIsNotInvariant)
        {
            Analyzed p(
                "const a = {}\n"
                "fun touch(t)\n{\n    t.n = lookup(t, \"n\", 0) + 1\n    return t\n}\n"
                "var b = touch(a)\n"
                "var out = a.n\n");

            EXPECT_FALSE(p.effects.invariant("a"));
            EXPECT_FALSE(p.inferred("b"));
            EXPECT_FALSE(p.inferred("out"));
        }

        TEST(ProgramEffects, AConstWrittenThroughACopyAfterItsRunIsNotInvariant)
        {
            Analyzed p(
                "const rows = [{x: 0}]\n"
                "var age = 0\n"
                "age = 1\n"
                "const seen = {n: 0}\n"
                "for (var i = 0; i < 1; i++)\n"
                "{\n"
                "    var c = copy(rows)\n"
                "    c[0].x = c[0].x + 1\n"
                "    seen.n = 1\n"
                "}\n"
                "var out = rows[0].x\n");

            EXPECT_FALSE(p.effects.invariant("rows"));
            EXPECT_FALSE(p.inferred("out"));
        }

        TEST(ProgramEffects, AConstAllWritersJoinStaysInvariant)
        {
            Analyzed p(
                "const places = [{id: \"a\"}, {id: \"b\"}]\n"
                "const byId = {}\n"
                "for (var i = 0; i < len(places); i++)\n"
                "{\n"
                "    byId[places[i].id] = places[i]\n"
                "}\n"
                "var first = byId[\"a\"]\n"
                "const plain = {a: 1}\n"
                "var read = plain.a\n");

            EXPECT_TRUE(p.effects.invariant("places"));
            EXPECT_TRUE(p.effects.invariant("byId"));
            EXPECT_TRUE(p.effects.invariant("plain"));
            EXPECT_TRUE(p.inferred("first"));
            EXPECT_TRUE(p.inferred("read"));

            // That is what a join is: run once, on the first run, whatever it computes
            Analyzed counted("const count = {n: 0}\ncount.n = count.n + 1\n");
            EXPECT_TRUE(counted.effects.joinsInFile(0)[1].joined);
            EXPECT_TRUE(counted.effects.invariant("count"));

            // Constant entries of two consts set in alternation all join: both stay invariant
            Analyzed alternating("const names = {}\nconst abbr = {}\nnames.a = \"a\"\nabbr.a = \"A\"\nnames.b = \"b\"\nabbr.b = \"B\"\nvar first = names.a\n");
            EXPECT_TRUE(alternating.effects.invariant("names"));
            EXPECT_TRUE(alternating.effects.invariant("abbr"));
            EXPECT_TRUE(alternating.inferred("first"));

            for (size_t i = 2; i < 6; ++i)
                EXPECT_TRUE(alternating.effects.joinsInFile(0)[i].joined) << i;
        }

        TEST(ProgramEffects, AConstantFillOfAConstWrittenOnEveryRunDoesNotJoin)
        {
            // `t.n = t.n + 1` runs on every run (`out` closed its run): t is not invariant, and the
            // constant entry set in its run is no longer joined, as the pass without the analysis does
            Analyzed p("const t = {n: 0}\nvar k = 5\nt.n = 1\nvar out = 0\nt.n = t.n + 1\nout = t.n\n");

            EXPECT_TRUE(p.inferred("k"));
            EXPECT_FALSE(p.effects.invariant("t"));

            for (const auto& join : p.effects.joinsInFile(0))
                EXPECT_FALSE(join.joined);

            // Right after its const, with nothing found between, it is part of the const as the pass
            // without the analysis has it, invariant or not
            Analyzed plain("const t = {n: 0}\nt.n = 1\nvar out = 0\nt.n = t.n + 1\nout = t.n\n");

            EXPECT_FALSE(plain.effects.invariant("t"));
            EXPECT_TRUE(plain.effects.joinsInFile(0)[1].joined);
            EXPECT_FALSE(plain.effects.joinsInFile(0)[3].joined);
        }

        TEST(ProgramEffects, ALosingConstCanUnjoinAnother)
        {
            // second.k reads base: it joins only while base is the same on every run, and it is not
            Analyzed p(
                "const base = {n: 0}\n"
                "var cut = 0\n"
                "cut = 1\n"
                "base.n = cut\n"
                "const second = {k: 0}\n"
                "second.k = base.n\n"
                "var out = second.k\n");

            EXPECT_FALSE(p.effects.invariant("base"));
            EXPECT_FALSE(p.effects.joinsInFile(0)[5].joined);
            EXPECT_FALSE(p.effects.invariant("second"));
            EXPECT_FALSE(p.inferred("out"));
        }

        TEST(ProgramEffects, AnOpenConstThatIsNotTheSameStopsAJoinThatReadsIt)
        {
            // a stays open past b, and is written on every run later: b.m cannot be made from it once
            Analyzed p(
                "const a = {n: 0}\n"
                "const b = {m: 0}\n"
                "b.m = a.n\n"
                "var gate = 0\n"
                "gate = 1\n"
                "a.n = a.n + 1\n"
                "var out = 0\n"
                "out = b.m\n");

            EXPECT_FALSE(p.effects.invariant("a"));
            EXPECT_FALSE(p.effects.joinsInFile(0)[2].joined);
            EXPECT_FALSE(p.effects.invariant("b"));

            // A const that is reads as one, open or not
            Analyzed fine("const a = {n: 1}\nconst b = {m: 0}\nb.m = a.n\n");
            EXPECT_TRUE(fine.effects.joinsInFile(0)[2].joined);
            EXPECT_TRUE(fine.effects.invariant("a"));
            EXPECT_TRUE(fine.effects.invariant("b"));
        }

        TEST(ProgramEffects, NothingJoinsBeforeAFunctionItCalls)
        {
            Analyzed direct("const a = {n: 0}\nfun fill(t)\n{\n    t.n = 1\n}\nfill(a)\n");
            EXPECT_FALSE(direct.effects.joinsInFile(0)[2].joined);
            EXPECT_FALSE(direct.effects.invariant("a"));

            // Through what it calls
            Analyzed through(
                "fun outer(t)\n{\n    inner(t)\n}\n"
                "const a = {n: 0}\n"
                "fun inner(t)\n{\n    t.n = 1\n}\n"
                "outer(a)\n");

            EXPECT_FALSE(through.effects.joinsInFile(0)[3].joined);

            // Defined before the const, it is there
            Analyzed before("fun fill(t)\n{\n    t.n = 1\n}\nconst a = {n: 0}\nfill(a)\n");
            EXPECT_TRUE(before.effects.joinsInFile(0)[2].joined);
            EXPECT_TRUE(before.effects.invariant("a"));

            // A constant fill calls nothing: it follows its const past a function
            Analyzed fill("const a = {n: 0}\nfun f()\n{\n    return 1\n}\na.n = 1\n");
            EXPECT_TRUE(fill.effects.joinsInFile(0)[2].joined);
        }

        TEST(ProgramEffects, RunOrderFollowsTheImports)
        {
            using Imports = std::vector<std::vector<std::pair<size_t, size_t>>>;

            auto helper = parsed("fun fill(t)\n{\n    t.n = 1\n}\n");
            auto module = parsed("const a = {n: 0}\nfill(a)\n");
            auto first = parsed("import \"h\"\nimport \"m\"\n");
            auto last = parsed("import \"m\"\nimport \"h\"\n");

            // Run before the const, in another file
            ast::ProgramEffects before;
            ASSERT_TRUE(before.analyze({first, helper, module}, Imports{{{0, 1}, {1, 2}}, {}, {}}));
            EXPECT_TRUE(before.joinsInFile(2)[1].joined);

            // After it
            ast::ProgramEffects after;
            ASSERT_TRUE(after.analyze({last, module, helper}, Imports{{{0, 1}, {1, 2}}, {}, {}}));
            EXPECT_FALSE(after.joinsInFile(1)[1].joined);

            // A module imports what it uses, above its const or below it
            auto above = parsed("import \"h\"\nconst a = {n: 0}\nfill(a)\n");
            auto below = parsed("const a = {n: 0}\nimport \"h\"\nfill(a)\n");
            auto script = parsed("import \"m\"\n");

            ast::ProgramEffects inAbove, inBelow;
            ASSERT_TRUE(inAbove.analyze({script, above, helper}, Imports{{{0, 1}}, {{0, 2}}, {}}));
            ASSERT_TRUE(inBelow.analyze({script, below, helper}, Imports{{{0, 1}}, {{1, 2}}, {}}));
            EXPECT_TRUE(inAbove.joinsInFile(1)[2].joined);
            EXPECT_FALSE(inBelow.joinsInFile(1)[2].joined);
        }

        TEST(ProgramEffects, AFileReachedTwiceJoinsNoStatementThatReads)
        {
            // Imported by the script and by m: it runs twice a run, and `c0.n = c0.n + 1` counts both.
            // Its constant entry is the same at each import: it joins as the plain walk has it
            using Imports = ast::FileImports;
            auto script = parsed("import \"m\"\nimport \"h\"\nvar out = c0.n\n");
            auto m = parsed("import \"h\"\n");
            auto h = parsed("const c0 = {n: 1}\nc0.k = 5\nc0.n = c0.n + 1\n");

            ast::ProgramEffects twice;
            ASSERT_TRUE(twice.analyze({script, m, h}, Imports{{{0, 1}, {1, 2}}, {{0, 2}}, {}}));
            EXPECT_TRUE(twice.joinsInFile(2)[1].joined);
            EXPECT_FALSE(twice.joinsInFile(2)[2].joined);
            EXPECT_FALSE(twice.invariant("c0"));

            // Imported once, it joins
            auto once = parsed("import \"h\"\nvar out = c0.n\n");

            ast::ProgramEffects single;
            ASSERT_TRUE(single.analyze({once, h}, Imports{{{0, 1}}, {}}));
            EXPECT_TRUE(single.joinsInFile(1)[2].joined);
            EXPECT_TRUE(single.invariant("c0"));
        }

        TEST(ProgramEffects, AJoinDoesNotGoBackInARun)
        {
            Analyzed writes("const a = {n: 0}\nconst b = {k: 7}\na.n = b.k\na.n = 5\nvar out = a.n\n");
            EXPECT_FALSE(writes.effects.joinsInFile(0)[3].joined);
            EXPECT_FALSE(writes.effects.invariant("a"));

            // It wrote a before b was made, and a stays open: a.n = b.k reads a const now lost
            EXPECT_FALSE(writes.effects.joinsInFile(0)[2].joined);
            EXPECT_FALSE(writes.inferred("out"));

            // Constant entries of distinct consts are independent: one may go back past another
            Analyzed fills("const a = {n: 0}\nconst b = {k: 7}\nb.k = 1\na.n = 5\nb.k = 2\n");
            EXPECT_TRUE(fills.effects.joinsInFile(0)[2].joined);
            EXPECT_TRUE(fills.effects.joinsInFile(0)[3].joined);
            EXPECT_TRUE(fills.effects.joinsInFile(0)[4].joined);
            EXPECT_TRUE(fills.effects.invariant("a"));
            EXPECT_TRUE(fills.effects.invariant("b"));

            // Not past a statement that may read its const: it ends the run, and what follows does
            // not join either
            Analyzed ended("const a = {n: 0}\nconst b = {k: 7}\nb.k = 1\nb.k = b.k + 1\na.n = 5\nvar out = a.n\n");
            EXPECT_TRUE(ended.effects.joinsInFile(0)[2].joined);
            EXPECT_TRUE(ended.effects.joinsInFile(0)[3].joined);
            EXPECT_FALSE(ended.effects.joinsInFile(0)[4].joined);
            EXPECT_FALSE(ended.effects.invariant("a"));
            EXPECT_FALSE(ended.inferred("out"));

            // In order, each stays
            Analyzed forward("const a = {n: 0}\nconst b = {k: 7}\na.n = 5\nb.k = 1\nb.k = a.n\n");
            EXPECT_TRUE(forward.effects.joinsInFile(0)[2].joined);
            EXPECT_TRUE(forward.effects.joinsInFile(0)[3].joined);
            EXPECT_TRUE(forward.effects.joinsInFile(0)[4].joined);
        }

        TEST(ProgramEffects, AReturnInATopLevelStatementGivesUp)
        {
            const std::vector<std::string> sources = {
                "var x = 1\nreturn 2\n",
                "var x = 1\nif (x == 1)\n{\n    return 2\n}\n",
                "var x = 1\nif (x == 2)\n{\n    x\n}\nelse\n{\n    return 2\n}\n",
                "var x = 1\nwhile (x == 1)\n{\n    return 2\n}\n",
                "var x = 1\nfor (var i = 0; i < 2; i++)\n{\n    return 2\n}\n",
                "var x = 1\nfor (var i : list)\n{\n    return 2\n}\n",
                "var x = 1\n{\n    {\n        return 2\n    }\n}\n",
            };

            for (const auto& source : sources)
            {
                Analyzed p(source);
                EXPECT_FALSE(p.usable) << source;
                EXPECT_TRUE(p.consts().empty()) << source;
            }

            ast::ProgramEffects effects;
            EXPECT_FALSE(effects.analyze({parsed("var x = 1\n"), parsed("if (x == 1)\n{\n    return 2\n}\n")}));

            // A function's own return is its own
            Analyzed function("var x = 1\nfun f()\n{\n    return 2\n}\n");
            EXPECT_TRUE(function.usable);
            EXPECT_TRUE(function.inferred("x"));
        }

        TEST(ProgramEffects, FactsCarryWhatWasDecidedForAConst)
        {
            Analyzed joined("const t = {a: 1}\nt.b = 2\n");
            Analyzed apart("const t = {a: 1}\nvar k = 0\nk = 1\nt.b = 2\n");

            EXPECT_NE(joined.effects.factsOf({"t"}).find("1 joins t"), std::string::npos);
            EXPECT_NE(apart.effects.factsOf({"t", "k"}).find("nothing;"), std::string::npos);

            // A join that rests on a function of another file: the facts move with it
            ast::ProgramEffects on, off;
            auto module = parsed("const t = {a: 1}\nt.b = read()\n");
            ASSERT_TRUE(on.analyze({parsed("const k = 1\nfun read()\n{\n    return k\n}\n"), module}));
            ASSERT_TRUE(off.analyze({parsed("var k = 1\nk = 2\nfun read()\n{\n    return k\n}\n"), module}));
            EXPECT_TRUE(on.joinsInFile(1)[1].joined);
            EXPECT_FALSE(off.joinsInFile(1)[1].joined);
            EXPECT_NE(on.factsOf({"t"}), off.factsOf({"t"}));
        }

        TEST(ProgramEffects, ANativeReboundIsNoNative)
        {
            Analyzed p(
                "var base = {n: 0}\n"
                "fun mine(t)\n{\n    t.n = t.n + 1\n    return t.n\n}\n"
                "len = mine\n"
                "var r = 0\n"
                "r = len(base)\n"
                "var out = 0\n"
                "out = base.n\n");

            EXPECT_FALSE(p.inferred("base"));
            EXPECT_FALSE(p.effects.invariant("len"));
            EXPECT_GT(p.effects.definitions("len"), 0);
            EXPECT_TRUE(p.effects.effectsOf(p.statements[4]).unknown);
        }

        TEST(ProgramEffects, AFunctionThatWritesAnythingBlocksEveryConst)
        {
            Analyzed p("var kept = 1\nfun hostOnly(f)\n{\n    f()\n}\n");
            EXPECT_TRUE(p.consts().empty());
        }

        TEST(ProgramEffects, AFunctionNotFollowedCountsToo)
        {
            Analyzed twice("var rows = {x: 0}\nvar age = 1\nfun g()\n{\n    return 0\n}\nfun g()\n{\n    rows.x = 1\n    age = 2\n}\n");
            EXPECT_FALSE(twice.inferred("rows"));
            EXPECT_FALSE(twice.inferred("age"));

            Analyzed rebound("var rows = {x: 0}\nfun g()\n{\n    rows.x = 1\n}\ng = 1\n");
            EXPECT_EQ(rebound.effects.summaryOf("g"), nullptr);
            EXPECT_FALSE(rebound.inferred("rows"));
            EXPECT_FALSE(rebound.effects.invariant("g"));
        }

        TEST(ProgramEffects, AStatementThatWritesAnythingBlocksEveryConst)
        {
            Analyzed p("var kept = 1\nvar cb = 1\ncb()\n");
            EXPECT_TRUE(p.consts().empty());

            Analyzed closure("var kept = 1\nvar f = fun()\n{\n    return 1\n}\n");
            EXPECT_TRUE(closure.consts().empty());
        }

        TEST(ProgramEffects, ATableHoldingOthersLeavesThemConsts)
        {
            Analyzed p("var a = {x: 1}\nvar b = {y: 1}\nvar c = {a: a, b: b}\n");
            EXPECT_EQ(p.consts(), (std::set<std::string>{"a", "b", "c"}));
            EXPECT_TRUE(share(p.effects.regionsOf("c"), p.effects.regionsOf("a")));
        }

        TEST(ProgramEffects, TwoTablesStoredInOneSlotStayApart)
        {
            Analyzed p("var y = {f: 1}\nvar z = {f: 1}\nvar x = {}\nx.a = y\nx.a = z\ny.f = 2\n");
            EXPECT_FALSE(p.inferred("y"));
            EXPECT_TRUE(p.inferred("z"));
            EXPECT_FALSE(share(p.effects.regionsOf("y"), p.effects.regionsOf("z")));
        }

        TEST(ProgramEffects, EntriesStoredInOneTableStayApart)
        {
            Analyzed p(
                "var y = {f: 1}\n"
                "var z = {f: 1}\n"
                "var x = []\n"
                "push(x, y)\n"
                "for (var i = 0; i < 2; i++)\n"
                "{\n"
                "    x[i] = z\n"
                "}\n"
                "y.f = 2\n");

            EXPECT_FALSE(p.inferred("y"));
            EXPECT_TRUE(p.inferred("z"));
            EXPECT_FALSE(share(p.effects.regionsOf("y"), p.effects.regionsOf("z")));
        }

        TEST(ProgramEffects, AWriteOfAFieldLeavesItsSiblingsAlone)
        {
            Analyzed p("var rows = {list: [1], n: 0}\nvar held = {n: 0, list: rows}\nheld.n = 1\n");
            EXPECT_TRUE(p.inferred("rows"));
        }

        TEST(ProgramEffects, IdempotentAsksEveryWriteAndWhatItNeeds)
        {
            Analyzed p(
                "var level = 1\n"
                "var rows = {x: 0, y: 0, n: 0}\n"
                "const fixed = 2\n"
                "rows.x = 3\n"
                "rows.y = fixed\n"
                "rows.n = rows.n + 1\n"
                "rows.y = level\n"
                "level = 2\n");

            EXPECT_TRUE(p.effects.idempotent(p.statements[3]));
            EXPECT_TRUE(p.effects.idempotent(p.statements[4]));
            EXPECT_FALSE(p.effects.idempotent(p.statements[5]));
            EXPECT_FALSE(p.effects.idempotent(p.statements[6]));  // level is rebound
            EXPECT_FALSE(p.effects.idempotent(p.statements[7]));  // a rebind is no write, and no fixed value
        }

        TEST(ProgramEffects, FactsAreTheSameFromOneCompileToTheNext)
        {
            Analyzed a(activitiesSource), b(activitiesSource);

            const std::set<std::string> names{"activities", "other", "amountsOf", "len", "total"};

            EXPECT_EQ(a.effects.factsOf(names), b.effects.factsOf(names));
            EXPECT_NE(a.effects.factsOf(names).find("var other;"), std::string::npos);
        }

        TEST(ProgramEffects, FactsTellOnlyWhatWasDecidedForTheFilesHeld)
        {
            // What the file is made of differs, what was decided for it does not: it is the same module
            auto module = parsed("const t = {a: 1}\nt.b = read()\n");
            ast::ProgramEffects one, other;
            ASSERT_TRUE(one.analyze({parsed("fun read()\n{\n    return 1\n}\n"), module}));
            ASSERT_TRUE(other.analyze({parsed("const k = {a: 1}\nvar seen = k\nfun read()\n{\n    return seen.a\n}\n"), module}));
            EXPECT_TRUE(one.joinsInFile(1)[1].joined);
            EXPECT_TRUE(other.joinsInFile(1)[1].joined);
            EXPECT_EQ(one.factsOf({"t"}), other.factsOf({"t"}));

            // A file not all of whose names are held is not what the module is made of
            ast::ProgramEffects held;
            ASSERT_TRUE(held.analyze({parsed("const t = {a: 1}\nt.b = 2\nvar other = 1\n")}));
            EXPECT_TRUE(held.factsOf({"t"}).empty());
            EXPECT_FALSE(held.factsOf({"t", "other"}).empty());

            // A file taken for another gets the plain fills: no file decided like it may stand in for it
            ast::ProgramEffects twins;
            ASSERT_TRUE(twins.analyze({parsed("const t = {a: 1}\nt.b = 2\n"), parsed("const t = {a: 2}\nt.b = 3\n")}));
            EXPECT_NE(twins.factsOf({"t"}).find("plain;"), std::string::npos);

            // A file's line is by its origin: another file of the same names is not it
            const auto line = parsed("const t = {a: 1}\nt.b = 2\n");
            ast::ProgramEffects here, there;
            ASSERT_TRUE(here.analyze({line}, {}, {"f.pg#1"}));
            ASSERT_TRUE(there.analyze({line}, {}, {"s.pg#1"}));
            EXPECT_NE(here.factsOf({"t"}).find("f.pg#1:"), std::string::npos);
            EXPECT_NE(here.factsOf({"t"}), there.factsOf({"t"}));

            // A file that declares nothing has nothing to tell
            ast::ProgramEffects nameless;
            ASSERT_TRUE(nameless.analyze({parsed("import \"m\"\n")}, {}, {"n.pg#1"}));
            EXPECT_TRUE(nameless.factsOf({"t"}).empty());
        }

        TEST(ProgramEffects, EffectsOfAStatementAreItsOwn)
        {
            Analyzed p("var rows = {x: 0}\nvar index = {}\nvar age = 1\nindex.r = rows\nage = rows.x\n");

            const auto store = p.effects.effectsOf(p.statements[3]);
            EXPECT_TRUE(store.reads.count("rows"));
            EXPECT_TRUE(store.reads.count("index"));
            ASSERT_EQ(store.writes.size(), 1u);
            EXPECT_TRUE(store.rebinds.empty());
            EXPECT_FALSE(store.unknown);

            EXPECT_TRUE(p.effects.effectsOf(p.statements[4]).rebinds.count("age"));
        }

        TEST(ProgramEffects, TheBodiesAreKeptAndMappedBackToTheAst)
        {
            Analyzed p("var rows = {x: 0}\n"
                       "var age = 1\n"
                       "fun followed(t)\n{\n    return t.x\n}\n"
                       "fun rebound(t)\n{\n    return t\n}\n"
                       "fun twice()\n{\n    return 1\n}\n"
                       "fun twice()\n{\n    return 2\n}\n"
                       "rebound = followed\n"
                       "age = followed(rows)\n");
            ASSERT_TRUE(p.usable);

            // The value of a function's return expression is one of its kept body, and the one it returns
            const auto function = std::static_pointer_cast<FunctionStatement>(p.statements[2]);
            const auto block = std::static_pointer_cast<BlockStatement>(function->body);
            const auto returned = std::static_pointer_cast<ReturnStatement>(block->statements.front());

            const BodyIR* body = p.effects.bodyOf("followed");
            ASSERT_NE(body, nullptr);

            const auto value = body->valueOf.find(returned->value.get());
            ASSERT_NE(value, body->valueOf.end());
            ASSERT_LT(value->second, body->nodes.size());
            EXPECT_EQ(body->nodes[value->second].op, Op::Field);
            ASSERT_EQ(body->returns.size(), 1u);
            EXPECT_EQ(body->returns[0], value->second);

            // Not followed, it still has its body
            EXPECT_NE(p.effects.bodyOf("rebound"), nullptr);

            // Which definition a call runs is not known; no function of that name
            EXPECT_EQ(p.effects.bodyOf("twice"), nullptr);
            EXPECT_EQ(p.effects.bodyOf("nothing"), nullptr);

            // A statement's, whose assignment is what the program's top level does
            const BodyIR* statement = p.effects.bodyOf(p.statements[7]);
            ASSERT_NE(statement, nullptr);
            ASSERT_EQ(statement->rebinds.size(), 1u);
            EXPECT_EQ(statement->rebinds[0].first, "age");

            EXPECT_EQ(p.effects.bodyOf(p.statements[2]), nullptr);  // A function's is by its name
            EXPECT_EQ(p.effects.bodyOf(parsed("var other = 1\n")[0]), nullptr);
            EXPECT_EQ(p.effects.bodyOf(StatementPtr()), nullptr);
        }

        // ---- classes ---------------------------------------------------------------------------

        namespace
        {
            const std::string box = "class Box\n{\n    init(t)\n    {\n        this.t = t\n    }\n    clear()\n    {\n        this.t.n = 0\n    }\n    size()\n    {\n        return 1\n    }\n}\n";
        }

        TEST(ProgramEffects, APlainClassIsFollowed)
        {
            Analyzed p(box + "var x = {n: 1}\nvar b = Box({n: 2})\nb.size()\n");
            EXPECT_TRUE(p.usable) << p.effects.refusal();
            EXPECT_TRUE(p.consts().count("x"));
        }

        TEST(ProgramEffects, AMethodNeverCalledStillWrites)
        {
            Analyzed p("var rows = {n: 1}\nclass Box\n{\n    clear()\n    {\n        rows.n = 0\n    }\n}\n");
            EXPECT_TRUE(p.usable) << p.effects.refusal();
            EXPECT_FALSE(p.consts().count("rows"));
        }

        TEST(ProgramEffects, InitRunsWhenTheClassIsCalled)
        {
            Analyzed p("var rows = {n: 1}\nclass Box\n{\n    init()\n    {\n        rows.n = 0\n    }\n}\nvar b = Box()\n");
            EXPECT_TRUE(p.usable) << p.effects.refusal();
            EXPECT_FALSE(p.consts().count("rows"));
        }

        TEST(ProgramEffects, WhatAnObjectHoldsIsWrittenThroughIt)
        {
            Analyzed p(box + "var rows = {n: 1}\nvar b = Box(rows)\nb.clear()\n");
            EXPECT_TRUE(p.usable) << p.effects.refusal();
            EXPECT_FALSE(p.consts().count("rows"));
        }

        TEST(ProgramEffects, WhatAnObjectHoldsIsWrittenThroughAFunction)
        {
            Analyzed p(box + "var rows = {n: 1}\nfun go(o)\n{\n    o.clear()\n}\nvar b = Box(rows)\ngo(b)\n");
            EXPECT_TRUE(p.usable) << p.effects.refusal();
            EXPECT_FALSE(p.consts().count("rows"));
        }

        TEST(ProgramEffects, EveryMethodOfTheNameMayRun)
        {
            Analyzed p("var rows = {n: 1}\n"
                       "class A\n{\n    go()\n    {\n        return 1\n    }\n}\n"
                       "class B\n{\n    init(t)\n    {\n        this.t = t\n    }\n    go()\n    {\n        this.t.n = 0\n    }\n}\n"
                       "fun run(o)\n{\n    o.go()\n}\n"
                       "run(A())\nrun(B(rows))\n");

            EXPECT_TRUE(p.usable) << p.effects.refusal();
            EXPECT_FALSE(p.consts().count("rows"));
        }

        TEST(ProgramEffects, ACallThatMayRunOtherCodeIsNotFollowed)
        {
            const std::vector<std::string> sources = {
                // A field of a method's name may hold anything
                "class Box\n{\n    go()\n    {\n        return 1\n    }\n}\nvar t = {}\nt.go = 1\nvar x = 1\n",
                "class Box\n{\n    go()\n    {\n        return 1\n    }\n}\nvar t = {go: 1}\nvar x = 1\n",
                "class Box\n{\n    go()\n    {\n        return 1\n    }\n}\nvar t = {}\nt[\"go\"] = 1\nvar x = 1\n",

                // A function in a field is what a call of a method would run
                "class Box\n{\n    go()\n    {\n        return 1\n    }\n}\nfun f()\n{\n    return 2\n}\nvar k = \"g\" + \"o\"\nvar t = {}\nt[k] = f\nt.go()\nvar x = 1\n",

                // A metamethod set by hand
                "class Box\n{\n    go()\n    {\n        return 1\n    }\n}\nvar b = Box()\nb.__get = 1\nvar x = 1\n",
                "class Box\n{\n    go()\n    {\n        return 1\n    }\n}\nvar b = Box()\nb[\"__get\"] = 1\nvar x = 1\n",

                // Not a class a name can tell
                "class Box\n{\n    go()\n    {\n        return 1\n    }\n}\nBox = 1\nvar x = 1\n",
                "class Box\n{\n    go()\n    {\n        return 1\n    }\n}\nclass Box\n{\n    go()\n    {\n        return 2\n    }\n}\nvar x = 1\n",
                "fun f()\n{\n    class Box\n    {\n        go()\n        {\n            return 1\n        }\n    }\n}\nvar x = 1\n",
            };

            for (const auto& source : sources)
            {
                Analyzed p(source);
                EXPECT_FALSE(p.usable) << source;
                EXPECT_FALSE(p.effects.refusal().empty()) << source;
            }
        }

        TEST(ProgramEffects, NoBodiesWhenNothingIsUsable)
        {
            Analyzed p("fun f()\n{\n    return 1\n}\nclass Box\n{\n    __next()\n    {\n        return 1\n    }\n}\n");
            EXPECT_FALSE(p.usable);
            EXPECT_EQ(p.effects.bodyOf("f"), nullptr);
            EXPECT_EQ(p.effects.bodyOf(p.statements[0]), nullptr);
        }
    }
}
