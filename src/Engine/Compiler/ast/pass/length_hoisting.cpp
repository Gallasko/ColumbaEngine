#include "stdafx.h"

#include "length_hoisting.h"

#include "ast_analysis.h"
#include "loop_ownership.h"
#include "program_view.h"

#include "Compiler/vm.h"
#include "logger.h"

#include <algorithm>

namespace pg
{
    namespace
    {
        using namespace ast;

        const char* DOM = "LengthHoisting";

        // The x of `len(x)` when it is a path, or of `strlen(x)` when it is a name, or nullptr
        ExprPtr lenArgOf(const ExprPtr& e)
        {
            if (not e or e->kind() != AstKind::CallExpression)
                return nullptr;

            auto c = std::static_pointer_cast<CallExpression>(e);
            const std::string callee = varName(c->caller);

            if (c->args.size() != 1 or pathOf(c->args.front()).empty())
                return nullptr;

            if (callee == "len" or (callee == "strlen" and c->args.front()->kind() == AstKind::Var))
                return c->args.front();

            return nullptr;
        }

        std::string calleeOf(const ExprPtr& call)
        {
            return varName(std::static_pointer_cast<CallExpression>(call)->caller);
        }

        // The call and its path: `len(t)` and `strlen(t)` are hoisted apart
        std::string lenKey(const ExprPtr& e)
        {
            auto x = lenArgOf(e);
            return x ? calleeOf(e) + " " + pathOf(x) : "";
        }

        // The `len` and `strlen` calls a condition makes on its first evaluation, by lenKey
        void firstLens(const ExprPtr& e, std::map<std::string, ExprPtr>& out)
        {
            if (not e)
                return;

            if (lenArgOf(e))
            {
                out[lenKey(e)] = e;
                return;
            }

            const AstKind type = e->kind();

            if (type == AstKind::BinaryExpression)
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                firstLens(b->leftExpr, out);
                firstLens(b->rightExpr, out);
            }
            else if (type == AstKind::LogicExpression)
                // The right side may not be evaluated
                firstLens(std::static_pointer_cast<LogicExpression>(e)->leftExpr, out);
            else if (type == AstKind::UnaryExpression)
                firstLens(std::static_pointer_cast<UnaryExpression>(e)->expr, out);
            else if (type == AstKind::CompoundAtom)
                firstLens(std::static_pointer_cast<CompoundAtom>(e)->expr, out);
            else if (type == AstKind::CallExpression)
            {
                for (const auto& a : listOf(std::static_pointer_cast<CallExpression>(e)->args))
                    firstLens(a, out);
            }
        }

        StatementPtr hoistLoop(const StatementPtr& s, FunctionScope& ctx)
        {
            const AstKind type = s->kind();
            ExprPtr condition;
            ast::LoopFacts facts;

            if (type == AstKind::WhileStatement)
            {
                auto w = std::static_pointer_cast<WhileStatement>(s);
                condition = w->condition;
            }
            else
            {
                auto f = std::static_pointer_cast<ForStatement>(s);
                condition = f->condition;
            }

            std::map<std::string, ExprPtr> lens;
            firstLens(condition, lens);

            if (lens.empty())
                return s;

            ast::analyzeStmt(s, facts);

            std::map<std::string, std::string> hidden;
            std::queue<StatementPtr> block;

            for (const auto& [key, call] : lens)
            {
                const ExprPtr x = lenArgOf(call);
                const std::string callee = calleeOf(call);

                if (not ctx.isNative(callee))
                    continue;

                std::set<std::string> names;
                namesOf(x, names);

                if (std::any_of(names.begin(), names.end(), [&](const std::string& name) { return facts.mutated.count(name); }))
                    continue;

                const std::string root = rootOf(x);

                // A string cannot be changed: what strlen measured stays as long as the name does. The
                // check still refuses script code in the region, and a write into a list it may be a row of
                const bool isStrlen = callee == "strlen";

                RegionCheck check{ctx, root, isStrlen or ctx.beyondWrites(x), false, isStrlen or ctx.isList(x)};
                check.stmt(s);

                if (not check.ok)
                    continue;

                if (ctx.slotsUsed + 1 > ast::kMaxLocalSlots)
                    break;

                ++ctx.slotsUsed;

                // '@' cannot start a source name: no user name collides
                const std::string name = "@len" + std::to_string(ctx.counter++);
                const Token lenToken(TokenType::EXPRESSION, callee, 0, 0);

                std::queue<ExprPtr> args;
                args.push(x);

                auto hoisted = std::make_shared<CallExpression>(std::make_shared<Var>(lenToken), lenToken, args);

                block.push(std::make_shared<VariableStatement>(Token(TokenType::EXPRESSION, name, 0, 0), hoisted));
                hidden[key] = name;
            }

            if (hidden.empty())
                return s;

            Replace replace{hidden, lenKey};
            replace.stmt(s);

            ctx.hoisted += hidden.size();
            block.push(s);

            return std::make_shared<BlockStatement>(block);
        }

        StatementPtr transform(const StatementPtr& s, FunctionScope& ctx);

        void transformFunction(const std::queue<ExprPtr>& parameters, StatementPtr& body, FunctionScope& ctx, int extraSlots)
        {
            std::set<std::string> savedLocals = ctx.locals;
            Ownership savedOwnership = ctx.ownership;
            const bool savedIn = ctx.inFunction;
            const int savedSlots = ctx.slotsUsed;

            ctx.locals.clear();

            for (const auto& p : listOf(parameters))
                ctx.locals.insert(p->getName());

            declsOf(body, ctx.locals);

            ctx.ownership = Ownership::ofFunction(parameters, body);

            ctx.inFunction = true;
            ctx.slotsUsed = extraSlots + static_cast<int>(parameters.size()) + ast::countLocalSlots(body);

            body = transform(body, ctx);

            ctx.locals = savedLocals;
            ctx.ownership = savedOwnership;
            ctx.inFunction = savedIn;
            ctx.slotsUsed = savedSlots;
        }

        void transformExpr(const ExprPtr& e, FunctionScope& ctx);

        // Functions defined inside expressions have loops of their own
        void transformExpr(const ExprPtr& e, FunctionScope& ctx)
        {
            if (not e)
                return;

            const AstKind type = e->kind();

            if (type == AstKind::AnonymousFunction)
            {
                auto f = std::static_pointer_cast<AnonymousFunction>(e);
                transformFunction(f->parameters, f->body, ctx, 0);
            }
            else if (type == AstKind::CallExpression)
            {
                auto c = std::static_pointer_cast<CallExpression>(e);
                transformExpr(c->caller, ctx);

                for (const auto& a : listOf(c->args))
                    transformExpr(a, ctx);
            }
            else if (type == AstKind::BinaryExpression)
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                transformExpr(b->leftExpr, ctx);
                transformExpr(b->rightExpr, ctx);
            }
            else if (type == AstKind::LogicExpression)
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);
                transformExpr(l->leftExpr, ctx);
                transformExpr(l->rightExpr, ctx);
            }
            else if (type == AstKind::Assign)
                transformExpr(std::static_pointer_cast<Assign>(e)->expr, ctx);
            else if (type == AstKind::Set)
                transformExpr(std::static_pointer_cast<Set>(e)->value, ctx);
            else if (type == AstKind::IndexSet)
                transformExpr(std::static_pointer_cast<IndexSet>(e)->value, ctx);
            else if (type == AstKind::CompoundAtom)
                transformExpr(std::static_pointer_cast<CompoundAtom>(e)->expr, ctx);
            else if (type == AstKind::List)
            {
                for (const auto& entry : listOf(std::static_pointer_cast<List>(e)->entries))
                    transformExpr(entry.value, ctx);
            }
        }

        StatementPtr transform(const StatementPtr& s, FunctionScope& ctx)
        {
            if (not s)
                return s;

            const AstKind type = s->kind();

            if (type == AstKind::BlockStatement)
            {
                auto b = std::static_pointer_cast<BlockStatement>(s);
                std::vector<StatementPtr> out;

                for (const auto& st : listOf(b->statements))
                    out.push_back(transform(st, ctx));

                b->statements = queueOf(out);
            }
            else if (type == AstKind::IfStatement)
            {
                auto i = std::static_pointer_cast<IfStatement>(s);
                transformExpr(i->condition, ctx);
                i->thenBranch = transform(i->thenBranch, ctx);
                i->elseBranch = transform(i->elseBranch, ctx);
            }
            else if (type == AstKind::FunctionStatement)
            {
                auto f = std::static_pointer_cast<FunctionStatement>(s);
                transformFunction(f->parameters, f->body, ctx, 0);
            }
            else if (type == AstKind::ClassStatement)
            {
                // (+1 for 'this')
                for (const auto& m : listOf(std::static_pointer_cast<ClassStatement>(s)->methods))
                    transformFunction(m->parameters, m->body, ctx, 1);
            }
            else if (type == AstKind::WhileStatement)
            {
                auto w = std::static_pointer_cast<WhileStatement>(s);
                transformExpr(w->condition, ctx);
                w->body = transform(w->body, ctx);

                return hoistLoop(s, ctx);
            }
            else if (type == AstKind::ForStatement)
            {
                auto f = std::static_pointer_cast<ForStatement>(s);
                f->initializer = transform(f->initializer, ctx);
                transformExpr(f->condition, ctx);
                transformExpr(f->increment, ctx);
                f->body = transform(f->body, ctx);

                return hoistLoop(s, ctx);
            }
            else if (type == AstKind::ForInStatement)
            {
                auto f = std::static_pointer_cast<ForInStatement>(s);
                transformExpr(f->iterable, ctx);
                f->body = transform(f->body, ctx);
            }
            else if (type == AstKind::ExpressionStatement)
                transformExpr(std::static_pointer_cast<ExpressionStatement>(s)->expr, ctx);
            else if (type == AstKind::VariableStatement)
                transformExpr(std::static_pointer_cast<VariableStatement>(s)->expr, ctx);
            else if (type == AstKind::ReturnStatement)
                transformExpr(std::static_pointer_cast<ReturnStatement>(s)->value, ctx);

            return s;
        }
    }

    bool LengthHoistingPass::runPass(VM* vm, std::queue<StatementPtr>& statements)
    {
        // The whole program: the script compiled (outermost) and every module it imports. A module
        // compiled while it is (deeper) belongs to that program
        if (not vm or vm->astCompileDepth <= 1)
        {
            const ast::ProgramView program = ast::readProgram(vm, statements);

            scriptNames = program.scriptNames();
            programComplete = program.complete;
        }

        // A module read as bytecode only could define `len` or `push`
        if (not programComplete)
            return false;

        FunctionScope ctx{scriptNames};

        // The script's own nested blocks declare locals too: their lists are the script's as a
        // function's are, a global's are not (a function could reach it)
        for (const auto& s : listOf(statements))
        {
            if (s and s->kind() != AstKind::FunctionStatement and s->kind() != AstKind::ClassStatement and s->kind() != AstKind::VariableStatement)
                declsOf(s, ctx.locals);

            ctx.slotsUsed += ast::countLocalSlots(s);
        }

        ctx.ownership = Ownership::ofScript(statements, ctx.locals);
        ctx.scriptRead = true;

        std::vector<StatementPtr> out;

        for (const auto& s : listOf(statements))
            out.push_back(transform(s, ctx));

        statements = queueOf(out);

        LOG_INFO(DOM, "Hoisted " << ctx.hoisted << " len() reads");

        hoisted = ctx.hoisted;

        return hoisted > 0;
    }

    std::string LengthHoistingPass::factsAbout(const std::set<std::string>& names, std::vector<std::shared_ptr<const void>>&) const
    {
        return listNativeFacts(names, scriptNames, programComplete);
    }
}
