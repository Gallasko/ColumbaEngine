#include "stdafx.h"

#include "ssa.h"

#include "Interpreter/statement.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>

namespace pg
{
    namespace ast
    {
        namespace analysis
        {
            NativeKind nativeKind(const std::string& name)
            {
                static const std::unordered_map<std::string, NativeKind> known = {
                    // algorithm
                    {"contain", NativeKind::Fresh}, {"lookup", NativeKind::Lookup}, {"pick", NativeKind::Lookup},
                    {"findBy", NativeKind::Entry}, {"indexBy", NativeKind::IndexBy}, {"copy", NativeKind::Copy},
                    {"merge", NativeKind::Merge}, {"toInt", NativeKind::Fresh}, {"toFloat", NativeKind::Fresh},
                    {"typeOf", NativeKind::Fresh}, {"len", NativeKind::Fresh}, {"push", NativeKind::Push},
                    {"pop", NativeKind::Take}, {"insert", NativeKind::Insert}, {"removeAt", NativeKind::Take},
                    // string
                    {"toString", NativeKind::Fresh}, {"strlen", NativeKind::Fresh}, {"split", NativeKind::Fresh},
                    {"splitLines", NativeKind::Fresh}, {"startsWith", NativeKind::Fresh}, {"endsWith", NativeKind::Fresh},
                    {"contains", NativeKind::Fresh}, {"charAt", NativeKind::Fresh}, {"substring", NativeKind::Fresh},
                    {"replace", NativeKind::Fresh}, {"join", NativeKind::Fresh}, {"capitalize", NativeKind::Fresh},
                    {"lowerize", NativeKind::Fresh}, {"toLowerCase", NativeKind::Fresh}, {"toUpperCase", NativeKind::Fresh},
                    {"trim", NativeKind::Fresh}, {"indexOf", NativeKind::Fresh},
                    // math
                    {"sqrt", NativeKind::Fresh}, {"abs", NativeKind::Fresh}, {"pow", NativeKind::Fresh}, {"floor", NativeKind::Fresh},
                    {"ceil", NativeKind::Fresh}, {"sin", NativeKind::Fresh}, {"cos", NativeKind::Fresh}, {"tan", NativeKind::Fresh},
                    {"asin", NativeKind::Fresh}, {"acos", NativeKind::Fresh}, {"atan", NativeKind::Fresh},
                };

                auto it = known.find(name);
                return it == known.end() ? NativeKind::None : it->second;
            }

            std::vector<ValueId> BodyIR::leaves(ValueId v) const
            {
                std::vector<bool> seen(nodes.size(), false);
                std::vector<ValueId> todo{v};
                std::vector<ValueId> out;

                while (not todo.empty())
                {
                    const ValueId at = todo.back();
                    todo.pop_back();

                    if (at >= nodes.size() or seen[at])
                        continue;

                    seen[at] = true;

                    if (nodes[at].op == Op::Phi)
                        todo.insert(todo.end(), nodes[at].in.begin(), nodes[at].in.end());
                    else
                        out.push_back(at);
                }

                std::sort(out.begin(), out.end());
                return out;
            }

            std::string BodyIR::dump() const
            {
                std::ostringstream out;

                auto keyText = [this](KeyId k) -> std::string
                {
                    auto it = keyNames.find(k);

                    if (it != keyNames.end())
                        return it->second;

                    return k == Locations::AnyKey ? "*" : "#" + std::to_string(k);
                };

                auto operands = [&out](const Node& n)
                {
                    for (auto v : n.in)
                        out << " %" << v;
                };

                for (size_t i = 0; i < nodes.size(); ++i)
                {
                    const Node& n = nodes[i];
                    out << '%' << i << " = ";

                    switch (n.op)
                    {
                        case Op::Const: out << "const " << n.name; break;
                        case Op::Param: out << "param " << n.param; break;
                        case Op::Global: out << "global " << n.name; break;
                        case Op::Literal: out << "literal " << n.name; break;
                        case Op::Field: out << "field %" << (n.in.empty() ? 0 : n.in[0]) << ' ' << keyText(n.key); break;
                        case Op::Phi: out << "phi"; operands(n); break;
                        case Op::Call: out << "call " << (n.name.empty() ? "?" : n.name); operands(n); break;
                        case Op::Native: out << "native " << n.name; operands(n); break;
                        case Op::Pure: out << "pure"; operands(n); break;
                        case Op::Unknown: out << "unknown"; break;
                    }

                    out << '\n';
                }

                for (const auto& s : stores)
                    out << "store %" << s.base << ' ' << keyText(s.key) << " %" << s.value << '\n';

                for (const auto& [name, v] : defines)
                    out << "define " << name << " %" << v << '\n';

                for (const auto& [name, v] : rebinds)
                    out << "rebind " << name << " %" << v << '\n';

                for (auto v : returns)
                    out << "return %" << v << '\n';

                if (unknown)
                    out << "unknown\n";

                return out.str();
            }

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

                void assignedIn(const StatementPtr& s, std::set<std::string>& out);

                // The names an expression assigns (a closure's body is not looked in)
                void assignedIn(const ExprPtr& e, std::set<std::string>& out)
                {
                    if (not e)
                        return;

                    switch (e->kind())
                    {
                        case AstKind::Assign:
                        {
                            auto a = std::static_pointer_cast<Assign>(e);
                            out.insert(a->name.text);
                            assignedIn(a->expr, out);
                            return;
                        }

                        case AstKind::PreFixExpression:
                            out.insert(std::static_pointer_cast<PreFixExpression>(e)->name.text);
                            return;

                        case AstKind::PostFixExpression:
                            out.insert(std::static_pointer_cast<PostFixExpression>(e)->name.text);
                            return;

                        case AstKind::BinaryExpression:
                        {
                            auto b = std::static_pointer_cast<BinaryExpression>(e);
                            assignedIn(b->leftExpr, out);
                            assignedIn(b->rightExpr, out);
                            return;
                        }

                        case AstKind::LogicExpression:
                        {
                            auto l = std::static_pointer_cast<LogicExpression>(e);
                            assignedIn(l->leftExpr, out);
                            assignedIn(l->rightExpr, out);
                            return;
                        }

                        case AstKind::UnaryExpression:
                            assignedIn(std::static_pointer_cast<UnaryExpression>(e)->expr, out);
                            return;

                        case AstKind::CompoundAtom:
                            assignedIn(std::static_pointer_cast<CompoundAtom>(e)->expr, out);
                            return;

                        case AstKind::CallExpression:
                        {
                            auto c = std::static_pointer_cast<CallExpression>(e);
                            assignedIn(c->caller, out);

                            for (const auto& a : listOf(c->args))
                                assignedIn(a, out);

                            return;
                        }

                        case AstKind::Get:
                            assignedIn(std::static_pointer_cast<Get>(e)->object, out);
                            return;

                        case AstKind::Set:
                        {
                            auto s = std::static_pointer_cast<Set>(e);
                            assignedIn(s->object, out);
                            assignedIn(s->value, out);
                            return;
                        }

                        case AstKind::IndexGet:
                        {
                            auto g = std::static_pointer_cast<IndexGet>(e);
                            assignedIn(g->object, out);
                            assignedIn(g->index, out);
                            return;
                        }

                        case AstKind::IndexSet:
                        {
                            auto s = std::static_pointer_cast<IndexSet>(e);
                            assignedIn(s->object, out);
                            assignedIn(s->index, out);
                            assignedIn(s->value, out);
                            return;
                        }

                        case AstKind::List:
                        {
                            for (const auto& entry : listOf(std::static_pointer_cast<List>(e)->entries))
                            {
                                assignedIn(entry.key, out);
                                assignedIn(entry.value, out);
                            }

                            return;
                        }

                        default:
                            return;
                    }
                }

                // The names a statement assigns (a nested fun's or a class's body is not looked in)
                void assignedIn(const StatementPtr& s, std::set<std::string>& out)
                {
                    if (not s)
                        return;

                    switch (s->kind())
                    {
                        case AstKind::ExpressionStatement:
                            assignedIn(std::static_pointer_cast<ExpressionStatement>(s)->expr, out);
                            return;

                        case AstKind::VariableStatement:
                        {
                            auto v = std::static_pointer_cast<VariableStatement>(s);
                            assignedIn(v->expr, out);

                            for (const auto& fill : v->keptFills)
                                assignedIn(fill, out);

                            return;
                        }

                        case AstKind::BlockStatement:
                        {
                            for (const auto& b : listOf(std::static_pointer_cast<BlockStatement>(s)->statements))
                                assignedIn(b, out);

                            return;
                        }

                        case AstKind::IfStatement:
                        {
                            auto i = std::static_pointer_cast<IfStatement>(s);
                            assignedIn(i->condition, out);
                            assignedIn(i->thenBranch, out);
                            assignedIn(i->elseBranch, out);
                            return;
                        }

                        case AstKind::WhileStatement:
                        {
                            auto w = std::static_pointer_cast<WhileStatement>(s);
                            assignedIn(w->condition, out);
                            assignedIn(w->body, out);
                            return;
                        }

                        case AstKind::ForStatement:
                        {
                            auto f = std::static_pointer_cast<ForStatement>(s);
                            assignedIn(f->initializer, out);
                            assignedIn(f->condition, out);
                            assignedIn(f->increment, out);
                            assignedIn(f->body, out);
                            return;
                        }

                        case AstKind::ForInStatement:
                        {
                            auto f = std::static_pointer_cast<ForInStatement>(s);
                            assignedIn(f->iterable, out);
                            assignedIn(f->body, out);
                            return;
                        }

                        case AstKind::ReturnStatement:
                            assignedIn(std::static_pointer_cast<ReturnStatement>(s)->value, out);
                            return;

                        case AstKind::DPrintStatement:
                            assignedIn(std::static_pointer_cast<DPrintStatement>(s)->expr, out);
                            return;

                        default:
                            return;
                    }
                }

                // What the locals hold at a point of the body
                struct Env
                {
                    std::vector<std::map<std::string, ValueId>> scopes;  // innermost last

                    // The tables of the body followed field by field: what each field holds
                    std::map<ValueId, std::map<KeyId, ValueId>> fields;

                    bool live = true;  // false after a return, a break or a continue
                };

                // An open loop: where its breaks and continues leave from
                struct Loop
                {
                    size_t depth;
                    std::vector<Env> breaks;
                    std::vector<Env> continues;
                };

                class Builder
                {
                public:
                    Builder(Locations& locs, const std::function<bool(const std::string&)>& isNative) : locs(locs), isNative(isNative)
                    {
                        ir.keyNames[Locations::AnyKey] = "*";
                    }

                    BodyIR function(const FunctionStatement& f)
                    {
                        env.scopes.emplace_back();

                        uint32_t i = 0;

                        for (const auto& p : listOf(f.parameters))
                        {
                            Node n;
                            n.op = Op::Param;
                            n.param = i++;
                            n.origin = p.get();

                            const ValueId v = add(std::move(n));

                            if (p)
                                ir.valueOf[p.get()] = v;

                            if (p and p->kind() == AstKind::Var)
                                env.scopes.back()[std::static_pointer_cast<Var>(p)->name.text] = v;
                        }

                        stmt(f.body);
                        return std::move(ir);
                    }

                    BodyIR topLevel(const StatementPtr& s)
                    {
                        stmt(s);
                        return std::move(ir);
                    }

                private:
                    Locations& locs;
                    const std::function<bool(const std::string&)>& isNative;

                    BodyIR ir;
                    Env env;
                    std::vector<Loop> loops;

                    // The tables no longer followed field by field
                    std::set<ValueId> escaped;

                    // False once something not followed is in the body: no table is followed after it
                    bool following = true;

                    ValueId add(Node n)
                    {
                        ir.nodes.push_back(std::move(n));
                        return static_cast<ValueId>(ir.nodes.size() - 1);
                    }

                    ValueId make(Op op, std::vector<ValueId> in, const void* origin, const std::string& name = "")
                    {
                        Node n;
                        n.op = op;
                        n.in = std::move(in);
                        n.origin = origin;
                        n.name = name;
                        return add(std::move(n));
                    }

                    ValueId constant(const std::string& text, const void* origin) { return make(Op::Const, {}, origin, text); }

                    KeyId nameKey(const std::string& name)
                    {
                        const KeyId k = locs.key(name);
                        ir.keyNames[k] = name;
                        return k;
                    }

                    bool isNameKey(KeyId k) const { return k != Locations::AnyKey and not locs.isParamKey(k); }

                    // The key a value names: a string constant, a parameter, else any
                    KeyId keyOf(ValueId v)
                    {
                        const Node& n = ir.nodes[v];

                        if (n.op == Op::Const and n.name.size() >= 2 and n.name.front() == '"')
                            return nameKey(n.name.substr(1, n.name.size() - 2));

                        if (n.op == Op::Param)
                        {
                            const KeyId k = locs.paramKey(n.param);
                            ir.keyNames[k] = "[#" + std::to_string(n.param) + "]";
                            return k;
                        }

                        return Locations::AnyKey;
                    }

                    // A table used otherwise than read or written with a constant key: no longer followed
                    void use(ValueId v)
                    {
                        if (ir.nodes[v].op == Op::Literal)
                        {
                            escaped.insert(v);
                            env.fields.erase(v);
                        }
                    }

                    void untrackAll()
                    {
                        for (const auto& t : env.fields)
                            escaped.insert(t.first);

                        env.fields.clear();
                    }

                    void markUnknown()
                    {
                        ir.unknown = true;
                        following = false;
                        untrackAll();
                    }

                    bool followed(ValueId t) const { return following and not escaped.count(t) and env.fields.count(t); }

                    ValueId phi(const std::vector<ValueId>& values, const void* origin)
                    {
                        std::vector<ValueId> in;

                        for (auto v : values)
                        {
                            if (std::find(in.begin(), in.end(), v) == in.end())
                                in.push_back(v);
                        }

                        if (in.empty())
                            return constant("none", origin);

                        if (in.size() == 1)
                            return in[0];

                        for (auto v : in)
                            use(v);

                        return make(Op::Phi, std::move(in), origin);
                    }

                    ValueId* local(const std::string& name)
                    {
                        for (auto it = env.scopes.rbegin(); it != env.scopes.rend(); ++it)
                        {
                            auto found = it->find(name);

                            if (found != it->end())
                                return &found->second;
                        }

                        return nullptr;
                    }

                    // A local or a global read
                    ValueId read(const std::string& name, const void* origin)
                    {
                        if (auto v = local(name))
                            return *v;

                        return make(Op::Global, {}, origin, name);
                    }

                    void assign(const std::string& name, ValueId v)
                    {
                        if (auto at = local(name))
                        {
                            *at = v;
                            return;
                        }

                        use(v);
                        ir.rebinds.emplace_back(name, v);
                    }

                    ValueId readField(ValueId base, KeyId key, const void* origin)
                    {
                        if (isNameKey(key))
                        {
                            if (followed(base))
                            {
                                const auto& f = env.fields[base];
                                auto it = f.find(key);

                                if (it != f.end())
                                    return it->second;
                            }
                        }
                        else
                            use(base);

                        Node n;
                        n.op = Op::Field;
                        n.in = {base};
                        n.key = key;
                        n.origin = origin;
                        return add(std::move(n));
                    }

                    void writeField(ValueId base, KeyId key, ValueId value)
                    {
                        use(value);

                        if (not isNameKey(key))
                            use(base);

                        ir.stores.push_back(Store{base, key, value});

                        if (isNameKey(key) and followed(base))
                            env.fields[base][key] = value;
                    }

                    // The envs that reach one point, joined: a phi for each local they disagree on
                    Env join(const std::vector<Env>& envs, size_t depth)
                    {
                        std::vector<const Env*> live;

                        for (const auto& e : envs)
                        {
                            if (e.live)
                                live.push_back(&e);
                        }

                        if (live.empty())
                        {
                            Env out = envs.empty() ? env : envs[0];
                            out.scopes.resize(std::min(depth, out.scopes.size()));
                            out.live = false;
                            return out;
                        }

                        Env out = *live[0];
                        out.scopes.resize(std::min(depth, out.scopes.size()));

                        for (size_t s = 0; s < out.scopes.size(); ++s)
                        {
                            for (auto& [name, v] : out.scopes[s])
                            {
                                std::vector<ValueId> values;

                                for (const auto* e : live)
                                {
                                    if (s >= e->scopes.size())
                                        continue;

                                    auto it = e->scopes[s].find(name);

                                    if (it != e->scopes[s].end())
                                        values.push_back(it->second);
                                }

                                v = phi(values, nullptr);
                            }
                        }

                        // A table is still followed when every env follows it; a field kept when every env knows it
                        std::map<ValueId, std::map<KeyId, ValueId>> fields;

                        for (const auto* e : live)
                        {
                            for (const auto& [t, f] : e->fields)
                            {
                                bool everywhere = not escaped.count(t);

                                for (const auto* other : live)
                                    everywhere = everywhere and other->fields.count(t);

                                if (not everywhere)
                                {
                                    escaped.insert(t);
                                    continue;
                                }

                                if (e != live[0])
                                    continue;

                                auto& joined = fields[t];

                                for (const auto& [k, v] : f)
                                {
                                    std::vector<ValueId> values;

                                    for (const auto* other : live)
                                    {
                                        const auto& of = other->fields.at(t);
                                        auto it = of.find(k);

                                        if (it != of.end())
                                            values.push_back(it->second);
                                    }

                                    if (values.size() == live.size())
                                        joined[k] = phi(values, nullptr);
                                }
                            }
                        }

                        // A phi above may have ended a table
                        for (auto it = fields.begin(); it != fields.end();)
                            it = escaped.count(it->first) ? fields.erase(it) : std::next(it);

                        out.fields = std::move(fields);
                        return out;
                    }

                    // A loop: phis at its header for the locals it assigns, fed by the end of the body and
                    // each continue; after it, its exit and each break joined
                    void loop(const ExprPtr& condition, const StatementPtr& body, const ExprPtr& increment)
                    {
                        // A table followed until now may be written by a later turn before an earlier read
                        untrackAll();

                        std::set<std::string> assigned;
                        assignedIn(condition, assigned);
                        assignedIn(body, assigned);
                        assignedIn(increment, assigned);

                        const size_t depth = env.scopes.size();

                        struct Header
                        {
                            size_t scope;
                            std::string name;
                            ValueId phi;
                        };

                        std::vector<Header> headers;

                        for (const auto& name : assigned)
                        {
                            for (size_t s = depth; s-- > 0;)
                            {
                                auto it = env.scopes[s].find(name);

                                if (it == env.scopes[s].end())
                                    continue;

                                use(it->second);

                                const ValueId p = make(Op::Phi, {it->second}, nullptr);
                                it->second = p;
                                headers.push_back({s, name, p});
                                break;
                            }
                        }

                        if (condition)
                            expr(condition);

                        Env exit = env;

                        loops.push_back(Loop{depth, {}, {}});
                        stmt(body);
                        Loop done = std::move(loops.back());
                        loops.pop_back();

                        std::vector<Env> back;

                        if (increment)
                        {
                            // A continue goes through the increment
                            std::vector<Env> ends{env};
                            ends.insert(ends.end(), done.continues.begin(), done.continues.end());
                            env = join(ends, depth);
                            expr(increment);
                            back.push_back(env);
                        }
                        else
                        {
                            back.push_back(env);
                            back.insert(back.end(), done.continues.begin(), done.continues.end());
                        }

                        for (const auto& h : headers)
                        {
                            for (const auto& b : back)
                            {
                                if (not b.live or h.scope >= b.scopes.size())
                                    continue;

                                auto it = b.scopes[h.scope].find(h.name);

                                if (it == b.scopes[h.scope].end())
                                    continue;

                                auto& in = ir.nodes[h.phi].in;

                                if (std::find(in.begin(), in.end(), it->second) != in.end() or it->second == h.phi)
                                    continue;

                                use(it->second);
                                ir.nodes[h.phi].in.push_back(it->second);
                            }
                        }

                        std::vector<Env> outs{exit};
                        outs.insert(outs.end(), done.breaks.begin(), done.breaks.end());
                        env = join(outs, depth);
                    }

                    ValueId literal(const std::shared_ptr<List>& list)
                    {
                        Node n;
                        n.op = Op::Literal;
                        n.site = locs.site(list.get());
                        n.name = locs.text(n.site);
                        n.origin = list.get();

                        const ValueId made = add(std::move(n));

                        // Followed when every entry has a name for a key, outside any loop
                        bool named = following and loops.empty();
                        std::map<KeyId, ValueId> fields;

                        for (const auto& entry : listOf(list->entries))
                        {
                            KeyId key = Locations::AnyKey;

                            if (entry.explicitKey)
                            {
                                if (entry.key and entry.key->kind() == AstKind::Var)
                                    key = nameKey(std::static_pointer_cast<Var>(entry.key)->name.text);
                                else if (entry.key)
                                    key = keyOf(expr(entry.key));
                            }
                            else
                            {
                                // A list's entry: its index
                                if (entry.key and entry.key->kind() == AstKind::Atom)
                                    key = nameKey(std::static_pointer_cast<Atom>(entry.key)->value.toString());

                                named = false;
                            }

                            if (not isNameKey(key))
                                named = false;

                            const ValueId value = entry.value ? expr(entry.value) : constant("none", nullptr);
                            fields[key] = value;

                            const Op op = ir.nodes[value].op;

                            if (op != Op::Const and op != Op::Pure)
                            {
                                use(value);
                                ir.stores.push_back(Store{made, key, value});
                            }
                        }

                        if (named)
                            env.fields[made] = std::move(fields);
                        else
                            escaped.insert(made);

                        return made;
                    }

                    ValueId call(const std::shared_ptr<CallExpression>& c)
                    {
                        const auto args = listOf(c->args);

                        if (c->caller and c->caller->kind() == AstKind::Var)
                        {
                            const std::string name = std::static_pointer_cast<Var>(c->caller)->name.text;

                            if (not local(name))
                                return namedCall(c, name, args);
                        }

                        // A value called (a local, a method, an expression): what it is handed is its
                        // arguments and its receiver, or the value called
                        ValueId callee;
                        ValueId handed;

                        if (c->caller and c->caller->kind() == AstKind::Get)
                        {
                            auto g = std::static_pointer_cast<Get>(c->caller);
                            handed = expr(g->object);
                            callee = readField(handed, nameKey(g->name.text), g.get());
                            ir.valueOf[g.get()] = callee;
                        }
                        else
                        {
                            callee = c->caller ? expr(c->caller) : constant("none", nullptr);
                            handed = callee;
                        }

                        std::vector<ValueId> in;

                        for (const auto& a : args)
                            in.push_back(expr(a));

                        in.push_back(handed);

                        for (auto v : in)
                            use(v);

                        return make(Op::Call, std::move(in), c.get());
                    }

                    ValueId namedCall(const std::shared_ptr<CallExpression>& c, const std::string& name, const std::vector<ExprPtr>& args)
                    {
                        const NativeKind kind = isNative(name) ? nativeKind(name) : NativeKind::None;

                        if (kind == NativeKind::Lookup)
                        {
                            // lookup(t, k, d): the field, or the default
                            const ValueId table = args.size() > 0 ? expr(args[0]) : constant("none", nullptr);
                            const KeyId key = args.size() > 1 ? keyOf(expr(args[1])) : Locations::AnyKey;
                            const ValueId field = readField(table, key, c.get());

                            std::vector<ValueId> rest;

                            for (size_t i = 2; i < args.size(); ++i)
                                rest.push_back(expr(args[i]));

                            return rest.empty() ? field : phi({field, rest[0]}, c.get());
                        }

                        std::vector<ValueId> in;

                        for (const auto& a : args)
                            in.push_back(expr(a));

                        if (kind != NativeKind::Fresh)
                        {
                            for (auto v : in)
                                use(v);
                        }

                        if (kind == NativeKind::None)
                            return make(Op::Call, std::move(in), c.get(), name);

                        const ValueId made = make(Op::Native, in, c.get(), name);

                        if (kind == NativeKind::Push and in.size() > 1)
                            writeField(in[0], Locations::AnyKey, in[1]);
                        else if (kind == NativeKind::Insert and in.size() > 2)
                            writeField(in[0], Locations::AnyKey, in[2]);

                        return made;
                    }

                    ValueId pure(std::vector<ValueId> in, const void* origin)
                    {
                        for (auto v : in)
                            use(v);

                        return make(Op::Pure, std::move(in), origin);
                    }

                    ValueId expr(const ExprPtr& e)
                    {
                        if (not e)
                            return constant("none", nullptr);

                        const ValueId v = value(e);
                        ir.valueOf[e.get()] = v;
                        return v;
                    }

                    ValueId value(const ExprPtr& e)
                    {
                        switch (e->kind())
                        {
                            case AstKind::Atom:
                            {
                                const auto& value = std::static_pointer_cast<Atom>(e)->value;
                                return constant(value.isLitteral() ? "\"" + value.toString() + "\"" : value.toString(), e.get());
                            }

                            case AstKind::NoneAtom:
                                return constant("none", e.get());

                            case AstKind::Var:
                                return read(std::static_pointer_cast<Var>(e)->name.text, e.get());

                            case AstKind::Assign:
                            {
                                auto a = std::static_pointer_cast<Assign>(e);
                                const ValueId v = expr(a->expr);
                                assign(a->name.text, v);
                                return v;
                            }

                            case AstKind::PreFixExpression:
                            case AstKind::PostFixExpression:
                            {
                                const bool pre = e->kind() == AstKind::PreFixExpression;
                                const std::string name = pre ? std::static_pointer_cast<PreFixExpression>(e)->name.text :
                                                               std::static_pointer_cast<PostFixExpression>(e)->name.text;

                                const ValueId old = read(name, e.get());
                                const ValueId next = pure({old}, e.get());
                                assign(name, next);
                                return pre ? next : old;
                            }

                            case AstKind::BinaryExpression:
                            {
                                auto b = std::static_pointer_cast<BinaryExpression>(e);
                                const ValueId l = expr(b->leftExpr);
                                const ValueId r = expr(b->rightExpr);
                                return pure({l, r}, e.get());
                            }

                            case AstKind::LogicExpression:
                            {
                                // Its right side runs or not: what it assigns is joined, its value is one side
                                auto l = std::static_pointer_cast<LogicExpression>(e);
                                const ValueId left = expr(l->leftExpr);
                                Env skipped = env;
                                const ValueId right = expr(l->rightExpr);
                                env = join({skipped, env}, env.scopes.size());
                                return phi({left, right}, e.get());
                            }

                            case AstKind::UnaryExpression:
                                return pure({expr(std::static_pointer_cast<UnaryExpression>(e)->expr)}, e.get());

                            case AstKind::CompoundAtom:
                                return expr(std::static_pointer_cast<CompoundAtom>(e)->expr);

                            case AstKind::CallExpression:
                                return call(std::static_pointer_cast<CallExpression>(e));

                            case AstKind::Get:
                            {
                                auto g = std::static_pointer_cast<Get>(e);
                                const ValueId base = expr(g->object);
                                return readField(base, nameKey(g->name.text), e.get());
                            }

                            case AstKind::IndexGet:
                            {
                                auto g = std::static_pointer_cast<IndexGet>(e);
                                const ValueId base = expr(g->object);
                                const KeyId key = keyOf(expr(g->index));
                                return readField(base, key, e.get());
                            }

                            case AstKind::Set:
                            {
                                auto s = std::static_pointer_cast<Set>(e);
                                const ValueId base = expr(s->object);
                                const ValueId v = expr(s->value);
                                writeField(base, nameKey(s->name.text), v);
                                return v;
                            }

                            case AstKind::IndexSet:
                            {
                                auto s = std::static_pointer_cast<IndexSet>(e);
                                const ValueId base = expr(s->object);
                                const KeyId key = keyOf(expr(s->index));
                                const ValueId v = expr(s->value);
                                writeField(base, key, v);
                                return v;
                            }

                            case AstKind::List:
                                return literal(std::static_pointer_cast<List>(e));

                            case AstKind::This:
                            case AstKind::AnonymousFunction:
                            default:
                                // Not followed: who calls a closure, and with what, is not known
                                markUnknown();
                                return make(Op::Unknown, {}, e.get());
                        }
                    }

                    void stmt(const StatementPtr& s)
                    {
                        if (not s)
                            return;

                        switch (s->kind())
                        {
                            case AstKind::ExpressionStatement:
                                expr(std::static_pointer_cast<ExpressionStatement>(s)->expr);
                                return;

                            case AstKind::VariableStatement:
                            {
                                auto v = std::static_pointer_cast<VariableStatement>(s);
                                const ValueId value = v->expr ? expr(v->expr) : constant("none", nullptr);

                                // At the top level of a file: a global of the program, then its kept fills
                                if (env.scopes.empty())
                                {
                                    use(value);
                                    ir.defines.emplace_back(v->name.text, value);

                                    for (const auto& fill : v->keptFills)
                                        stmt(fill);
                                }
                                else
                                    env.scopes.back()[v->name.text] = value;

                                return;
                            }

                            case AstKind::FunctionStatement:
                            {
                                // One of the top level is built of its own; one inside is a closure
                                if (env.scopes.empty())
                                    return;

                                markUnknown();
                                env.scopes.back()[std::static_pointer_cast<FunctionStatement>(s)->name.text] = make(Op::Unknown, {}, nullptr);
                                return;
                            }

                            case AstKind::ClassStatement:
                                markUnknown();
                                return;

                            case AstKind::BlockStatement:
                            {
                                env.scopes.emplace_back();

                                for (const auto& b : listOf(std::static_pointer_cast<BlockStatement>(s)->statements))
                                    stmt(b);

                                env.scopes.pop_back();
                                return;
                            }

                            case AstKind::IfStatement:
                            {
                                auto i = std::static_pointer_cast<IfStatement>(s);
                                expr(i->condition);

                                Env before = env;
                                stmt(i->thenBranch);
                                Env then = std::move(env);

                                env = std::move(before);
                                stmt(i->elseBranch);

                                env = join({then, env}, env.scopes.size());
                                return;
                            }

                            case AstKind::WhileStatement:
                            {
                                auto w = std::static_pointer_cast<WhileStatement>(s);
                                loop(w->condition, w->body, nullptr);
                                return;
                            }

                            case AstKind::ForStatement:
                            {
                                auto f = std::static_pointer_cast<ForStatement>(s);
                                env.scopes.emplace_back();
                                stmt(f->initializer);
                                loop(f->condition, f->body, f->increment);
                                env.scopes.pop_back();
                                return;
                            }

                            case AstKind::ForInStatement:
                            {
                                // Its variable takes the entries of a list or the keys of a table
                                auto f = std::static_pointer_cast<ForInStatement>(s);
                                const ValueId of = expr(f->iterable);

                                env.scopes.emplace_back();
                                env.scopes.back()[f->varName.text] = readField(of, Locations::AnyKey, f->iterable.get());
                                loop(nullptr, f->body, nullptr);
                                env.scopes.pop_back();
                                return;
                            }

                            case AstKind::ReturnStatement:
                            {
                                auto r = std::static_pointer_cast<ReturnStatement>(s);
                                const ValueId v = r->value ? expr(r->value) : constant("none", nullptr);
                                use(v);
                                ir.returns.push_back(v);
                                env.live = false;
                                return;
                            }

                            case AstKind::BreakStatement:
                            case AstKind::ContinueStatement:
                            {
                                if (env.live and not loops.empty())
                                {
                                    auto& to = s->kind() == AstKind::BreakStatement ? loops.back().breaks : loops.back().continues;
                                    to.push_back(env);
                                }

                                env.live = false;
                                return;
                            }

                            case AstKind::DPrintStatement:
                                expr(std::static_pointer_cast<DPrintStatement>(s)->expr);
                                markUnknown();
                                return;

                            default:
                                return;
                        }
                    }
                };
            }

            BodyIR buildFunction(const FunctionStatement& f, Locations& locs, const std::function<bool(const std::string&)>& isNative)
            {
                return Builder(locs, isNative).function(f);
            }

            BodyIR buildTopLevel(const std::shared_ptr<Statement>& s, Locations& locs, const std::function<bool(const std::string&)>& isNative)
            {
                return Builder(locs, isNative).topLevel(s);
            }
        }
    }
}
