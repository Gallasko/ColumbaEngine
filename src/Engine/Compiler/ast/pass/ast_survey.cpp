#include "stdafx.h"

#include "ast_survey.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <vector>

#include "ast_analysis.h"
#include "Compiler/vm.h"

namespace pg
{
    namespace
    {
        template <typename T>
        std::vector<T> listOf(std::queue<T> q)
        {
            std::vector<T> out;

            while (not q.empty())
            {
                out.push_back(q.front());
                q.pop();
            }

            return out;
        }

        std::string varName(const ExprPtr& e)
        {
            if (e and e->kind() == AstKind::Var)
                return std::static_pointer_cast<Var>(e)->name.text;

            return "";
        }

        // The line an expression starts on, as near as its tokens say (0 when none does)
        int lineOf(const ExprPtr& e)
        {
            if (not e)
                return 0;

            const AstKind type = e->kind();

            if (type == AstKind::BinaryExpression)
                return std::static_pointer_cast<BinaryExpression>(e)->op.line;
            if (type == AstKind::LogicExpression)
                return std::static_pointer_cast<LogicExpression>(e)->op.line;
            if (type == AstKind::CallExpression)
                return std::static_pointer_cast<CallExpression>(e)->paren.line;
            if (type == AstKind::Var)
                return std::static_pointer_cast<Var>(e)->name.line;
            if (type == AstKind::Get)
                return std::static_pointer_cast<Get>(e)->name.line;
            if (type == AstKind::UnaryExpression)
                return std::static_pointer_cast<UnaryExpression>(e)->op.line;
            if (type == AstKind::CompoundAtom)
                return lineOf(std::static_pointer_cast<CompoundAtom>(e)->expr);

            return 0;
        }

        struct Survey
        {
            // name -> "(params) -> body" of a function whose body is one return
            std::map<std::string, std::string> wrappers;

            // Call sites by the name called: everywhere, and inside a loop
            std::map<std::string, long> calls;
            std::map<std::string, long> loopCalls;

            long loops = 0;
            long loopsWithCalls = 0;

            std::vector<std::string> lenHoists;
            std::vector<std::string> repeatedGets;

            std::string function = "<top level>";
            std::set<std::string> locals;
            int loopDepth = 0;
        };

        void walkStmt(Survey& s, const StatementPtr& st);

        void walkExpr(Survey& s, const ExprPtr& e)
        {
            if (not e)
                return;

            const AstKind type = e->kind();

            if (type == AstKind::BinaryExpression)
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                walkExpr(s, b->leftExpr);
                walkExpr(s, b->rightExpr);
            }
            else if (type == AstKind::LogicExpression)
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);
                walkExpr(s, l->leftExpr);
                walkExpr(s, l->rightExpr);
            }
            else if (type == AstKind::UnaryExpression)
                walkExpr(s, std::static_pointer_cast<UnaryExpression>(e)->expr);
            else if (type == AstKind::CompoundAtom)
                walkExpr(s, std::static_pointer_cast<CompoundAtom>(e)->expr);
            else if (type == AstKind::Assign)
                walkExpr(s, std::static_pointer_cast<Assign>(e)->expr);
            else if (type == AstKind::CallExpression)
            {
                auto c = std::static_pointer_cast<CallExpression>(e);
                const std::string name = varName(c->caller);

                if (not name.empty())
                {
                    ++s.calls[name];

                    if (s.loopDepth > 0)
                        ++s.loopCalls[name];
                }

                walkExpr(s, c->caller);

                for (const auto& a : listOf(c->args))
                    walkExpr(s, a);
            }
            else if (type == AstKind::Get)
                walkExpr(s, std::static_pointer_cast<Get>(e)->object);
            else if (type == AstKind::Set)
            {
                auto st = std::static_pointer_cast<Set>(e);
                walkExpr(s, st->object);
                walkExpr(s, st->value);
            }
            else if (type == AstKind::IndexGet)
            {
                auto g = std::static_pointer_cast<IndexGet>(e);
                walkExpr(s, g->object);
                walkExpr(s, g->index);
            }
            else if (type == AstKind::IndexSet)
            {
                auto g = std::static_pointer_cast<IndexSet>(e);
                walkExpr(s, g->object);
                walkExpr(s, g->index);
                walkExpr(s, g->value);
            }
            else if (type == AstKind::List)
            {
                for (const auto& entry : listOf(std::static_pointer_cast<List>(e)->entries))
                {
                    walkExpr(s, entry.key);
                    walkExpr(s, entry.value);
                }
            }
            else if (type == AstKind::AnonymousFunction)
            {
                auto f = std::static_pointer_cast<AnonymousFunction>(e);
                const std::string outer = s.function;
                const int outerDepth = s.loopDepth;
                s.function = outer + " > fun()";
                s.loopDepth = 0;
                walkStmt(s, f->body);
                s.function = outer;
                s.loopDepth = outerDepth;
            }
        }

        // `len(x)` calls of an expression: the x's
        void lenArgs(const ExprPtr& e, std::set<std::string>& out)
        {
            if (not e)
                return;

            const AstKind type = e->kind();

            if (type == AstKind::CallExpression)
            {
                auto c = std::static_pointer_cast<CallExpression>(e);
                auto args = listOf(c->args);

                if (varName(c->caller) == "len" and args.size() == 1 and not varName(args[0]).empty())
                    out.insert(varName(args[0]));

                for (const auto& a : args)
                    lenArgs(a, out);
            }
            else if (type == AstKind::BinaryExpression)
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                lenArgs(b->leftExpr, out);
                lenArgs(b->rightExpr, out);
            }
            else if (type == AstKind::LogicExpression)
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);
                lenArgs(l->leftExpr, out);
                lenArgs(l->rightExpr, out);
            }
            else if (type == AstKind::CompoundAtom)
                lenArgs(std::static_pointer_cast<CompoundAtom>(e)->expr, out);
            else if (type == AstKind::UnaryExpression)
                lenArgs(std::static_pointer_cast<UnaryExpression>(e)->expr, out);
        }

        // What a body changes in place: what it passes first to push / insert / removeAt / pop, and
        // what it writes a field or an index of. Field reads it repeats: "base.name" -> count
        struct BodyFacts
        {
            std::set<std::string> grown;
            std::map<std::string, int> gets;
        };

        void bodyExpr(const ExprPtr& e, BodyFacts& f);
        void bodyStmt(const StatementPtr& st, BodyFacts& f);

        void bodyExpr(const ExprPtr& e, BodyFacts& f)
        {
            if (not e)
                return;

            const AstKind type = e->kind();

            if (type == AstKind::CallExpression)
            {
                auto c = std::static_pointer_cast<CallExpression>(e);
                const std::string name = varName(c->caller);
                auto args = listOf(c->args);

                if ((name == "push" or name == "insert" or name == "removeAt" or name == "pop") and not args.empty() and not varName(args[0]).empty())
                    f.grown.insert(varName(args[0]));

                for (const auto& a : args)
                    bodyExpr(a, f);
            }
            else if (type == AstKind::Get)
            {
                auto g = std::static_pointer_cast<Get>(e);

                if (not varName(g->object).empty())
                    ++f.gets[varName(g->object) + "." + g->name.text];

                bodyExpr(g->object, f);
            }
            else if (type == AstKind::Set)
            {
                auto st = std::static_pointer_cast<Set>(e);
                f.grown.insert(varName(st->object));
                bodyExpr(st->object, f);
                bodyExpr(st->value, f);
            }
            else if (type == AstKind::IndexSet)
            {
                auto g = std::static_pointer_cast<IndexSet>(e);
                f.grown.insert(varName(g->object));
                bodyExpr(g->object, f);
                bodyExpr(g->index, f);
                bodyExpr(g->value, f);
            }
            else if (type == AstKind::IndexGet)
            {
                auto g = std::static_pointer_cast<IndexGet>(e);
                bodyExpr(g->object, f);
                bodyExpr(g->index, f);
            }
            else if (type == AstKind::BinaryExpression)
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                bodyExpr(b->leftExpr, f);
                bodyExpr(b->rightExpr, f);
            }
            else if (type == AstKind::LogicExpression)
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);
                bodyExpr(l->leftExpr, f);
                bodyExpr(l->rightExpr, f);
            }
            else if (type == AstKind::UnaryExpression)
                bodyExpr(std::static_pointer_cast<UnaryExpression>(e)->expr, f);
            else if (type == AstKind::CompoundAtom)
                bodyExpr(std::static_pointer_cast<CompoundAtom>(e)->expr, f);
            else if (type == AstKind::Assign)
                bodyExpr(std::static_pointer_cast<Assign>(e)->expr, f);
            else if (type == AstKind::List)
            {
                for (const auto& entry : listOf(std::static_pointer_cast<List>(e)->entries))
                {
                    bodyExpr(entry.key, f);
                    bodyExpr(entry.value, f);
                }
            }
        }

        void bodyStmt(const StatementPtr& st, BodyFacts& f)
        {
            if (not st)
                return;

            const AstKind type = st->kind();

            if (type == AstKind::ExpressionStatement)
                bodyExpr(std::static_pointer_cast<ExpressionStatement>(st)->expr, f);
            else if (type == AstKind::VariableStatement)
                bodyExpr(std::static_pointer_cast<VariableStatement>(st)->expr, f);
            else if (type == AstKind::BlockStatement)
            {
                for (const auto& s : listOf(std::static_pointer_cast<BlockStatement>(st)->statements))
                    bodyStmt(s, f);
            }
            else if (type == AstKind::IfStatement)
            {
                auto i = std::static_pointer_cast<IfStatement>(st);
                bodyExpr(i->condition, f);
                bodyStmt(i->thenBranch, f);
                bodyStmt(i->elseBranch, f);
            }
            else if (type == AstKind::WhileStatement)
            {
                auto w = std::static_pointer_cast<WhileStatement>(st);
                bodyExpr(w->condition, f);
                bodyStmt(w->body, f);
            }
            else if (type == AstKind::ForStatement)
            {
                auto fo = std::static_pointer_cast<ForStatement>(st);
                bodyStmt(fo->initializer, f);
                bodyExpr(fo->condition, f);
                bodyExpr(fo->increment, f);
                bodyStmt(fo->body, f);
            }
            else if (type == AstKind::ForInStatement)
            {
                auto fi = std::static_pointer_cast<ForInStatement>(st);
                bodyExpr(fi->iterable, f);
                bodyStmt(fi->body, f);
            }
            else if (type == AstKind::ReturnStatement)
                bodyExpr(std::static_pointer_cast<ReturnStatement>(st)->value, f);
        }

        void surveyLoop(Survey& s, const ExprPtr& condition, const ExprPtr& increment, const StatementPtr& body, int line)
        {
            ++s.loops;

            ast::LoopFacts facts;
            ast::analyzeExpr(condition, facts);
            ast::analyzeExpr(increment, facts);
            ast::analyzeStmt(body, facts);

            if (facts.hasCalls)
                ++s.loopsWithCalls;

            BodyFacts inBody;
            bodyStmt(body, inBody);

            std::set<std::string> lens;
            lenArgs(condition, lens);

            for (const auto& x : lens)
            {
                if (facts.mutated.count(x) == 0 and inBody.grown.count(x) == 0)
                {
                    const std::string scope = s.locals.count(x) ? "local" : "global";
                    s.lenHoists.push_back(s.function + ", line " + std::to_string(line) + ": len(" + x + ") (" + scope + ") read every turn, unchanged by the loop");
                }
            }

            for (const auto& [get, count] : inBody.gets)
            {
                const std::string base = get.substr(0, get.find('.'));

                if (count >= 3 and facts.mutated.count(base) == 0)
                    s.repeatedGets.push_back(s.function + ", line " + std::to_string(line) + ": " + get + " read " + std::to_string(count) + " times in one turn");
            }
        }

        void walkStmt(Survey& s, const StatementPtr& st)
        {
            if (not st)
                return;

            const AstKind type = st->kind();

            if (type == AstKind::ExpressionStatement)
                walkExpr(s, std::static_pointer_cast<ExpressionStatement>(st)->expr);
            else if (type == AstKind::VariableStatement)
            {
                auto v = std::static_pointer_cast<VariableStatement>(st);
                s.locals.insert(v->name.text);
                walkExpr(s, v->expr);
            }
            else if (type == AstKind::FunctionStatement)
            {
                auto f = std::static_pointer_cast<FunctionStatement>(st);
                auto params = listOf(f->parameters);

                std::string paramText;

                for (const auto& p : params)
                    paramText += (paramText.empty() ? "" : ", ") + p->getName();

                // One return and nothing else: what calling it costs is a frame for that expression
                if (f->body and f->body->kind() == AstKind::BlockStatement)
                {
                    auto body = listOf(std::static_pointer_cast<BlockStatement>(f->body)->statements);

                    if (body.size() == 1 and body[0] and body[0]->kind() == AstKind::ReturnStatement)
                    {
                        auto r = std::static_pointer_cast<ReturnStatement>(body[0]);

                        if (r->value)
                            s.wrappers[f->name.text] = "(" + paramText + ") -> " + r->value->prettyPrint();
                    }
                }

                const std::string outer = s.function;
                const auto outerLocals = s.locals;
                const int outerDepth = s.loopDepth;

                s.function = "fun " + f->name.text;
                s.locals.clear();

                for (const auto& p : params)
                    s.locals.insert(p->getName());

                s.loopDepth = 0;
                walkStmt(s, f->body);

                s.function = outer;
                s.locals = outerLocals;
                s.loopDepth = outerDepth;
            }
            else if (type == AstKind::ClassStatement)
            {
                for (const auto& m : listOf(std::static_pointer_cast<ClassStatement>(st)->methods))
                    walkStmt(s, m);
            }
            else if (type == AstKind::BlockStatement)
            {
                for (const auto& b : listOf(std::static_pointer_cast<BlockStatement>(st)->statements))
                    walkStmt(s, b);
            }
            else if (type == AstKind::IfStatement)
            {
                auto i = std::static_pointer_cast<IfStatement>(st);
                walkExpr(s, i->condition);
                walkStmt(s, i->thenBranch);
                walkStmt(s, i->elseBranch);
            }
            else if (type == AstKind::WhileStatement)
            {
                auto w = std::static_pointer_cast<WhileStatement>(st);
                surveyLoop(s, w->condition, nullptr, w->body, lineOf(w->condition));
                walkExpr(s, w->condition);
                ++s.loopDepth;
                walkStmt(s, w->body);
                --s.loopDepth;
            }
            else if (type == AstKind::ForStatement)
            {
                auto fo = std::static_pointer_cast<ForStatement>(st);
                walkStmt(s, fo->initializer);
                surveyLoop(s, fo->condition, fo->increment, fo->body, lineOf(fo->condition));
                ++s.loopDepth;
                walkExpr(s, fo->condition);
                walkExpr(s, fo->increment);
                walkStmt(s, fo->body);
                --s.loopDepth;
            }
            else if (type == AstKind::ForInStatement)
            {
                auto fi = std::static_pointer_cast<ForInStatement>(st);
                walkExpr(s, fi->iterable);
                s.locals.insert(fi->varName.text);
                surveyLoop(s, nullptr, nullptr, fi->body, static_cast<int>(fi->varName.line));
                ++s.loopDepth;
                walkStmt(s, fi->body);
                --s.loopDepth;
            }
            else if (type == AstKind::ReturnStatement)
                walkExpr(s, std::static_pointer_cast<ReturnStatement>(st)->value);
            else if (type == AstKind::DPrintStatement)
                walkExpr(s, std::static_pointer_cast<DPrintStatement>(st)->expr);
        }
    }

    bool AstSurveyPass::runPass(VM* vm, std::queue<StatementPtr>& statements)
    {
        Survey s;

        for (const auto& st : listOf(statements))
            walkStmt(s, st);

        std::ofstream out(outPath, std::ios::app);

        if (not out)
            return false;

        out << "== " << (vm ? vm->currentFileName : std::string("?")) << " (" << statements.size() << " top-level statements)\n";
        out << "loops: " << s.loops << ", " << s.loopsWithCalls << " of them with a call (left alone by LoopInvariantHoisting)\n";

        out << "functions that are one return, with the call sites of each in this file:\n";

        for (const auto& [name, body] : s.wrappers)
            out << "   " << name << body << "   [" << s.calls[name] << " sites, " << s.loopCalls[name] << " in loops]\n";

        // The most called, by call sites
        std::vector<std::pair<long, std::string>> top;

        for (const auto& [name, count] : s.calls)
            top.emplace_back(count, name);

        std::sort(top.rbegin(), top.rend());

        out << "most called (sites, in loops):";

        for (size_t i = 0; i < top.size() and i < 20; ++i)
            out << " " << top[i].second << " " << top[i].first << "/" << s.loopCalls[top[i].second];

        out << "\n";

        out << "len() read every turn of a loop that leaves it unchanged: " << s.lenHoists.size() << "\n";

        for (const auto& line : s.lenHoists)
            out << "   " << line << "\n";

        out << "field reads repeated in one turn of a loop: " << s.repeatedGets.size() << "\n";

        for (const auto& line : s.repeatedGets)
            out << "   " << line << "\n";

        out << "\n";

        return false;
    }
}
