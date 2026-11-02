#include "stdafx.h"

#include "loop_ownership.h"

#include <algorithm>

namespace pg
{
    namespace ast
    {
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

        namespace
        {
            // Natives whose result is never a table
            const std::set<std::string> noTableNatives = {
                "readFile", "len", "toInt", "toFloat", "toString", "strlen", "charAt", "substring", "replace", "join",
                "capitalize", "lowerize", "toLowerCase", "toUpperCase", "trim"
            };
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
        std::string indexOf(const ExprPtr& e)
        {
            if (e and e->kind() == AstKind::Atom)
            {
                const auto& value = std::static_pointer_cast<Atom>(e)->value;

                return value.isNumber() ? value.toString() : "";
            }

            return varName(e);
        }

        std::string pathOf(const ExprPtr& e)
        {
            if (not e or e->kind() != AstKind::IndexGet)
                return varName(e);

            auto g = std::static_pointer_cast<IndexGet>(e);
            const std::string object = pathOf(g->object);

            if (object.empty())
                return "";

            const std::string index = indexOf(g->index);

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

        std::string listNativeFacts(const std::set<std::string>& names, const std::set<std::string>& scriptNames, bool programComplete)
        {
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

        namespace
        {
            // No table, whatever the program defines: a value and what operators make of values
            bool plainValue(const ExprPtr& e)
            {
                if (not e)
                    return false;

                const AstKind type = e->kind();

                if (type == AstKind::CompoundAtom)
                    return plainValue(std::static_pointer_cast<CompoundAtom>(e)->expr);

                return type == AstKind::Atom or type == AstKind::NoneAtom or type == AstKind::UnaryExpression or type == AstKind::BinaryExpression;
            }
        }

        void Ownership::gave(const std::string& list, const ExprPtr& value)
        {
            if (value and value->kind() == AstKind::Var)
                givenNames[list].insert(varName(value));
            else if (not fresh(value))
                given.insert(list);
        }

        bool Ownership::givenOnlyPlain(const std::string& name) const
        {
            auto it = givenNames.find(name);

            if (it == givenNames.end())
                return true;

            return std::all_of(it->second.begin(), it->second.end(), [&](const std::string& local) {
                return declared.count(local) and intWrites.count(local) and not notPlain.count(local);
            });
        }

        void Ownership::wrote(const std::string& name, const ExprPtr& value)
        {
            if (value and value->kind() == AstKind::List)
            {
                auto l = std::static_pointer_cast<List>(value);

                if (l->braceForm or l->hasExplicitKeys)
                    tableLiterals.insert(name);
            }

            if (value and not noTable(value))
                maybeTables.insert(name);
            else
                noTables.insert(name);

            if (not plainValue(value))
                notPlain.insert(name);

            if (value)
                intWrites[name].push_back(value);
            else
                notInts.insert(name);
        }

        bool Ownership::noTable(const ExprPtr& e)
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

        bool Ownership::isInt(const ExprPtr& e) const
        {
            if (not e)
                return false;

            const AstKind type = e->kind();

            if (type == AstKind::Atom)
                return std::static_pointer_cast<Atom>(e)->value.type == UnionType::INT;

            if (type == AstKind::Var)
                return ints.count(varName(e)) > 0;

            if (type == AstKind::CompoundAtom)
                return isInt(std::static_pointer_cast<CompoundAtom>(e)->expr);

            if (type == AstKind::UnaryExpression)
            {
                auto u = std::static_pointer_cast<UnaryExpression>(e);
                return u->op.text == "-" and isInt(u->expr);
            }

            if (type == AstKind::BinaryExpression)
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                const std::string& op = b->op.text;

                // A division may have no answer, and `+` of what this does not know may be a string
                return (op == "+" or op == "-" or op == "*") and isInt(b->leftExpr) and isInt(b->rightExpr);
            }

            return false;
        }

        void Ownership::settleInts()
        {
            ints.clear();

            for (const auto& [name, writes] : intWrites)
            {
                // (a name not declared here is a global: anything may give it anything)
                if (not notInts.count(name) and declared.count(name))
                    ints.insert(name);
            }

            // Each only falls once: what is left is what is an integer whatever the order
            for (bool lost = true; lost; )
            {
                lost = false;

                for (auto it = ints.begin(); it != ints.end(); )
                {
                    const auto& writes = intWrites.at(*it);

                    if (std::all_of(writes.begin(), writes.end(), [&](const ExprPtr& e) { return isInt(e); }))
                        ++it;
                    else
                    {
                        it = ints.erase(it);
                        lost = true;
                    }
                }
            }
        }

        bool Ownership::rowsStay(const std::string& name, std::set<std::string>& seen) const
        {
            if (not seen.insert(name).second)
                return true;

            for (const auto& [holder, lists] : rowOf)
            {
                if (not lists.count(name))
                    continue;

                if (escaped.count(holder) or given.count(holder) or rowsRead.count(holder) or not rowsStay(holder, seen))
                    return false;
            }

            return true;
        }

        std::set<std::string> Ownership::listsBehind(const std::string& name) const
        {
            std::set<std::string> out = {name};
            std::vector<std::string> left = {name};

            while (not left.empty())
            {
                const std::string at = left.back();
                left.pop_back();

                auto it = rowOf.find(at);

                if (it == rowOf.end())
                    continue;

                for (const auto& list : it->second)
                {
                    if (out.insert(list).second)
                        left.push_back(list);
                }
            }

            return out;
        }

        bool Ownership::rowOfOwned(const std::string& name) const
        {
            const std::set<std::string> lists = listsBehind(name);

            for (const auto& list : lists)
            {
                if (list == name and not rowOf.count(name))
                    return false;

                // Every holder on the way is a row, and every list at the end owned down to the last row
                if (rowOf.count(list) ? notRows.count(list) > 0 : not ownedDeep(list))
                    return false;
            }

            return true;
        }

        Ownership Ownership::ofFunction(const std::queue<ExprPtr>& parameters, const StatementPtr& body)
        {
            Ownership o;

            for (const auto& p : listOf(parameters))
            {
                o.otherWrites.insert(p->getName());
                o.maybeTables.insert(p->getName());
                o.notRows.insert(p->getName());
                o.notInts.insert(p->getName());
                o.notPlain.insert(p->getName());
                ++o.declared[p->getName()];
            }

            ownStmt(body, o, false);

            o.settleInts();

            return o;
        }

        void ownExpr(const ExprPtr& e, Ownership& o, bool nested);

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

        // The t of a local given `t[i]`: the row stays t's as long as the local keeps it; "" for anything
        // else, and in a function defined here, which is called who knows when
        std::string rowRoot(const ExprPtr& value, bool nested)
        {
            return not nested and value and value->kind() == AstKind::IndexGet ? rootOf(value) : "";
        }

        // A local is given a value: it holds a row, or what `ownExpr` says of it
        void ownGiven(const std::string& name, const ExprPtr& value, Ownership& o, bool nested)
        {
            const std::string row = rowRoot(value, nested);

            if (row.empty())
            {
                if (not fresh(value))
                    o.given.insert(name);

                o.notRows.insert(name);
            }
            else
                o.rowOf[name].insert(row);

            if (nested)
                o.notInts.insert(name);

            o.wrote(name, value);

            if (row.empty())
                ownExpr(value, o, nested);
            else
                ownObject(value, o, nested);
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
                    else if (args.size() < 2)
                        o.given.insert(rootOf(args[0]));
                    else
                        o.gave(rootOf(args[0]), args.back());
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

                o.gave(rootOf(g->object), g->value);
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

                o.gave(rootOf(g->object), g->value);
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

                o.rebound.insert(a->name.text);

                ownGiven(a->name.text, a->expr, o, nested);
            }
            else if (type == AstKind::PreFixExpression or type == AstKind::PostFixExpression)
            {
                const std::string name = type == AstKind::PreFixExpression ? std::static_pointer_cast<PreFixExpression>(e)->name.text
                                                                           : std::static_pointer_cast<PostFixExpression>(e)->name.text;

                o.otherWrites.insert(name);
                o.rebound.insert(name);

                if (nested)
                    o.notInts.insert(name);
            }
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

                ++o.declared[v->name.text];

                ownGiven(v->name.text, v->expr, o, nested);
            }
            else if (type == AstKind::ReturnStatement)
            {
                auto r = std::static_pointer_cast<ReturnStatement>(s);

                // Returned: it leaves, but the function ends with it
                if (nested or not r->value or (r->value->kind() != AstKind::Var and r->value->kind() != AstKind::IndexGet))
                    ownExpr(r->value, o, nested);
                else if (r->value->kind() == AstKind::IndexGet)
                    ownObject(r->value, o, nested);
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
                o.rebound.insert(f->varName.text);
                o.notInts.insert(f->varName.text);
                o.notPlain.insert(f->varName.text);

                // Iterated, not kept: only its rows are, and the loop's variable holds them
                if (nested or f->iterable->kind() != AstKind::Var)
                {
                    ownExpr(f->iterable, o, nested);
                    o.notRows.insert(f->varName.text);
                }
                else
                    o.rowOf[f->varName.text].insert(varName(f->iterable));

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

        bool RegionCheck::mayWrite(const ExprPtr& list) const
        {
            const std::string root = rootOf(list);

            if (root.empty())
                return false;

            const bool whole = list->kind() == AstKind::Var;
            const Ownership& own = ctx.ownership;

            // Either may be a row of the other, or of the same list
            const std::set<std::string> written = own.listsBehind(root);
            const std::set<std::string> read = own.listsBehind(x);

            if (std::any_of(written.begin(), written.end(), [&](const std::string& name) { return read.count(name) > 0; }))
            {
                // A row of a list that holds all it holds alone is not the list
                return slotOnly and not (whole and root == x) and ctx.ownershipHolds() and own.ownedDeep(x);
            }

            if (beyondWrites)
                return true;

            if (not ctx.ownershipHolds())
                return false;

            return (whole ? own.owned(root) : own.ownedDeep(root)) or own.rowOfOwned(root);
        }

        void RegionCheck::expr(const ExprPtr& e)
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

                // A new field makes a table longer, and any field is a slot
                if (not noFields and not mayWrite(g->object))
                {
                    ok = false;
                    return;
                }

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

        void RegionCheck::stmt(const StatementPtr& s)
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

        ExprPtr Replace::expr(const ExprPtr& e)
        {
            if (not e)
                return e;

            const std::string key = keyOf(e);

            if (not key.empty())
            {
                auto it = hidden.find(key);

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

        void Replace::stmt(const StatementPtr& s)
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
    }
}
