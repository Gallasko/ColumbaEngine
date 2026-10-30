#include "gtest/gtest.h"

#include "Compiler/ast/analysis/locations.h"
#include "Compiler/ast/analysis/ssa.h"
#include "Compiler/ast/analysis/summaries.h"

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

            EXPECT_EQ(returns, (std::set<std::string>{"#0", "#0.*"}));
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
    }
}
