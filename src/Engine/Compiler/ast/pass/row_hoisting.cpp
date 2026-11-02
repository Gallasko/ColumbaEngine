#include "stdafx.h"

#include "row_hoisting.h"

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

        const char* DOM = "RowHoisting";

        // The `t[k]` of a name and a name or a number; "" for anything else
        std::string rowKey(const ExprPtr& e)
        {
            if (not e or e->kind() != AstKind::IndexGet)
                return "";

            return std::static_pointer_cast<IndexGet>(e)->object->kind() == AstKind::Var ? pathOf(e) : "";
        }

        // The `x` of `len(x)`, the real one or not
        ExprPtr measuredBy(const ExprPtr& e)
        {
            if (not e or e->kind() != AstKind::CallExpression)
                return nullptr;

            auto c = std::static_pointer_cast<CallExpression>(e);

            return varName(c->caller) == "len" and c->args.size() == 1 ? c->args.front() : nullptr;
        }

        struct Rows : FunctionScope
        {
            // Where the walk stands: the locals a `len` that has run measured, and the rows the hidden
            // declarations right here read
            std::set<std::string> measured;
            std::set<std::string> justRead;

            // Holds the same value from its declaration on
            bool neverChanges(const std::string& name) const
            {
                auto it = ownership.declared.find(name);

                return it != ownership.declared.end() and it->second == 1 and locals.count(name) and not ownership.rebound.count(name);
            }

            // Reading it cannot run code or stop the script
            bool safe(const ExprPtr& read) const
            {
                if (justRead.count(pathOf(read)))
                    return true;

                auto g = std::static_pointer_cast<IndexGet>(read);
                const std::string list = varName(g->object);

                if (not ownership.isInt(g->index))
                    return false;

                return measured.count(list) or (ownershipHolds() and ownership.owned(list));
            }
        };

        // What running an expression shows, by what surely runs of it
        void learn(const ExprPtr& e, Rows& ctx)
        {
            if (not e)
                return;

            if (auto x = measuredBy(e))
            {
                if (x->kind() == AstKind::Var and ctx.isNative("len") and ctx.neverChanges(varName(x)))
                    ctx.measured.insert(varName(x));
            }

            const AstKind type = e->kind();

            if (type == AstKind::CallExpression)
            {
                for (const auto& a : listOf(std::static_pointer_cast<CallExpression>(e)->args))
                    learn(a, ctx);
            }
            else if (type == AstKind::BinaryExpression)
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                learn(b->leftExpr, ctx);
                learn(b->rightExpr, ctx);
            }
            else if (type == AstKind::LogicExpression)
                // The right side may not be evaluated
                learn(std::static_pointer_cast<LogicExpression>(e)->leftExpr, ctx);
            else if (type == AstKind::UnaryExpression)
                learn(std::static_pointer_cast<UnaryExpression>(e)->expr, ctx);
            else if (type == AstKind::CompoundAtom)
                learn(std::static_pointer_cast<CompoundAtom>(e)->expr, ctx);
            else if (type == AstKind::Assign)
                learn(std::static_pointer_cast<Assign>(e)->expr, ctx);
        }

        void learnStatement(const StatementPtr& s, Rows& ctx)
        {
            if (not s)
                return;

            const AstKind type = s->kind();

            if (type == AstKind::VariableStatement)
            {
                auto v = std::static_pointer_cast<VariableStatement>(s);

                learn(v->expr, ctx);

                // '@' cannot start a source name: this is one of the passes' own
                if (v->name.text[0] != '@')
                    ctx.justRead.clear();
                else if (auto x = measuredBy(v->expr))
                    ctx.justRead.insert(pathOf(x));

                return;
            }

            ctx.justRead.clear();

            if (type == AstKind::ExpressionStatement)
                learn(std::static_pointer_cast<ExpressionStatement>(s)->expr, ctx);
        }

        // `t[a][k] += 1`: what is written is what is read, and the path to it is the same names read twice.
        // Read once, in a block of its own
        StatementPtr readOnce(const StatementPtr& s, Rows& ctx)
        {
            auto statement = std::static_pointer_cast<ExpressionStatement>(s);

            if (not statement->expr or statement->expr->kind() != AstKind::IndexSet)
                return s;

            auto write = std::static_pointer_cast<IndexSet>(statement->expr);

            if (not write->value or write->value->kind() != AstKind::BinaryExpression or write->object->kind() != AstKind::IndexGet)
                return s;

            auto left = std::static_pointer_cast<BinaryExpression>(write->value)->leftExpr;

            if (not left or left->kind() != AstKind::IndexGet)
                return s;

            auto read = std::static_pointer_cast<IndexGet>(left);
            const std::string path = pathOf(write->object);

            const std::string index = indexOf(write->index);

            if (path.empty() or index.empty() or pathOf(read->object) != path or indexOf(read->index) != index)
                return s;

            if (ctx.slotsUsed + 1 > kMaxLocalSlots)
                return s;

            ++ctx.slotsUsed;

            const Token name(TokenType::EXPRESSION, "@row" + std::to_string(ctx.counter++), 0, 0);

            std::queue<StatementPtr> block;
            block.push(std::make_shared<VariableStatement>(name, write->object));

            write->object = std::make_shared<Var>(name);
            read->object = write->object;

            block.push(s);

            ++ctx.hoisted;

            return std::make_shared<BlockStatement>(block);
        }

        // The reads of a loop that hold for all of it, a hidden local each, before it
        StatementPtr hoistLoop(const StatementPtr& s, Rows& ctx)
        {
            if (not ctx.inFunction)
                return s;

            std::map<std::string, ExprPtr> rows;

            eachExpr(s, [&](const ExprPtr& e) {
                const std::string key = rowKey(e);

                if (not key.empty())
                    rows.emplace(key, e);
            });

            if (rows.empty())
                return s;

            LoopFacts facts;
            analyzeStmt(s, facts);

            std::map<std::string, std::string> hidden;
            std::queue<StatementPtr> block;

            for (const auto& [path, read] : rows)
            {
                std::set<std::string> names;
                namesOf(read, names);

                if (std::any_of(names.begin(), names.end(), [&](const std::string& name) { return facts.mutated.count(name); }))
                    continue;

                if (not ctx.safe(read))
                    continue;

                const std::string root = rootOf(read);

                RegionCheck check{ctx, root, ctx.ownershipHolds() and ctx.ownership.owned(root), true};
                check.stmt(s);

                if (not check.ok)
                    continue;

                if (ctx.slotsUsed + 1 > kMaxLocalSlots)
                    break;

                ++ctx.slotsUsed;

                const std::string name = "@row" + std::to_string(ctx.counter++);

                block.push(std::make_shared<VariableStatement>(Token(TokenType::EXPRESSION, name, 0, 0), read));
                hidden[path] = name;

                // What it reads stands where the read stood: what the loops inside know of it, they
                // know of the row
                ctx.locals.insert(name);
                ctx.ownership.rowOf[name].insert(root);
                ctx.ownership.declared[name] = 1;
            }

            if (hidden.empty())
                return s;

            Replace replace{hidden, rowKey};
            replace.stmt(s);

            ctx.hoisted += hidden.size();
            block.push(s);

            return std::make_shared<BlockStatement>(block);
        }

        StatementPtr transform(const StatementPtr& s, Rows& ctx);

        void transformFunction(const std::queue<ExprPtr>& parameters, StatementPtr& body, Rows& ctx, int extraSlots)
        {
            const std::set<std::string> savedLocals = ctx.locals;
            const std::set<std::string> savedMeasured = ctx.measured;
            const std::set<std::string> savedRead = ctx.justRead;
            Ownership savedOwnership = std::move(ctx.ownership);
            const bool savedIn = ctx.inFunction;
            const int savedSlots = ctx.slotsUsed;

            ctx.locals.clear();

            for (const auto& p : listOf(parameters))
                ctx.locals.insert(p->getName());

            declsOf(body, ctx.locals);

            ctx.measured.clear();
            ctx.justRead.clear();
            ctx.ownership = Ownership::ofFunction(parameters, body);
            ctx.inFunction = true;
            ctx.slotsUsed = extraSlots + static_cast<int>(parameters.size()) + countLocalSlots(body);

            body = transform(body, ctx);

            ctx.locals = savedLocals;
            ctx.measured = savedMeasured;
            ctx.justRead = savedRead;
            ctx.ownership = std::move(savedOwnership);
            ctx.inFunction = savedIn;
            ctx.slotsUsed = savedSlots;
        }

        // Functions defined inside expressions have loops of their own
        template <typename Where>
        void transformFunctionsIn(const Where& where, Rows& ctx)
        {
            eachExpr(where, [&](const ExprPtr& e) {
                if (e->kind() == AstKind::AnonymousFunction)
                {
                    auto f = std::static_pointer_cast<AnonymousFunction>(e);
                    transformFunction(f->parameters, f->body, ctx, 0);
                }
            });
        }

        StatementPtr transformLoop(const StatementPtr& s, Rows& ctx)
        {
            StatementPtr hoisted = hoistLoop(s, ctx);

            const std::set<std::string> saved = ctx.measured;

            ctx.justRead.clear();

            const AstKind type = s->kind();

            if (type == AstKind::WhileStatement)
            {
                auto w = std::static_pointer_cast<WhileStatement>(s);
                transformFunctionsIn(w->condition, ctx);
                learn(w->condition, ctx);
                w->body = transform(w->body, ctx);
            }
            else if (type == AstKind::ForStatement)
            {
                auto f = std::static_pointer_cast<ForStatement>(s);
                f->initializer = transform(f->initializer, ctx);
                transformFunctionsIn(f->condition, ctx);
                transformFunctionsIn(f->increment, ctx);
                learnStatement(f->initializer, ctx);
                learn(f->condition, ctx);
                ctx.justRead.clear();
                f->body = transform(f->body, ctx);
            }
            else
            {
                auto f = std::static_pointer_cast<ForInStatement>(s);
                transformFunctionsIn(f->iterable, ctx);
                learn(f->iterable, ctx);
                f->body = transform(f->body, ctx);
            }

            ctx.measured = saved;
            ctx.justRead.clear();

            return hoisted;
        }

        StatementPtr transform(const StatementPtr& s, Rows& ctx)
        {
            if (not s)
                return s;

            const AstKind type = s->kind();

            if (type == AstKind::BlockStatement)
            {
                auto b = std::static_pointer_cast<BlockStatement>(s);
                const std::set<std::string> saved = ctx.measured;
                std::vector<StatementPtr> out;

                for (const auto& st : listOf(b->statements))
                {
                    out.push_back(transform(st, ctx));
                    learnStatement(st, ctx);
                }

                b->statements = queueOf(out);
                ctx.measured = saved;
            }
            else if (type == AstKind::IfStatement)
            {
                auto i = std::static_pointer_cast<IfStatement>(s);
                const std::set<std::string> saved = ctx.measured;

                transformFunctionsIn(i->condition, ctx);
                learn(i->condition, ctx);

                ctx.justRead.clear();

                i->thenBranch = transform(i->thenBranch, ctx);

                ctx.measured = saved;
                learn(i->condition, ctx);

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
            else if (type == AstKind::WhileStatement or type == AstKind::ForStatement or type == AstKind::ForInStatement)
                return transformLoop(s, ctx);
            else
            {
                transformFunctionsIn(s, ctx);

                if (type == AstKind::ExpressionStatement)
                    return readOnce(s, ctx);
            }

            return s;
        }
    }

    bool RowHoistingPass::runPass(VM* vm, std::queue<StatementPtr>& statements)
    {
        // The whole program: the script compiled (outermost) and every module it imports. A module
        // compiled while it is (deeper) belongs to that program
        if (not vm or vm->astCompileDepth <= 1)
        {
            const ProgramView program = readProgram(vm, statements);

            scriptNames = program.scriptNames();
            programComplete = program.complete;
        }

        // A module read as bytecode only could define `len` or `push`
        if (not programComplete)
            return false;

        Rows ctx{{scriptNames}};
        std::vector<StatementPtr> out;

        // Hidden locals at top level are the script's
        for (const auto& s : listOf(statements))
            ctx.slotsUsed += countLocalSlots(s);

        for (const auto& s : listOf(statements))
            out.push_back(transform(s, ctx));

        statements = queueOf(out);

        LOG_INFO(DOM, "Hoisted " << ctx.hoisted << " row reads");

        hoisted = ctx.hoisted;

        return hoisted > 0;
    }

    std::string RowHoistingPass::factsAbout(const std::set<std::string>& names, std::vector<std::shared_ptr<const void>>&) const
    {
        return listNativeFacts(names, scriptNames, programComplete);
    }
}
