#include "stdafx.h"

#include "summaries.h"

#include "Interpreter/expression.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace pg
{
    namespace ast
    {
        namespace analysis
        {
            namespace
            {
                using LocSet = std::vector<LocId>;  // sorted, unique

                void sortUnique(LocSet& s)
                {
                    std::sort(s.begin(), s.end());
                    s.erase(std::unique(s.begin(), s.end()), s.end());
                }

                void join(Idempotence& into, const Idempotence& from)
                {
                    into.shape = into.shape and from.shape;
                    into.needsParams.insert(from.needsParams.begin(), from.needsParams.end());
                    into.needsGlobals.insert(from.needsGlobals.begin(), from.needsGlobals.end());
                }

                bool edgeLess(const Edge& a, const Edge& b)
                {
                    return a.at < b.at or (a.at == b.at and a.value < b.value);
                }

                bool edgeSame(const Edge& a, const Edge& b)
                {
                    return a.at == b.at and a.value == b.value;
                }

                // A native of the Fresh kind whose result is a table a body may write: a list of its own
                bool makesTable(const std::string& name)
                {
                    return name == "split" or name == "splitLines";
                }

                uint32_t arityOf(const BodyIR& body)
                {
                    uint32_t arity = 0;

                    for (const auto& n : body.nodes)
                    {
                        if (n.op == Op::Param)
                            arity = std::max(arity, n.param + 1);
                    }

                    return arity;
                }

                // Two summaries joined: each part the union, a write idempotent only when it is in both
                Summary joined(const Summary& a, const Summary& b)
                {
                    Summary out;
                    out.arity = std::max(a.arity, b.arity);

                    out.returns = a.returns;
                    out.returns.insert(out.returns.end(), b.returns.begin(), b.returns.end());
                    sortUnique(out.returns);

                    std::map<LocId, Idempotence> writes;

                    for (const auto* s : {&a, &b})
                    {
                        for (const auto& w : s->writes)
                        {
                            auto it = writes.find(w.at);

                            if (it == writes.end())
                                writes.emplace(w.at, w.idempotence);
                            else
                                join(it->second, w.idempotence);
                        }
                    }

                    for (auto& [at, idempotence] : writes)
                        out.writes.push_back(Write{at, std::move(idempotence)});

                    out.edges = a.edges;
                    out.edges.insert(out.edges.end(), b.edges.begin(), b.edges.end());
                    std::sort(out.edges.begin(), out.edges.end(), edgeLess);
                    out.edges.erase(std::unique(out.edges.begin(), out.edges.end(), edgeSame), out.edges.end());

                    out.reads = a.reads;
                    out.reads.insert(b.reads.begin(), b.reads.end());
                    out.rebinds = a.rebinds;
                    out.rebinds.insert(b.rebinds.begin(), b.rebinds.end());
                    out.unknown = a.unknown or b.unknown;
                    return out;
                }

                // One evaluation of one body
                class Run
                {
                public:
                    Run(Locations& locs, const std::map<std::string, Summary>& summaries,
                        const std::function<bool(const std::string&)>& followed,
                        const std::function<bool(const std::string&)>& defined, const BodyIR& body) :
                        locs(locs), summaries(summaries), followed(followed), defined(defined), body(body), pts(body.nodes.size())
                    {
                    }

                    Summary go(size_t& collapsed)
                    {
                        // The points-to sets: evaluated in id order, again until none grows (the loop phis)
                        bool grew = true;

                        while (grew)
                        {
                            grew = false;

                            for (ValueId v = 0; v < body.nodes.size(); ++v)
                                grew = grow(v, value(v)) or grew;
                        }

                        collapsed += collapsedValues.size();

                        effects();

                        Summary out;
                        out.arity = arityOf(body);
                        out.reads = std::move(reads);
                        out.rebinds = std::move(rebinds);
                        out.unknown = unknown;

                        for (auto r : body.returns)
                            out.returns.insert(out.returns.end(), pts[r].begin(), pts[r].end());

                        sortUnique(out.returns);

                        out.edges.assign(edges.begin(), edges.end());
                        std::sort(out.edges.begin(), out.edges.end(), edgeLess);
                        out.edges.erase(std::unique(out.edges.begin(), out.edges.end(), edgeSame), out.edges.end());

                        decideWrites(out);
                        return out;
                    }

                private:
                    // A write found, its idempotence decided once every write of the body is known
                    struct Pending
                    {
                        enum class From : uint8_t { Store, Call, Never };

                        LocId at;
                        From from;
                        size_t store = 0;        // Store: its index in the body's stores
                        ValueId call = 0;        // Call: the call node
                        Idempotence callee;      // Call: the callee's, its params not yet put in place
                    };

                    Locations& locs;
                    const std::map<std::string, Summary>& summaries;
                    const std::function<bool(const std::string&)>& followed;
                    const std::function<bool(const std::string&)>& defined;
                    const BodyIR& body;

                    std::vector<LocSet> pts;
                    std::set<ValueId> collapsedValues;

                    std::vector<Pending> pending;
                    LocSet written;  // every location the body writes, known once effects() has collected them
                    std::vector<Edge> edges;
                    std::set<std::string> reads;
                    std::set<std::string> rebinds;
                    bool unknown = false;

                    bool isUnknown(LocId l) const { return locs.rootKind(l) == RootKind::Unknown; }

                    // A field of a location; the unknown has no fields of its own, it is all of them
                    LocId childOf(LocId l, KeyId k) { return isUnknown(l) ? locs.unknown() : locs.child(l, k); }

                    void normalize(LocSet& s)
                    {
                        sortUnique(s);

                        for (auto l : s)
                        {
                            if (isUnknown(l))
                            {
                                s = {locs.unknown()};
                                return;
                            }
                        }
                    }

                    // The points-to set of `v` grown by `next`; past MaxLocsPerValue it is the unknown
                    bool grow(ValueId v, LocSet next)
                    {
                        LocSet& cur = pts[v];

                        if (cur.size() == 1 and isUnknown(cur[0]))
                            return false;

                        next.insert(next.end(), cur.begin(), cur.end());
                        normalize(next);

                        if (next.size() > MaxLocsPerValue)
                        {
                            next = {locs.unknown()};
                            collapsedValues.insert(v);
                        }

                        if (next == cur)
                            return false;

                        cur = std::move(next);
                        return true;
                    }

                    const Summary* calleeOf(const Node& n) const
                    {
                        if (n.op != Op::Call or n.name.empty() or not followed(n.name))
                            return nullptr;

                        auto it = summaries.find(n.name);
                        return it == summaries.end() ? nullptr : &it->second;
                    }

                    // A call not followed that runs code of the program (a value, a function not followed):
                    // that code may reach any global. A name the program does not define is an engine native,
                    // which sees only what it is handed
                    bool runsProgramCode(const Node& n) const
                    {
                        return n.name.empty() or followed(n.name) or defined(n.name);
                    }

                    LocId siteOf(const Node& n) { return locs.site(n.origin ? n.origin : static_cast<const void*>(&n)); }

                    // Every location of every operand of a node
                    LocSet operandLocs(const Node& n) const
                    {
                        LocSet all;

                        for (auto in : n.in)
                            all.insert(all.end(), pts[in].begin(), pts[in].end());

                        sortUnique(all);
                        return all;
                    }

                    LocSet argument(const Node& n, size_t i) const { return i < n.in.size() ? pts[n.in[i]] : LocSet{}; }

                    // What a callee's parameter key names at a call: the string constants the argument may
                    // be, a parameter of this body, else any key
                    std::vector<KeyId> keysAt(KeyId k, const Node& call)
                    {
                        if (not locs.isParamKey(k))
                            return {k};

                        const uint32_t p = locs.paramOfKey(k);

                        if (p >= call.in.size())
                            return {Locations::AnyKey};

                        std::vector<KeyId> out;

                        for (auto leaf : body.leaves(call.in[p]))
                        {
                            const Node& n = body.nodes[leaf];

                            if (n.op == Op::Const and n.name.size() >= 2 and n.name.front() == '"' and n.name.back() == '"')
                                out.push_back(locs.key(n.name.substr(1, n.name.size() - 2)));
                            else if (n.op == Op::Param)
                                out.push_back(locs.paramKey(n.param));
                            else
                                return {Locations::AnyKey};
                        }

                        if (out.empty())
                            return {Locations::AnyKey};

                        std::sort(out.begin(), out.end());
                        out.erase(std::unique(out.begin(), out.end()), out.end());
                        return out;
                    }

                    // A location of a callee's summary, seen from a call: its parameter root becomes each
                    // location of the argument, its parameter keys the keys the arguments name
                    LocSet substitute(LocId l, const Node& call)
                    {
                        std::vector<KeyId> path;
                        LocId root = l;

                        while (locs.depth(root) > 0)
                        {
                            path.push_back(locs.keyOf(root));
                            root = locs.parent(root);
                        }

                        std::reverse(path.begin(), path.end());

                        LocSet at;

                        switch (locs.rootKind(root))
                        {
                            case RootKind::Unknown: return {locs.unknown()};
                            case RootKind::Param: at = argument(call, locs.keyOf(root)); break;
                            default: at = {root}; break;
                        }

                        for (auto k : path)
                        {
                            const auto keys = keysAt(k, call);
                            LocSet next;

                            for (auto base : at)
                            {
                                for (auto key : keys)
                                    next.push_back(childOf(base, key));
                            }

                            normalize(next);
                            at = std::move(next);
                        }

                        normalize(at);
                        return at;
                    }

                    // What a call not followed may return: anything when it runs code of the program, else
                    // all it is handed and all reachable from it
                    LocSet unknownResult(const Node& n)
                    {
                        if (runsProgramCode(n))
                            return {locs.unknown()};

                        LocSet out;

                        for (auto l : operandLocs(n))
                        {
                            out.push_back(l);
                            out.push_back(childOf(l, Locations::AnyKey));
                        }

                        return out;
                    }

                    LocSet value(ValueId v)
                    {
                        const Node& n = body.nodes[v];
                        LocSet out;

                        switch (n.op)
                        {
                            case Op::Const:
                            case Op::Pure:
                                break;

                            case Op::Param: out.push_back(locs.param(n.param)); break;
                            case Op::Global: out.push_back(locs.global(n.name)); break;
                            case Op::Literal: out.push_back(n.site); break;
                            case Op::Unknown: out.push_back(locs.unknown()); break;

                            case Op::Field:
                            {
                                for (auto l : argument(n, 0))
                                    out.push_back(childOf(l, n.key));

                                break;
                            }

                            case Op::Phi: out = operandLocs(n); break;

                            case Op::Call:
                            {
                                if (const Summary* callee = calleeOf(n))
                                {
                                    for (auto r : callee->returns)
                                    {
                                        const auto at = substitute(r, n);
                                        out.insert(out.end(), at.begin(), at.end());
                                    }
                                }
                                else
                                    out = unknownResult(n);

                                break;
                            }

                            case Op::Native:
                            {
                                switch (nativeKind(n.name))
                                {
                                    case NativeKind::Fresh:
                                    {
                                        if (makesTable(n.name))
                                            out.push_back(siteOf(n));

                                        break;
                                    }

                                    case NativeKind::Lookup:
                                    {
                                        for (auto l : argument(n, 0))
                                            out.push_back(childOf(l, Locations::AnyKey));

                                        const auto fallback = argument(n, 2);
                                        out.insert(out.end(), fallback.begin(), fallback.end());
                                        break;
                                    }

                                    case NativeKind::Entry:
                                    case NativeKind::Take:
                                    {
                                        for (auto l : argument(n, 0))
                                            out.push_back(childOf(l, Locations::AnyKey));

                                        break;
                                    }

                                    case NativeKind::Copy:
                                    case NativeKind::IndexBy:
                                        out.push_back(siteOf(n));
                                        break;

                                    // What they return is taken as the first, which they write
                                    case NativeKind::Push:
                                    case NativeKind::Insert:
                                    case NativeKind::Merge:
                                        out = argument(n, 0);
                                        break;

                                    case NativeKind::None:
                                        out = unknownResult(n);
                                        break;
                                }

                                break;
                            }
                        }

                        return out;
                    }

                    void write(LocId at, Pending::From from, size_t store = 0, ValueId call = 0, const Idempotence* callee = nullptr)
                    {
                        Pending p{at, from, store, call, {}};

                        if (callee)
                            p.callee = *callee;

                        pending.push_back(std::move(p));
                    }

                    // An object stored at a location. One stored in its own location is kept: a path may name
                    // several objects (entries, or a slot rewritten), and one of them has moved to another
                    void edge(LocId at, LocId value)
                    {
                        edges.push_back(Edge{at, value});
                    }

                    // A call not followed: it may write all it is handed, and store any of it in any other;
                    // when it runs code of the program, it may write anything
                    void unknownCall(const Node& n)
                    {
                        unknown = true;

                        if (runsProgramCode(n))
                            write(locs.unknown(), Pending::From::Never);

                        const LocSet all = operandLocs(n);

                        for (auto a : all)
                        {
                            const LocId inside = childOf(a, Locations::AnyKey);
                            write(inside, Pending::From::Never);

                            for (auto b : all)
                            {
                                edge(inside, b);
                                edge(inside, childOf(b, Locations::AnyKey));
                            }
                        }
                    }

                    void applyCallee(ValueId v, const Summary& callee)
                    {
                        const Node& n = body.nodes[v];

                        reads.insert(callee.reads.begin(), callee.reads.end());
                        rebinds.insert(callee.rebinds.begin(), callee.rebinds.end());
                        unknown = unknown or callee.unknown;

                        for (const auto& w : callee.writes)
                        {
                            for (auto at : substitute(w.at, n))
                                write(at, Pending::From::Call, 0, v, &w.idempotence);
                        }

                        for (const auto& e : callee.edges)
                        {
                            const LocSet values = substitute(e.value, n);

                            for (auto at : substitute(e.at, n))
                            {
                                for (auto value : values)
                                    edge(at, value);
                            }
                        }
                    }

                    // What the body does to the world, once the points-to sets are known
                    void effects()
                    {
                        for (ValueId v = 0; v < body.nodes.size(); ++v)
                        {
                            const Node& n = body.nodes[v];

                            if (n.op == Op::Global)
                                reads.insert(n.name);
                            else if (n.op == Op::Call)
                            {
                                if (const Summary* callee = calleeOf(n))
                                    applyCallee(v, *callee);
                                else
                                    unknownCall(n);
                            }
                            else if (n.op == Op::Native)
                            {
                                switch (nativeKind(n.name))
                                {
                                    // Their store of what they put in is in the body's stores
                                    case NativeKind::Push:
                                    case NativeKind::Insert:
                                    case NativeKind::Merge:
                                    case NativeKind::Take:
                                    {
                                        for (auto l : argument(n, 0))
                                            write(childOf(l, Locations::AnyKey), Pending::From::Never);

                                        break;
                                    }

                                    case NativeKind::Copy:
                                    case NativeKind::IndexBy:
                                    {
                                        const LocId inside = childOf(siteOf(n), Locations::AnyKey);

                                        for (auto l : argument(n, 0))
                                            edge(inside, childOf(l, Locations::AnyKey));

                                        break;
                                    }

                                    case NativeKind::None:
                                        unknownCall(n);
                                        break;

                                    default:
                                        break;
                                }
                            }
                        }

                        for (size_t i = 0; i < body.stores.size(); ++i)
                        {
                            for (auto l : pts[body.stores[i].base])
                                write(childOf(l, body.stores[i].key), Pending::From::Store, i);
                        }

                        if (body.unknown)
                        {
                            unknown = true;
                            write(locs.unknown(), Pending::From::Never);
                        }

                        // Every write of the body is known: what depends on them may be decided
                        for (const auto& p : pending)
                            written.push_back(p.at);

                        sortUnique(written);

                        for (const auto& s : body.stores)
                        {
                            // The field's own old value written back makes no new alias
                            LocSet stored;

                            for (auto leaf : newLeaves(s))
                                stored.insert(stored.end(), pts[leaf].begin(), pts[leaf].end());

                            sortUnique(stored);

                            for (auto l : pts[s.base])
                            {
                                const LocId at = childOf(l, s.key);

                                for (auto value : stored)
                                    edge(at, value);
                            }
                        }

                        for (const auto& [name, value] : body.rebinds)
                        {
                            rebinds.insert(name);

                            for (auto l : pts[value])
                                edge(locs.global(name), l);
                        }

                        for (const auto& [name, value] : body.defines)
                        {
                            for (auto l : pts[value])
                                edge(locs.global(name), l);
                        }
                    }

                    // The value a field node is read with, when it is read by `t[i]`: `i`'s value
                    bool indexOf(ValueId f, ValueId& index) const
                    {
                        const Node& n = body.nodes[f];

                        if (not n.origin)
                            return false;

                        // Only an expression whose value is this very node is looked into
                        const auto* e = static_cast<const Expression*>(n.origin);
                        auto it = body.valueOf.find(e);

                        if (it == body.valueOf.end() or it->second != f or e->kind() != AstKind::IndexGet)
                            return false;

                        const auto* get = static_cast<const IndexGet*>(e);
                        auto at = body.valueOf.find(get->index.get());

                        if (at == body.valueOf.end())
                            return false;

                        index = at->second;
                        return true;
                    }

                    // A write of the slot a field reads, or above it: the field may read another object after it.
                    // A write below the slot leaves the object in place
                    bool slotRewritten(ValueId field) const
                    {
                        for (auto l : pts[field])
                        {
                            for (auto w : written)
                            {
                                if (locs.depth(w) <= locs.depth(l) and locs.overlaps(w, l))
                                    return true;
                            }
                        }

                        return false;
                    }

                    // Two values that are the one object: the same value, or the same field of the one object,
                    // a slot this body does not rewrite (a field of a key not known only when both are read with
                    // the same index value)
                    bool sameObject(ValueId a, ValueId b) const
                    {
                        if (a == b)
                            return true;

                        const Node& na = body.nodes[a];
                        const Node& nb = body.nodes[b];

                        if (na.op != Op::Field or nb.op != Op::Field or na.key != nb.key or na.in.empty() or nb.in.empty())
                            return false;

                        if (na.key == Locations::AnyKey)
                        {
                            ValueId ia, ib;

                            if (not indexOf(a, ia) or not indexOf(b, ib) or ia != ib)
                                return false;
                        }

                        return not slotRewritten(a) and not slotRewritten(b) and sameObject(na.in[0], nb.in[0]);
                    }

                    // A store's value that is the field it writes, read before: `t.f = lookup(t, "f", 1)`
                    bool ownOldValue(ValueId leaf, const Store& s) const
                    {
                        const Node& n = body.nodes[leaf];
                        return n.op == Op::Field and n.key == s.key and not n.in.empty() and sameObject(n.in[0], s.base);
                    }

                    // A loop's header phi: fed by the end of the loop's body, which comes after it
                    bool loopPhi(ValueId v) const
                    {
                        const Node& n = body.nodes[v];
                        return n.op == Op::Phi and std::any_of(n.in.begin(), n.in.end(), [v](ValueId in) { return in > v; });
                    }

                    // The leaves of a value that may come from an earlier turn of a loop: all behind a header phi.
                    // One SSA id stands for every turn, so such a leaf is not what the field holds now
                    std::set<ValueId> carriedLeaves(ValueId v) const
                    {
                        std::set<ValueId> out;
                        std::vector<bool> seen(body.nodes.size(), false);
                        std::vector<ValueId> todo{v};

                        while (not todo.empty())
                        {
                            const ValueId at = todo.back();
                            todo.pop_back();

                            if (seen[at] or body.nodes[at].op != Op::Phi)
                                continue;

                            seen[at] = true;

                            if (loopPhi(at))
                            {
                                const auto behind = body.leaves(at);
                                out.insert(behind.begin(), behind.end());
                            }
                            else
                                todo.insert(todo.end(), body.nodes[at].in.begin(), body.nodes[at].in.end());
                        }

                        return out;
                    }

                    // The leaves of a store's value, without the field's own old value
                    std::vector<ValueId> newLeaves(const Store& s) const
                    {
                        const auto carried = carriedLeaves(s.value);
                        std::vector<ValueId> out;

                        for (auto leaf : body.leaves(s.value))
                        {
                            if (carried.count(leaf) or not ownOldValue(leaf, s))
                                out.push_back(leaf);
                        }

                        return out;
                    }

                    // A slot whose write may be idempotent: not the unknown, not deep, not of a key not known
                    bool mayBeIdempotent(LocId at) const
                    {
                        return not isUnknown(at) and locs.depth(at) > 0 and not locs.deep(at) and locs.keyOf(at) != Locations::AnyKey;
                    }

                    bool writtenHere(const LocSet& at) const
                    {
                        for (auto l : at)
                        {
                            for (auto w : written)
                            {
                                if (locs.overlaps(l, w))
                                    return true;
                            }
                        }

                        return false;
                    }

                    // Made only of what is the same on every run (the arguments and globals it needs noted);
                    // `state`: 1 while looked at (a cycle is not shown invariant), 2 invariant, 3 not
                    bool invariantMade(ValueId v, Idempotence& out, std::map<ValueId, int>& state) const
                    {
                        for (auto leaf : body.leaves(v))
                        {
                            if (not invariantLeaf(leaf, out, state))
                                return false;
                        }

                        return true;
                    }

                    void noteNeed(const Node& n, Idempotence& out) const
                    {
                        if (n.op == Op::Param)
                            out.needsParams.insert(n.param);
                        else
                            out.needsGlobals.insert(n.name);
                    }

                    // The base of a field read: only its reference must be the same on every run, the field
                    // read itself is what must not be written
                    bool invariantBase(ValueId base, Idempotence& out, std::map<ValueId, int>& state) const
                    {
                        for (auto leaf : body.leaves(base))
                        {
                            const Node& n = body.nodes[leaf];

                            if (n.op == Op::Param or n.op == Op::Global)
                                noteNeed(n, out);
                            else if (not invariantLeaf(leaf, out, state))
                                return false;
                        }

                        return true;
                    }

                    bool invariantLeaf(ValueId v, Idempotence& out, std::map<ValueId, int>& state) const
                    {
                        auto it = state.find(v);

                        if (it != state.end())
                            return it->second == 2;

                        state[v] = 1;

                        const Node& n = body.nodes[v];
                        bool fine = false;

                        switch (n.op)
                        {
                            case Op::Const: fine = true; break;

                            // What this body writes is not the same on every run, even handed in
                            case Op::Param:
                            case Op::Global:
                                noteNeed(n, out);
                                fine = not writtenHere(pts[v]);
                                break;

                            // A native making a table makes a new one each run, as a literal does
                            case Op::Native:
                            case Op::Pure:
                            {
                                fine = n.op == Op::Pure or (nativeKind(n.name) == NativeKind::Fresh and not makesTable(n.name));

                                for (auto in : n.in)
                                    fine = fine and invariantMade(in, out, state);

                                break;
                            }

                            case Op::Field:
                                fine = not n.in.empty() and invariantBase(n.in[0], out, state) and not writtenHere(pts[v]);
                                break;

                            default:
                                break;
                        }

                        state[v] = fine ? 2 : 3;
                        return fine;
                    }

                    Idempotence decide(const Pending& p) const
                    {
                        Idempotence out;

                        if (p.from == Pending::From::Never or not mayBeIdempotent(p.at))
                            return out;

                        std::map<ValueId, int> state;

                        if (p.from == Pending::From::Store)
                        {
                            const Store& s = body.stores[p.store];
                            out.shape = true;

                            for (auto leaf : newLeaves(s))
                            {
                                if (not invariantLeaf(leaf, out, state))
                                {
                                    out.shape = false;
                                    break;
                                }
                            }

                            return out;
                        }

                        // A callee's write: the arguments it needs must be invariant-made here
                        const Node& call = body.nodes[p.call];
                        out.shape = p.callee.shape;
                        out.needsGlobals = p.callee.needsGlobals;

                        for (const auto& name : p.callee.needsGlobals)
                            out.shape = out.shape and not writtenHere({locs.global(name)});

                        for (auto param : p.callee.needsParams)
                        {
                            if (not out.shape)
                                break;

                            if (param < call.in.size() and not invariantMade(call.in[param], out, state))
                                out.shape = false;
                        }

                        return out;
                    }

                    void decideWrites(Summary& out) const
                    {
                        std::map<LocId, Idempotence> writes;

                        for (const auto& p : pending)
                        {
                            const Idempotence decided = decide(p);
                            auto it = writes.find(p.at);

                            if (it == writes.end())
                                writes.emplace(p.at, decided);
                            else
                                join(it->second, decided);
                        }

                        for (auto& [at, idempotence] : writes)
                            out.writes.push_back(Write{at, std::move(idempotence)});
                    }
                };

                std::string idempotenceText(const Idempotence& i)
                {
                    std::string out = i.shape ? "=" : "!";

                    for (auto p : i.needsParams)
                        out += " #" + std::to_string(p);

                    for (const auto& g : i.needsGlobals)
                        out += " g:" + g;

                    return out;
                }

                std::string listed(std::vector<std::string> texts)
                {
                    std::sort(texts.begin(), texts.end());
                    std::string out;

                    for (const auto& t : texts)
                        out += (out.empty() ? "" : ",") + t;

                    return out;
                }

                // The strongly connected components of the call graph, callees before their callers (Tarjan)
                std::vector<std::vector<size_t>> components(const std::vector<std::vector<size_t>>& calls)
                {
                    const size_t n = calls.size();
                    const size_t unseen = static_cast<size_t>(-1);

                    std::vector<size_t> index(n, unseen), low(n, 0);
                    std::vector<bool> onStack(n, false);
                    std::vector<size_t> stack;
                    std::vector<std::vector<size_t>> out;
                    size_t next = 0;

                    // The depth-first walk without recursion: each frame is a function and its next callee
                    std::vector<std::pair<size_t, size_t>> frames;

                    for (size_t start = 0; start < n; ++start)
                    {
                        if (index[start] != unseen)
                            continue;

                        frames.push_back({start, 0});
                        index[start] = low[start] = next++;
                        stack.push_back(start);
                        onStack[start] = true;

                        while (not frames.empty())
                        {
                            auto& [v, i] = frames.back();

                            if (i < calls[v].size())
                            {
                                const size_t w = calls[v][i++];

                                if (index[w] == unseen)
                                {
                                    index[w] = low[w] = next++;
                                    stack.push_back(w);
                                    onStack[w] = true;
                                    frames.push_back({w, 0});
                                }
                                else if (onStack[w])
                                    low[v] = std::min(low[v], index[w]);

                                continue;
                            }

                            const size_t done = v;
                            frames.pop_back();

                            if (not frames.empty())
                                low[frames.back().first] = std::min(low[frames.back().first], low[done]);

                            if (low[done] == index[done])
                            {
                                std::vector<size_t> component;
                                size_t w;

                                do
                                {
                                    w = stack.back();
                                    stack.pop_back();
                                    onStack[w] = false;
                                    component.push_back(w);
                                } while (w != done);

                                out.push_back(std::move(component));
                            }
                        }
                    }

                    return out;
                }
            }

            std::string Summary::digest(const Locations& locs) const
            {
                std::vector<std::string> returned, written, stored;

                for (auto r : returns)
                    returned.push_back(locs.text(r));

                for (const auto& w : writes)
                    written.push_back(locs.text(w.at) + idempotenceText(w.idempotence));

                for (const auto& e : edges)
                    stored.push_back(locs.text(e.at) + "<-" + locs.text(e.value));

                std::ostringstream out;
                out << "arity " << arity << "; returns " << listed(returned) << "; writes " << listed(written) << "; edges " << listed(stored)
                    << "; reads " << listed({reads.begin(), reads.end()}) << "; rebinds " << listed({rebinds.begin(), rebinds.end()})
                    << (unknown ? "; unknown" : "");
                return out.str();
            }

            Evaluator::Evaluator(Locations& locs, const std::map<std::string, Summary>& summaries,
                                 std::function<bool(const std::string&)> followed, std::function<bool(const std::string&)> defined) :
                locs(locs), summaries(summaries), followed(std::move(followed)), defined(std::move(defined))
            {
            }

            Summary Evaluator::evaluate(const BodyIR& body)
            {
                return Run(locs, summaries, followed, defined, body).go(collapsedCount);
            }

            size_t Evaluator::collapsed() const
            {
                return collapsedCount;
            }

            std::map<std::string, Summary> summarize(const std::map<std::string, BodyIR>& bodies, Locations& locs,
                                                     const std::function<bool(const std::string&)>& followed,
                                                     const std::function<bool(const std::string&)>& defined, size_t& collapsed)
            {
                collapsed = 0;

                std::vector<std::string> names;
                std::map<std::string, size_t> indexOf;

                for (const auto& [name, body] : bodies)
                {
                    if (followed(name))
                    {
                        indexOf[name] = names.size();
                        names.push_back(name);
                    }
                }

                // The call graph over the functions followed
                std::vector<std::vector<size_t>> calls(names.size());
                std::vector<bool> selfCall(names.size(), false);

                for (size_t f = 0; f < names.size(); ++f)
                {
                    std::set<size_t> callees;

                    for (const auto& n : bodies.at(names[f]).nodes)
                    {
                        if (n.op != Op::Call or n.name.empty())
                            continue;

                        auto it = indexOf.find(n.name);

                        if (it != indexOf.end())
                            callees.insert(it->second);
                    }

                    selfCall[f] = callees.count(f) > 0;
                    calls[f].assign(callees.begin(), callees.end());
                }

                std::map<std::string, Summary> out;

                for (const auto& component : components(calls))
                {
                    if (component.size() == 1 and not selfCall[component[0]])
                    {
                        const std::string& name = names[component[0]];
                        Evaluator evaluator(locs, out, followed, defined);
                        out[name] = evaluator.evaluate(bodies.at(name));
                        collapsed += evaluator.collapsed();
                        continue;
                    }

                    // A cycle: from nothing, each summary joined with its next evaluation until none changes.
                    // Summaries only grow over finitely many locations (bounded paths), so this ends.
                    std::map<std::string, std::string> digests;
                    std::map<std::string, size_t> lastCollapsed;

                    for (auto f : component)
                    {
                        Summary empty;
                        empty.arity = arityOf(bodies.at(names[f]));
                        digests[names[f]] = empty.digest(locs);
                        out[names[f]] = std::move(empty);
                    }

                    bool changed = true;

                    while (changed)
                    {
                        changed = false;

                        for (auto f : component)
                        {
                            const std::string& name = names[f];
                            Evaluator evaluator(locs, out, followed, defined);
                            Summary next = joined(out[name], evaluator.evaluate(bodies.at(name)));
                            lastCollapsed[name] = evaluator.collapsed();

                            std::string digest = next.digest(locs);

                            if (digest != digests[name])
                            {
                                changed = true;
                                digests[name] = std::move(digest);
                            }

                            out[name] = std::move(next);
                        }
                    }

                    for (const auto& [name, count] : lastCollapsed)
                        collapsed += count;
                }

                return out;
            }
        }
    }
}
