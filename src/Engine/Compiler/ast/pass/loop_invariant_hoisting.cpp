#include "stdafx.h"

#include "loop_invariant_hoisting.h"

#include "ast_analysis.h"

#include "Interpreter/expression.h"
#include "Interpreter/statement.h"

#include "logger.h"

#include <map>
#include <set>
#include <vector>

namespace pg
{
    namespace
    {
        using ast::LoopFacts;
        using ast::analyzeExpr;
        using ast::analyzeStmt;

        // Local slots are addressed with a single byte; leave headroom under
        // the 255 limit so hidden hoist vars can never exhaust a function's
        // slots (the emitter would silently truncate the operand otherwise)
        constexpr int kMaxLocalSlots = 240;

        struct Ctx
        {
            int hoistCounter = 0;
            bool changed = false;

            // Conservative count of local slots the CURRENT function may use
            // (declarations + hidden vars added so far); hoisting stops when
            // the budget is reached
            int slotsUsed = 0;
        };

        /**
         * Conservative upper bound of the local slots a function body can
         * create: every declaration counts, sibling blocks are summed even
         * though their slots are reused, for-in adds its three hidden
         * iteration locals plus the key variable. Nested function/class
         * bodies have their own slot space and are excluded.
         */
        int countLocalSlots(const StatementPtr& s)
        {
            if (not s)
                return 0;

            const auto& type = s->getType();

            if (type == "VariableStatement")
                return 1;

            if (type == "BlockStatement")
            {
                int count = 0;

                auto statements = std::static_pointer_cast<BlockStatement>(s)->statements;
                while (not statements.empty())
                {
                    count += countLocalSlots(statements.front());
                    statements.pop();
                }

                return count;
            }

            if (type == "IfStatement")
            {
                auto i = std::static_pointer_cast<IfStatement>(s);
                return countLocalSlots(i->thenBranch) + countLocalSlots(i->elseBranch);
            }

            if (type == "WhileStatement")
                return countLocalSlots(std::static_pointer_cast<WhileStatement>(s)->body);

            if (type == "ForStatement")
            {
                auto fs = std::static_pointer_cast<ForStatement>(s);
                return countLocalSlots(fs->initializer) + countLocalSlots(fs->body);
            }

            if (type == "ForInStatement")
            {
                // __table, __size, __i + the key variable
                return 4 + countLocalSlots(std::static_pointer_cast<ForInStatement>(s)->body);
            }

            // Local function/class declarations occupy one slot themselves
            if (type == "FunctionStatement" or type == "ClassStatement")
                return 1;

            return 0;
        }

        // ------------------------------------------------------------------
        // Invariance and candidate collection
        // ------------------------------------------------------------------

        bool isInvariant(const ExprPtr& e, const LoopFacts& f)
        {
            if (not e)
                return false;

            const auto& type = e->getType();

            if (type == "Atom")
                return true;

            if (type == "Var")
                return f.mutated.count(std::static_pointer_cast<Var>(e)->name.text) == 0;

            if (type == "CompoundAtom")
                return isInvariant(std::static_pointer_cast<CompoundAtom>(e)->expr, f);

            if (type == "UnaryExpression")
                return isInvariant(std::static_pointer_cast<UnaryExpression>(e)->expr, f);

            if (type == "BinaryExpression")
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                return isInvariant(b->leftExpr, f) and isInvariant(b->rightExpr, f);
            }

            if (type == "LogicExpression")
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);
                return isInvariant(l->leftExpr, f) and isInvariant(l->rightExpr, f);
            }

            return false;
        }

        /** Non-trivial expressions worth a hidden variable */
        bool isCandidateType(const ExprPtr& e)
        {
            const auto& type = e->getType();
            return type == "BinaryExpression" or type == "LogicExpression" or type == "UnaryExpression";
        }

        /**
         * Type-aware structural key: prettyPrint alone would collide int 1
         * with string "1", so tag every node with its kind.
         */
        std::string keyOf(const ExprPtr& e)
        {
            const auto& type = e->getType();

            if (type == "Atom")
            {
                const auto& v = std::static_pointer_cast<Atom>(e)->value;
                return "A" + std::to_string(static_cast<int>(v.type)) + ":" + v.toString();
            }

            if (type == "Var")
                return "V:" + std::static_pointer_cast<Var>(e)->name.text;

            if (type == "CompoundAtom")
                return keyOf(std::static_pointer_cast<CompoundAtom>(e)->expr);

            if (type == "UnaryExpression")
            {
                auto u = std::static_pointer_cast<UnaryExpression>(e);
                return "U" + u->op.text + "(" + keyOf(u->expr) + ")";
            }

            if (type == "BinaryExpression")
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                return "B" + b->op.text + "(" + keyOf(b->leftExpr) + "," + keyOf(b->rightExpr) + ")";
            }

            if (type == "LogicExpression")
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);
                return "L" + l->op.text + "(" + keyOf(l->leftExpr) + "," + keyOf(l->rightExpr) + ")";
            }

            // Never matches a candidate key
            return "!";
        }

        using Candidates = std::vector<std::pair<std::string, ExprPtr>>;

        void collectStmt(const StatementPtr& s, const LoopFacts& f, Candidates& out, std::set<std::string>& seen);

        void collectExpr(const ExprPtr& e, const LoopFacts& f, Candidates& out, std::set<std::string>& seen)
        {
            if (not e)
                return;

            // Maximal invariant subtree: record and stop descending
            if (isCandidateType(e) and isInvariant(e, f))
            {
                auto key = keyOf(e);

                if (seen.insert(key).second)
                    out.emplace_back(key, e);

                return;
            }

            const auto& type = e->getType();

            if (type == "BinaryExpression")
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                collectExpr(b->leftExpr, f, out, seen);
                collectExpr(b->rightExpr, f, out, seen);
            }
            else if (type == "LogicExpression")
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);
                collectExpr(l->leftExpr, f, out, seen);
                collectExpr(l->rightExpr, f, out, seen);
            }
            else if (type == "UnaryExpression")
            {
                collectExpr(std::static_pointer_cast<UnaryExpression>(e)->expr, f, out, seen);
            }
            else if (type == "CompoundAtom")
            {
                collectExpr(std::static_pointer_cast<CompoundAtom>(e)->expr, f, out, seen);
            }
            else if (type == "Assign")
            {
                collectExpr(std::static_pointer_cast<Assign>(e)->expr, f, out, seen);
            }
            else if (type == "CallExpression")
            {
                // Unreachable in practice (hasCalls blocks hoisting), kept for safety
                auto c = std::static_pointer_cast<CallExpression>(e);
                collectExpr(c->caller, f, out, seen);

                auto args = c->args;
                while (not args.empty())
                {
                    collectExpr(args.front(), f, out, seen);
                    args.pop();
                }
            }
            else if (type == "Get")
            {
                collectExpr(std::static_pointer_cast<Get>(e)->object, f, out, seen);
            }
            else if (type == "Set")
            {
                auto st = std::static_pointer_cast<Set>(e);
                collectExpr(st->object, f, out, seen);
                collectExpr(st->value, f, out, seen);
            }
            else if (type == "IndexGet")
            {
                auto i = std::static_pointer_cast<IndexGet>(e);
                collectExpr(i->object, f, out, seen);
                collectExpr(i->index, f, out, seen);
            }
            else if (type == "IndexSet")
            {
                auto i = std::static_pointer_cast<IndexSet>(e);
                collectExpr(i->object, f, out, seen);
                collectExpr(i->index, f, out, seen);
                collectExpr(i->value, f, out, seen);
            }
            else if (type == "List")
            {
                auto entries = std::static_pointer_cast<List>(e)->entries;
                while (not entries.empty())
                {
                    collectExpr(entries.front().key, f, out, seen);
                    collectExpr(entries.front().value, f, out, seen);
                    entries.pop();
                }
            }
            // AnonymousFunction bodies skipped; Atom/Var/This/Pre/PostFix: no candidates inside
        }

        void collectStmt(const StatementPtr& s, const LoopFacts& f, Candidates& out, std::set<std::string>& seen)
        {
            if (not s)
                return;

            const auto& type = s->getType();

            if (type == "ExpressionStatement")
            {
                collectExpr(std::static_pointer_cast<ExpressionStatement>(s)->expr, f, out, seen);
            }
            else if (type == "VariableStatement")
            {
                collectExpr(std::static_pointer_cast<VariableStatement>(s)->expr, f, out, seen);
            }
            else if (type == "BlockStatement")
            {
                auto statements = std::static_pointer_cast<BlockStatement>(s)->statements;
                while (not statements.empty())
                {
                    collectStmt(statements.front(), f, out, seen);
                    statements.pop();
                }
            }
            else if (type == "IfStatement")
            {
                auto i = std::static_pointer_cast<IfStatement>(s);
                collectExpr(i->condition, f, out, seen);
                collectStmt(i->thenBranch, f, out, seen);
                collectStmt(i->elseBranch, f, out, seen);
            }
            else if (type == "WhileStatement")
            {
                auto w = std::static_pointer_cast<WhileStatement>(s);
                collectExpr(w->condition, f, out, seen);
                collectStmt(w->body, f, out, seen);
            }
            else if (type == "ForStatement")
            {
                auto fs = std::static_pointer_cast<ForStatement>(s);
                collectStmt(fs->initializer, f, out, seen);
                collectExpr(fs->condition, f, out, seen);
                collectExpr(fs->increment, f, out, seen);
                collectStmt(fs->body, f, out, seen);
            }
            else if (type == "ForInStatement")
            {
                auto fi = std::static_pointer_cast<ForInStatement>(s);
                collectExpr(fi->iterable, f, out, seen);
                collectStmt(fi->body, f, out, seen);
            }
            else if (type == "ReturnStatement")
            {
                collectExpr(std::static_pointer_cast<ReturnStatement>(s)->value, f, out, seen);
            }
            else if (type == "DPrintStatement")
            {
                collectExpr(std::static_pointer_cast<DPrintStatement>(s)->expr, f, out, seen);
            }
            // FunctionStatement / ClassStatement bodies skipped (not per-iteration)
        }

        // ------------------------------------------------------------------
        // Rewrite: replace hoisted subtrees with references to the hidden vars
        // ------------------------------------------------------------------

        using Replacements = std::map<std::string, std::string>; // structural key -> hidden var name

        void rewriteStmt(const StatementPtr& s, const Replacements& repl);

        ExprPtr rewriteExpr(ExprPtr e, const Replacements& repl)
        {
            if (not e)
                return e;

            auto found = repl.find(keyOf(e));

            if (found != repl.end())
                return std::make_shared<Var>(Token(TokenType::EXPRESSION, found->second, 0, 0));

            const auto& type = e->getType();

            if (type == "BinaryExpression")
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                b->leftExpr = rewriteExpr(b->leftExpr, repl);
                b->rightExpr = rewriteExpr(b->rightExpr, repl);
            }
            else if (type == "LogicExpression")
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);
                l->leftExpr = rewriteExpr(l->leftExpr, repl);
                l->rightExpr = rewriteExpr(l->rightExpr, repl);
            }
            else if (type == "UnaryExpression")
            {
                auto u = std::static_pointer_cast<UnaryExpression>(e);
                u->expr = rewriteExpr(u->expr, repl);
            }
            else if (type == "CompoundAtom")
            {
                auto c = std::static_pointer_cast<CompoundAtom>(e);
                c->expr = rewriteExpr(c->expr, repl);
            }
            else if (type == "Assign")
            {
                auto a = std::static_pointer_cast<Assign>(e);
                a->expr = rewriteExpr(a->expr, repl);
            }
            else if (type == "CallExpression")
            {
                auto c = std::static_pointer_cast<CallExpression>(e);
                c->caller = rewriteExpr(c->caller, repl);

                std::queue<ExprPtr> rewritten;
                while (not c->args.empty())
                {
                    rewritten.push(rewriteExpr(c->args.front(), repl));
                    c->args.pop();
                }
                c->args = std::move(rewritten);
            }
            else if (type == "Get")
            {
                auto g = std::static_pointer_cast<Get>(e);
                g->object = rewriteExpr(g->object, repl);
            }
            else if (type == "Set")
            {
                auto st = std::static_pointer_cast<Set>(e);
                st->object = rewriteExpr(st->object, repl);
                st->value = rewriteExpr(st->value, repl);
            }
            else if (type == "IndexGet")
            {
                auto i = std::static_pointer_cast<IndexGet>(e);
                i->object = rewriteExpr(i->object, repl);
                i->index = rewriteExpr(i->index, repl);
            }
            else if (type == "IndexSet")
            {
                auto i = std::static_pointer_cast<IndexSet>(e);
                i->object = rewriteExpr(i->object, repl);
                i->index = rewriteExpr(i->index, repl);
                i->value = rewriteExpr(i->value, repl);
            }
            else if (type == "List")
            {
                auto l = std::static_pointer_cast<List>(e);

                std::queue<ListElement> rewritten;
                while (not l->entries.empty())
                {
                    auto entry = l->entries.front();
                    l->entries.pop();

                    entry.key = rewriteExpr(entry.key, repl);
                    entry.value = rewriteExpr(entry.value, repl);
                    rewritten.push(entry);
                }
                l->entries = std::move(rewritten);
            }
            // AnonymousFunction bodies untouched (closures must re-evaluate)

            return e;
        }

        void rewriteStmt(const StatementPtr& s, const Replacements& repl)
        {
            if (not s)
                return;

            const auto& type = s->getType();

            if (type == "ExpressionStatement")
            {
                auto es = std::static_pointer_cast<ExpressionStatement>(s);
                es->expr = rewriteExpr(es->expr, repl);
            }
            else if (type == "VariableStatement")
            {
                auto v = std::static_pointer_cast<VariableStatement>(s);
                v->expr = rewriteExpr(v->expr, repl);
            }
            else if (type == "BlockStatement")
            {
                auto b = std::static_pointer_cast<BlockStatement>(s);

                std::queue<StatementPtr> statements = b->statements;
                std::queue<StatementPtr> rebuilt;
                while (not statements.empty())
                {
                    rewriteStmt(statements.front(), repl);
                    rebuilt.push(statements.front());
                    statements.pop();
                }
                b->statements = std::move(rebuilt);
            }
            else if (type == "IfStatement")
            {
                auto i = std::static_pointer_cast<IfStatement>(s);
                i->condition = rewriteExpr(i->condition, repl);
                rewriteStmt(i->thenBranch, repl);
                rewriteStmt(i->elseBranch, repl);
            }
            else if (type == "WhileStatement")
            {
                auto w = std::static_pointer_cast<WhileStatement>(s);
                w->condition = rewriteExpr(w->condition, repl);
                rewriteStmt(w->body, repl);
            }
            else if (type == "ForStatement")
            {
                auto fs = std::static_pointer_cast<ForStatement>(s);
                rewriteStmt(fs->initializer, repl);
                fs->condition = rewriteExpr(fs->condition, repl);
                fs->increment = rewriteExpr(fs->increment, repl);
                rewriteStmt(fs->body, repl);
            }
            else if (type == "ForInStatement")
            {
                auto fi = std::static_pointer_cast<ForInStatement>(s);
                fi->iterable = rewriteExpr(fi->iterable, repl);
                rewriteStmt(fi->body, repl);
            }
            else if (type == "ReturnStatement")
            {
                auto r = std::static_pointer_cast<ReturnStatement>(s);
                r->value = rewriteExpr(r->value, repl);
            }
            else if (type == "DPrintStatement")
            {
                auto d = std::static_pointer_cast<DPrintStatement>(s);
                d->expr = rewriteExpr(d->expr, repl);
            }
            // FunctionStatement / ClassStatement bodies untouched
        }

        // ------------------------------------------------------------------
        // Loop transformation (bottom-up)
        // ------------------------------------------------------------------

        StatementPtr transformStatement(StatementPtr s, Ctx& ctx);

        void transformExprFunctions(const ExprPtr& e, Ctx& ctx);

        /** Recurse into anonymous function bodies found inside expressions */
        void transformExprFunctions(const ExprPtr& e, Ctx& ctx)
        {
            if (not e)
                return;

            const auto& type = e->getType();

            if (type == "AnonymousFunction")
            {
                auto a = std::static_pointer_cast<AnonymousFunction>(e);

                // Fresh slot budget: own local slot space
                int savedSlots = ctx.slotsUsed;
                ctx.slotsUsed = static_cast<int>(a->parameters.size()) + countLocalSlots(a->body);

                a->body = transformStatement(a->body, ctx);

                ctx.slotsUsed = savedSlots;
            }
            else if (type == "BinaryExpression")
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                transformExprFunctions(b->leftExpr, ctx);
                transformExprFunctions(b->rightExpr, ctx);
            }
            else if (type == "LogicExpression")
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);
                transformExprFunctions(l->leftExpr, ctx);
                transformExprFunctions(l->rightExpr, ctx);
            }
            else if (type == "UnaryExpression")
            {
                transformExprFunctions(std::static_pointer_cast<UnaryExpression>(e)->expr, ctx);
            }
            else if (type == "CompoundAtom")
            {
                transformExprFunctions(std::static_pointer_cast<CompoundAtom>(e)->expr, ctx);
            }
            else if (type == "Assign")
            {
                transformExprFunctions(std::static_pointer_cast<Assign>(e)->expr, ctx);
            }
            else if (type == "CallExpression")
            {
                auto c = std::static_pointer_cast<CallExpression>(e);
                transformExprFunctions(c->caller, ctx);

                auto args = c->args;
                while (not args.empty())
                {
                    transformExprFunctions(args.front(), ctx);
                    args.pop();
                }
            }
            else if (type == "Get")
            {
                transformExprFunctions(std::static_pointer_cast<Get>(e)->object, ctx);
            }
            else if (type == "Set")
            {
                auto st = std::static_pointer_cast<Set>(e);
                transformExprFunctions(st->object, ctx);
                transformExprFunctions(st->value, ctx);
            }
            else if (type == "IndexGet")
            {
                auto i = std::static_pointer_cast<IndexGet>(e);
                transformExprFunctions(i->object, ctx);
                transformExprFunctions(i->index, ctx);
            }
            else if (type == "IndexSet")
            {
                auto i = std::static_pointer_cast<IndexSet>(e);
                transformExprFunctions(i->object, ctx);
                transformExprFunctions(i->index, ctx);
                transformExprFunctions(i->value, ctx);
            }
            else if (type == "List")
            {
                auto entries = std::static_pointer_cast<List>(e)->entries;
                while (not entries.empty())
                {
                    transformExprFunctions(entries.front().key, ctx);
                    transformExprFunctions(entries.front().value, ctx);
                    entries.pop();
                }
            }
        }

        /**
         * Try to hoist invariants out of one loop statement (whose children
         * were already transformed). Returns either the loop itself or a
         * Block wrapping the hidden declarations + the rewritten loop.
         */
        StatementPtr hoistLoop(const StatementPtr& s, Ctx& ctx)
        {
            const auto& type = s->getType();

            LoopFacts facts;
            analyzeStmt(s, facts);

            if (facts.hasCalls)
                return s;

            Candidates candidates;
            std::set<std::string> seen;

            // Only per-iteration regions produce candidates: bodies and
            // conditions/increments. For-initializers and for-in iterables
            // run once already.
            if (type == "WhileStatement")
            {
                auto w = std::static_pointer_cast<WhileStatement>(s);
                collectExpr(w->condition, facts, candidates, seen);
                collectStmt(w->body, facts, candidates, seen);
            }
            else if (type == "ForStatement")
            {
                auto fs = std::static_pointer_cast<ForStatement>(s);
                collectExpr(fs->condition, facts, candidates, seen);
                collectExpr(fs->increment, facts, candidates, seen);
                collectStmt(fs->body, facts, candidates, seen);
            }
            else if (type == "ForInStatement")
            {
                collectStmt(std::static_pointer_cast<ForInStatement>(s)->body, facts, candidates, seen);
            }

            if (candidates.empty())
                return s;

            // Respect the enclosing function's local-slot budget: skip the
            // hoist rather than risk exhausting the byte-addressed slots
            if (ctx.slotsUsed + static_cast<int>(candidates.size()) > kMaxLocalSlots)
                return s;

            ctx.slotsUsed += static_cast<int>(candidates.size());

            // One hidden declaration per distinct invariant; '@' cannot
            // appear in a source identifier, so no user name can collide
            std::queue<StatementPtr> block;
            Replacements repl;

            for (const auto& [key, expr] : candidates)
            {
                std::string name = "@hoist" + std::to_string(ctx.hoistCounter++);

                block.push(std::make_shared<VariableStatement>(Token(TokenType::EXPRESSION, name, 0, 0), expr));
                repl.emplace(key, name);
            }

            if (type == "WhileStatement")
            {
                auto w = std::static_pointer_cast<WhileStatement>(s);
                w->condition = rewriteExpr(w->condition, repl);
                rewriteStmt(w->body, repl);
            }
            else if (type == "ForStatement")
            {
                auto fs = std::static_pointer_cast<ForStatement>(s);
                fs->condition = rewriteExpr(fs->condition, repl);
                fs->increment = rewriteExpr(fs->increment, repl);
                rewriteStmt(fs->body, repl);
            }
            else if (type == "ForInStatement")
            {
                rewriteStmt(std::static_pointer_cast<ForInStatement>(s)->body, repl);
            }

            ctx.changed = true;

            block.push(s);

            return std::make_shared<BlockStatement>(block);
        }

        StatementPtr transformStatement(StatementPtr s, Ctx& ctx)
        {
            if (not s)
                return s;

            const auto& type = s->getType();

            if (type == "BlockStatement")
            {
                auto b = std::static_pointer_cast<BlockStatement>(s);

                std::queue<StatementPtr> rebuilt;
                while (not b->statements.empty())
                {
                    rebuilt.push(transformStatement(b->statements.front(), ctx));
                    b->statements.pop();
                }
                b->statements = std::move(rebuilt);

                return s;
            }

            if (type == "IfStatement")
            {
                auto i = std::static_pointer_cast<IfStatement>(s);
                transformExprFunctions(i->condition, ctx);
                i->thenBranch = transformStatement(i->thenBranch, ctx);
                i->elseBranch = transformStatement(i->elseBranch, ctx);

                return s;
            }

            if (type == "FunctionStatement")
            {
                auto fn = std::static_pointer_cast<FunctionStatement>(s);

                // Fresh slot budget: a function has its own local slot space
                int savedSlots = ctx.slotsUsed;
                ctx.slotsUsed = static_cast<int>(fn->parameters.size()) + countLocalSlots(fn->body);

                fn->body = transformStatement(fn->body, ctx);

                ctx.slotsUsed = savedSlots;

                return s;
            }

            if (type == "ClassStatement")
            {
                auto c = std::static_pointer_cast<ClassStatement>(s);

                std::queue<std::shared_ptr<FunctionStatement>> rebuilt;
                while (not c->methods.empty())
                {
                    auto method = c->methods.front();
                    c->methods.pop();

                    // Fresh slot budget per method (+1 for 'this')
                    int savedSlots = ctx.slotsUsed;
                    ctx.slotsUsed = 1 + static_cast<int>(method->parameters.size()) + countLocalSlots(method->body);

                    method->body = transformStatement(method->body, ctx);

                    ctx.slotsUsed = savedSlots;

                    rebuilt.push(method);
                }
                c->methods = std::move(rebuilt);

                return s;
            }

            if (type == "WhileStatement")
            {
                auto w = std::static_pointer_cast<WhileStatement>(s);
                transformExprFunctions(w->condition, ctx);
                w->body = transformStatement(w->body, ctx);

                return hoistLoop(s, ctx);
            }

            if (type == "ForStatement")
            {
                auto fs = std::static_pointer_cast<ForStatement>(s);
                fs->initializer = transformStatement(fs->initializer, ctx);
                transformExprFunctions(fs->condition, ctx);
                transformExprFunctions(fs->increment, ctx);
                fs->body = transformStatement(fs->body, ctx);

                return hoistLoop(s, ctx);
            }

            if (type == "ForInStatement")
            {
                auto fi = std::static_pointer_cast<ForInStatement>(s);
                transformExprFunctions(fi->iterable, ctx);
                fi->body = transformStatement(fi->body, ctx);

                return hoistLoop(s, ctx);
            }

            if (type == "ExpressionStatement")
            {
                transformExprFunctions(std::static_pointer_cast<ExpressionStatement>(s)->expr, ctx);
                return s;
            }

            if (type == "VariableStatement")
            {
                transformExprFunctions(std::static_pointer_cast<VariableStatement>(s)->expr, ctx);
                return s;
            }

            if (type == "ReturnStatement")
            {
                transformExprFunctions(std::static_pointer_cast<ReturnStatement>(s)->value, ctx);
                return s;
            }

            return s;
        }
    }

    bool LoopInvariantHoistingPass::runPass(VM*, std::queue<StatementPtr>& statements)
    {
        Ctx ctx;

        // Slot budget of the top-level script function (hidden hoist vars in
        // wrapping blocks are locals of the script, not globals)
        {
            auto counting = statements;
            while (not counting.empty())
            {
                ctx.slotsUsed += countLocalSlots(counting.front());
                counting.pop();
            }
        }

        std::queue<StatementPtr> rebuilt;

        while (not statements.empty())
        {
            rebuilt.push(transformStatement(statements.front(), ctx));
            statements.pop();
        }

        statements = std::move(rebuilt);

        return ctx.changed;
    }
}
