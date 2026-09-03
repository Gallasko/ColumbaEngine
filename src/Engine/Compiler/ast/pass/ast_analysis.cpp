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

            const auto& type = e->getType();

            if (type == "BinaryExpression")
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);

                analyzeExpr(b->leftExpr, f);
                analyzeExpr(b->rightExpr, f);
            }
            else if (type == "LogicExpression")
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);

                analyzeExpr(l->leftExpr, f);
                analyzeExpr(l->rightExpr, f);
            }
            else if (type == "UnaryExpression")
            {
                analyzeExpr(std::static_pointer_cast<UnaryExpression>(e)->expr, f);
            }
            else if (type == "CompoundAtom")
            {
                analyzeExpr(std::static_pointer_cast<CompoundAtom>(e)->expr, f);
            }
            else if (type == "PreFixExpression")
            {
                f.mutated.insert(std::static_pointer_cast<PreFixExpression>(e)->name.text);
            }
            else if (type == "PostFixExpression")
            {
                f.mutated.insert(std::static_pointer_cast<PostFixExpression>(e)->name.text);
            }
            else if (type == "Assign")
            {
                auto a = std::static_pointer_cast<Assign>(e);

                f.mutated.insert(a->name.text);

                analyzeExpr(a->expr, f);
            }
            else if (type == "CallExpression")
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
            else if (type == "Get")
            {
                analyzeExpr(std::static_pointer_cast<Get>(e)->object, f);
            }
            else if (type == "Set")
            {
                auto s = std::static_pointer_cast<Set>(e);

                analyzeExpr(s->object, f);
                analyzeExpr(s->value, f);
            }
            else if (type == "IndexGet")
            {
                auto i = std::static_pointer_cast<IndexGet>(e);

                analyzeExpr(i->object, f);
                analyzeExpr(i->index, f);
            }
            else if (type == "IndexSet")
            {
                auto i = std::static_pointer_cast<IndexSet>(e);

                analyzeExpr(i->object, f);
                analyzeExpr(i->index, f);
                analyzeExpr(i->value, f);
            }
            else if (type == "List")
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

            const auto& type = s->getType();

            if (type == "ExpressionStatement")
            {
                analyzeExpr(std::static_pointer_cast<ExpressionStatement>(s)->expr, f);
            }
            else if (type == "VariableStatement")
            {
                auto v = std::static_pointer_cast<VariableStatement>(s);

                f.mutated.insert(v->name.text);

                analyzeExpr(v->expr, f);
            }
            else if (type == "FunctionStatement")
            {
                // Declaration shadows the name; body only runs when called
                f.mutated.insert(std::static_pointer_cast<FunctionStatement>(s)->name.text);
            }
            else if (type == "ClassStatement")
            {
                f.mutated.insert(std::static_pointer_cast<ClassStatement>(s)->name.text);
            }
            else if (type == "BlockStatement")
            {
                auto statements = std::static_pointer_cast<BlockStatement>(s)->statements;

                while (not statements.empty())
                {
                    analyzeStmt(statements.front(), f);

                    statements.pop();
                }
            }
            else if (type == "IfStatement")
            {
                auto i = std::static_pointer_cast<IfStatement>(s);

                analyzeExpr(i->condition, f);
                analyzeStmt(i->thenBranch, f);
                analyzeStmt(i->elseBranch, f);
            }
            else if (type == "WhileStatement")
            {
                auto w = std::static_pointer_cast<WhileStatement>(s);

                analyzeExpr(w->condition, f);
                analyzeStmt(w->body, f);
            }
            else if (type == "ForStatement")
            {
                auto fs = std::static_pointer_cast<ForStatement>(s);

                analyzeStmt(fs->initializer, f);
                analyzeExpr(fs->condition, f);
                analyzeExpr(fs->increment, f);
                analyzeStmt(fs->body, f);
            }
            else if (type == "ForInStatement")
            {
                auto fi = std::static_pointer_cast<ForInStatement>(s);

                f.mutated.insert(fi->varName.text);

                analyzeExpr(fi->iterable, f);
                analyzeStmt(fi->body, f);
            }
            else if (type == "ReturnStatement")
            {
                analyzeExpr(std::static_pointer_cast<ReturnStatement>(s)->value, f);
            }
            else if (type == "DPrintStatement")
            {
                analyzeExpr(std::static_pointer_cast<DPrintStatement>(s)->expr, f);
            }
            // Break / Continue / Import: nothing to record
        }
    }
}
