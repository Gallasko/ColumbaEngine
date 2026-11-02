#include "stdafx.h"

#include "ast_analysis.h"

namespace pg
{
    namespace ast
    {
        void analyzeExpr(const ExprPtr& e, LoopFacts& f)
        {
            if (not e)
                return;

            const AstKind type = e->kind();

            if (type == AstKind::BinaryExpression)
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);

                analyzeExpr(b->leftExpr, f);
                analyzeExpr(b->rightExpr, f);
            }
            else if (type == AstKind::LogicExpression)
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);

                analyzeExpr(l->leftExpr, f);
                analyzeExpr(l->rightExpr, f);
            }
            else if (type == AstKind::UnaryExpression)
            {
                analyzeExpr(std::static_pointer_cast<UnaryExpression>(e)->expr, f);
            }
            else if (type == AstKind::CompoundAtom)
            {
                analyzeExpr(std::static_pointer_cast<CompoundAtom>(e)->expr, f);
            }
            else if (type == AstKind::PreFixExpression)
            {
                f.mutated.insert(std::static_pointer_cast<PreFixExpression>(e)->name.text);
            }
            else if (type == AstKind::PostFixExpression)
            {
                f.mutated.insert(std::static_pointer_cast<PostFixExpression>(e)->name.text);
            }
            else if (type == AstKind::Assign)
            {
                auto a = std::static_pointer_cast<Assign>(e);

                f.mutated.insert(a->name.text);

                analyzeExpr(a->expr, f);
            }
            else if (type == AstKind::CallExpression)
            {
                auto c = std::static_pointer_cast<CallExpression>(e);

                f.hasCalls = true;

                analyzeExpr(c->caller, f);

                auto args = c->args;

                while (not args.empty())
                {
                    analyzeExpr(args.front(), f);
                    args.pop();
                }
            }
            else if (type == AstKind::Get)
            {
                analyzeExpr(std::static_pointer_cast<Get>(e)->object, f);
            }
            else if (type == AstKind::Set)
            {
                auto s = std::static_pointer_cast<Set>(e);

                analyzeExpr(s->object, f);
                analyzeExpr(s->value, f);
            }
            else if (type == AstKind::IndexGet)
            {
                auto i = std::static_pointer_cast<IndexGet>(e);

                analyzeExpr(i->object, f);
                analyzeExpr(i->index, f);
            }
            else if (type == AstKind::IndexSet)
            {
                auto i = std::static_pointer_cast<IndexSet>(e);

                analyzeExpr(i->object, f);
                analyzeExpr(i->index, f);
                analyzeExpr(i->value, f);
            }
            else if (type == AstKind::List)
            {
                auto entries = std::static_pointer_cast<List>(e)->entries;

                while (not entries.empty())
                {
                    analyzeExpr(entries.front().key, f);
                    analyzeExpr(entries.front().value, f);

                    entries.pop();
                }
            }
            // AnonymousFunction bodies are skipped: creating the closure runs
            // no body code; calling it sets hasCalls at the call site.
            // Atom / Var / This: nothing to record.
        }

        void analyzeStmt(const StatementPtr& s, LoopFacts& f)
        {
            if (not s)
                return;

            const AstKind type = s->kind();

            if (type == AstKind::ExpressionStatement)
            {
                analyzeExpr(std::static_pointer_cast<ExpressionStatement>(s)->expr, f);
            }
            else if (type == AstKind::VariableStatement)
            {
                auto v = std::static_pointer_cast<VariableStatement>(s);

                f.mutated.insert(v->name.text);

                analyzeExpr(v->expr, f);
            }
            else if (type == AstKind::FunctionStatement)
            {
                // Declaration shadows the name; body only runs when called
                f.mutated.insert(std::static_pointer_cast<FunctionStatement>(s)->name.text);
            }
            else if (type == AstKind::ClassStatement)
            {
                f.mutated.insert(std::static_pointer_cast<ClassStatement>(s)->name.text);
            }
            else if (type == AstKind::BlockStatement)
            {
                auto statements = std::static_pointer_cast<BlockStatement>(s)->statements;

                while (not statements.empty())
                {
                    analyzeStmt(statements.front(), f);

                    statements.pop();
                }
            }
            else if (type == AstKind::IfStatement)
            {
                auto i = std::static_pointer_cast<IfStatement>(s);

                analyzeExpr(i->condition, f);
                analyzeStmt(i->thenBranch, f);
                analyzeStmt(i->elseBranch, f);
            }
            else if (type == AstKind::WhileStatement)
            {
                auto w = std::static_pointer_cast<WhileStatement>(s);

                analyzeExpr(w->condition, f);
                analyzeStmt(w->body, f);
            }
            else if (type == AstKind::ForStatement)
            {
                auto fs = std::static_pointer_cast<ForStatement>(s);

                analyzeStmt(fs->initializer, f);
                analyzeExpr(fs->condition, f);
                analyzeExpr(fs->increment, f);
                analyzeStmt(fs->body, f);
            }
            else if (type == AstKind::ForInStatement)
            {
                auto fi = std::static_pointer_cast<ForInStatement>(s);

                f.mutated.insert(fi->varName.text);

                analyzeExpr(fi->iterable, f);
                analyzeStmt(fi->body, f);
            }
            else if (type == AstKind::ReturnStatement)
            {
                analyzeExpr(std::static_pointer_cast<ReturnStatement>(s)->value, f);
            }
            else if (type == AstKind::DPrintStatement)
            {
                analyzeExpr(std::static_pointer_cast<DPrintStatement>(s)->expr, f);
            }
            // Break / Continue / Import: nothing to record
        }

        void eachExpr(const ExprPtr& e, const ExprVisit& visit, bool intoFunctions)
        {
            if (not e)
                return;

            visit(e);

            const AstKind type = e->kind();

            if (type == AstKind::CallExpression)
            {
                auto c = std::static_pointer_cast<CallExpression>(e);
                eachExpr(c->caller, visit, intoFunctions);

                auto args = c->args;

                while (not args.empty())
                {
                    eachExpr(args.front(), visit, intoFunctions);
                    args.pop();
                }
            }
            else if (type == AstKind::IndexSet)
            {
                auto g = std::static_pointer_cast<IndexSet>(e);
                eachExpr(g->object, visit, intoFunctions);
                eachExpr(g->index, visit, intoFunctions);
                eachExpr(g->value, visit, intoFunctions);
            }
            else if (type == AstKind::IndexGet)
            {
                auto g = std::static_pointer_cast<IndexGet>(e);
                eachExpr(g->object, visit, intoFunctions);
                eachExpr(g->index, visit, intoFunctions);
            }
            else if (type == AstKind::Get)
                eachExpr(std::static_pointer_cast<Get>(e)->object, visit, intoFunctions);
            else if (type == AstKind::Set)
            {
                auto g = std::static_pointer_cast<Set>(e);
                eachExpr(g->object, visit, intoFunctions);
                eachExpr(g->value, visit, intoFunctions);
            }
            else if (type == AstKind::BinaryExpression)
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                eachExpr(b->leftExpr, visit, intoFunctions);
                eachExpr(b->rightExpr, visit, intoFunctions);
            }
            else if (type == AstKind::LogicExpression)
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);
                eachExpr(l->leftExpr, visit, intoFunctions);
                eachExpr(l->rightExpr, visit, intoFunctions);
            }
            else if (type == AstKind::UnaryExpression)
                eachExpr(std::static_pointer_cast<UnaryExpression>(e)->expr, visit, intoFunctions);
            else if (type == AstKind::CompoundAtom)
                eachExpr(std::static_pointer_cast<CompoundAtom>(e)->expr, visit, intoFunctions);
            else if (type == AstKind::Assign)
                eachExpr(std::static_pointer_cast<Assign>(e)->expr, visit, intoFunctions);
            else if (type == AstKind::List)
            {
                auto entries = std::static_pointer_cast<List>(e)->entries;

                while (not entries.empty())
                {
                    if (not entries.front().explicitKey)
                        eachExpr(entries.front().key, visit, intoFunctions);

                    eachExpr(entries.front().value, visit, intoFunctions);
                    entries.pop();
                }
            }
            else if (type == AstKind::AnonymousFunction and intoFunctions)
                eachExpr(std::static_pointer_cast<AnonymousFunction>(e)->body, visit, intoFunctions);
        }

        void eachExpr(const StatementPtr& s, const ExprVisit& visit, bool intoFunctions)
        {
            if (not s)
                return;

            const AstKind type = s->kind();

            if (type == AstKind::ExpressionStatement)
                eachExpr(std::static_pointer_cast<ExpressionStatement>(s)->expr, visit, intoFunctions);
            else if (type == AstKind::VariableStatement)
                eachExpr(std::static_pointer_cast<VariableStatement>(s)->expr, visit, intoFunctions);
            else if (type == AstKind::ReturnStatement)
                eachExpr(std::static_pointer_cast<ReturnStatement>(s)->value, visit, intoFunctions);
            else if (type == AstKind::DPrintStatement)
                eachExpr(std::static_pointer_cast<DPrintStatement>(s)->expr, visit, intoFunctions);
            else if (type == AstKind::BlockStatement)
            {
                auto statements = std::static_pointer_cast<BlockStatement>(s)->statements;

                while (not statements.empty())
                {
                    eachExpr(statements.front(), visit, intoFunctions);
                    statements.pop();
                }
            }
            else if (type == AstKind::IfStatement)
            {
                auto i = std::static_pointer_cast<IfStatement>(s);
                eachExpr(i->condition, visit, intoFunctions);
                eachExpr(i->thenBranch, visit, intoFunctions);
                eachExpr(i->elseBranch, visit, intoFunctions);
            }
            else if (type == AstKind::WhileStatement)
            {
                auto w = std::static_pointer_cast<WhileStatement>(s);
                eachExpr(w->condition, visit, intoFunctions);
                eachExpr(w->body, visit, intoFunctions);
            }
            else if (type == AstKind::ForStatement)
            {
                auto f = std::static_pointer_cast<ForStatement>(s);
                eachExpr(f->initializer, visit, intoFunctions);
                eachExpr(f->condition, visit, intoFunctions);
                eachExpr(f->increment, visit, intoFunctions);
                eachExpr(f->body, visit, intoFunctions);
            }
            else if (type == AstKind::ForInStatement)
            {
                auto f = std::static_pointer_cast<ForInStatement>(s);
                eachExpr(f->iterable, visit, intoFunctions);
                eachExpr(f->body, visit, intoFunctions);
            }
            else if (type == AstKind::FunctionStatement and intoFunctions)
                eachExpr(std::static_pointer_cast<FunctionStatement>(s)->body, visit, intoFunctions);
            else if (type == AstKind::ClassStatement and intoFunctions)
            {
                auto methods = std::static_pointer_cast<ClassStatement>(s)->methods;

                while (not methods.empty())
                {
                    eachExpr(methods.front()->body, visit, intoFunctions);
                    methods.pop();
                }
            }
        }

        int countLocalSlots(const StatementPtr& s)
        {
            if (not s)
                return 0;

            const AstKind type = s->kind();

            if (type == AstKind::VariableStatement)
                return 1;

            if (type == AstKind::BlockStatement)
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

            if (type == AstKind::IfStatement)
            {
                auto i = std::static_pointer_cast<IfStatement>(s);
                return countLocalSlots(i->thenBranch) + countLocalSlots(i->elseBranch);
            }

            if (type == AstKind::WhileStatement)
                return countLocalSlots(std::static_pointer_cast<WhileStatement>(s)->body);

            if (type == AstKind::ForStatement)
            {
                auto fs = std::static_pointer_cast<ForStatement>(s);
                return countLocalSlots(fs->initializer) + countLocalSlots(fs->body);
            }

            if (type == AstKind::ForInStatement)
            {
                // __table, __size, __i + the key variable
                return 4 + countLocalSlots(std::static_pointer_cast<ForInStatement>(s)->body);
            }

            // Local function/class declarations occupy one slot themselves
            if (type == AstKind::FunctionStatement or type == AstKind::ClassStatement)
                return 1;

            return 0;
        }
    }
}
