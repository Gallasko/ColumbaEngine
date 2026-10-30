#include "stdafx.h"

#include "effects.h"

#include "logger.h"

#include <sstream>

namespace pg
{
    namespace ast
    {
        namespace
        {
            const char* DOM = "Effects";

            // Walks of a function or a statement until what its locals hold stops growing
            constexpr int MaxWalks = 32;

            // Rounds of the whole program until nothing it knows grows
            constexpr int MaxRounds = 64;

            // The field of entries not known when compiled: a list's, a key computed
            const std::string AnyField = "*";

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

            // What a native known to the analysis does with its arguments
            enum class Native
            {
                Fresh,      // Returns a value of its own (a number, a string, a new list of strings)
                Lookup,     // Returns the field of its first named by its second, or its third (lookup, pick)
                Entry,      // Returns an entry of its first (findBy)
                IndexBy,    // Returns a new table of the entries of its first
                Copy,       // Returns a new table or list holding what its first holds
                Push,       // Writes its first, puts its second in it
                Insert,     // Writes its first, puts its third in it
                Take,       // Writes its first, returns an entry of it (pop, removeAt)
                Merge,      // Writes its first, puts what its second holds in it, returns the first
            };

            const std::map<std::string, Native>& natives()
            {
                static const std::map<std::string, Native> known = {
                    // algorithm
                    {"contain", Native::Fresh}, {"lookup", Native::Lookup}, {"pick", Native::Lookup},
                    {"findBy", Native::Entry}, {"indexBy", Native::IndexBy}, {"copy", Native::Copy},
                    {"merge", Native::Merge}, {"toInt", Native::Fresh}, {"toFloat", Native::Fresh},
                    {"typeOf", Native::Fresh}, {"len", Native::Fresh}, {"push", Native::Push},
                    {"pop", Native::Take}, {"insert", Native::Insert}, {"removeAt", Native::Take},
                    // string
                    {"toString", Native::Fresh}, {"strlen", Native::Fresh}, {"split", Native::Fresh},
                    {"splitLines", Native::Fresh}, {"startsWith", Native::Fresh}, {"endsWith", Native::Fresh},
                    {"contains", Native::Fresh}, {"charAt", Native::Fresh}, {"substring", Native::Fresh},
                    {"replace", Native::Fresh}, {"join", Native::Fresh}, {"capitalize", Native::Fresh},
                    {"lowerize", Native::Fresh}, {"toLowerCase", Native::Fresh}, {"toUpperCase", Native::Fresh},
                    {"trim", Native::Fresh}, {"indexOf", Native::Fresh},
                    // math
                    {"sqrt", Native::Fresh}, {"abs", Native::Fresh}, {"pow", Native::Fresh}, {"floor", Native::Fresh},
                    {"ceil", Native::Fresh}, {"sin", Native::Fresh}, {"cos", Native::Fresh}, {"tan", Native::Fresh},
                    {"asin", Native::Fresh}, {"acos", Native::Fresh}, {"atan", Native::Fresh},
                };

                return known;
            }

            bool isParam(const std::string& name) { return not name.empty() and name[0] == '#'; }
            bool isInput(const std::string& name) { return name.compare(0, 3, "in:") == 0; }
            bool isSite(const std::string& name) { return not name.empty() and name[0] == '@'; }

            size_t paramIndex(const std::string& name) { return static_cast<size_t>(std::stoul(name.substr(1))); }
            std::string paramName(size_t i) { return "#" + std::to_string(i); }

            std::string siteOf(const void* node)
            {
                std::ostringstream out;
                out << '@' << node;
                return out.str();
            }

            std::string varName(const ExprPtr& e)
            {
                if (e and e->kind() == AstKind::Var)
                    return std::static_pointer_cast<Var>(e)->name.text;

                return "";
            }

            // The field an index or a key names when it is a string written out, else any
            std::string fieldOf(const ExprPtr& key)
            {
                if (key and key->kind() == AstKind::Atom)
                {
                    const auto& value = std::static_pointer_cast<Atom>(key)->value;

                    if (value.isLitteral())
                        return value.toString();
                }

                return AnyField;
            }

            Roots one(const std::string& name)
            {
                Roots r;
                r.names.insert(name);
                return r;
            }

            Roots anything()
            {
                Roots r;
                r.any = true;
                return r;
            }

            // The elements the analysis on this thread has gone through: what its work is counted in
            thread_local uint64_t elementsSeen = 0;
        }

        bool Roots::add(const Roots& other)
        {
            bool grew = false;

            elementsSeen += other.names.size() + other.deep.size() + 1;

            for (const auto& name : other.names)
                grew = names.insert(name).second or grew;

            for (const auto& name : other.deep)
                grew = deep.insert(name).second or grew;

            if (other.any and not any)
            {
                any = true;
                grew = true;
            }

            return grew;
        }

        bool Roots::add(const std::string& name)
        {
            return names.insert(name).second;
        }

        namespace
        {
            bool sameRoots(const Roots& a, const Roots& b) { return a.names == b.names and a.deep == b.deep and a.any == b.any; }

            template <typename K>
            bool sameMap(const std::map<K, Roots>& a, const std::map<K, Roots>& b)
            {
                if (a.size() != b.size())
                    return false;

                for (const auto& [k, v] : a)
                {
                    auto it = b.find(k);

                    if (it == b.end() or not sameRoots(v, it->second))
                        return false;
                }

                return true;
            }

            void text(std::ostream& out, const Roots& r)
            {
                out << '{';
                for (const auto& n : r.names) out << n << ',';
                for (const auto& n : r.deep) out << n << "...,";
                out << (r.any ? "*" : "") << '}';
            }
        }

        bool FunctionEffects::operator==(const FunctionEffects& o) const
        {
            return arity == o.arity and writesParams == o.writesParams and sameRoots(result, o.result) and
                   sameMap(paramStores, o.paramStores) and sameMap(pending, o.pending) and reads == o.reads and
                   writes == o.writes and writesDeep == o.writesDeep and writesAny == o.writesAny and
                   rebinds == o.rebinds and unknown == o.unknown;
        }

        std::string FunctionEffects::digest() const
        {
            std::ostringstream out;

            out << "w";
            for (auto i : writesParams) out << i << ',';
            out << "r";
            text(out, result);
            for (const auto& [i, r] : paramStores) { out << 's' << i; text(out, r); }
            for (const auto& [at, r] : pending) { out << 'p' << at.first << '.' << at.second; text(out, r); }
            out << "R";
            for (const auto& n : reads) out << n << ',';
            out << "W";
            for (const auto& n : writes) out << n << ',';
            for (const auto& n : writesDeep) out << n << "...,";
            out << (writesAny ? "*" : "") << "B";
            for (const auto& n : rebinds) out << n << ',';
            out << (unknown ? "?" : "");

            return out.str();
        }

        // Walks a function's body or a top-level statement: what each value may be, what it writes
        class EffectsWalker
        {
        public:
            // `learning`: what is learnt goes into the program (globals, objects' fields); else only
            // `function` or `statement` is told
            EffectsWalker(const ProgramEffects& program, ProgramEffects* learning, FunctionEffects* function, StatementEffects* statement)
                : program(program), learning(learning), function(function), statement(statement) {}

            void walkFunction(const FunctionStatement& f)
            {
                auto params = listOf(f.parameters);

                for (int walk = 0; walk < MaxWalks; ++walk)
                {
                    grew = false;
                    scopes.clear();
                    scopes.emplace_back();

                    for (size_t i = 0; i < params.size(); ++i)
                    {
                        const std::string name = varName(params[i]);

                        if (not name.empty())
                            declare(name, params[i].get(), one(paramName(i)));
                    }

                    stmt(f.body);

                    if (not grew)
                        return;
                }

                LOG_INFO(DOM, "Function " << f.name.text << " still growing after " << MaxWalks << " walks");
            }

            void walkTopLevel(const StatementPtr& s)
            {
                for (int walk = 0; walk < MaxWalks; ++walk)
                {
                    grew = false;
                    scopes.clear();
                    stmt(s);

                    if (not grew)
                        return;
                }
            }

        private:
            const ProgramEffects& program;
            ProgramEffects* learning;
            FunctionEffects* function;
            StatementEffects* statement;

            // The locals in the scopes around, innermost last: a name and the declaration it stands for
            std::vector<std::map<std::string, const void*>> scopes;

            // What each local (by its declaration) may hold, kept from walk to walk
            std::map<const void*, Roots> localValues;

            // What this walk put in objects: seen by its own reads before the program learns it (and
            // the only place a function's values holding a parameter are kept)
            std::map<std::string, std::map<std::string, Roots>> localHeap;

            bool grew = false;

            const void* localOf(const std::string& name) const
            {
                for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
                {
                    auto found = it->find(name);

                    if (found != it->end())
                        return found->second;
                }

                return nullptr;
            }

            void declare(const std::string& name, const void* key, const Roots& value)
            {
                if (localValues[key].add(value))
                    grew = true;

                if (not scopes.empty())
                    scopes.back()[name] = key;
            }

            void read(const std::string& name)
            {
                if (function)
                    function->reads.insert(name);
                else if (statement)
                    statement->reads.insert(name);
            }

            void markUnknown()
            {
                if (function)
                    function->unknown = true;
                else if (statement)
                    statement->unknown = true;
            }

            void rebind(const std::string& name)
            {
                if (function)
                    function->rebinds.insert(name);
                else if (statement)
                    statement->rebinds.insert(name);

                if (learning)
                    learning->rebinds.insert(name);
            }

            template <typename F>
            void eachField(const std::string& object, F&& f) const
            {
                for (const auto* heap : {&program.heap, &localHeap})
                {
                    auto it = heap->find(object);

                    if (it == heap->end())
                        continue;

                    for (const auto& [name, held] : it->second)
                        f(name, held);
                }
            }

            // What the objects' field may hold
            Roots field(const Roots& objects, const std::string& name) const
            {
                Roots out;
                out.any = objects.any;

                // Inside what is reachable from them: still reachable from them
                out.deep = objects.deep;

                for (const auto& o : objects.names)
                {
                    // A parameter's or an input's insides are themselves: not known apart
                    if (not isSite(o))
                    {
                        out.add(o);
                        continue;
                    }

                    eachField(o, [&](const std::string& f, const Roots& held)
                    {
                        if (name == AnyField or f == name or f == AnyField)
                            out.add(held);
                    });
                }

                return out;
            }

            // The objects and all they may hold, unexpanded
            static Roots reachable(const Roots& r)
            {
                Roots out;
                out.any = r.any;
                out.deep = r.deep;
                out.deep.insert(r.names.begin(), r.names.end());
                return out;
            }

            // The objects and all they may hold, every one named (for putting something in each)
            Roots expanded(const Roots& r) const
            {
                // What the program knows alone (this walk holds nothing of its own): kept while it holds
                if (localHeap.empty() and r.names.empty())
                {
                    auto& memo = program.expandedAt[r.deep];

                    if (memo.first != program.heapVersion + 1)
                    {
                        Roots deep;
                        deep.deep = r.deep;
                        memo.second = program.reachable(deep);
                        memo.first = program.heapVersion + 1;
                    }

                    elementsSeen += memo.second.size();

                    Roots out;
                    out.any = r.any;
                    out.names = memo.second;
                    return out;
                }

                Roots out;
                out.any = r.any;
                out.names = r.names;

                std::vector<std::string> todo(r.deep.begin(), r.deep.end());
                todo.insert(todo.end(), r.names.begin(), r.names.end());

                std::set<std::string> seen;

                while (not todo.empty())
                {
                    const std::string o = todo.back();
                    todo.pop_back();

                    if (not seen.insert(o).second)
                        continue;

                    ++elementsSeen;
                    out.names.insert(o);

                    if (not isSite(o))
                        continue;

                    eachField(o, [&](const std::string&, const Roots& held)
                    {
                        if (held.any)
                            out.any = true;

                        elementsSeen += held.names.size() + held.deep.size() + 1;
                        todo.insert(todo.end(), held.names.begin(), held.names.end());
                        todo.insert(todo.end(), held.deep.begin(), held.deep.end());
                    });
                }

                return out;
            }

            void write(const Roots& objects)
            {
                for (const auto& o : objects.names)
                {
                    if (isParam(o))
                    {
                        if (function)
                            function->writesParams.insert(paramIndex(o));
                    }
                    else if (function)
                        function->writes.insert(o);
                    else if (statement)
                        statement->writes.insert(o);
                }

                for (const auto& o : objects.deep)
                {
                    if (isParam(o))
                    {
                        if (function)
                            function->writesParams.insert(paramIndex(o));
                    }
                    else if (function)
                        function->writesDeep.insert(o);
                    else if (statement)
                        statement->writesDeep.insert(o);
                }

                if (objects.any)
                {
                    if (function)
                        function->writesAny = true;
                    else if (statement)
                        statement->writesAny = true;
                }
            }

            static bool holdsParam(const Roots& r)
            {
                for (const auto* set : {&r.names, &r.deep})
                {
                    for (const auto& n : *set)
                    {
                        if (isParam(n))
                            return true;
                    }
                }

                return false;
            }

            // `value` put in the objects' field
            void store(const Roots& into, const std::string& name, const Roots& value)
            {
                if (into.any)
                    write(reachable(value)); // Into anything: whatever writes anything may write it

                // Into all reachable from some: each of them named
                Roots objects = into;

                if (not into.deep.empty())
                {
                    Roots concreteDeep;
                    Roots params;

                    for (const auto& o : into.deep)
                        (isParam(o) ? params : concreteDeep).names.insert(o);

                    objects.add(params);
                    objects.add(expanded(concreteDeep));
                    objects.deep.clear();
                }

                elementsSeen += objects.names.size();

                for (const auto& o : objects.names)
                {
                    if (learning and elementsSeen - learning->workFrom > ProgramEffects::Budget)
                        return;

                    if (isParam(o))
                    {
                        if (function and function->paramStores[paramIndex(o)].add(value))
                            grew = true;
                    }
                    else if (isInput(o))
                    {
                        // Into what was handed in: whoever handed it may do anything with it
                        write(reachable(value));
                    }
                    else
                    {
                        if (function and holdsParam(value) and function->pending[{o, name}].add(value))
                            grew = true;

                        // What only this walk knows (it learns nothing, or a parameter is in it): for its own reads
                        if ((not learning or holdsParam(value)) and localHeap[o][name].add(value))
                            grew = true;

                        if (learning)
                        {
                            Roots concrete;
                            concrete.any = value.any;

                            for (const auto& v : value.names)
                            {
                                if (not isParam(v))
                                    concrete.names.insert(v);
                            }

                            for (const auto& v : value.deep)
                            {
                                if (not isParam(v))
                                    concrete.deep.insert(v);
                            }

                            if (not concrete.empty() and learning->heap[o][name].add(concrete))
                                ++learning->heapVersion;
                        }
                    }
                }
            }

            Roots unknownCall(const std::vector<Roots>& args)
            {
                markUnknown();

                Roots out;

                for (const auto& a : args)
                {
                    const Roots all = reachable(a);
                    write(all);
                    out.add(all);
                }

                return out;
            }

            // A summary's value with the call's arguments in its parameters' places
            Roots substitute(const Roots& r, const std::vector<Roots>& args) const
            {
                Roots out;
                out.any = r.any;

                for (const auto& n : r.names)
                {
                    if (not isParam(n))
                        out.names.insert(n);
                    else if (paramIndex(n) < args.size())
                        out.add(reachable(args[paramIndex(n)]));
                }

                for (const auto& n : r.deep)
                {
                    if (not isParam(n))
                        out.deep.insert(n);
                    else if (paramIndex(n) < args.size())
                        out.add(reachable(args[paramIndex(n)]));
                }

                return out;
            }

            Roots callFunction(const FunctionEffects& f, const std::vector<Roots>& args)
            {
                if (function)
                    function->reads.insert(f.reads.begin(), f.reads.end());
                else if (statement)
                    statement->reads.insert(f.reads.begin(), f.reads.end());

                Roots written;
                written.any = f.writesAny;
                written.names = f.writes;
                written.deep = f.writesDeep;

                write(written);

                for (auto i : f.writesParams)
                {
                    if (i < args.size())
                        write(reachable(args[i]));
                }

                for (const auto& name : f.rebinds)
                    rebind(name);

                for (const auto& [i, value] : f.paramStores)
                {
                    if (i < args.size())
                        store(reachable(args[i]), AnyField, substitute(value, args));
                }

                for (const auto& [at, value] : f.pending)
                    store(one(at.first), at.second, substitute(value, args));

                if (f.unknown)
                    markUnknown();

                return substitute(f.result, args);
            }

            // Every field of the objects put in `into`, field by field (a copy, a merge)
            void storeFields(const Roots& into, const Roots& from)
            {
                if (not from.deep.empty())
                {
                    Roots deep;
                    deep.deep = from.deep;
                    store(into, AnyField, deep);
                }

                for (const auto& o : from.names)
                {
                    if (not isSite(o))
                    {
                        store(into, AnyField, one(o));
                        continue;
                    }

                    std::vector<std::pair<std::string, Roots>> fields;
                    eachField(o, [&](const std::string& f, const Roots& held) { fields.emplace_back(f, held); });

                    for (const auto& [f, held] : fields)
                        store(into, f, held);
                }

                if (from.any)
                    store(into, AnyField, anything());
            }

            Roots callNative(Native kind, const std::shared_ptr<CallExpression>& c, const std::vector<Roots>& args)
            {
                auto arg = [&args](size_t i) { return i < args.size() ? args[i] : Roots{}; };
                const auto exprs = listOf(c->args);

                switch (kind)
                {
                    case Native::Fresh:
                        return {};

                    case Native::Lookup:
                    {
                        Roots out = field(arg(0), exprs.size() > 1 ? fieldOf(exprs[1]) : AnyField);
                        out.add(arg(2));
                        return out;
                    }

                    case Native::Entry:
                        return field(arg(0), AnyField);

                    case Native::IndexBy:
                    {
                        const Roots made = one(siteOf(c.get()));
                        store(made, AnyField, field(arg(0), AnyField));
                        return made;
                    }

                    case Native::Copy:
                    {
                        const Roots made = one(siteOf(c.get()));
                        storeFields(made, arg(0));
                        return made;
                    }

                    case Native::Push:
                        write(arg(0));
                        store(arg(0), AnyField, arg(1));
                        return {};

                    case Native::Insert:
                        write(arg(0));
                        store(arg(0), AnyField, arg(2));
                        return {};

                    case Native::Take:
                        write(arg(0));
                        return field(arg(0), AnyField);

                    case Native::Merge:
                        write(arg(0));
                        storeFields(arg(0), arg(1));
                        return arg(0);
                }

                return {};
            }

            Roots call(const std::shared_ptr<CallExpression>& c)
            {
                std::vector<Roots> args;

                for (const auto& a : listOf(c->args))
                    args.push_back(expr(a));

                const std::string name = varName(c->caller);

                if (name.empty() or localOf(name))
                {
                    expr(c->caller);
                    return unknownCall(args);
                }

                read(name);

                // One of the program's functions, as it is defined once and never rebound
                auto summary = program.summaries.find(name);

                if (summary != program.summaries.end() and not program.rebinds.count(name))
                    return callFunction(summary->second, args);

                // A native, when the program defines nothing of its name
                auto native = natives().find(name);

                if (native != natives().end() and program.definitions(name) == 0)
                    return callNative(native->second, c, args);

                return unknownCall(args);
            }

            Roots globalValue(const std::string& name)
            {
                read(name);

                // A function or a native is a value of no object
                if (program.summaries.count(name) or (natives().count(name) and program.definitions(name) == 0))
                    return {};

                // A global of the program: what it is ever given
                if (program.vars.count(name) or program.consts.count(name))
                {
                    auto it = program.globalValue.find(name);
                    return it == program.globalValue.end() ? Roots{} : it->second;
                }

                // Defined nowhere: handed in
                return one("in:" + name);
            }

            void assignGlobal(const std::string& name, const Roots& value)
            {
                if (learning and learning->globalValue[name].add(value))
                    grew = true;
            }

            Roots literal(const std::shared_ptr<List>& list)
            {
                const Roots made = one(siteOf(list.get()));

                for (const auto& entry : listOf(list->entries))
                {
                    std::string name = AnyField;

                    if (entry.explicitKey and entry.key)
                        name = entry.key->kind() == AstKind::Var ? varName(entry.key) : fieldOf(entry.key);
                    else
                        expr(entry.key);

                    // Made, not written: an object being built
                    store(made, name, expr(entry.value));
                }

                return made;
            }

            Roots expr(const ExprPtr& e)
            {
                if (not e)
                    return {};

                // Past the budget the analysis is given up: nothing more is walked
                if (learning)
                {
                    learning->work = ++elementsSeen - learning->workFrom;

                    if (learning->work > ProgramEffects::Budget)
                        return {};
                }

                switch (e->kind())
                {
                    case AstKind::Atom:
                    case AstKind::NoneAtom:
                        return {};

                    case AstKind::This:
                        return anything();

                    case AstKind::Var:
                    {
                        const std::string name = varName(e);

                        if (auto key = localOf(name))
                        {
                            auto it = localValues.find(key);
                            return it == localValues.end() ? Roots{} : it->second;
                        }

                        return globalValue(name);
                    }

                    case AstKind::Assign:
                    {
                        auto a = std::static_pointer_cast<Assign>(e);
                        Roots value = expr(a->expr);

                        if (auto key = localOf(a->name.text))
                        {
                            if (localValues[key].add(value))
                                grew = true;
                        }
                        else
                        {
                            rebind(a->name.text);
                            assignGlobal(a->name.text, value);
                        }

                        return value;
                    }

                    case AstKind::PreFixExpression:
                    case AstKind::PostFixExpression:
                    {
                        const std::string name = e->kind() == AstKind::PreFixExpression ?
                            std::static_pointer_cast<PreFixExpression>(e)->name.text :
                            std::static_pointer_cast<PostFixExpression>(e)->name.text;

                        if (not localOf(name))
                            rebind(name);

                        return {};
                    }

                    case AstKind::BinaryExpression:
                    {
                        auto b = std::static_pointer_cast<BinaryExpression>(e);
                        expr(b->leftExpr);
                        expr(b->rightExpr);
                        return {};
                    }

                    case AstKind::LogicExpression:
                    {
                        // `a or b` is one of them
                        auto l = std::static_pointer_cast<LogicExpression>(e);
                        Roots out = expr(l->leftExpr);
                        out.add(expr(l->rightExpr));
                        return out;
                    }

                    case AstKind::UnaryExpression:
                        expr(std::static_pointer_cast<UnaryExpression>(e)->expr);
                        return {};

                    case AstKind::CompoundAtom:
                        return expr(std::static_pointer_cast<CompoundAtom>(e)->expr);

                    case AstKind::CallExpression:
                        return call(std::static_pointer_cast<CallExpression>(e));

                    case AstKind::Get:
                    {
                        auto g = std::static_pointer_cast<Get>(e);
                        return field(expr(g->object), g->name.text);
                    }

                    case AstKind::IndexGet:
                    {
                        auto g = std::static_pointer_cast<IndexGet>(e);
                        Roots of = expr(g->object);
                        expr(g->index);
                        return field(of, fieldOf(g->index));
                    }

                    case AstKind::Set:
                    {
                        auto s = std::static_pointer_cast<Set>(e);
                        Roots of = expr(s->object);
                        Roots value = expr(s->value);
                        write(of);
                        store(of, s->name.text, value);
                        return value;
                    }

                    case AstKind::IndexSet:
                    {
                        auto s = std::static_pointer_cast<IndexSet>(e);
                        Roots of = expr(s->object);
                        expr(s->index);
                        Roots value = expr(s->value);
                        write(of);
                        store(of, fieldOf(s->index), value);
                        return value;
                    }

                    case AstKind::List:
                        return literal(std::static_pointer_cast<List>(e));

                    case AstKind::AnonymousFunction:
                        // A closure: who calls it and with what is not followed. Its parameters may
                        // be anything, so whatever it writes may be anything
                        markUnknown();
                        write(anything());
                        return {};

                    default:
                        return {};
                }
            }

            void stmt(const StatementPtr& s)
            {
                if (not s or (learning and elementsSeen - learning->workFrom > ProgramEffects::Budget))
                    return;

                switch (s->kind())
                {
                    case AstKind::ExpressionStatement:
                        expr(std::static_pointer_cast<ExpressionStatement>(s)->expr);
                        return;

                    case AstKind::VariableStatement:
                    {
                        auto v = std::static_pointer_cast<VariableStatement>(s);
                        Roots value = expr(v->expr);

                        // At a file's top level it defines a global of the program
                        if (scopes.empty())
                        {
                            assignGlobal(v->name.text, value);

                            for (const auto& fill : v->keptFills)
                                stmt(fill);
                        }
                        else
                        {
                            declare(v->name.text, v.get(), value);
                        }

                        return;
                    }

                    case AstKind::FunctionStatement:
                    {
                        // A function of the top level is summed up of its own; one declared inside
                        // another is a closure: not followed
                        if (not scopes.empty())
                        {
                            auto f = std::static_pointer_cast<FunctionStatement>(s);
                            declare(f->name.text, f.get(), {});
                            markUnknown();
                            write(anything());
                        }

                        return;
                    }

                    case AstKind::ClassStatement:
                        // Its methods run on instances not followed
                        markUnknown();
                        write(anything());
                        return;

                    case AstKind::BlockStatement:
                    {
                        scopes.emplace_back();

                        for (const auto& b : listOf(std::static_pointer_cast<BlockStatement>(s)->statements))
                            stmt(b);

                        scopes.pop_back();
                        return;
                    }

                    case AstKind::IfStatement:
                    {
                        auto i = std::static_pointer_cast<IfStatement>(s);
                        expr(i->condition);
                        stmt(i->thenBranch);
                        stmt(i->elseBranch);
                        return;
                    }

                    case AstKind::WhileStatement:
                    {
                        auto w = std::static_pointer_cast<WhileStatement>(s);
                        expr(w->condition);
                        stmt(w->body);
                        return;
                    }

                    case AstKind::ForStatement:
                    {
                        auto f = std::static_pointer_cast<ForStatement>(s);
                        scopes.emplace_back();
                        stmt(f->initializer);
                        expr(f->condition);
                        expr(f->increment);
                        stmt(f->body);
                        scopes.pop_back();
                        return;
                    }

                    case AstKind::ForInStatement:
                    {
                        // Its variable takes the keys of a table or the entries of a list
                        auto f = std::static_pointer_cast<ForInStatement>(s);
                        Roots entries = field(expr(f->iterable), AnyField);

                        scopes.emplace_back();
                        declare(f->varName.text, f.get(), entries);
                        stmt(f->body);
                        scopes.pop_back();
                        return;
                    }

                    case AstKind::ReturnStatement:
                    {
                        Roots value = expr(std::static_pointer_cast<ReturnStatement>(s)->value);

                        if (function and function->result.add(value))
                            grew = true;

                        return;
                    }

                    case AstKind::DPrintStatement:
                        // It prints: something the run does, not a value it makes
                        expr(std::static_pointer_cast<DPrintStatement>(s)->expr);
                        markUnknown();
                        return;

                    default:
                        return;
                }
            }
        };

        int ProgramEffects::definitions(const std::string& name) const
        {
            auto it = defined.find(name);
            return it == defined.end() ? 0 : it->second;
        }

        std::set<std::string> ProgramEffects::reachable(const Roots& r) const
        {
            std::set<std::string> out;
            std::vector<std::string> todo(r.names.begin(), r.names.end());
            todo.insert(todo.end(), r.deep.begin(), r.deep.end());

            while (not todo.empty())
            {
                const std::string o = todo.back();
                todo.pop_back();

                if (not out.insert(o).second or not isSite(o))
                    continue;

                auto it = heap.find(o);

                if (it == heap.end())
                    continue;

                for (const auto& [f, held] : it->second)
                {
                    todo.insert(todo.end(), held.names.begin(), held.names.end());
                    todo.insert(todo.end(), held.deep.begin(), held.deep.end());
                }
            }

            return out;
        }

        std::set<std::string> ProgramEffects::written(const StatementEffects& effects) const
        {
            Roots deep;
            deep.deep = effects.writesDeep;

            std::set<std::string> out = reachable(deep);
            out.insert(effects.writes.begin(), effects.writes.end());

            return out;
        }

        bool ProgramEffects::analyze(const std::vector<std::vector<StatementPtr>>& files)
        {
            *this = ProgramEffects();
            workFrom = elementsSeen;

            // What the top levels define
            for (const auto& file : files)
            {
                for (const auto& s : file)
                {
                    if (not s)
                        continue;

                    if (s->kind() == AstKind::VariableStatement)
                    {
                        auto v = std::static_pointer_cast<VariableStatement>(s);
                        ++defined[v->name.text];
                        (v->isConst ? consts : vars).insert(v->name.text);
                        definitionOf[v->name.text] = v;
                    }
                    else if (s->kind() == AstKind::FunctionStatement)
                    {
                        auto f = std::static_pointer_cast<FunctionStatement>(s);
                        ++defined[f->name.text];
                        bodies[f->name.text] = f;
                    }
                    else if (s->kind() == AstKind::ClassStatement)
                    {
                        ++defined[std::static_pointer_cast<ClassStatement>(s)->name.text];
                    }
                }
            }

            // A function defined twice is the last one at run time: not followed
            for (auto it = bodies.begin(); it != bodies.end();)
            {
                if (defined[it->first] != 1)
                    it = bodies.erase(it);
                else
                {
                    summaries[it->first].arity = it->second->parameters.size();
                    ++it;
                }
            }

            auto size = [this]()
            {
                size_t n = rebinds.size();

                for (const auto& [g, v] : globalValue)
                    n += v.names.size() + v.deep.size() + (v.any ? 1 : 0);

                for (const auto& [o, fields] : heap)
                {
                    for (const auto& [f, held] : fields)
                        n += held.names.size() + held.deep.size() + (held.any ? 1 : 0);
                }

                return n;
            };

            // Every function and every top-level statement again, until nothing grows
            for (int round = 0; round < MaxRounds; ++round)
            {
                bool changed = false;
                const size_t before = size();

                for (const auto& [name, body] : bodies)
                {
                    FunctionEffects fresh;
                    fresh.arity = body->parameters.size();

                    EffectsWalker walker(*this, this, &fresh, nullptr);
                    walker.walkFunction(*body);

                    // What it learnt joins what was known of it: a summary only grows, so the rounds end
                    FunctionEffects& known = summaries[name];
                    const FunctionEffects before = known;

                    known.writesParams.insert(fresh.writesParams.begin(), fresh.writesParams.end());
                    known.result.add(fresh.result);

                    for (const auto& [i, r] : fresh.paramStores)
                        known.paramStores[i].add(r);

                    for (const auto& [at, r] : fresh.pending)
                        known.pending[at].add(r);

                    known.reads.insert(fresh.reads.begin(), fresh.reads.end());
                    known.writes.insert(fresh.writes.begin(), fresh.writes.end());
                    known.writesDeep.insert(fresh.writesDeep.begin(), fresh.writesDeep.end());
                    known.writesAny = known.writesAny or fresh.writesAny;
                    known.rebinds.insert(fresh.rebinds.begin(), fresh.rebinds.end());
                    known.unknown = known.unknown or fresh.unknown;

                    if (not (known == before))
                        changed = true;
                }

                for (const auto& file : files)
                {
                    for (const auto& s : file)
                    {
                        if (not s or s->kind() == AstKind::FunctionStatement or s->kind() == AstKind::ImportStatement)
                            continue;

                        StatementEffects effects;
                        EffectsWalker walker(*this, this, nullptr, &effects);
                        walker.walkTopLevel(s);
                        topLevel[s.get()] = effects;
                    }
                }

                if (size() != before)
                    changed = true;


                work = elementsSeen - workFrom;


                if (work > Budget)
                {
                    LOG_INFO(DOM, "Given up: more work than " << Budget);
                    return false;
                }

                if (not changed)
                    break;
            }

            // Which objects each global reaches, and which globals reach each object
            for (const auto& [name, value] : globalValue)
            {
                reachOf[name] = reachable(value);

                for (const auto& o : reachOf[name])
                    reachedBy[o].insert(name);
            }

            // Every object a statement writes, and whether one writes anything
            std::map<std::string, std::set<const Statement*>> writers;
            bool anyWritten = false;

            for (const auto& [s, effects] : topLevel)
            {
                anyWritten = anyWritten or effects.writesAny;

                for (const auto& o : written(effects))
                    writers[o].insert(s);
            }

            // The vars as good as consts: defined once, never rebound, what they reach written by
            // no other statement, their own made of what is the same on every run (found again as
            // each one found makes more of them so)
            bool found = not anyWritten;

            while (found)
            {
                found = false;

                for (const auto& name : vars)
                {
                    if (inferred.count(name) or definitions(name) != 1 or rebinds.count(name))
                        continue;

                    const Statement* definition = definitionOf[name].get();
                    auto effects = topLevel.find(definition);

                    if (effects == topLevel.end() or not onlyMakes(effects->second, {name}))
                        continue;

                    bool writtenElsewhere = false;

                    for (const auto& o : reachOf[name])
                    {
                        auto w = writers.find(o);

                        if (w != writers.end() and (w->second.size() > 1 or not w->second.count(definition)))
                            writtenElsewhere = true;
                    }

                    if (writtenElsewhere)
                        continue;

                    inferred.insert(name);
                    found = true;
                }
            }

            return true;
        }

        StatementEffects ProgramEffects::effectsOf(const StatementPtr& statement) const
        {
            StatementEffects effects;
            EffectsWalker walker(*this, nullptr, nullptr, &effects);
            walker.walkTopLevel(statement);

            return effects;
        }

        bool ProgramEffects::invariant(const std::string& name) const
        {
            if (consts.count(name) and definitions(name) == 1 and not rebinds.count(name))
                return true;

            if (summaries.count(name) and not rebinds.count(name))
                return true;

            if (natives().count(name) and definitions(name) == 0)
                return true;

            return inferred.count(name) > 0;
        }

        bool ProgramEffects::onlyMakes(const StatementEffects& effects, const std::set<std::string>& targets) const
        {
            if (effects.unknown or effects.writesAny or not effects.rebinds.empty())
                return false;

            for (const auto& name : effects.reads)
            {
                if (not targets.count(name) and not invariant(name))
                {
                    return false;
                }
            }

            // What the targets hold: theirs to make, whoever else reaches it (whatever reaches it
            // later in a run sees it made, as it did)
            std::set<std::string> theirs;

            for (const auto& g : targets)
            {
                auto reach = reachOf.find(g);

                if (reach != reachOf.end())
                    theirs.insert(reach->second.begin(), reach->second.end());
            }

            for (const auto& o : written(effects))
            {
                if (not isSite(o))
                    return false; // An input, or a parameter's insides

                if (theirs.count(o))
                    continue;

                // Else made by itself: reached by no global
                if (reachedBy.count(o))
                {
                    return false;
                }
            }

            return true;
        }

        std::string ProgramEffects::factsOf(const std::string& name) const
        {
            std::ostringstream out;

            if (inferred.count(name))
                out << 'c';

            if (invariant(name))
                out << 'i';

            auto f = summaries.find(name);

            if (f != summaries.end())
                out << 'f' << f->second.digest();

            // What it reaches, and which other globals reach some of it
            auto reach = reachOf.find(name);

            if (reach != reachOf.end() and not reach->second.empty())
            {
                std::set<std::string> sharing;

                for (const auto& o : reach->second)
                {
                    auto by = reachedBy.find(o);

                    if (by != reachedBy.end())
                        sharing.insert(by->second.begin(), by->second.end());
                }

                sharing.erase(name);

                out << 'r' << reach->second.size() << 's';
                for (const auto& g : sharing) out << g << ',';
            }

            return out.str();
        }
    }
}
