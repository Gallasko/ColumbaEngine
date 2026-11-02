#include "stdafx.h"

#include "length_hoisting.h"

#include "ast_analysis.h"
#include "program_view.h"

#include "Compiler/vm.h"
#include "logger.h"

#include <algorithm>

namespace pg
{
    namespace
    {
        const char* DOM = "LengthHoisting";

        // Natives that change no table and call no script code
        const std::set<std::string> readOnlyNatives = {
            "len", "contain", "lookup", "toInt", "toFloat", "typeOf",
            "sqrt", "abs", "pow", "floor", "ceil", "sin", "cos", "tan", "atan2", "asin", "acos", "atan",
            "toString", "strlen", "split", "splitLines", "startsWith", "endsWith", "contains", "charAt",
            "substring", "replace", "join", "capitalize", "lowerize", "toLowerCase", "toUpperCase", "trim", "indexOf",
            "random", "randomRange", "randomInt"
        };

        // Natives that change the list they are given first
        const std::set<std::string> listNatives = {"push", "insert", "pop", "removeAt"};

        // Natives whose result is never a table
        const std::set<std::string> noTableNatives = {
            "readFile", "len", "toInt", "toFloat", "toString", "strlen", "charAt", "substring", "replace", "join",
            "capitalize", "lowerize", "toLowerCase", "toUpperCase", "trim"
        };

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

        template <typename T>
        std::queue<T> queueOf(const std::vector<T>& v)
        {
            std::queue<T> out;

            for (const auto& e : v)
                out.push(e);

            return out;
        }

        std::string varName(const ExprPtr& e)
        {
            if (e and e->kind() == AstKind::Var)
                return std::static_pointer_cast<Var>(e)->name.text;

            return "";
        }

        // The t of `t` or `t[i][j]`, or ""
        std::string rootOf(const ExprPtr& e)
        {
            if (e and e->kind() == AstKind::IndexGet)
                return rootOf(std::static_pointer_cast<IndexGet>(e)->object);

            return varName(e);
        }

        // A name or a chain of indexing by names and numbers, as text; "" for anything else
        std::string pathOf(const ExprPtr& e)
        {
            if (not e or e->kind() != AstKind::IndexGet)
                return varName(e);

            auto g = std::static_pointer_cast<IndexGet>(e);
            const std::string object = pathOf(g->object);

            if (object.empty())
                return "";

            std::string index = varName(g->index);

            if (g->index->kind() == AstKind::Atom)
            {
                const auto& value = std::static_pointer_cast<Atom>(g->index)->value;

                if (value.isNumber())
                    index = value.toString();
            }

            return index.empty() ? "" : object + "[" + index + "]";
        }

        // The names of a path
        void namesOf(const ExprPtr& e, std::set<std::string>& out)
        {
            if (e and e->kind() == AstKind::IndexGet)
            {
                auto g = std::static_pointer_cast<IndexGet>(e);
                namesOf(g->object, out);
                namesOf(g->index, out);
            }
            else if (e and e->kind() == AstKind::Var)
                out.insert(varName(e));
        }

        // The x of `len(x)` when it is a path, or nullptr
        ExprPtr lenArgOf(const ExprPtr& e)
        {
            if (not e or e->kind() != AstKind::CallExpression)
                return nullptr;

            auto c = std::static_pointer_cast<CallExpression>(e);

            if (varName(c->caller) != "len" or c->args.size() != 1 or pathOf(c->args.front()).empty())
                return nullptr;

            return c->args.front();
        }

        // Whether nothing but what the expression makes can reach its value: no table, or a new one
        // holding only such values. An operator makes a number or a string: lists do not add
        bool fresh(const ExprPtr& e)
        {
            if (not e)
                return false;

            const AstKind type = e->kind();

            if (type == AstKind::Atom or type == AstKind::NoneAtom or type == AstKind::UnaryExpression or type == AstKind::BinaryExpression)
                return true;

            if (type == AstKind::CompoundAtom)
                return fresh(std::static_pointer_cast<CompoundAtom>(e)->expr);

            if (type != AstKind::List)
                return false;

            for (const auto& entry : listOf(std::static_pointer_cast<List>(e)->entries))
            {
                if (not fresh(entry.value))
                    return false;
            }

            return true;
        }

        // The names a function body declares
        void declsOf(const StatementPtr& s, std::set<std::string>& out)
        {
            if (not s)
                return;

            const AstKind type = s->kind();

            if (type == AstKind::VariableStatement)
                out.insert(std::static_pointer_cast<VariableStatement>(s)->name.text);
            else if (type == AstKind::FunctionStatement)
                out.insert(std::static_pointer_cast<FunctionStatement>(s)->name.text);
            else if (type == AstKind::ClassStatement)
                out.insert(std::static_pointer_cast<ClassStatement>(s)->name.text);
            else if (type == AstKind::BlockStatement)
            {
                for (const auto& b : listOf(std::static_pointer_cast<BlockStatement>(s)->statements))
                    declsOf(b, out);
            }
            else if (type == AstKind::IfStatement)
            {
                auto i = std::static_pointer_cast<IfStatement>(s);
                declsOf(i->thenBranch, out);
                declsOf(i->elseBranch, out);
            }
            else if (type == AstKind::WhileStatement)
                declsOf(std::static_pointer_cast<WhileStatement>(s)->body, out);
            else if (type == AstKind::ForStatement)
            {
                auto f = std::static_pointer_cast<ForStatement>(s);
                declsOf(f->initializer, out);
                declsOf(f->body, out);
            }
            else if (type == AstKind::ForInStatement)
            {
                auto f = std::static_pointer_cast<ForInStatement>(s);
                out.insert(f->varName.text);
                declsOf(f->body, out);
            }
        }

        // ---- the lists a function owns alone -------------------------------------------------------

        // Per name of a function: how it is declared and assigned, and whether it leaves
        struct Ownership
        {
            std::map<std::string, int> listDecls;          // `var name = [...]`
            std::set<std::string> otherWrites;             // Declared or assigned anything else, a parameter, a loop variable
            std::set<std::string> escaped;                 // Read anywhere a list could leave the function
            std::set<std::string> given;                   // Given a value that is not fresh: its rows may be another's
            std::set<std::string> rowsRead;                // A row read anywhere it could leave the function
            std::set<std::string> noTables;                // Declared or assigned what is no table
            std::set<std::string> maybeTables;             // Declared or assigned what may be one
            std::set<std::string> nativesTrusted;          // The noTableNatives noTables rests on

            bool owned(const std::string& name) const
            {
                auto it = listDecls.find(name);
                return it != listDecls.end() and it->second == 1 and not otherWrites.count(name) and not escaped.count(name);
            }

            // Owned with all it holds, down to the last row
            bool ownedDeep(const std::string& name) const
            {
                return owned(name) and not given.count(name) and not rowsRead.count(name);
            }

            bool holdsNoTable(const std::string& name) const
            {
                return noTables.count(name) and not maybeTables.count(name);
            }

            void wrote(const std::string& name, const ExprPtr& value)
            {
                if (value and not noTable(value))
                    maybeTables.insert(name);
                else
                    noTables.insert(name);
            }

            // A value that is no table when the natives it calls are the natives
            bool noTable(const ExprPtr& e)
            {
                const AstKind type = e->kind();

                if (type == AstKind::Atom or type == AstKind::NoneAtom or type == AstKind::UnaryExpression or type == AstKind::BinaryExpression)
                    return true;

                if (type == AstKind::CompoundAtom)
                    return noTable(std::static_pointer_cast<CompoundAtom>(e)->expr);

                if (type != AstKind::CallExpression)
                    return false;

                const std::string callee = varName(std::static_pointer_cast<CallExpression>(e)->caller);

                if (not noTableNatives.count(callee))
                    return false;

                nativesTrusted.insert(callee);

                return true;
            }
        };

        void ownExpr(const ExprPtr& e, Ownership& o, bool nested);
        void ownStmt(const StatementPtr& s, Ownership& o, bool nested);

        // Read where a list stays: the object of an index, a field, a measure or a list native
        void ownObject(const ExprPtr& x, Ownership& o, bool nested)
        {
            if (nested)
                ownExpr(x, o, nested);
            else if (x->kind() == AstKind::IndexGet)
            {
                auto g = std::static_pointer_cast<IndexGet>(x);
                ownObject(g->object, o, nested);
                ownExpr(g->index, o, nested);
            }
            else if (x->kind() != AstKind::Var)
                ownExpr(x, o, nested);
        }

        // `nested`: inside a function the function defines, where every name read is captured
        void ownExpr(const ExprPtr& e, Ownership& o, bool nested)
        {
            if (not e)
                return;

            const AstKind type = e->kind();

            if (type == AstKind::Var)
            {
                o.escaped.insert(varName(e));
                return;
            }

            // A row an operator reads stays, as a number or a string comes out
            auto operand = [&](const ExprPtr& x) {
                if (x->kind() == AstKind::IndexGet)
                    ownObject(x, o, nested);
                else
                    ownExpr(x, o, nested);
            };

            if (type == AstKind::CallExpression)
            {
                auto c = std::static_pointer_cast<CallExpression>(e);
                const std::string callee = varName(c->caller);
                auto args = listOf(c->args);

                if (callee.empty())
                    ownExpr(c->caller, o, nested);

                for (size_t i = 0; i < args.size(); ++i)
                {
                    if (i == 0 and (callee == "len" or listNatives.count(callee)))
                        ownObject(args[i], o, nested);
                    else
                        ownExpr(args[i], o, nested);
                }

                if (listNatives.count(callee) and not args.empty())
                {
                    // What comes off a list is a row read, what goes in is given
                    if (callee == "pop" or callee == "removeAt")
                        o.rowsRead.insert(rootOf(args[0]));
                    else if (args.size() < 2 or not fresh(args.back()))
                        o.given.insert(rootOf(args[0]));
                }
            }
            else if (type == AstKind::IndexGet)
            {
                auto g = std::static_pointer_cast<IndexGet>(e);
                o.rowsRead.insert(rootOf(e));
                ownObject(g->object, o, nested);
                ownExpr(g->index, o, nested);
            }
            else if (type == AstKind::IndexSet)
            {
                auto g = std::static_pointer_cast<IndexSet>(e);
                ownObject(g->object, o, nested);
                ownExpr(g->index, o, nested);
                ownExpr(g->value, o, nested);

                if (not fresh(g->value))
                    o.given.insert(rootOf(g->object));
            }
            else if (type == AstKind::Get)
            {
                auto g = std::static_pointer_cast<Get>(e);
                o.rowsRead.insert(rootOf(g->object));
                ownObject(g->object, o, nested);
            }
            else if (type == AstKind::Set)
            {
                auto g = std::static_pointer_cast<Set>(e);
                ownObject(g->object, o, nested);
                ownExpr(g->value, o, nested);

                if (not fresh(g->value))
                    o.given.insert(rootOf(g->object));
            }
            else if (type == AstKind::BinaryExpression)
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);

                // Compared, not kept
                if (b->op.text == "==" or b->op.text == "!=")
                {
                    ownObject(b->leftExpr, o, nested);
                    ownObject(b->rightExpr, o, nested);
                }
                else
                {
                    operand(b->leftExpr);
                    operand(b->rightExpr);
                }
            }
            else if (type == AstKind::LogicExpression)
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);
                ownExpr(l->leftExpr, o, nested);
                ownExpr(l->rightExpr, o, nested);
            }
            else if (type == AstKind::UnaryExpression)
                ownExpr(std::static_pointer_cast<UnaryExpression>(e)->expr, o, nested);
            else if (type == AstKind::CompoundAtom)
                ownExpr(std::static_pointer_cast<CompoundAtom>(e)->expr, o, nested);
            else if (type == AstKind::Assign)
            {
                auto a = std::static_pointer_cast<Assign>(e);

                if (nested or not a->expr or a->expr->kind() != AstKind::List)
                    o.otherWrites.insert(a->name.text);

                if (not fresh(a->expr))
                    o.given.insert(a->name.text);

                o.wrote(a->name.text, a->expr);
                ownExpr(a->expr, o, nested);
            }
            else if (type == AstKind::PreFixExpression)
                o.otherWrites.insert(std::static_pointer_cast<PreFixExpression>(e)->name.text);
            else if (type == AstKind::PostFixExpression)
                o.otherWrites.insert(std::static_pointer_cast<PostFixExpression>(e)->name.text);
            else if (type == AstKind::List)
            {
                auto l = std::static_pointer_cast<List>(e);

                ownExpr(l->self, o, nested);

                for (const auto& entry : listOf(l->entries))
                {
                    if (not entry.explicitKey)
                        ownExpr(entry.key, o, nested);

                    ownExpr(entry.value, o, nested);
                }
            }
            else if (type == AstKind::AnonymousFunction)
            {
                auto f = std::static_pointer_cast<AnonymousFunction>(e);
                ownStmt(f->body, o, true);
            }
        }

        void ownStmt(const StatementPtr& s, Ownership& o, bool nested)
        {
            if (not s)
                return;

            const AstKind type = s->kind();

            if (type == AstKind::ExpressionStatement)
                ownExpr(std::static_pointer_cast<ExpressionStatement>(s)->expr, o, nested);
            else if (type == AstKind::VariableStatement)
            {
                auto v = std::static_pointer_cast<VariableStatement>(s);

                if (not nested and v->expr and v->expr->kind() == AstKind::List)
                    ++o.listDecls[v->name.text];
                else
                    o.otherWrites.insert(v->name.text);

                if (not fresh(v->expr))
                    o.given.insert(v->name.text);

                o.wrote(v->name.text, v->expr);
                ownExpr(v->expr, o, nested);
            }
            else if (type == AstKind::ReturnStatement)
            {
                auto r = std::static_pointer_cast<ReturnStatement>(s);

                // Returned: it leaves, but the function ends with it
                if (nested or not r->value or r->value->kind() != AstKind::Var)
                    ownExpr(r->value, o, nested);
            }
            else if (type == AstKind::DPrintStatement)
                ownExpr(std::static_pointer_cast<DPrintStatement>(s)->expr, o, nested);
            else if (type == AstKind::BlockStatement)
            {
                for (const auto& b : listOf(std::static_pointer_cast<BlockStatement>(s)->statements))
                    ownStmt(b, o, nested);
            }
            else if (type == AstKind::IfStatement)
            {
                auto i = std::static_pointer_cast<IfStatement>(s);
                ownExpr(i->condition, o, nested);
                ownStmt(i->thenBranch, o, nested);
                ownStmt(i->elseBranch, o, nested);
            }
            else if (type == AstKind::WhileStatement)
            {
                auto w = std::static_pointer_cast<WhileStatement>(s);
                ownExpr(w->condition, o, nested);
                ownStmt(w->body, o, nested);
            }
            else if (type == AstKind::ForStatement)
            {
                auto f = std::static_pointer_cast<ForStatement>(s);
                ownStmt(f->initializer, o, nested);
                ownExpr(f->condition, o, nested);
                ownExpr(f->increment, o, nested);
                ownStmt(f->body, o, nested);
            }
            else if (type == AstKind::ForInStatement)
            {
                auto f = std::static_pointer_cast<ForInStatement>(s);
                o.otherWrites.insert(f->varName.text);
                o.maybeTables.insert(f->varName.text);

                // Iterated, not kept: only its rows are
                if (nested or f->iterable->kind() != AstKind::Var)
                    ownExpr(f->iterable, o, nested);
                else
                    o.rowsRead.insert(varName(f->iterable));

                ownStmt(f->body, o, nested);
            }
            else if (type == AstKind::FunctionStatement)
            {
                auto f = std::static_pointer_cast<FunctionStatement>(s);
                o.otherWrites.insert(f->name.text);
                o.maybeTables.insert(f->name.text);
                ownStmt(f->body, o, true);
            }
            else if (type == AstKind::ClassStatement)
            {
                auto c = std::static_pointer_cast<ClassStatement>(s);
                o.otherWrites.insert(c->name.text);
                o.maybeTables.insert(c->name.text);

                for (const auto& m : listOf(c->methods))
                    ownStmt(m->body, o, true);
            }
        }

        // ---- the loops -----------------------------------------------------------------------------

        struct Ctx
        {
            const std::set<std::string>& scriptNames;

            // Of the function being rewritten (none at top level)
            std::set<std::string> locals;
            Ownership ownership;
            bool inFunction = false;

            int slotsUsed = 0;
            int counter = 0;
            long hoisted = 0;

            bool isNative(const std::string& name) const
            {
                return not scriptNames.count(name) and not locals.count(name);
            }

            // Ownership took them for the natives
            bool ownershipHolds() const
            {
                if (not inFunction)
                    return false;

                for (const auto& native : {"len", "push", "insert", "pop", "removeAt"})
                {
                    if (not isNative(native))
                        return false;
                }

                return true;
            }

            // Whether no write but those that name it can reach what a path measures: a list that
            // never left the function, or no table at all
            bool beyondWrites(const ExprPtr& path) const
            {
                const std::string root = rootOf(path);
                const bool whole = path->kind() == AstKind::Var;

                if (ownershipHolds() and (whole ? ownership.owned(root) : ownership.ownedDeep(root)))
                    return true;

                if (not whole or not locals.count(root) or not ownership.holdsNoTable(root))
                    return false;

                for (const auto& native : ownership.nativesTrusted)
                {
                    if (not isNative(native))
                        return false;
                }

                return true;
            }
        };

        // Whether a loop's region (not the functions it defines) leaves the length of a path alone
        struct RegionCheck
        {
            const Ctx& ctx;
            const std::string& x;       // The path's root
            const bool beyondWrites;    // Ctx::beyondWrites
            bool ok = true;

            // Whether writing into a list leaves the path alone
            bool mayWrite(const ExprPtr& list) const
            {
                const std::string root = rootOf(list);

                if (root.empty() or root == x)
                    return false;

                if (beyondWrites)
                    return true;

                if (not ctx.ownershipHolds())
                    return false;

                return list->kind() == AstKind::Var ? ctx.ownership.owned(root) : ctx.ownership.ownedDeep(root);
            }

            void expr(const ExprPtr& e)
            {
                if (not ok or not e)
                    return;

                const AstKind type = e->kind();

                if (type == AstKind::CallExpression)
                {
                    auto c = std::static_pointer_cast<CallExpression>(e);
                    const std::string callee = varName(c->caller);
                    auto args = listOf(c->args);

                    if (callee.empty() or ctx.scriptNames.count(callee) or ctx.locals.count(callee))
                    {
                        ok = false;
                        return;
                    }

                    if (listNatives.count(callee))
                    {
                        if (args.empty() or not mayWrite(args[0]))
                        {
                            ok = false;
                            return;
                        }
                    }
                    else if (not readOnlyNatives.count(callee))
                    {
                        ok = false;
                        return;
                    }

                    for (const auto& a : args)
                        expr(a);
                }
                else if (type == AstKind::IndexSet)
                {
                    auto g = std::static_pointer_cast<IndexSet>(e);

                    if (not mayWrite(g->object))
                    {
                        ok = false;
                        return;
                    }

                    expr(g->object);
                    expr(g->index);
                    expr(g->value);
                }
                else if (type == AstKind::IndexGet)
                {
                    auto g = std::static_pointer_cast<IndexGet>(e);
                    expr(g->object);
                    expr(g->index);
                }
                else if (type == AstKind::Get)
                    expr(std::static_pointer_cast<Get>(e)->object);
                else if (type == AstKind::Set)
                {
                    auto g = std::static_pointer_cast<Set>(e);
                    expr(g->object);
                    expr(g->value);
                }
                else if (type == AstKind::BinaryExpression)
                {
                    auto b = std::static_pointer_cast<BinaryExpression>(e);
                    expr(b->leftExpr);
                    expr(b->rightExpr);
                }
                else if (type == AstKind::LogicExpression)
                {
                    auto l = std::static_pointer_cast<LogicExpression>(e);
                    expr(l->leftExpr);
                    expr(l->rightExpr);
                }
                else if (type == AstKind::UnaryExpression)
                    expr(std::static_pointer_cast<UnaryExpression>(e)->expr);
                else if (type == AstKind::CompoundAtom)
                    expr(std::static_pointer_cast<CompoundAtom>(e)->expr);
                else if (type == AstKind::Assign)
                    expr(std::static_pointer_cast<Assign>(e)->expr);
                else if (type == AstKind::List)
                {
                    auto l = std::static_pointer_cast<List>(e);

                    for (const auto& entry : listOf(l->entries))
                    {
                        if (not entry.explicitKey)
                            expr(entry.key);

                        expr(entry.value);
                    }
                }
                // Var, Atom, NoneAtom, This, prefix / postfix (names only), AnonymousFunction (only defined)
            }

            void stmt(const StatementPtr& s)
            {
                if (not ok or not s)
                    return;

                const AstKind type = s->kind();

                if (type == AstKind::ExpressionStatement)
                    expr(std::static_pointer_cast<ExpressionStatement>(s)->expr);
                else if (type == AstKind::VariableStatement)
                    expr(std::static_pointer_cast<VariableStatement>(s)->expr);
                else if (type == AstKind::ReturnStatement)
                    expr(std::static_pointer_cast<ReturnStatement>(s)->value);
                else if (type == AstKind::DPrintStatement)
                    expr(std::static_pointer_cast<DPrintStatement>(s)->expr);
                else if (type == AstKind::BlockStatement)
                {
                    for (const auto& b : listOf(std::static_pointer_cast<BlockStatement>(s)->statements))
                        stmt(b);
                }
                else if (type == AstKind::IfStatement)
                {
                    auto i = std::static_pointer_cast<IfStatement>(s);
                    expr(i->condition);
                    stmt(i->thenBranch);
                    stmt(i->elseBranch);
                }
                else if (type == AstKind::WhileStatement)
                {
                    auto w = std::static_pointer_cast<WhileStatement>(s);
                    expr(w->condition);
                    stmt(w->body);
                }
                else if (type == AstKind::ForStatement)
                {
                    auto f = std::static_pointer_cast<ForStatement>(s);
                    stmt(f->initializer);
                    expr(f->condition);
                    expr(f->increment);
                    stmt(f->body);
                }
                else if (type == AstKind::ForInStatement)
                {
                    auto f = std::static_pointer_cast<ForInStatement>(s);
                    expr(f->iterable);
                    stmt(f->body);
                }
                // Function and class statements: only defined here
            }
        };

        // The paths whose `len` a condition reads on its first evaluation
        void firstLens(const ExprPtr& e, std::map<std::string, ExprPtr>& out)
        {
            if (not e)
                return;

            if (auto x = lenArgOf(e))
            {
                out[pathOf(x)] = x;
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

        // `len(x)` read by the region, the hidden local instead (not in the functions it defines)
        struct Replace
        {
            const std::map<std::string, std::string>& hidden;

            ExprPtr expr(const ExprPtr& e)
            {
                if (not e)
                    return e;

                if (auto x = lenArgOf(e))
                {
                    auto it = hidden.find(pathOf(x));

                    if (it != hidden.end())
                        return std::make_shared<Var>(Token(TokenType::EXPRESSION, it->second, 0, 0));
                }

                const AstKind type = e->kind();

                if (type == AstKind::CallExpression)
                {
                    auto c = std::static_pointer_cast<CallExpression>(e);
                    std::vector<ExprPtr> args;

                    for (const auto& a : listOf(c->args))
                        args.push_back(expr(a));

                    c->args = queueOf(args);
                }
                else if (type == AstKind::IndexSet)
                {
                    auto g = std::static_pointer_cast<IndexSet>(e);
                    g->object = expr(g->object);
                    g->index = expr(g->index);
                    g->value = expr(g->value);
                }
                else if (type == AstKind::IndexGet)
                {
                    auto g = std::static_pointer_cast<IndexGet>(e);
                    g->object = expr(g->object);
                    g->index = expr(g->index);
                }
                else if (type == AstKind::Get)
                {
                    auto g = std::static_pointer_cast<Get>(e);
                    g->object = expr(g->object);
                }
                else if (type == AstKind::Set)
                {
                    auto g = std::static_pointer_cast<Set>(e);
                    g->object = expr(g->object);
                    g->value = expr(g->value);
                }
                else if (type == AstKind::BinaryExpression)
                {
                    auto b = std::static_pointer_cast<BinaryExpression>(e);
                    b->leftExpr = expr(b->leftExpr);
                    b->rightExpr = expr(b->rightExpr);
                }
                else if (type == AstKind::LogicExpression)
                {
                    auto l = std::static_pointer_cast<LogicExpression>(e);
                    l->leftExpr = expr(l->leftExpr);
                    l->rightExpr = expr(l->rightExpr);
                }
                else if (type == AstKind::UnaryExpression)
                {
                    auto u = std::static_pointer_cast<UnaryExpression>(e);
                    u->expr = expr(u->expr);
                }
                else if (type == AstKind::CompoundAtom)
                {
                    auto c = std::static_pointer_cast<CompoundAtom>(e);
                    c->expr = expr(c->expr);
                }
                else if (type == AstKind::Assign)
                {
                    auto a = std::static_pointer_cast<Assign>(e);
                    a->expr = expr(a->expr);
                }
                else if (type == AstKind::List)
                {
                    auto l = std::static_pointer_cast<List>(e);
                    std::vector<ListElement> entries;

                    for (auto entry : listOf(l->entries))
                    {
                        if (not entry.explicitKey)
                            entry.key = expr(entry.key);

                        entry.value = expr(entry.value);
                        entries.push_back(entry);
                    }

                    l->entries = queueOf(entries);
                }

                return e;
            }

            void stmt(const StatementPtr& s)
            {
                if (not s)
                    return;

                const AstKind type = s->kind();

                if (type == AstKind::ExpressionStatement)
                {
                    auto x = std::static_pointer_cast<ExpressionStatement>(s);
                    x->expr = expr(x->expr);
                }
                else if (type == AstKind::VariableStatement)
                {
                    auto v = std::static_pointer_cast<VariableStatement>(s);
                    v->expr = expr(v->expr);
                }
                else if (type == AstKind::ReturnStatement)
                {
                    auto r = std::static_pointer_cast<ReturnStatement>(s);
                    r->value = expr(r->value);
                }
                else if (type == AstKind::DPrintStatement)
                {
                    auto d = std::static_pointer_cast<DPrintStatement>(s);
                    d->expr = expr(d->expr);
                }
                else if (type == AstKind::BlockStatement)
                {
                    for (const auto& b : listOf(std::static_pointer_cast<BlockStatement>(s)->statements))
                        stmt(b);
                }
                else if (type == AstKind::IfStatement)
                {
                    auto i = std::static_pointer_cast<IfStatement>(s);
                    i->condition = expr(i->condition);
                    stmt(i->thenBranch);
                    stmt(i->elseBranch);
                }
                else if (type == AstKind::WhileStatement)
                {
                    auto w = std::static_pointer_cast<WhileStatement>(s);
                    w->condition = expr(w->condition);
                    stmt(w->body);
                }
                else if (type == AstKind::ForStatement)
                {
                    auto f = std::static_pointer_cast<ForStatement>(s);
                    stmt(f->initializer);
                    f->condition = expr(f->condition);
                    f->increment = expr(f->increment);
                    stmt(f->body);
                }
                else if (type == AstKind::ForInStatement)
                {
                    auto f = std::static_pointer_cast<ForInStatement>(s);
                    f->iterable = expr(f->iterable);
                    stmt(f->body);
                }
            }
        };

        StatementPtr hoistLoop(const StatementPtr& s, Ctx& ctx)
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

            for (const auto& [path, x] : lens)
            {
                std::set<std::string> names;
                namesOf(x, names);

                if (std::any_of(names.begin(), names.end(), [&](const std::string& name) { return facts.mutated.count(name); }))
                    continue;

                const std::string root = rootOf(x);

                RegionCheck check{ctx, root, ctx.beyondWrites(x)};
                check.stmt(s);

                if (not check.ok)
                    continue;

                if (ctx.slotsUsed + 1 > ast::kMaxLocalSlots)
                    break;

                ++ctx.slotsUsed;

                // '@' cannot start a source name: no user name collides
                const std::string name = "@len" + std::to_string(ctx.counter++);
                const Token lenToken(TokenType::EXPRESSION, "len", 0, 0);

                std::queue<ExprPtr> args;
                args.push(x);

                auto call = std::make_shared<CallExpression>(std::make_shared<Var>(lenToken), lenToken, args);

                block.push(std::make_shared<VariableStatement>(Token(TokenType::EXPRESSION, name, 0, 0), call));
                hidden[path] = name;
            }

            if (hidden.empty())
                return s;

            Replace replace{hidden};
            replace.stmt(s);

            ctx.hoisted += hidden.size();
            block.push(s);

            return std::make_shared<BlockStatement>(block);
        }

        StatementPtr transform(const StatementPtr& s, Ctx& ctx);

        void transformFunction(const std::queue<ExprPtr>& parameters, StatementPtr& body, Ctx& ctx, int extraSlots)
        {
            std::set<std::string> savedLocals = ctx.locals;
            Ownership savedOwnership = ctx.ownership;
            const bool savedIn = ctx.inFunction;
            const int savedSlots = ctx.slotsUsed;

            ctx.locals.clear();

            for (const auto& p : listOf(parameters))
                ctx.locals.insert(p->getName());

            declsOf(body, ctx.locals);

            ctx.ownership = Ownership();

            for (const auto& p : listOf(parameters))
            {
                ctx.ownership.otherWrites.insert(p->getName());
                ctx.ownership.maybeTables.insert(p->getName());
            }

            ownStmt(body, ctx.ownership, false);

            ctx.inFunction = true;
            ctx.slotsUsed = extraSlots + static_cast<int>(parameters.size()) + ast::countLocalSlots(body);

            body = transform(body, ctx);

            ctx.locals = savedLocals;
            ctx.ownership = savedOwnership;
            ctx.inFunction = savedIn;
            ctx.slotsUsed = savedSlots;
        }

        void transformExpr(const ExprPtr& e, Ctx& ctx);

        // Functions defined inside expressions have loops of their own
        void transformExpr(const ExprPtr& e, Ctx& ctx)
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

        StatementPtr transform(const StatementPtr& s, Ctx& ctx)
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

        Ctx ctx{scriptNames};

        // The script's own nested blocks declare locals too (its lists are not its own: a function
        // could reach a global)
        for (const auto& s : listOf(statements))
        {
            if (s and s->kind() != AstKind::FunctionStatement and s->kind() != AstKind::ClassStatement and s->kind() != AstKind::VariableStatement)
                declsOf(s, ctx.locals);

            ownStmt(s, ctx.ownership, false);

            ctx.slotsUsed += ast::countLocalSlots(s);
        }

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
        // Whether the program could be read, and which of the names, and of the list natives it
        // always asks about, may be script code
        std::string facts = programComplete ? "complete;" : "incomplete;";

        for (const auto& native : {"len", "push", "insert", "pop", "removeAt"})
        {
            if (scriptNames.count(native))
                facts += std::string(native) + ";";
        }

        for (const auto& name : names)
        {
            if (scriptNames.count(name))
                facts += name + ";";
        }

        return facts;
    }
}
