#include "stdafx.h"

#include "entity_loop_lowering.h"

#include "ast_analysis.h"

#include "Interpreter/expression.h"
#include "Interpreter/statement.h"

#include "logger.h"

#include <set>
#include <string>

namespace pg
{
    namespace
    {
        const char* kGetEntities = "getEntities";
        const char* kIdsNative = "__ecsEntityIds";
        const char* kViewNative = "__ecsEntityView";

        struct Ctx
        {
            int loopCounter = 0;
            bool changed = false;
        };

        // ------------------------------------------------------------------
        // Shadow guard: the whole program (including nested function/class
        // bodies and parameters) must not declare or assign any of the names
        // the lowering relies on.
        // ------------------------------------------------------------------

        bool isGuardedName(const std::string& name)
        {
            return name == kGetEntities or name == kIdsNative or name == kViewNative;
        }

        bool shadowsGuardedNameStmt(const StatementPtr& s);

        bool shadowsGuardedNameExpr(const ExprPtr& e)
        {
            if (not e)
                return false;

            const auto& type = e->getType();

            if (type == "Assign")
            {
                auto a = std::static_pointer_cast<Assign>(e);
                return isGuardedName(a->name.text) or shadowsGuardedNameExpr(a->expr);
            }

            if (type == "PreFixExpression")
                return isGuardedName(std::static_pointer_cast<PreFixExpression>(e)->name.text);

            if (type == "PostFixExpression")
                return isGuardedName(std::static_pointer_cast<PostFixExpression>(e)->name.text);

            if (type == "BinaryExpression")
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                return shadowsGuardedNameExpr(b->leftExpr) or shadowsGuardedNameExpr(b->rightExpr);
            }

            if (type == "LogicExpression")
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);
                return shadowsGuardedNameExpr(l->leftExpr) or shadowsGuardedNameExpr(l->rightExpr);
            }

            if (type == "UnaryExpression")
                return shadowsGuardedNameExpr(std::static_pointer_cast<UnaryExpression>(e)->expr);

            if (type == "CompoundAtom")
                return shadowsGuardedNameExpr(std::static_pointer_cast<CompoundAtom>(e)->expr);

            if (type == "CallExpression")
            {
                auto c = std::static_pointer_cast<CallExpression>(e);

                if (shadowsGuardedNameExpr(c->caller))
                    return true;

                auto args = c->args;
                while (not args.empty())
                {
                    if (shadowsGuardedNameExpr(args.front()))
                        return true;

                    args.pop();
                }

                return false;
            }

            if (type == "Get")
                return shadowsGuardedNameExpr(std::static_pointer_cast<Get>(e)->object);

            if (type == "Set")
            {
                auto st = std::static_pointer_cast<Set>(e);
                return shadowsGuardedNameExpr(st->object) or shadowsGuardedNameExpr(st->value);
            }

            if (type == "IndexGet")
            {
                auto i = std::static_pointer_cast<IndexGet>(e);
                return shadowsGuardedNameExpr(i->object) or shadowsGuardedNameExpr(i->index);
            }

            if (type == "IndexSet")
            {
                auto i = std::static_pointer_cast<IndexSet>(e);
                return shadowsGuardedNameExpr(i->object) or shadowsGuardedNameExpr(i->index)
                    or shadowsGuardedNameExpr(i->value);
            }

            if (type == "List")
            {
                auto entries = std::static_pointer_cast<List>(e)->entries;
                while (not entries.empty())
                {
                    if (shadowsGuardedNameExpr(entries.front().key) or
                        shadowsGuardedNameExpr(entries.front().value))
                        return true;

                    entries.pop();
                }

                return false;
            }

            if (type == "AnonymousFunction")
            {
                auto a = std::static_pointer_cast<AnonymousFunction>(e);

                auto params = a->parameters;
                while (not params.empty())
                {
                    if (params.front()->getType() == "Var" and
                        isGuardedName(std::static_pointer_cast<Var>(params.front())->name.text))
                        return true;

                    params.pop();
                }

                return shadowsGuardedNameStmt(a->body);
            }

            return false;
        }

        bool shadowsGuardedNameStmt(const StatementPtr& s)
        {
            if (not s)
                return false;

            const auto& type = s->getType();

            if (type == "VariableStatement")
            {
                auto v = std::static_pointer_cast<VariableStatement>(s);
                return isGuardedName(v->name.text) or shadowsGuardedNameExpr(v->expr);
            }

            if (type == "FunctionStatement")
            {
                auto fn = std::static_pointer_cast<FunctionStatement>(s);

                if (isGuardedName(fn->name.text))
                    return true;

                auto params = fn->parameters;
                while (not params.empty())
                {
                    if (params.front()->getType() == "Var" and
                        isGuardedName(std::static_pointer_cast<Var>(params.front())->name.text))
                        return true;

                    params.pop();
                }

                return shadowsGuardedNameStmt(fn->body);
            }

            if (type == "ClassStatement")
            {
                auto c = std::static_pointer_cast<ClassStatement>(s);

                if (isGuardedName(c->name.text))
                    return true;

                auto methods = c->methods;
                while (not methods.empty())
                {
                    if (shadowsGuardedNameStmt(methods.front()))
                        return true;

                    methods.pop();
                }

                return false;
            }

            if (type == "ExpressionStatement")
                return shadowsGuardedNameExpr(std::static_pointer_cast<ExpressionStatement>(s)->expr);

            if (type == "BlockStatement")
            {
                auto statements = std::static_pointer_cast<BlockStatement>(s)->statements;
                while (not statements.empty())
                {
                    if (shadowsGuardedNameStmt(statements.front()))
                        return true;

                    statements.pop();
                }

                return false;
            }

            if (type == "IfStatement")
            {
                auto i = std::static_pointer_cast<IfStatement>(s);
                return shadowsGuardedNameExpr(i->condition) or
                       shadowsGuardedNameStmt(i->thenBranch) or
                       shadowsGuardedNameStmt(i->elseBranch);
            }

            if (type == "WhileStatement")
            {
                auto w = std::static_pointer_cast<WhileStatement>(s);
                return shadowsGuardedNameExpr(w->condition) or shadowsGuardedNameStmt(w->body);
            }

            if (type == "ForStatement")
            {
                auto fs = std::static_pointer_cast<ForStatement>(s);
                return shadowsGuardedNameStmt(fs->initializer) or
                       shadowsGuardedNameExpr(fs->condition) or
                       shadowsGuardedNameExpr(fs->increment) or
                       shadowsGuardedNameStmt(fs->body);
            }

            if (type == "ForInStatement")
            {
                auto fi = std::static_pointer_cast<ForInStatement>(s);
                return isGuardedName(fi->varName.text) or
                       shadowsGuardedNameExpr(fi->iterable) or
                       shadowsGuardedNameStmt(fi->body);
            }

            if (type == "ReturnStatement")
                return shadowsGuardedNameExpr(std::static_pointer_cast<ReturnStatement>(s)->value);

            if (type == "DPrintStatement")
                return shadowsGuardedNameExpr(std::static_pointer_cast<DPrintStatement>(s)->expr);

            return false;
        }

        // ------------------------------------------------------------------
        // Loop-variable usage analysis (whitelist): every occurrence of the
        // loop variable must be the object of a property read/write. Any
        // other appearance - or an appearance inside a nested function/class
        // body (capture escape) - bails.
        // ------------------------------------------------------------------

        bool isLoopVar(const ExprPtr& e, const std::string& name)
        {
            return e and e->getType() == "Var" and
                std::static_pointer_cast<Var>(e)->name.text == name;
        }

        /** Does this subtree mention the loop variable at all? (used for nested fn bodies) */
        bool mentionsVarStmt(const StatementPtr& s, const std::string& name);

        bool mentionsVarExpr(const ExprPtr& e, const std::string& name)
        {
            if (not e)
                return false;

            if (isLoopVar(e, name))
                return true;

            const auto& type = e->getType();

            if (type == "BinaryExpression")
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                return mentionsVarExpr(b->leftExpr, name) or mentionsVarExpr(b->rightExpr, name);
            }
            if (type == "LogicExpression")
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);
                return mentionsVarExpr(l->leftExpr, name) or mentionsVarExpr(l->rightExpr, name);
            }
            if (type == "UnaryExpression")
                return mentionsVarExpr(std::static_pointer_cast<UnaryExpression>(e)->expr, name);
            if (type == "CompoundAtom")
                return mentionsVarExpr(std::static_pointer_cast<CompoundAtom>(e)->expr, name);
            if (type == "Assign")
                return mentionsVarExpr(std::static_pointer_cast<Assign>(e)->expr, name);
            if (type == "CallExpression")
            {
                auto c = std::static_pointer_cast<CallExpression>(e);
                if (mentionsVarExpr(c->caller, name))
                    return true;
                auto args = c->args;
                while (not args.empty())
                {
                    if (mentionsVarExpr(args.front(), name))
                        return true;
                    args.pop();
                }
                return false;
            }
            if (type == "Get")
                return mentionsVarExpr(std::static_pointer_cast<Get>(e)->object, name);
            if (type == "Set")
            {
                auto st = std::static_pointer_cast<Set>(e);
                return mentionsVarExpr(st->object, name) or mentionsVarExpr(st->value, name);
            }
            if (type == "IndexGet")
            {
                auto i = std::static_pointer_cast<IndexGet>(e);
                return mentionsVarExpr(i->object, name) or mentionsVarExpr(i->index, name);
            }
            if (type == "IndexSet")
            {
                auto i = std::static_pointer_cast<IndexSet>(e);
                return mentionsVarExpr(i->object, name) or mentionsVarExpr(i->index, name)
                    or mentionsVarExpr(i->value, name);
            }
            if (type == "List")
            {
                auto entries = std::static_pointer_cast<List>(e)->entries;
                while (not entries.empty())
                {
                    if (mentionsVarExpr(entries.front().key, name) or
                        mentionsVarExpr(entries.front().value, name))
                        return true;
                    entries.pop();
                }
                return false;
            }
            if (type == "AnonymousFunction")
                return mentionsVarStmt(std::static_pointer_cast<AnonymousFunction>(e)->body, name);

            return false;
        }

        bool mentionsVarStmt(const StatementPtr& s, const std::string& name)
        {
            if (not s)
                return false;

            const auto& type = s->getType();

            if (type == "ExpressionStatement")
                return mentionsVarExpr(std::static_pointer_cast<ExpressionStatement>(s)->expr, name);
            if (type == "VariableStatement")
                return mentionsVarExpr(std::static_pointer_cast<VariableStatement>(s)->expr, name);
            if (type == "BlockStatement")
            {
                auto statements = std::static_pointer_cast<BlockStatement>(s)->statements;
                while (not statements.empty())
                {
                    if (mentionsVarStmt(statements.front(), name))
                        return true;
                    statements.pop();
                }
                return false;
            }
            if (type == "IfStatement")
            {
                auto i = std::static_pointer_cast<IfStatement>(s);
                return mentionsVarExpr(i->condition, name) or
                       mentionsVarStmt(i->thenBranch, name) or
                       mentionsVarStmt(i->elseBranch, name);
            }
            if (type == "WhileStatement")
            {
                auto w = std::static_pointer_cast<WhileStatement>(s);
                return mentionsVarExpr(w->condition, name) or mentionsVarStmt(w->body, name);
            }
            if (type == "ForStatement")
            {
                auto fs = std::static_pointer_cast<ForStatement>(s);
                return mentionsVarStmt(fs->initializer, name) or
                       mentionsVarExpr(fs->condition, name) or
                       mentionsVarExpr(fs->increment, name) or
                       mentionsVarStmt(fs->body, name);
            }
            if (type == "ForInStatement")
            {
                auto fi = std::static_pointer_cast<ForInStatement>(s);
                return mentionsVarExpr(fi->iterable, name) or mentionsVarStmt(fi->body, name);
            }
            if (type == "ReturnStatement")
                return mentionsVarExpr(std::static_pointer_cast<ReturnStatement>(s)->value, name);
            if (type == "DPrintStatement")
                return mentionsVarExpr(std::static_pointer_cast<DPrintStatement>(s)->expr, name);
            if (type == "FunctionStatement")
                return mentionsVarStmt(std::static_pointer_cast<FunctionStatement>(s)->body, name);
            if (type == "ClassStatement")
            {
                auto methods = std::static_pointer_cast<ClassStatement>(s)->methods;
                while (not methods.empty())
                {
                    if (mentionsVarStmt(methods.front()->body, name))
                        return true;
                    methods.pop();
                }
                return false;
            }

            return false;
        }

        struct UsageResult
        {
            std::set<std::string> members;
            bool ok = true;

            void bail() { ok = false; }
        };

        /** String-literal index (e["PositionComponent"]) - the common bracket form */
        bool isStringLiteral(const ExprPtr& e, std::string& out)
        {
            if (not e or e->getType() != "Atom")
                return false;

            const auto& v = std::static_pointer_cast<Atom>(e)->value;

            if (v.type != UnionType::STRING)
                return false;

            out = v.toString();
            return true;
        }

        void collectUsageStmt(const StatementPtr& s, const std::string& name, UsageResult& r);

        void collectUsageExpr(const ExprPtr& e, const std::string& name, UsageResult& r)
        {
            if (not e or not r.ok)
                return;

            // A bare occurrence that reaches this point (not consumed as a
            // Get/Set object below) is an escape
            if (isLoopVar(e, name))
            {
                r.bail();
                return;
            }

            const auto& type = e->getType();

            if (type == "Get")
            {
                auto g = std::static_pointer_cast<Get>(e);

                if (isLoopVar(g->object, name))
                {
                    r.members.insert(g->name.text);
                    return;
                }

                collectUsageExpr(g->object, name, r);
                return;
            }

            if (type == "Set")
            {
                auto st = std::static_pointer_cast<Set>(e);

                if (isLoopVar(st->object, name))
                    r.members.insert(st->name.text);
                else
                    collectUsageExpr(st->object, name, r);

                collectUsageExpr(st->value, name, r);
                return;
            }

            if (type == "BinaryExpression")
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                collectUsageExpr(b->leftExpr, name, r);
                collectUsageExpr(b->rightExpr, name, r);
                return;
            }

            if (type == "LogicExpression")
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);
                collectUsageExpr(l->leftExpr, name, r);
                collectUsageExpr(l->rightExpr, name, r);
                return;
            }

            if (type == "UnaryExpression")
            {
                collectUsageExpr(std::static_pointer_cast<UnaryExpression>(e)->expr, name, r);
                return;
            }

            if (type == "CompoundAtom")
            {
                collectUsageExpr(std::static_pointer_cast<CompoundAtom>(e)->expr, name, r);
                return;
            }

            if (type == "Assign")
            {
                collectUsageExpr(std::static_pointer_cast<Assign>(e)->expr, name, r);
                return;
            }

            if (type == "CallExpression")
            {
                auto c = std::static_pointer_cast<CallExpression>(e);

                collectUsageExpr(c->caller, name, r);

                auto args = c->args;
                while (not args.empty())
                {
                    collectUsageExpr(args.front(), name, r);
                    args.pop();
                }

                return;
            }

            if (type == "IndexGet")
            {
                auto i = std::static_pointer_cast<IndexGet>(e);

                // e["Component"] with a string literal is member access (the
                // common bracket form in real scripts); dynamic keys bail
                std::string literal;
                if (isLoopVar(i->object, name))
                {
                    if (isStringLiteral(i->index, literal))
                        r.members.insert(literal);
                    else
                        r.bail();

                    return;
                }

                collectUsageExpr(i->object, name, r);
                collectUsageExpr(i->index, name, r);
                return;
            }

            if (type == "IndexSet")
            {
                auto i = std::static_pointer_cast<IndexSet>(e);

                std::string literal;
                if (isLoopVar(i->object, name))
                {
                    if (isStringLiteral(i->index, literal))
                        r.members.insert(literal);
                    else
                        r.bail();
                }
                else
                {
                    collectUsageExpr(i->object, name, r);
                    collectUsageExpr(i->index, name, r);
                }

                collectUsageExpr(i->value, name, r);
                return;
            }

            if (type == "List")
            {
                auto entries = std::static_pointer_cast<List>(e)->entries;
                while (not entries.empty())
                {
                    collectUsageExpr(entries.front().key, name, r);
                    collectUsageExpr(entries.front().value, name, r);
                    entries.pop();
                }
                return;
            }

            if (type == "AnonymousFunction")
            {
                // Capturing the loop variable in a closure is an escape
                if (mentionsVarStmt(std::static_pointer_cast<AnonymousFunction>(e)->body, name))
                    r.bail();

                return;
            }

            // Atom / Var(other) / This / Pre/PostFix(other var): nothing to do
        }

        void collectUsageStmt(const StatementPtr& s, const std::string& name, UsageResult& r)
        {
            if (not s or not r.ok)
                return;

            const auto& type = s->getType();

            if (type == "ExpressionStatement")
            {
                collectUsageExpr(std::static_pointer_cast<ExpressionStatement>(s)->expr, name, r);
            }
            else if (type == "VariableStatement")
            {
                collectUsageExpr(std::static_pointer_cast<VariableStatement>(s)->expr, name, r);
            }
            else if (type == "BlockStatement")
            {
                auto statements = std::static_pointer_cast<BlockStatement>(s)->statements;
                while (not statements.empty())
                {
                    collectUsageStmt(statements.front(), name, r);
                    statements.pop();
                }
            }
            else if (type == "IfStatement")
            {
                auto i = std::static_pointer_cast<IfStatement>(s);
                collectUsageExpr(i->condition, name, r);
                collectUsageStmt(i->thenBranch, name, r);
                collectUsageStmt(i->elseBranch, name, r);
            }
            else if (type == "WhileStatement")
            {
                auto w = std::static_pointer_cast<WhileStatement>(s);
                collectUsageExpr(w->condition, name, r);
                collectUsageStmt(w->body, name, r);
            }
            else if (type == "ForStatement")
            {
                auto fs = std::static_pointer_cast<ForStatement>(s);
                collectUsageStmt(fs->initializer, name, r);
                collectUsageExpr(fs->condition, name, r);
                collectUsageExpr(fs->increment, name, r);
                collectUsageStmt(fs->body, name, r);
            }
            else if (type == "ForInStatement")
            {
                auto fi = std::static_pointer_cast<ForInStatement>(s);
                collectUsageExpr(fi->iterable, name, r);

                // Inner loop shadowing the name means uses inside refer to
                // the inner variable; handled by the mutated-set bail
                collectUsageStmt(fi->body, name, r);
            }
            else if (type == "ReturnStatement")
            {
                collectUsageExpr(std::static_pointer_cast<ReturnStatement>(s)->value, name, r);
            }
            else if (type == "DPrintStatement")
            {
                collectUsageExpr(std::static_pointer_cast<DPrintStatement>(s)->expr, name, r);
            }
            else if (type == "FunctionStatement")
            {
                // Nested function capturing the loop variable is an escape
                if (mentionsVarStmt(std::static_pointer_cast<FunctionStatement>(s)->body, name))
                    r.bail();
            }
            else if (type == "ClassStatement")
            {
                auto methods = std::static_pointer_cast<ClassStatement>(s)->methods;
                while (not methods.empty())
                {
                    if (mentionsVarStmt(methods.front()->body, name))
                    {
                        r.bail();
                        return;
                    }
                    methods.pop();
                }
            }
            // Break/Continue/Import: nothing
        }

        // ------------------------------------------------------------------
        // The lowering itself
        // ------------------------------------------------------------------

        /** Matches `getEntities(<one arg>)` as the loop iterable */
        std::shared_ptr<CallExpression> matchGetEntitiesCall(const ExprPtr& iterable)
        {
            if (not iterable or iterable->getType() != "CallExpression")
                return nullptr;

            auto call = std::static_pointer_cast<CallExpression>(iterable);

            if (not call->caller or call->caller->getType() != "Var")
                return nullptr;

            if (std::static_pointer_cast<Var>(call->caller)->name.text != kGetEntities)
                return nullptr;

            if (call->args.size() != 1)
                return nullptr;

            return call;
        }

        void tryLowerLoop(const std::shared_ptr<ForInStatement>& fi, Ctx& ctx)
        {
            auto call = matchGetEntitiesCall(fi->iterable);

            if (not call)
                return;

            const std::string& varName = fi->varName.text;

            // Reassignment/shadowing of the loop variable inside the body
            ast::LoopFacts facts;
            ast::analyzeStmt(fi->body, facts);

            if (facts.mutated.count(varName) > 0)
                return;

            // Whitelisted usage + accessed member collection
            UsageResult usage;
            collectUsageStmt(fi->body, varName, usage);

            if (not usage.ok)
                return;

            // The lazy view does not attach the has/attachComp closures
            if (usage.members.count("has") > 0 or usage.members.count("attachComp") > 0)
                return;

            // __entityId is always populated by the view; it is not a component
            usage.members.erase("__entityId");

            // --- Rewrite ---
            const int line = fi->varName.line;

            Token parenTok(TokenType::PENTER, "(", line, 0);
            Token eidTok(TokenType::EXPRESSION, "@eid" + std::to_string(ctx.loopCounter++), line, 0);

            // Iterable: __ecsEntityIds(<original arg>)
            auto idsCall = std::make_shared<CallExpression>(
                std::make_shared<Var>(Token(TokenType::EXPRESSION, kIdsNative, line, 0)),
                parenTok, call->args);

            // Per-iteration view: var <e> = __ecsEntityView(@eidN, "CompA", ...)
            std::queue<ExprPtr> viewArgs;
            viewArgs.push(std::make_shared<Var>(eidTok));

            for (const auto& member : usage.members)
                viewArgs.push(std::make_shared<Atom>(member));

            auto viewCall = std::make_shared<CallExpression>(
                std::make_shared<Var>(Token(TokenType::EXPRESSION, kViewNative, line, 0)),
                parenTok, viewArgs);

            std::queue<StatementPtr> newBody;
            newBody.push(std::make_shared<VariableStatement>(fi->varName, viewCall));
            newBody.push(fi->body);

            fi->varName = eidTok;
            fi->iterable = idsCall;
            fi->body = std::make_shared<BlockStatement>(newBody);

            ctx.changed = true;

            LOG_INFO("EntityLoopLowering", "Lowered getEntities loop over '" << varName
                     << "' (" << usage.members.size() << " component(s))");
        }

        // ------------------------------------------------------------------
        // Traversal: find every ForIn in the program (including nested
        // function/class bodies) and try to lower it. In-place - the ForIn
        // node itself is rewritten, no parent replacement needed.
        // ------------------------------------------------------------------

        void walkStmt(const StatementPtr& s, Ctx& ctx);

        void walkExpr(const ExprPtr& e, Ctx& ctx)
        {
            if (not e)
                return;

            const auto& type = e->getType();

            if (type == "AnonymousFunction")
            {
                walkStmt(std::static_pointer_cast<AnonymousFunction>(e)->body, ctx);
            }
            else if (type == "BinaryExpression")
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                walkExpr(b->leftExpr, ctx);
                walkExpr(b->rightExpr, ctx);
            }
            else if (type == "LogicExpression")
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);
                walkExpr(l->leftExpr, ctx);
                walkExpr(l->rightExpr, ctx);
            }
            else if (type == "UnaryExpression")
            {
                walkExpr(std::static_pointer_cast<UnaryExpression>(e)->expr, ctx);
            }
            else if (type == "CompoundAtom")
            {
                walkExpr(std::static_pointer_cast<CompoundAtom>(e)->expr, ctx);
            }
            else if (type == "Assign")
            {
                walkExpr(std::static_pointer_cast<Assign>(e)->expr, ctx);
            }
            else if (type == "CallExpression")
            {
                auto c = std::static_pointer_cast<CallExpression>(e);
                walkExpr(c->caller, ctx);

                auto args = c->args;
                while (not args.empty())
                {
                    walkExpr(args.front(), ctx);
                    args.pop();
                }
            }
            else if (type == "Get")
            {
                walkExpr(std::static_pointer_cast<Get>(e)->object, ctx);
            }
            else if (type == "Set")
            {
                auto st = std::static_pointer_cast<Set>(e);
                walkExpr(st->object, ctx);
                walkExpr(st->value, ctx);
            }
            else if (type == "IndexGet")
            {
                auto i = std::static_pointer_cast<IndexGet>(e);
                walkExpr(i->object, ctx);
                walkExpr(i->index, ctx);
            }
            else if (type == "IndexSet")
            {
                auto i = std::static_pointer_cast<IndexSet>(e);
                walkExpr(i->object, ctx);
                walkExpr(i->index, ctx);
                walkExpr(i->value, ctx);
            }
            else if (type == "List")
            {
                auto entries = std::static_pointer_cast<List>(e)->entries;
                while (not entries.empty())
                {
                    walkExpr(entries.front().key, ctx);
                    walkExpr(entries.front().value, ctx);
                    entries.pop();
                }
            }
        }

        void walkStmt(const StatementPtr& s, Ctx& ctx)
        {
            if (not s)
                return;

            const auto& type = s->getType();

            if (type == "ForInStatement")
            {
                auto fi = std::static_pointer_cast<ForInStatement>(s);

                // Lower inner loops first (the body walk), then this one
                walkExpr(fi->iterable, ctx);
                walkStmt(fi->body, ctx);

                tryLowerLoop(fi, ctx);
            }
            else if (type == "BlockStatement")
            {
                auto statements = std::static_pointer_cast<BlockStatement>(s)->statements;
                while (not statements.empty())
                {
                    walkStmt(statements.front(), ctx);
                    statements.pop();
                }
            }
            else if (type == "IfStatement")
            {
                auto i = std::static_pointer_cast<IfStatement>(s);
                walkExpr(i->condition, ctx);
                walkStmt(i->thenBranch, ctx);
                walkStmt(i->elseBranch, ctx);
            }
            else if (type == "WhileStatement")
            {
                auto w = std::static_pointer_cast<WhileStatement>(s);
                walkExpr(w->condition, ctx);
                walkStmt(w->body, ctx);
            }
            else if (type == "ForStatement")
            {
                auto fs = std::static_pointer_cast<ForStatement>(s);
                walkStmt(fs->initializer, ctx);
                walkExpr(fs->condition, ctx);
                walkExpr(fs->increment, ctx);
                walkStmt(fs->body, ctx);
            }
            else if (type == "FunctionStatement")
            {
                walkStmt(std::static_pointer_cast<FunctionStatement>(s)->body, ctx);
            }
            else if (type == "ClassStatement")
            {
                auto methods = std::static_pointer_cast<ClassStatement>(s)->methods;
                while (not methods.empty())
                {
                    walkStmt(methods.front()->body, ctx);
                    methods.pop();
                }
            }
            else if (type == "ExpressionStatement")
            {
                walkExpr(std::static_pointer_cast<ExpressionStatement>(s)->expr, ctx);
            }
            else if (type == "VariableStatement")
            {
                walkExpr(std::static_pointer_cast<VariableStatement>(s)->expr, ctx);
            }
            else if (type == "ReturnStatement")
            {
                walkExpr(std::static_pointer_cast<ReturnStatement>(s)->value, ctx);
            }
            else if (type == "DPrintStatement")
            {
                walkExpr(std::static_pointer_cast<DPrintStatement>(s)->expr, ctx);
            }
        }
    }

    bool EntityLoopLoweringPass::runPass(VM*, std::queue<StatementPtr>& statements)
    {
        // Shadow guard: any declaration/assignment of the involved names
        // anywhere in the program disables the whole pass
        {
            auto scan = statements;
            while (not scan.empty())
            {
                if (shadowsGuardedNameStmt(scan.front()))
                    return false;

                scan.pop();
            }
        }

        Ctx ctx;

        auto walk = statements;
        while (not walk.empty())
        {
            walkStmt(walk.front(), ctx);
            walk.pop();
        }

        return ctx.changed;
    }
}
