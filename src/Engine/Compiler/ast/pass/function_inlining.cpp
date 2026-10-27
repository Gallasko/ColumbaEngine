#include "stdafx.h"

#include "function_inlining.h"

#include "program_view.h"

#include "Compiler/vm.h"
#include "logger.h"

#include <sstream>

namespace pg
{
    namespace
    {
        const char* DOM = "FunctionInlining";

        // How deep inlined code is inlined again
        constexpr int maxDepth = 4;

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

        // ---- what can be inlined ----------------------------------------------------------------

        // Only operators, constants, variables, field and index reads, table literals and calls
        bool inlinableExpr(const ExprPtr& e, FunctionInliningPass::Inlinable& def, const std::set<std::string>& params)
        {
            if (not e)
                return true;

            const AstKind type = e->kind();

            if (type == AstKind::Atom or type == AstKind::NoneAtom)
                return true;

            if (type == AstKind::Var)
            {
                const std::string name = varName(e);

                if (params.count(name))
                    ++def.uses[name];
                else
                    def.freeNames.insert(name);

                return true;
            }

            if (type == AstKind::BinaryExpression)
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                return inlinableExpr(b->leftExpr, def, params) and inlinableExpr(b->rightExpr, def, params);
            }

            if (type == AstKind::LogicExpression)
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);
                return inlinableExpr(l->leftExpr, def, params) and inlinableExpr(l->rightExpr, def, params);
            }

            if (type == AstKind::UnaryExpression)
                return inlinableExpr(std::static_pointer_cast<UnaryExpression>(e)->expr, def, params);

            if (type == AstKind::CompoundAtom)
                return inlinableExpr(std::static_pointer_cast<CompoundAtom>(e)->expr, def, params);

            if (type == AstKind::Get)
                return inlinableExpr(std::static_pointer_cast<Get>(e)->object, def, params);

            if (type == AstKind::IndexGet)
            {
                auto g = std::static_pointer_cast<IndexGet>(e);
                return inlinableExpr(g->object, def, params) and inlinableExpr(g->index, def, params);
            }

            if (type == AstKind::CallExpression)
            {
                auto c = std::static_pointer_cast<CallExpression>(e);

                if (not inlinableExpr(c->caller, def, params))
                    return false;

                for (const auto& a : listOf(c->args))
                {
                    if (not inlinableExpr(a, def, params))
                        return false;
                }

                return true;
            }

            if (type == AstKind::List)
            {
                for (const auto& entry : listOf(std::static_pointer_cast<List>(e)->entries))
                {
                    // An explicit key is a name, not a variable
                    if (not entry.explicitKey and not inlinableExpr(entry.key, def, params))
                        return false;

                    if (not inlinableExpr(entry.value, def, params))
                        return false;
                }

                return true;
            }

            // Assign, Set, IndexSet, prefix and postfix operators, this, anonymous functions
            return false;
        }

        // Whether an expression may run script code (which may write any global or field) rather
        // than only natives: a call to a function of the program, to a name it assigns anywhere, to a
        // parameter, or to anything but a plain name
        bool callsScript(const ExprPtr& e, const std::set<std::string>& scriptNames, const std::set<std::string>& params)
        {
            if (not e)
                return false;

            const AstKind type = e->kind();

            if (type == AstKind::CallExpression)
            {
                auto c = std::static_pointer_cast<CallExpression>(e);
                const std::string name = varName(c->caller);

                if (name.empty() or scriptNames.count(name) or params.count(name))
                    return true;

                for (const auto& a : listOf(c->args))
                {
                    if (callsScript(a, scriptNames, params))
                        return true;
                }

                return false;
            }

            if (type == AstKind::BinaryExpression)
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                return callsScript(b->leftExpr, scriptNames, params) or callsScript(b->rightExpr, scriptNames, params);
            }

            if (type == AstKind::LogicExpression)
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);
                return callsScript(l->leftExpr, scriptNames, params) or callsScript(l->rightExpr, scriptNames, params);
            }

            if (type == AstKind::UnaryExpression)
                return callsScript(std::static_pointer_cast<UnaryExpression>(e)->expr, scriptNames, params);

            if (type == AstKind::CompoundAtom)
                return callsScript(std::static_pointer_cast<CompoundAtom>(e)->expr, scriptNames, params);

            if (type == AstKind::Get)
                return callsScript(std::static_pointer_cast<Get>(e)->object, scriptNames, params);

            if (type == AstKind::IndexGet)
            {
                auto g = std::static_pointer_cast<IndexGet>(e);
                return callsScript(g->object, scriptNames, params) or callsScript(g->index, scriptNames, params);
            }

            if (type == AstKind::List)
            {
                for (const auto& entry : listOf(std::static_pointer_cast<List>(e)->entries))
                {
                    if (callsScript(entry.value, scriptNames, params) or (not entry.explicitKey and callsScript(entry.key, scriptNames, params)))
                        return true;
                }
            }

            return false;
        }

        // ---- copying an expression, its parameters replaced ------------------------------------

        ExprPtr copyOf(const ExprPtr& e, const std::map<std::string, ExprPtr>* args)
        {
            if (not e)
                return nullptr;

            const AstKind type = e->kind();

            if (type == AstKind::Atom)
                return std::make_shared<Atom>(std::static_pointer_cast<Atom>(e)->value);

            if (type == AstKind::NoneAtom)
                return std::make_shared<NoneAtom>();

            if (type == AstKind::Var)
            {
                auto v = std::static_pointer_cast<Var>(e);

                if (args)
                {
                    auto it = args->find(v->name.text);

                    // The argument, copied as it is (its own names are the caller's)
                    if (it != args->end())
                        return copyOf(it->second, nullptr);
                }

                return std::make_shared<Var>(v->name);
            }

            if (type == AstKind::BinaryExpression)
            {
                auto b = std::static_pointer_cast<BinaryExpression>(e);
                return std::make_shared<BinaryExpression>(copyOf(b->leftExpr, args), b->op, copyOf(b->rightExpr, args));
            }

            if (type == AstKind::LogicExpression)
            {
                auto l = std::static_pointer_cast<LogicExpression>(e);
                return std::make_shared<LogicExpression>(copyOf(l->leftExpr, args), l->op, copyOf(l->rightExpr, args));
            }

            if (type == AstKind::UnaryExpression)
            {
                auto u = std::static_pointer_cast<UnaryExpression>(e);
                return std::make_shared<UnaryExpression>(copyOf(u->expr, args), u->op);
            }

            if (type == AstKind::CompoundAtom)
                return std::make_shared<CompoundAtom>(copyOf(std::static_pointer_cast<CompoundAtom>(e)->expr, args));

            if (type == AstKind::Get)
            {
                auto g = std::static_pointer_cast<Get>(e);
                return std::make_shared<Get>(copyOf(g->object, args), g->name);
            }

            if (type == AstKind::IndexGet)
            {
                auto g = std::static_pointer_cast<IndexGet>(e);
                return std::make_shared<IndexGet>(copyOf(g->object, args), copyOf(g->index, args), g->bracket);
            }

            if (type == AstKind::CallExpression)
            {
                auto c = std::static_pointer_cast<CallExpression>(e);
                std::vector<ExprPtr> callArgs;

                for (const auto& a : listOf(c->args))
                    callArgs.push_back(copyOf(a, args));

                return std::make_shared<CallExpression>(copyOf(c->caller, args), c->paren, queueOf(callArgs));
            }

            if (type == AstKind::List)
            {
                auto l = std::static_pointer_cast<List>(e);
                std::vector<ListElement> entries;

                for (auto entry : listOf(l->entries))
                {
                    ListElement copy;
                    copy.explicitKey = entry.explicitKey;
                    copy.key = entry.explicitKey ? copyOf(entry.key, nullptr) : copyOf(entry.key, args);
                    copy.value = copyOf(entry.value, args);
                    entries.push_back(copy);
                }

                auto out = std::make_shared<List>(copyOf(l->self, args), l->squareBracket, queueOf(entries));
                out->braceForm = l->braceForm;
                out->hasExplicitKeys = l->hasExplicitKeys;

                return out;
            }

            return e;
        }

        // ---- the rewrite ---------------------------------------------------------------------------

        // The names a function body (or a script's nested blocks) declares: its locals
        void declsOf(const StatementPtr& s, std::set<std::string>& out, bool topLevel)
        {
            if (not s)
                return;

            const AstKind type = s->kind();

            if (type == AstKind::VariableStatement)
            {
                // (a top-level `var` is a global, not a local)
                if (not topLevel)
                    out.insert(std::static_pointer_cast<VariableStatement>(s)->name.text);
            }
            else if (type == AstKind::FunctionStatement)
            {
                if (not topLevel)
                    out.insert(std::static_pointer_cast<FunctionStatement>(s)->name.text);
            }
            else if (type == AstKind::BlockStatement)
            {
                for (const auto& b : listOf(std::static_pointer_cast<BlockStatement>(s)->statements))
                    declsOf(b, out, false);
            }
            else if (type == AstKind::IfStatement)
            {
                auto i = std::static_pointer_cast<IfStatement>(s);
                declsOf(i->thenBranch, out, false);
                declsOf(i->elseBranch, out, false);
            }
            else if (type == AstKind::WhileStatement)
                declsOf(std::static_pointer_cast<WhileStatement>(s)->body, out, false);
            else if (type == AstKind::ForStatement)
            {
                auto f = std::static_pointer_cast<ForStatement>(s);
                declsOf(f->initializer, out, false);
                declsOf(f->body, out, false);
            }
            else if (type == AstKind::ForInStatement)
            {
                auto f = std::static_pointer_cast<ForInStatement>(s);
                out.insert(f->varName.text);
                declsOf(f->body, out, false);
            }
        }

        struct Rewriter
        {
            const std::map<std::string, FunctionInliningPass::Inlinable>& inlinable;
            std::vector<std::set<std::string>> scopes;
            long inlined = 0;

            bool shadowed(const std::string& name) const
            {
                for (const auto& s : scopes)
                {
                    if (s.count(name))
                        return true;
                }

                return false;
            }

            // A chain of field reads on a variable: `req.stat`, `a.b.c`
            static bool getChain(const ExprPtr& e)
            {
                if (not e)
                    return false;

                if (e->kind() == AstKind::Var)
                    return true;

                if (e->kind() == AstKind::Get)
                    return getChain(std::static_pointer_cast<Get>(e)->object);

                return false;
            }

            static std::string rootName(const ExprPtr& e)
            {
                if (e->kind() == AstKind::Var)
                    return varName(e);

                return rootName(std::static_pointer_cast<Get>(e)->object);
            }

            ExprPtr tryInline(const std::shared_ptr<CallExpression>& call, int depth)
            {
                const std::string name = varName(call->caller);

                if (name.empty() or depth >= maxDepth or shadowed(name))
                    return nullptr;

                auto it = inlinable.find(name);

                if (it == inlinable.end())
                    return nullptr;

                const auto& def = it->second;
                auto args = listOf(call->args);

                if (args.size() != def.params.size())
                    return nullptr;

                for (const auto& free : def.freeNames)
                {
                    if (shadowed(free))
                        return nullptr;
                }

                std::map<std::string, ExprPtr> byParam;

                for (size_t i = 0; i < args.size(); ++i)
                {
                    const ExprPtr& arg = args[i];
                    const AstKind type = arg->kind();
                    auto used = def.uses.find(def.params[i]);
                    const int uses = used == def.uses.end() ? 0 : used->second;

                    if (type == AstKind::Atom or type == AstKind::NoneAtom)
                    {
                        // Nothing to evaluate twice, nothing to miss
                    }
                    else if (type == AstKind::Var)
                    {
                        // Script code the expression runs before reading it could write it (a global,
                        // or a local through a closure)
                        if (def.callsScript)
                            return nullptr;
                    }
                    else if (getChain(arg))
                    {
                        // Read once, where the expression reads it
                        if (uses != 1 or def.callsScript)
                            return nullptr;
                    }
                    else
                    {
                        return nullptr;
                    }

                    byParam[def.params[i]] = arg;
                }

                ++inlined;

                return copyOf(def.body, &byParam);
            }

            ExprPtr expr(const ExprPtr& e, int depth)
            {
                if (not e)
                    return e;

                const AstKind type = e->kind();

                if (type == AstKind::BinaryExpression)
                {
                    auto b = std::static_pointer_cast<BinaryExpression>(e);
                    b->leftExpr = expr(b->leftExpr, depth);
                    b->rightExpr = expr(b->rightExpr, depth);
                }
                else if (type == AstKind::LogicExpression)
                {
                    auto l = std::static_pointer_cast<LogicExpression>(e);
                    l->leftExpr = expr(l->leftExpr, depth);
                    l->rightExpr = expr(l->rightExpr, depth);
                }
                else if (type == AstKind::UnaryExpression)
                {
                    auto u = std::static_pointer_cast<UnaryExpression>(e);
                    u->expr = expr(u->expr, depth);
                }
                else if (type == AstKind::CompoundAtom)
                {
                    auto c = std::static_pointer_cast<CompoundAtom>(e);
                    c->expr = expr(c->expr, depth);
                }
                else if (type == AstKind::Assign)
                {
                    auto a = std::static_pointer_cast<Assign>(e);
                    a->expr = expr(a->expr, depth);
                }
                else if (type == AstKind::Get)
                {
                    auto g = std::static_pointer_cast<Get>(e);
                    g->object = expr(g->object, depth);
                }
                else if (type == AstKind::Set)
                {
                    auto s = std::static_pointer_cast<Set>(e);
                    s->object = expr(s->object, depth);
                    s->value = expr(s->value, depth);
                }
                else if (type == AstKind::IndexGet)
                {
                    auto g = std::static_pointer_cast<IndexGet>(e);
                    g->object = expr(g->object, depth);
                    g->index = expr(g->index, depth);
                }
                else if (type == AstKind::IndexSet)
                {
                    auto g = std::static_pointer_cast<IndexSet>(e);
                    g->object = expr(g->object, depth);
                    g->index = expr(g->index, depth);
                    g->value = expr(g->value, depth);
                }
                else if (type == AstKind::List)
                {
                    auto l = std::static_pointer_cast<List>(e);
                    std::vector<ListElement> entries;

                    for (auto entry : listOf(l->entries))
                    {
                        if (not entry.explicitKey)
                            entry.key = expr(entry.key, depth);

                        entry.value = expr(entry.value, depth);
                        entries.push_back(entry);
                    }

                    l->entries = queueOf(entries);
                }
                else if (type == AstKind::AnonymousFunction)
                {
                    auto f = std::static_pointer_cast<AnonymousFunction>(e);
                    std::set<std::string> locals;

                    for (const auto& p : listOf(f->parameters))
                        locals.insert(p->getName());

                    declsOf(f->body, locals, false);
                    scopes.push_back(locals);
                    stmt(f->body);
                    scopes.pop_back();
                }
                else if (type == AstKind::CallExpression)
                {
                    auto c = std::static_pointer_cast<CallExpression>(e);
                    c->caller = expr(c->caller, depth);

                    std::vector<ExprPtr> args;

                    for (const auto& a : listOf(c->args))
                        args.push_back(expr(a, depth));

                    c->args = queueOf(args);

                    if (auto folded = tryInline(c, depth))
                        return expr(folded, depth + 1);
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
                    x->expr = expr(x->expr, 0);
                }
                else if (type == AstKind::VariableStatement)
                {
                    auto v = std::static_pointer_cast<VariableStatement>(s);
                    v->expr = expr(v->expr, 0);
                }
                else if (type == AstKind::ReturnStatement)
                {
                    auto r = std::static_pointer_cast<ReturnStatement>(s);
                    r->value = expr(r->value, 0);
                }
                else if (type == AstKind::DPrintStatement)
                {
                    auto d = std::static_pointer_cast<DPrintStatement>(s);
                    d->expr = expr(d->expr, 0);
                }
                else if (type == AstKind::IfStatement)
                {
                    auto i = std::static_pointer_cast<IfStatement>(s);
                    i->condition = expr(i->condition, 0);
                    stmt(i->thenBranch);
                    stmt(i->elseBranch);
                }
                else if (type == AstKind::WhileStatement)
                {
                    auto w = std::static_pointer_cast<WhileStatement>(s);
                    w->condition = expr(w->condition, 0);
                    stmt(w->body);
                }
                else if (type == AstKind::ForStatement)
                {
                    auto f = std::static_pointer_cast<ForStatement>(s);
                    stmt(f->initializer);
                    f->condition = expr(f->condition, 0);
                    f->increment = expr(f->increment, 0);
                    stmt(f->body);
                }
                else if (type == AstKind::ForInStatement)
                {
                    auto f = std::static_pointer_cast<ForInStatement>(s);
                    f->iterable = expr(f->iterable, 0);
                    stmt(f->body);
                }
                else if (type == AstKind::BlockStatement)
                {
                    for (const auto& b : listOf(std::static_pointer_cast<BlockStatement>(s)->statements))
                        stmt(b);
                }
                else if (type == AstKind::FunctionStatement)
                {
                    auto f = std::static_pointer_cast<FunctionStatement>(s);
                    std::set<std::string> locals;

                    for (const auto& p : listOf(f->parameters))
                        locals.insert(p->getName());

                    declsOf(f->body, locals, false);
                    scopes.push_back(locals);
                    stmt(f->body);
                    scopes.pop_back();
                }
                else if (type == AstKind::ClassStatement)
                {
                    for (const auto& m : listOf(std::static_pointer_cast<ClassStatement>(s)->methods))
                        stmt(m);
                }
            }
        };
    }

    bool FunctionInliningPass::runPass(VM* vm, std::queue<StatementPtr>& statements)
    {
        // The whole program: the script compiled (outermost) and every module it imports. A module
        // compiled while it is (deeper) belongs to that program
        if (not vm or vm->astCompileDepth <= 1)
        {
            inlinable.clear();

            const ast::ProgramView program = ast::readProgram(vm, statements);
            std::set<std::string> scriptNames = program.scriptNames();

            if (program.complete)
            {
                for (const auto& [name, count] : program.defined)
                {
                    if (count != 1 or program.poisoned.count(name))
                        continue;

                    auto f = program.functions.at(name);

                    if (not f->body or f->body->kind() != AstKind::BlockStatement)
                        continue;

                    auto body = listOf(std::static_pointer_cast<BlockStatement>(f->body)->statements);

                    if (body.size() != 1 or not body[0] or body[0]->kind() != AstKind::ReturnStatement)
                        continue;

                    auto ret = std::static_pointer_cast<ReturnStatement>(body[0]);

                    if (not ret->value)
                        continue;

                    Inlinable def;
                    std::set<std::string> params;
                    bool ok = true;

                    for (const auto& p : listOf(f->parameters))
                    {
                        if (p->kind() != AstKind::Var or params.count(p->getName()))
                            ok = false;

                        def.params.push_back(p->getName());
                        params.insert(p->getName());
                    }

                    if (not ok or not inlinableExpr(ret->value, def, params) or def.freeNames.count(name))
                        continue;

                    // A copy: the rewrite below changes the function's own statements in place
                    def.body = copyOf(ret->value, nullptr);
                    def.from = f;
                    inlinable[name] = def;
                }
            }

            // What they call may be script code that writes: unless it is one of them, as none of
            // them writes anything. Whichever calls one that may write may write too, until no more do
            std::set<std::string> mayWrite = scriptNames;

            for (const auto& [name, def] : inlinable)
                mayWrite.erase(name);

            bool grew = true;

            while (grew)
            {
                grew = false;

                for (auto& [name, def] : inlinable)
                {
                    if (def.callsScript)
                        continue;

                    std::set<std::string> params(def.params.begin(), def.params.end());

                    if (callsScript(def.body, mayWrite, params))
                    {
                        def.callsScript = true;
                        mayWrite.insert(name);
                        grew = true;
                    }
                }
            }

            LOG_INFO(DOM, "Inlinable functions: " << inlinable.size() << (program.complete ? "" : " (a module could not be read: none)"));
        }

        if (inlinable.empty())
            return false;

        Rewriter rewriter{inlinable, {}, 0};

        // The script's own nested blocks declare locals too
        std::set<std::string> topLocals;

        for (const auto& s : listOf(statements))
        {
            if (s and s->kind() != AstKind::FunctionStatement and s->kind() != AstKind::ClassStatement)
                declsOf(s, topLocals, true);
        }

        rewriter.scopes.push_back(topLocals);

        std::vector<StatementPtr> out;

        for (const auto& s : listOf(statements))
        {
            rewriter.stmt(s);
            out.push_back(s);
        }

        statements = queueOf(out);

        LOG_INFO(DOM, "Inlined " << rewriter.inlined << " calls");

        return rewriter.inlined > 0;
    }

    void FunctionInliningPass::namesBrought(std::set<std::string>& names) const
    {
        std::vector<std::string> brought;

        for (const auto& name : names)
        {
            const auto it = inlinable.find(name);

            if (it != inlinable.end())
                brought.insert(brought.end(), it->second.freeNames.begin(), it->second.freeNames.end());
        }

        names.insert(brought.begin(), brought.end());
    }

    std::string FunctionInliningPass::factsAbout(const std::set<std::string>& names, std::vector<std::shared_ptr<const void>>& keep) const
    {
        // A name inlines the function it is, and the same function always inlines the same: its
        // address says which (kept, so no other node takes it), with what the program says of its calls
        std::ostringstream facts;

        for (const auto& name : names)
        {
            const auto it = inlinable.find(name);

            if (it == inlinable.end())
                continue;

            facts << name << '=' << static_cast<const void*>(it->second.from.get()) << (it->second.callsScript ? 'w' : 'r') << ';';
            keep.push_back(it->second.from);
        }

        return facts.str();
    }
}
