#include "stdafx.h"

#include "useless_statements.h"

#include "Interpreter/expression.h"
#include "Interpreter/statement.h"

#include <set>
#include <vector>

namespace pg
{
    namespace
    {
        // The locals in scope, innermost last. None at the top level: a `var` there is a global
        struct Scopes
        {
            std::vector<std::set<std::string>> frames;

            bool isLocal(const std::string& name) const
            {
                for (const auto& frame : frames)
                {
                    if (frame.count(name))
                        return true;
                }

                return false;
            }

            void declare(const std::string& name)
            {
                if (not frames.empty())
                    frames.back().insert(name);
            }
        };

        // Surely a bool or none, when it does not stop the script
        bool givesBoolOrNone(const ExprPtr& e)
        {
            switch (e->kind())
            {
                case AstKind::NoneAtom: return true;
                case AstKind::Atom: return std::static_pointer_cast<Atom>(e)->value.isBool();
                case AstKind::CompoundAtom: return givesBoolOrNone(std::static_pointer_cast<CompoundAtom>(e)->expr);

                case AstKind::UnaryExpression: return std::static_pointer_cast<UnaryExpression>(e)->op.type == TokenType::NOT;

                case AstKind::BinaryExpression:
                {
                    const auto type = std::static_pointer_cast<BinaryExpression>(e)->op.type;
                    return type == TokenType::EQUALEQUAL or type == TokenType::NOTEQUAL;
                }

                // `and` / `or` give one of their operands
                case AstKind::LogicExpression:
                {
                    auto l = std::static_pointer_cast<LogicExpression>(e);
                    return givesBoolOrNone(l->leftExpr) and givesBoolOrNone(l->rightExpr);
                }

                default: return false;
            }
        }

        // Evaluated, it can neither run code nor stop the script
        bool inert(const ExprPtr& e, const Scopes& scopes)
        {
            if (not e)
                return false;

            switch (e->kind())
            {
                case AstKind::Atom:
                case AstKind::NoneAtom:
                    return true;

                case AstKind::Var: return scopes.isLocal(std::static_pointer_cast<Var>(e)->name.text);

                case AstKind::CompoundAtom: return inert(std::static_pointer_cast<CompoundAtom>(e)->expr, scopes);

                case AstKind::UnaryExpression:
                {
                    auto u = std::static_pointer_cast<UnaryExpression>(e);
                    return u->op.type == TokenType::NOT and inert(u->expr, scopes) and givesBoolOrNone(u->expr);
                }

                case AstKind::BinaryExpression:
                {
                    auto b = std::static_pointer_cast<BinaryExpression>(e);
                    const bool compares = b->op.type == TokenType::EQUALEQUAL or b->op.type == TokenType::NOTEQUAL;
                    return compares and inert(b->leftExpr, scopes) and inert(b->rightExpr, scopes);
                }

                case AstKind::LogicExpression:
                {
                    auto l = std::static_pointer_cast<LogicExpression>(e);
                    return inert(l->leftExpr, scopes) and inert(l->rightExpr, scopes);
                }

                default: return false;
            }
        }

        struct Remover
        {
            Scopes scopes;
            bool changed = false;

            bool useless(const StatementPtr& s) const
            {
                return s and s->kind() == AstKind::ExpressionStatement and inert(std::static_pointer_cast<ExpressionStatement>(s)->expr, scopes);
            }

            void list(std::queue<StatementPtr>& statements)
            {
                std::queue<StatementPtr> kept;

                for (; not statements.empty(); statements.pop())
                {
                    auto s = statements.front();

                    if (useless(s))
                    {
                        changed = true;
                        continue;
                    }

                    statement(s);
                    kept.push(s);
                }

                statements = std::move(kept);
            }

            // The body of an `if` or a loop: one statement, which may be the useless one
            StatementPtr body(const StatementPtr& s)
            {
                if (useless(s))
                {
                    changed = true;
                    return std::make_shared<BlockStatement>(std::queue<StatementPtr>{});
                }

                scopes.frames.emplace_back();
                statement(s);
                scopes.frames.pop_back();
                return s;
            }

            void function(const std::queue<ExprPtr>& parameters, const StatementPtr& functionBody)
            {
                scopes.frames.emplace_back();

                for (auto p = parameters; not p.empty(); p.pop())
                {
                    if (p.front() and p.front()->kind() == AstKind::Var)
                        scopes.declare(std::static_pointer_cast<Var>(p.front())->name.text);
                }

                statement(functionBody);
                scopes.frames.pop_back();
            }

            void statement(const StatementPtr& s)
            {
                if (not s)
                    return;

                switch (s->kind())
                {
                    case AstKind::VariableStatement:
                        scopes.declare(std::static_pointer_cast<VariableStatement>(s)->name.text);
                        break;

                    case AstKind::FunctionStatement:
                    {
                        auto f = std::static_pointer_cast<FunctionStatement>(s);
                        scopes.declare(f->name.text);
                        function(f->parameters, f->body);
                        break;
                    }

                    case AstKind::ClassStatement:
                    {
                        auto c = std::static_pointer_cast<ClassStatement>(s);
                        scopes.declare(c->name.text);

                        for (auto methods = c->methods; not methods.empty(); methods.pop())
                            function(methods.front()->parameters, methods.front()->body);

                        break;
                    }

                    case AstKind::BlockStatement:
                    {
                        scopes.frames.emplace_back();
                        list(std::static_pointer_cast<BlockStatement>(s)->statements);
                        scopes.frames.pop_back();
                        break;
                    }

                    case AstKind::IfStatement:
                    {
                        auto i = std::static_pointer_cast<IfStatement>(s);
                        i->thenBranch = body(i->thenBranch);

                        if (i->elseBranch)
                            i->elseBranch = body(i->elseBranch);

                        break;
                    }

                    case AstKind::WhileStatement:
                    {
                        auto w = std::static_pointer_cast<WhileStatement>(s);
                        w->body = body(w->body);
                        break;
                    }

                    case AstKind::ForStatement:
                    {
                        auto f = std::static_pointer_cast<ForStatement>(s);
                        scopes.frames.emplace_back();
                        statement(f->initializer);
                        f->body = body(f->body);
                        scopes.frames.pop_back();
                        break;
                    }

                    case AstKind::ForInStatement:
                    {
                        auto f = std::static_pointer_cast<ForInStatement>(s);
                        scopes.frames.emplace_back();
                        scopes.declare(f->varName.text);
                        statement(f->initializer);
                        f->body = body(f->body);
                        scopes.frames.pop_back();
                        break;
                    }

                    default:
                        break;
                }
            }
        };
    }

    bool UselessStatementsPass::runPass(VM*, std::queue<StatementPtr>& statements)
    {
        Remover remover;
        remover.list(statements);
        return remover.changed;
    }
}
