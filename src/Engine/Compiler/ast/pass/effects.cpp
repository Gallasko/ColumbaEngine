#include "stdafx.h"

#include "effects.h"

#include "ast_analysis.h"

#include "logger.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <iterator>
#include <utility>

namespace pg
{
    namespace ast
    {
        using namespace analysis;

        namespace
        {
            const char* DOM = "Effects";

            void sortUnique(std::vector<LocId>& s)
            {
                std::sort(s.begin(), s.end());
                s.erase(std::unique(s.begin(), s.end()), s.end());
            }

            // A function that runs behind a field read or a loop, which no body shows as a call
            bool isMetamethod(const std::string& name)
            {
                return name == "__get" or name == "__set" or name == "__iter" or name == "__next";
            }

            // Whether a class is defined anywhere but at the top level
            bool definesClassBelow(const StatementPtr& s)
            {
                if (s->kind() != AstKind::ClassStatement)
                    return holdsStatement(s, AstKind::ClassStatement, true);

                for (auto methods = std::static_pointer_cast<ClassStatement>(s)->methods; not methods.empty(); methods.pop())
                {
                    if (holdsStatement(methods.front()->body, AstKind::ClassStatement, true))
                        return true;
                }

                return false;
            }

            // Why a program's classes are not all that runs behind what they show, "" when they are.
            // `functions`: what the program can call by name
            std::string whyNotFollowed(const std::vector<std::vector<StatementPtr>>& files, const ClassModel& model, const std::map<std::string, int>& functions)
            {
                std::string why;

                auto isMethodOrMetamethod = [&](const std::string& name) {
                    return isMetamethod(name) or model.methods.count(name) > 0;
                };

                for (const auto& [method, runs] : model.methods)
                {
                    if (isMetamethod(method))
                        return "a class's metamethod runs behind what the code shows";
                }

                std::unordered_set<const Expression*> called;

                const ExprVisit look = [&](const ExprPtr& e) {
                    const AstKind type = e->kind();

                    if (type == AstKind::CallExpression)
                        called.insert(std::static_pointer_cast<CallExpression>(e)->caller.get());
                    else if (type == AstKind::Set and isMethodOrMetamethod(std::static_pointer_cast<Set>(e)->name.text))
                        why = "a class has a method or a metamethod of the name of a field: `" + std::static_pointer_cast<Set>(e)->name.text + "`";
                    else if (type == AstKind::Atom)
                    {
                        const auto& value = std::static_pointer_cast<Atom>(e)->value;

                        if (value.type == UnionType::STRING and isMethodOrMetamethod(value.toString()))
                            why = "a class has a method or a metamethod of the name of a field: `" + value.toString() + "`";
                    }
                    else if (type == AstKind::List)
                    {
                        auto entries = std::static_pointer_cast<List>(e)->entries;

                        for (; not entries.empty(); entries.pop())
                        {
                            const auto& entry = entries.front();

                            if (entry.explicitKey and entry.key and isMethodOrMetamethod(entry.key->getName()))
                                why = "a class has a method or a metamethod of the name of a field: `" + entry.key->getName() + "`";
                        }
                    }
                    else if (type == AstKind::Var and not called.count(e.get()))
                    {
                        const std::string& name = std::static_pointer_cast<Var>(e)->name.text;

                        // In a field, it is what a call of a method may run
                        if (functions.count(name) or model.classes.count(name))
                            why = "a class's program hands a function around: `" + name + "`";
                    }
                };

                for (const auto& file : files)
                {
                    for (const auto& s : file)
                    {
                        eachExpr(s, look, true);

                        if (not why.empty())
                            return why;
                    }
                }

                return "";
            }

            // What a statement declares at the top level, "" when nothing
            std::string declaredBy(const StatementPtr& s)
            {
                switch (s->kind())
                {
                    case AstKind::VariableStatement: return std::static_pointer_cast<VariableStatement>(s)->name.text;
                    case AstKind::FunctionStatement: return std::static_pointer_cast<FunctionStatement>(s)->name.text;
                    case AstKind::ClassStatement: return std::static_pointer_cast<ClassStatement>(s)->name.text;
                    default: return "";
                }
            }

            // The same statements by kind and name, at every index
            bool shapeIs(const std::vector<StatementPtr>& a, const std::vector<StatementPtr>& b)
            {
                return a.size() == b.size() and std::equal(a.begin(), a.end(), b.begin(), [](const StatementPtr& x, const StatementPtr& y)
                {
                    return x->kind() == y->kind() and declaredBy(x) == declaredBy(y);
                });
            }

            // A list's entry: a key that is an index
            bool isIndex(const Locations& locs, LocId l)
            {
                const std::string text = locs.text(l);
                const size_t dot = text.rfind('.');
                return dot != std::string::npos and dot + 1 < text.size() and
                       std::all_of(text.begin() + dot + 1, text.end(), [](unsigned char c) { return std::isdigit(c); });
            }

            std::vector<std::string> textsOf(const Locations& locs, const std::vector<LocId>& at)
            {
                std::vector<std::string> out;

                for (auto l : at)
                    out.push_back(locs.text(l));

                std::sort(out.begin(), out.end());
                return out;
            }
        }

        ObjectSet::ObjectSet(std::vector<LocId> objects) : items(std::move(objects))
        {
            sortUnique(items);
        }

        size_t ObjectSet::count(LocId object) const
        {
            return std::binary_search(items.begin(), items.end(), object) ? 1 : 0;
        }

        bool ObjectSet::insert(LocId object)
        {
            auto at = std::lower_bound(items.begin(), items.end(), object);

            if (at != items.end() and *at == object)
                return false;

            items.insert(at, object);
            return true;
        }

        bool ObjectSet::insert(const ObjectSet& objects)
        {
            if (std::includes(items.begin(), items.end(), objects.items.begin(), objects.items.end()))
                return false;

            std::vector<LocId> both;
            both.reserve(items.size() + objects.items.size());
            std::set_union(items.begin(), items.end(), objects.items.begin(), objects.items.end(), std::back_inserter(both));
            items = std::move(both);
            return true;
        }

        Summary ProgramEffects::evaluate(const BodyIR& body, size_t& collapsed) const
        {
            auto isFollowed = [this](const std::string& name) { return followed.count(name) > 0; };
            auto isDefined = [this](const std::string& name) { return defined.count(name) > 0; };

            Evaluator evaluator(locs, summaries, isFollowed, isDefined);
            Summary out = evaluator.evaluate(body);
            collapsed += evaluator.collapsed();
            return out;
        }

        const BodyIR* ProgramEffects::bodyOf(const std::string& function) const
        {
            const auto it = followedBodies.find(function);

            if (it != followedBodies.end())
                return &it->second;

            const BodyIR* found = nullptr;

            for (const auto& [name, body] : otherBodies)
            {
                if (name != function)
                    continue;

                // Which of them a call runs is not known
                if (found)
                    return nullptr;

                found = &body;
            }

            return found;
        }

        const BodyIR* ProgramEffects::bodyOf(const StatementPtr& statement) const
        {
            const auto it = statementBodyAt.find(statement.get());

            return it == statementBodyAt.end() ? nullptr : &statementBodies[it->second].second;
        }

        Summary ProgramEffects::summaryOfStatement(const StatementPtr& statement) const
        {
            if (not statement)
                return {};

            auto known = topLevel.find(statement.get());

            if (known != topLevel.end())
                return known->second;

            // Not one of the program's: it may do anything
            Summary anything;
            anything.unknown = true;
            anything.writes.push_back(Write{locs.unknown(), {}});
            return anything;
        }

        StatementEffects ProgramEffects::effectsFrom(const Summary& summary) const
        {
            StatementEffects effects;
            effects.reads = summary.reads;
            effects.rebinds = summary.rebinds;
            effects.unknown = summary.unknown;

            for (const auto& w : summary.writes)
                effects.writes.push_back(w.at);

            return effects;
        }

        bool ProgramEffects::isInput(LocId l) const
        {
            const LocId root = locs.rootOf(l);
            return locs.rootKind(root) == RootKind::Input or (locs.rootKind(root) == RootKind::Global and definedRoots.count(root) == 0);
        }

        void ProgramEffects::findHolders(const std::vector<Edge>& edges)
        {
            std::map<LocId, std::set<LocId>> storedAt;

            for (const auto& e : edges)
            {
                if (locs.rootKind(e.value) == RootKind::Site and locs.depth(e.value) == 0)
                    storedAt[e.value].insert(e.at);
            }

            for (const auto& [site, places] : storedAt)
            {
                if (places.size() == 1 and locs.rootKind(*places.begin()) != RootKind::Unknown and locs.rootKind(*places.begin()) != RootKind::Param)
                    holderOf[site] = *places.begin();
            }

            // A table stored inside itself keeps its own name: a path of holders must end
            for (const auto& [site, places] : storedAt)
            {
                std::unordered_set<LocId> seen{site};
                LocId root = site;

                for (auto holder = holderOf.find(root); holder != holderOf.end(); holder = holderOf.find(root))
                {
                    root = locs.rootOf(holder->second);

                    if (not seen.insert(root).second)
                    {
                        holderOf.erase(holder);
                        break;
                    }
                }
            }
        }

        LocId ProgramEffects::objectNamed(LocId site) const
        {
            auto memo = namedObjects.find(site);

            if (memo != namedObjects.end())
                return memo->second;

            // The path of its holders down from a root that is not one, entries as one key
            std::vector<LocId> ups;
            LocId root = site;

            for (auto holder = holderOf.find(root); holder != holderOf.end(); holder = holderOf.find(root))
            {
                ups.push_back(holder->second);
                root = locs.rootOf(holder->second);
            }

            LocId named = root;

            for (auto up = ups.rbegin(); up != ups.rend(); ++up)
            {
                std::vector<LocId> path;

                for (LocId at = *up; locs.depth(at) > 0; at = locs.parent(at))
                    path.push_back(at);

                for (auto at = path.rbegin(); at != path.rend(); ++at)
                    named = locs.child(named, isIndex(locs, *at) ? Locations::AnyKey : locs.keyOf(*at));
            }

            return namedObjects[site] = named;
        }

        bool ProgramEffects::reachesAll(LocId l) const
        {
            return locs.deep(l) or reachingAll.count(l) > 0;
        }

        bool ProgramEffects::addedNow(LocId object) const
        {
            if (object >= addedAt.size())
                addedAt.resize(std::max<size_t>(object + 1, addedAt.size() * 2), 0);

            if (addedAt[object] == addRound)
                return false;

            addedAt[object] = addRound;
            return true;
        }

        void ProgramEffects::addFieldsOf(LocId object, KeyId key, std::vector<LocId>& out) const
        {
            auto add = [&](LocId o)
            {
                if (addedNow(o))
                    out.push_back(o);
            };

            // Nothing is known of what is inside these
            if (locs.rootKind(object) == RootKind::Unknown or isInput(object))
                add(object);

            auto fields = heap.find(object);

            if (fields == heap.end())
                return;

            if (key == Locations::AnyKey)
            {
                for (const auto& [name, held] : fields->second)
                {
                    for (auto o : held)
                        add(o);
                }

                return;
            }

            for (const KeyId k : {key, Locations::AnyKey})
            {
                auto held = fields->second.find(k);

                if (held != fields->second.end())
                {
                    for (auto o : held->second)
                        add(o);
                }
            }
        }

        ProgramEffects::Objects ProgramEffects::reachableFrom(const Objects& objects) const
        {
            // Many locations hold the same objects
            auto memo = reachableOfSet.find(objects);

            if (memo != reachableOfSet.end())
                return memo->second;

            std::vector<LocId> reached(objects.begin(), objects.end());

            ++addRound;

            for (auto o : reached)
                addedNow(o);

            for (size_t at = 0; at < reached.size(); ++at)
            {
                auto fields = heap.find(reached[at]);

                if (fields == heap.end())
                    continue;

                for (const auto& [name, held] : fields->second)
                {
                    for (auto o : held)
                    {
                        if (addedNow(o))
                            reached.push_back(o);
                    }
                }
            }

            return reachableOfSet[objects] = Objects(std::move(reached));
        }

        const ProgramEffects::Objects& ProgramEffects::objectsAt(LocId l) const
        {
            auto memo = objectsMemo.find(l);

            if (memo != objectsMemo.end())
                return memo->second;

            Objects out;

            if (locs.depth(l) == 0)
            {
                // A global is an object of its own besides what it holds: what it is given and what
                // is stored in it when nothing shows it holds a table
                if (locs.rootKind(l) == RootKind::Site)
                    out.insert(objectNamed(l));
                else if (locs.rootKind(l) != RootKind::Param)
                    out.insert(l);

                auto held = holds.find(l);

                if (held != holds.end())
                    out.insert(held->second);
            }
            else
            {
                const Objects& parents = objectsAt(locs.parent(l));
                const KeyId key = locs.isParamKey(locs.keyOf(l)) ? Locations::AnyKey : locs.keyOf(l);

                // Many locations are the same field of the same objects
                auto [memoField, added] = fieldOfSet.try_emplace({parents, key});

                if (added)
                {
                    std::vector<LocId> found;

                    ++addRound;

                    for (auto o : parents)
                        addFieldsOf(o, key, found);

                    memoField->second = Objects(std::move(found));
                }

                out = memoField->second;
                out.insert(anywhere);

                if (locs.deep(l))
                    out = reachableFrom(out);
            }

            return objectsMemo[l] = std::move(out);
        }

        const ProgramEffects::Objects& ProgramEffects::reachableAt(LocId l) const
        {
            auto memo = reachableMemo.find(l);

            if (memo != reachableMemo.end())
                return memo->second;

            return reachableMemo[l] = reachableFrom(objectsAt(l));
        }

        ProgramEffects::Objects ProgramEffects::objectsOf(LocId write) const
        {
            if (locs.depth(write) == 0)
                return {write};

            Objects objects = reachesAll(write) ? reachableAt(locs.parent(write)) : objectsAt(locs.parent(write));

            // A slot of no object known: of a parameter, or of a value no table is seen in
            if (objects.empty())
                objects.insert(locs.rootOf(write));

            return objects;
        }

        ProgramEffects::Objects ProgramEffects::written(const std::vector<LocId>& writes) const
        {
            Objects out;

            for (auto w : writes)
            {
                out.insert(objectsOf(w));
            }

            return out;
        }

        namespace
        {
            // The edges of a location
            struct EdgesAt
            {
                LocId at;
                bool all;                   // It stands for all that is reachable
                std::vector<LocId> values;
                size_t read;                // How much it read when last made: what it reads only grows
            };
        }

        void ProgramEffects::resolveEdges(const std::vector<Edge>& all)
        {
            // The same edges come from every statement that calls the same functions
            std::unordered_set<uint64_t> seen;
            std::vector<Edge> edges;

            for (const auto& e : all)
            {
                if (seen.insert((static_cast<uint64_t>(e.at) << 32) | e.value).second)
                    edges.push_back(e);
            }

            findHolders(edges);

            // A location holding a value and all inside it: what a call not followed left, the two go
            // together through every summary. It stands for all that is reachable, a store for one slot
            for (const auto& e : edges)
            {
                if (locs.depth(e.at) == 0 or locs.keyOf(e.at) != Locations::AnyKey or locs.rootKind(e.value) == RootKind::Unknown)
                    continue;

                const LocId inside = locs.child(e.value, Locations::AnyKey);

                if (inside != e.value and seen.count((static_cast<uint64_t>(e.at) << 32) | inside))
                    reachingAll.insert(e.at);
            }

            // What each location is given, every edge of it at once: a location has thousands of them
            std::vector<EdgesAt> stores;
            std::unordered_map<LocId, size_t> storeAt;

            for (const auto& e : edges)
            {
                auto [it, added] = storeAt.try_emplace(e.at, stores.size());

                if (added)
                    stores.push_back({e.at, locs.depth(e.at) > 0 and reachesAll(e.at), {}, 0});

                stores[it->second].values.push_back(e.value);
            }

            // Each edge holds at every object its location may name, itself found through the edges.
            // All are read as the last round left them. What a store reads only grows: read the same
            // as when it was last made, it has nothing to add
            bool grew = true;

            while (grew)
            {
                grew = false;
                objectsMemo.clear();
                reachableMemo.clear();
                reachableOfSet.clear();
                fieldOfSet.clear();

                for (auto& store : stores)
                {
                    const bool all = store.all;
                    const LocId at = store.at;

                    auto given = [&](LocId value) -> const Objects& { return all ? reachableAt(value) : objectsAt(value); };

                    size_t read = 0;

                    for (auto value : store.values)
                        read += given(value).size();

                    const bool slot = locs.depth(at) > 0 and locs.rootKind(at) != RootKind::Unknown;

                    if (slot)
                        read += all ? reachableAt(locs.parent(at)).size() : objectsAt(locs.parent(at)).size();

                    if (read == store.read)
                        continue;

                    store.read = read;

                    // All read above: nothing below reads anew
                    std::vector<LocId> everyValue;

                    ++addRound;

                    for (auto value : store.values)
                    {
                        for (auto o : given(value))
                        {
                            if (addedNow(o))
                                everyValue.push_back(o);
                        }
                    }

                    const Objects values(std::move(everyValue));

                    if (values.empty())
                        continue;

                    if (locs.rootKind(at) == RootKind::Unknown)
                        grew = anywhere.insert(values) or grew;
                    else if (locs.depth(at) == 0)
                        grew = holds[at].insert(values) or grew;
                    else
                    {
                        const Objects& parents = objectsAt(locs.parent(at));

                        // A field named by a parameter may be any field
                        const KeyId key = all or locs.isParamKey(locs.keyOf(at)) ? Locations::AnyKey : locs.keyOf(at);

                        // No object known: it is in the global the path starts from
                        const Objects root = parents.empty() ? Objects{locs.rootOf(at)} : Objects();
                        const Objects& held = parents.empty() ? root : all ? reachableAt(locs.parent(at)) : parents;

                        for (auto o : held)
                        {
                            if (locs.rootKind(o) == RootKind::Unknown)
                                grew = anywhere.insert(values) or grew;
                            else
                                grew = heap[o][key].insert(values) or grew;
                        }
                    }
                }
            }
        }

        void ProgramEffects::findRegions()
        {
            // Every global: those defined, those assigned, those stored into
            std::set<std::string> globals = rebinds;

            for (const auto& [name, count] : defined)
                globals.insert(name);

            for (const auto& [object, fields] : heap)
            {
                if (locs.rootKind(object) == RootKind::Global)
                    globals.insert(locs.text(locs.rootOf(object)).substr(2));
            }

            for (const auto& name : globals)
            {
                Objects reached = objectsAt(locs.global(name));
                reached.insert(anywhere);
                reached = reachableFrom(reached);

                for (auto o : reached)
                    reachedBy[o].insert(name);

                regions[name] = std::move(reached);
            }
        }

        bool ProgramEffects::staysInOwnTables(LocId write, const std::vector<Edge>& edges) const
        {
            if (locs.rootKind(write) != RootKind::Site or locs.deep(write))
                return false;

            std::vector<KeyId> path;

            for (LocId l = write; locs.depth(l) > 0; l = locs.parent(l))
                path.push_back(locs.keyOf(l));

            // A step's holders: the tables it may hold, and the paths that name them
            std::set<LocId> holders{locs.rootOf(write)};

            for (size_t step = path.size() - 1; step > 0; --step)
            {
                const KeyId key = path[step];
                std::set<LocId> next;

                for (const auto& e : edges)
                {
                    if (locs.depth(e.at) == 0 or not holders.count(locs.parent(e.at)))
                        continue;

                    if (key != Locations::AnyKey and locs.keyOf(e.at) != Locations::AnyKey and locs.keyOf(e.at) != key)
                        continue;

                    if (locs.deep(e.at) or locs.rootKind(e.value) != RootKind::Site or locs.depth(e.value) != 0)
                        return false;

                    next.insert(e.at);
                    next.insert(e.value);
                }

                holders = std::move(next);
            }

            return true;
        }

        std::string ProgramEffects::placeOf(const Statement* statement, unsigned line) const
        {
            for (size_t f = 0; f < programFiles.size(); ++f)
            {
                for (const auto& s : programFiles[f])
                {
                    if (s.get() != statement)
                        continue;

                    const std::string origin = f < origins.size() ? origins[f].substr(0, origins[f].rfind('#')) : "";

                    return (origin.empty() ? "line " : origin + ":") + std::to_string(line);
                }
            }

            return "line " + std::to_string(line);
        }

        std::string ProgramEffects::madeOfChange(const std::string& name, const std::map<std::string, std::vector<StatementPtr>>& making, std::set<std::string>& seen) const
        {
            if (not seen.insert(name).second)
                return "";

            if (not invariant(name))
                return name;

            auto statements = making.find(name);

            if (statements == making.end())
                return "";

            for (const auto& s : statements->second)
            {
                for (const auto& read : effectsOf(s).reads)
                {
                    const std::string found = madeOfChange(read, making, seen);

                    if (not found.empty())
                        return found;
                }
            }

            return "";
        }

        void ProgramEffects::settleConsts(const std::vector<Objects>& writers, const std::unordered_map<const Statement*, size_t>& writerOf,
                                          const std::vector<std::string>& writerNames)
        {
            // Who writes what, by object
            std::unordered_map<LocId, std::vector<size_t>> writersOf;
            std::set<size_t> writeAnything;

            for (size_t writer = 0; writer < writers.size(); ++writer)
            {
                for (auto object : writers[writer])
                {
                    if (locs.rootKind(object) == RootKind::Unknown)
                        writeAnything.insert(writer);
                    else
                        writersOf[object].push_back(writer);
                }
            }

            // All that may write what a global reaches
            auto writersReaching = [&](const std::string& name)
            {
                std::set<size_t> out = writeAnything;

                for (auto object : regions[name])
                {
                    // It may be anything: all that is written may be in it
                    if (locs.rootKind(object) == RootKind::Unknown)
                    {
                        for (size_t writer = 0; writer < writers.size(); ++writer)
                        {
                            if (not writers[writer].empty())
                                out.insert(writer);
                        }

                        continue;
                    }

                    auto found = writersOf.find(object);

                    if (found != writersOf.end())
                        out.insert(found->second.begin(), found->second.end());
                }

                return out;
            };

            auto writtenElsewhere = [&](const std::string& name)
            {
                const auto reaching = writersReaching(name);
                return std::any_of(reaching.begin(), reaching.end(), [&](size_t writer) { return writer != writerOf.at(definitionOf[name]); });
            };

            for (const auto& name : consts)
            {
                if (definitions(name) == 1 and not rebinds.count(name))
                    invariantConsts.insert(name);
            }

            // Files that cannot be told apart: a module of them could not be given what was decided
            std::vector<size_t> sameShape;

            for (size_t f = 0; f < programFiles.size(); ++f)
            {
                for (size_t other = 0; other < programFiles.size(); ++other)
                {
                    if (f != other and shapeIs(programFiles[f], programFiles[other]))
                    {
                        sameShape.push_back(f);
                        break;
                    }
                }
            }

            fileJoins.resize(programFiles.size());
            undecidedFiles.insert(sameShape.begin(), sameShape.end());

            // A var of a file that runs more than once: each run of the file makes it anew, which only
            // shows when it holds an object (its identity). A scalar is as good as a const all the same
            std::set<std::string> newTableEachImport;

            for (size_t f = 0; f < programFiles.size(); ++f)
            {
                if (not runTwice[f])
                    continue;

                for (const auto& s : programFiles[f])
                {
                    const std::string name = s->kind() == AstKind::VariableStatement ? declaredBy(s) : "";

                    if (not name.empty() and holds.count(locs.global(name)))
                        newTableEachImport.insert(name);
                }
            }

            // What will join which const, by the walk the pass does
            std::unordered_map<size_t, std::set<std::string>> joinedAmong;

            auto makes = [&](size_t writer, const std::string& name)
            {
                auto joined = joinedAmong.find(writer);
                return writer == writerOf.at(definitionOf.at(name)) or (joined != joinedAmong.end() and joined->second.count(name) > 0);
            };

            // A constexpr is written by nothing else, so what it holds is done by the time anything
            // after it reads it: making it does not write what is made of it
            auto makesOrCameBefore = [&](size_t writer, const std::string& name)
            {
                return makes(writer, name) or std::any_of(constexprs.begin(), constexprs.end(), [&](const std::string& earlier)
                {
                    return earlier != name and ranAt.at(definitionOf.at(earlier)) < ranAt.at(definitionOf.at(name)) and makes(writer, earlier);
                });
            };

            bool lost = true;

            while (lost)
            {
                lost = false;
                inferred.clear();
                joinedAmong.clear();

                // The vars as good as consts: defined once, never rebound, what they reach written by
                // no other statement, their own made of what is the same on every run (found again as
                // each one found makes more of them so). A statement that writes anything may write
                // any of them
                bool found = writeAnything.empty();

                while (found)
                {
                    found = false;

                    for (const auto& name : vars)
                    {
                        if (inferred.count(name) or definitions(name) != 1 or rebinds.count(name))
                            continue;

                        auto facts = topLevel.find(definitionOf[name]);

                        if (facts == topLevel.end() or not onlyMakes(effectsFrom(facts->second), {name}))
                            continue;

                        if (writtenElsewhere(name) or newTableEachImport.count(name))
                            continue;

                        inferred.insert(name);
                        found = true;
                    }
                }

                for (size_t f = 0; f < programFiles.size(); ++f)
                {
                    const auto& file = programFiles[f];
                    fileJoins[f] = decided(f) ? walkFile(f) : std::vector<Join>(file.size());

                    for (size_t i = 0; i < file.size(); ++i)
                    {
                        if (fileJoins[f][i].joined)
                            joinedAmong[writerOf.at(file[i].get())] = fileJoins[f][i].open;
                    }
                }

                // A const is made once when what writes it, besides its definition, joins its run. Any
                // other runs on every run: what reads it was not made of what is the same, and a
                // statement that took it for so may no longer join
                for (auto it = invariantConsts.begin(); it != invariantConsts.end();)
                {
                    const auto reaching = writersReaching(*it);

                    const bool madeOnce = std::all_of(reaching.begin(), reaching.end(), [&](size_t writer) { return makesOrCameBefore(writer, *it); });

                    if (madeOnce)
                        ++it;
                    else
                    {
                        it = invariantConsts.erase(it);
                        lost = true;
                    }
                }
            }

            for (size_t f = 0; f < programFiles.size(); ++f)
                decidedText.push_back(fileFacts(f));

            if (constexprOrder.empty())
                return;

            // The statements that make each const or var: its definition, what joined it
            std::map<std::string, std::vector<StatementPtr>> making;
            std::unordered_map<const Statement*, size_t> fileOfStatement;
            std::unordered_map<size_t, StatementPtr> statementOfWriter;

            for (size_t f = 0; f < programFiles.size(); ++f)
            {
                const auto& file = programFiles[f];

                for (size_t i = 0; i < file.size(); ++i)
                {
                    const std::string declared = file[i]->kind() == AstKind::VariableStatement ? declaredBy(file[i]) : "";

                    if (not declared.empty())
                        making[declared].push_back(file[i]);

                    if (i < fileJoins[f].size() and fileJoins[f][i].joined)
                        making[declaredBy(file[fileJoins[f][i].target])].push_back(file[i]);

                    fileOfStatement[file[i].get()] = f;

                    auto writer = writerOf.find(file[i].get());

                    if (writer != writerOf.end())
                        statementOfWriter[writer->second] = file[i];
                }
            }

            for (const auto& name : constexprOrder)
            {
                const Statement* definition = definitionOf.at(name);
                const auto& token = static_cast<const VariableStatement*>(definition)->name;
                const std::string place = placeOf(definition, token.line);

                auto refuse = [&](const std::string& why)
                {
                    problems.push_back({name, place + ": constexpr `" + name + "` " + why});
                };

                if (definitions(name) != 1)
                {
                    refuse("is defined more than once");
                    continue;
                }

                if (rebinds.count(name))
                {
                    refuse("is assigned besides its definition");
                    continue;
                }

                if (undecidedFiles.count(fileOfStatement.at(definition)))
                {
                    refuse("stands in a file the program cannot tell from another with the same statements");
                    continue;
                }

                // Nothing but what makes it may write what it reaches
                std::string outside;

                for (const size_t writer : writersReaching(name))
                {
                    if (makesOrCameBefore(writer, name))
                        continue;

                    outside = "may be written by " + writerNames[writer];

                    auto statement = statementOfWriter.find(writer);

                    if (statement == statementOfWriter.end())
                    {
                        outside += ": a function may run at any time";
                        break;
                    }

                    std::set<std::string> seen{name};
                    std::string change;

                    for (const auto& read : effectsOf(statement->second).reads)
                    {
                        change = change.empty() ? madeOfChange(read, making, seen) : change;
                    }

                    outside += change.empty() ? ", which is not part of making it: making it ends at the first statement that does something else"
                                              : ", which reads `" + change + "`: that is not the same on every run";
                    break;
                }

                if (not outside.empty())
                {
                    refuse(outside);
                    continue;
                }

                std::set<std::string> seen;
                const std::string change = madeOfChange(name, making, seen);

                if (not change.empty())
                {
                    refuse("reads `" + change + "`: that is not the same on every run");
                    continue;
                }

                const StatementEffects e = effectsOf(making.at(name).front());

                if (e.unknown)
                    refuse("calls something the compiler cannot follow");
                else if (not onlyMakes(e, {name}))
                    refuse("does more than make itself: its initializer writes or assigns something else");
            }
        }

        int ProgramEffects::joinTarget(const std::vector<StatementPtr>& file, size_t statement, const std::vector<size_t>& open) const
        {
            const StatementPtr& s = file[statement];

            if (not usable or not s)
                return -1;

            switch (s->kind())
            {
                case AstKind::ExpressionStatement:
                case AstKind::ForStatement:
                case AstKind::ForInStatement:
                case AstKind::WhileStatement:
                case AstKind::IfStatement:
                case AstKind::BlockStatement:
                    break;

                default:
                    return -1; // A definition is no part of making another
            }

            const StatementEffects e = effectsOf(s);

            std::set<std::string> openNames;

            for (auto o : open)
                openNames.insert(declaredBy(file[o]));

            if (not onlyMakes(e, openNames))
                return -1;

            // It reads the consts whose data it writes (itself or through what it calls): the last
            // of those it reads is where it joins, after all it uses is made
            for (auto it = open.rbegin(); it != open.rend(); ++it)
            {
                if (not e.reads.count(declaredBy(file[*it])))
                    continue;

                // It would run there: what the program defines after that (a function in the run, one of
                // a file that runs later) is not there yet, whatever a pass may have made of its call
                const size_t at = ranAt.at(file[*it].get());

                for (const auto& name : e.reads)
                {
                    auto defined = definedAt.find(name);

                    if (defined != definedAt.end() and defined->second > at)
                        return -1;
                }

                return static_cast<int>(*it);
            }

            return -1;
        }

        std::vector<Join> ProgramEffects::walkFile(size_t index) const
        {
            const std::vector<StatementPtr>& file = programFiles[index];

            // A file that runs more than once (a module imported twice, or imported by one) runs its
            // statements at each import: a statement that reads what it writes would give its first value
            // only, joined. Its constant entries are set the same at each import, those join as ever
            const bool twice = index < runTwice.size() and runTwice[index];

            // A constant entry joins its const on the same terms as any other statement: the const must
            // be one made once, else the fill runs on every run and closes the run like anything else
            return walkConsts(file,
                [this](const VariableStatement& v) { return v.isConst or inferred.count(v.name.text) > 0; },
                [this, &file](size_t target) { return invariantConsts.count(declaredBy(file[target])) > 0; },
                [this, &file, twice](size_t statement, const std::vector<size_t>& open)
                {
                    if (not twice)
                        return joinTarget(file, statement, open);

                    // A constexpr is made once whatever runs its file
                    std::vector<size_t> once;

                    for (auto o : open)
                    {
                        if (constexprs.count(declaredBy(file[o])))
                            once.push_back(o);
                    }

                    return once.empty() ? -1 : joinTarget(file, statement, once);
                }, true);
        }

        void ProgramEffects::findRunOrder(const FileImports& imports)
        {
            std::vector<bool> ran(programFiles.size(), false);
            runTwice.assign(programFiles.size(), false);
            size_t position = 0;

            std::function<void(size_t)> run = [&](size_t f)
            {
                if (ran[f])
                {
                    runTwice[f] = true;
                    return;
                }

                ran[f] = true;

                size_t next = 0;

                for (size_t i = 0; i < programFiles[f].size(); ++i)
                {
                    // A module runs at every import of it; its names exist from the first one on, so
                    // that is where what it defines is placed
                    while (f < imports.size() and next < imports[f].size() and imports[f][next].first == i)
                        run(imports[f][next++].second);

                    const StatementPtr& s = programFiles[f][i];
                    const std::string name = declaredBy(s);

                    ranAt[s.get()] = position;

                    if (not name.empty())
                        definedAt[name] = position;

                    ++position;
                }
            };

            for (size_t f = 0; f < programFiles.size(); ++f)
            {
                if (not ran[f])
                    run(f);
            }

            // A file that runs again runs all it imports again: only the first reach walks them
            std::vector<size_t> todo;

            for (size_t f = 0; f < programFiles.size(); ++f)
            {
                if (runTwice[f])
                    todo.push_back(f);
            }

            while (not todo.empty())
            {
                const size_t f = todo.back();
                todo.pop_back();

                if (f >= imports.size())
                    continue;

                for (const auto& [statement, imported] : imports[f])
                {
                    if (not runTwice[imported])
                    {
                        runTwice[imported] = true;
                        todo.push_back(imported);
                    }
                }
            }
        }

        int ProgramEffects::fileOf(const std::vector<StatementPtr>& statements) const
        {
            int found = NoFile;

            for (size_t f = 0; f < programFiles.size(); ++f)
            {
                if (not shapeIs(programFiles[f], statements))
                    continue;

                if (found >= 0)
                    return TwinFiles;

                found = static_cast<int>(f);
            }

            return found;
        }

        bool ProgramEffects::analyze(const std::vector<std::vector<StatementPtr>>& files, const FileImports& imports, const std::vector<std::string>& fileOrigins)
        {
            *this = ProgramEffects();

            origins = fileOrigins;

            // What the top levels define
            std::map<std::string, int> functions;
            ClassModel classes;
            bool hiddenCode = false;
            bool classBelow = false;
            bool returnsEarly = false;

            for (const auto& file : files)
            {
                programFiles.emplace_back();

                for (const auto& s : file)
                {
                    if (not s)
                        continue;

                    programFiles.back().push_back(s);
                    returnsEarly = returnsEarly or holdsStatement(s, AstKind::ReturnStatement, false);
                    classBelow = classBelow or definesClassBelow(s);

                    if (s->kind() == AstKind::VariableStatement)
                    {
                        auto v = std::static_pointer_cast<VariableStatement>(s);
                        ++defined[v->name.text];
                        (v->isConst ? consts : vars).insert(v->name.text);
                        definitionOf[v->name.text] = v.get();

                        if (v->isConstexpr and constexprs.insert(v->name.text).second)
                            constexprOrder.push_back(v->name.text);
                    }
                    else if (s->kind() == AstKind::FunctionStatement)
                    {
                        const std::string& name = std::static_pointer_cast<FunctionStatement>(s)->name.text;
                        ++defined[name];
                        ++functions[name];
                        hiddenCode = hiddenCode or isMetamethod(name);
                    }
                    else if (s->kind() == AstKind::ClassStatement)
                    {
                        auto c = std::static_pointer_cast<ClassStatement>(s);
                        const std::string& className = c->name.text;

                        ++defined[className];
                        classes.classes.insert(className);

                        for (auto methods = c->methods; not methods.empty(); methods.pop())
                        {
                            const std::string& method = methods.front()->name.text;
                            const std::string function = ClassModel::functionOf(className, method);

                            if (method == "init")
                                classes.withInit.insert(className);

                            classes.methods[method].push_back(function);
                            ++defined[function];
                            ++functions[function];
                        }
                    }
                }
            }

            // Code runs behind a field read or a loop, which the IR does not show
            if (hiddenCode)
            {
                refused = "a metamethod runs behind what the code shows";
                LOG_INFO(DOM, "Not analyzed: " << refused);
                return false;
            }

            if (classBelow)
                refused = "a class is defined below the top level";
            else if (not classes.classes.empty())
                refused = whyNotFollowed(programFiles, classes, functions);

            for (const auto& name : classes.classes)
            {
                if (refused.empty() and defined[name] != 1)
                    refused = "a class is defined twice: `" + name + "`";
            }

            if (not refused.empty())
            {
                LOG_INFO(DOM, "Not analyzed: " << refused);
                return false;
            }

            // What runs after it depends on what the bodies do not show
            if (returnsEarly)
            {
                refused = "a return at the top level decides what runs after it";
                LOG_INFO(DOM, "Not analyzed: " << refused);
                return false;
            }

            usable = true;

            findRunOrder(imports);

            auto isNative = [this](const std::string& name) { return defined.count(name) == 0; };
            auto isDefined = [this](const std::string& name) { return defined.count(name) > 0; };

            // Every body and every statement: what they assign decides which functions are followed
            std::vector<std::pair<std::string, BodyIR>> functionBodies;

            auto buildBodies = [&]()
            {
                functionBodies.clear();
                statementBodies.clear();

                auto noteRebinds = [this](const BodyIR& body)
                {
                    for (const auto& [name, value] : body.rebinds)
                        rebinds.insert(name);
                };

                for (const auto& file : programFiles)
                {
                    for (const auto& s : file)
                    {
                        if (s->kind() == AstKind::FunctionStatement)
                        {
                            auto f = std::static_pointer_cast<FunctionStatement>(s);
                            functionBodies.emplace_back(f->name.text, buildFunction(*f, locs, isNative, &classes));
                            noteRebinds(functionBodies.back().second);
                        }
                        else if (s->kind() != AstKind::ImportStatement)
                        {
                            if (s->kind() == AstKind::ClassStatement)
                            {
                                auto c = std::static_pointer_cast<ClassStatement>(s);

                                for (auto methods = c->methods; not methods.empty(); methods.pop())
                                {
                                    const auto& m = methods.front();

                                    functionBodies.emplace_back(ClassModel::functionOf(c->name.text, m->name.text), buildMethod(*m, locs, isNative, &classes));
                                    noteRebinds(functionBodies.back().second);
                                }
                            }

                            statementBodies.emplace_back(s.get(), buildTopLevel(s, locs, isNative, &classes));
                            noteRebinds(statementBodies.back().second);
                        }
                    }
                }
            };

            buildBodies();

            // A native the program assigns is what it is given: a name of the program. The bodies are
            // read again for it, which assigns nothing new
            bool nativeRebound = false;

            for (const auto& name : rebinds)
            {
                if (nativeKind(name) != NativeKind::None and defined.count(name) == 0)
                {
                    ++defined[name];
                    nativeRebound = true;
                }
            }

            if (nativeRebound)
                buildBodies();

            // A name that may hold something else is no class a call can tell
            for (const auto& name : classes.classes)
            {
                if (rebinds.count(name))
                {
                    usable = false;
                    refused = "a class is assigned: `" + name + "`";
                    LOG_INFO(DOM, "Not analyzed: " << refused);
                    return false;
                }
            }

            for (const auto& [name, count] : defined)
                definedRoots.insert(locs.global(name));

            // Defined once, as a function, and never assigned: what a call of it runs is known
            for (const auto& [name, count] : functions)
            {
                if (count == 1 and definitions(name) == 1 and not rebinds.count(name))
                    followed.insert(name);
            }

            for (auto& [name, body] : functionBodies)
            {
                if (followed.count(name))
                    followedBodies.emplace(name, std::move(body));
                else
                    otherBodies.emplace_back(name, std::move(body));
            }

            for (size_t i = 0; i < statementBodies.size(); ++i)
                statementBodyAt[statementBodies[i].first] = i;

            summaries = summarize(followedBodies, locs, [this](const std::string& name) { return followed.count(name) > 0; }, isDefined, collapsedValues);

            // Who writes what, each with the edges it adds: the statements, and every function (a host
            // may call one by name, or a statement read as anything may), but for what it does to its
            // parameters, which is only known at a call
            std::vector<std::vector<LocId>> ownWrites;
            std::unordered_map<const Statement*, size_t> writerOf;
            std::vector<Edge> edges;
            std::vector<std::string> writerNames;

            auto notAParameter = [this](LocId l) { return locs.rootKind(l) != RootKind::Param; };

            for (const auto& [statement, body] : statementBodies)
            {
                Summary summary = evaluate(body, collapsedValues);

                writerOf[statement] = ownWrites.size();
                writerNames.push_back(placeOf(statement, statement->line));
                ownWrites.emplace_back();

                for (const auto& w : summary.writes)
                    ownWrites.back().push_back(w.at);

                edges.insert(edges.end(), summary.edges.begin(), summary.edges.end());
                topLevel.emplace(statement, std::move(summary));
            }

            const size_t statementsWriting = ownWrites.size();

            // By function: the writes that stay in what it made
            std::vector<std::set<LocId>> inOwnTables;

            auto addFunction = [&](const Summary& summary)
            {
                ownWrites.emplace_back();
                inOwnTables.emplace_back();

                for (const auto& w : summary.writes)
                {
                    if (notAParameter(w.at))
                    {
                        ownWrites.back().push_back(w.at);

                        if (locs.depth(w.at) > 1 and staysInOwnTables(w.at, summary.edges))
                            inOwnTables.back().insert(w.at);
                    }
                }

                for (const auto& e : summary.edges)
                {
                    if (notAParameter(e.at) and notAParameter(e.value))
                        edges.push_back(e);
                }
            };

            for (const auto& [name, summary] : summaries)
            {
                addFunction(summary);
                writerNames.push_back("function `" + name + "`");
            }

            for (const auto& [name, body] : otherBodies)
            {
                addFunction(evaluate(body, collapsedValues));
                writerNames.push_back("function `" + name + "`");
            }

            resolveEdges(edges);
            findRegions();

            // A slot of a table a call makes is written in that table, no table of the program: stored in
            // one, that is the store's write. Anything deeper may be below what that table holds, unless
            // all it holds on the way was made by the call too
            for (size_t i = statementsWriting; i < ownWrites.size(); ++i)
            {
                auto& writes = ownWrites[i];
                const auto& own = inOwnTables[i - statementsWriting];

                writes.erase(std::remove_if(writes.begin(), writes.end(), [this, &own](LocId w)
                {
                    return locs.rootKind(w) == RootKind::Site and (locs.depth(w) == 1 or own.count(w)) and not reachesAll(w);
                }), writes.end());
            }

            std::vector<Objects> writers;

            for (const auto& writes : ownWrites)
                writers.push_back(written(writes));

            settleConsts(writers, writerOf, writerNames);

            return true;
        }

        StatementEffects ProgramEffects::effectsOf(const StatementPtr& statement) const
        {
            if (not usable)
            {
                StatementEffects effects;
                effects.unknown = true;
                return effects;
            }

            return effectsFrom(summaryOfStatement(statement));
        }

        bool ProgramEffects::invariant(const std::string& name) const
        {
            if (not usable or rebinds.count(name))
                return false;

            if (invariantConsts.count(name) or followed.count(name))
                return true;

            if (nativeKind(name) != NativeKind::None and definitions(name) == 0)
                return true;

            return inferred.count(name) > 0;
        }

        bool ProgramEffects::onlyMakes(const StatementEffects& effects, const std::set<std::string>& targets) const
        {
            if (not usable or effects.unknown or not effects.rebinds.empty())
                return false;

            // A target says what it may write. What it reads must be the same on every run: a const that
            // is not does not become so by being open, only a var in the making may read itself
            for (const auto& name : effects.reads)
            {
                if (not invariant(name) and (not targets.count(name) or consts.count(name)))
                    return false;
            }

            for (auto object : written(effects.writes))
            {
                const RootKind kind = locs.rootKind(object);

                // A global never defined is an input, whoever holds it
                if (kind == RootKind::Unknown or kind == RootKind::Param or isInput(object))
                    return false;

                // What the targets reach: theirs to make, whoever else reaches it (whatever reaches it
                // later in a run sees it made, as it did)
                auto reached = reachedBy.find(object);

                if (reached != reachedBy.end() and std::any_of(targets.begin(), targets.end(), [&](const std::string& t) { return reached->second.count(t) > 0; }))
                    continue;

                // Else made by itself: reached by no global
                if (kind != RootKind::Site or reached != reachedBy.end() or reachedBy.count(locs.unknown()) > 0)
                    return false;
            }

            return true;
        }

        bool ProgramEffects::idempotent(const StatementPtr& statement) const
        {
            if (not usable)
                return false;

            const Summary summary = summaryOfStatement(statement);

            // A rebind or a call not followed is no write, and nothing shows it the same on every run
            if (summary.unknown or not summary.rebinds.empty())
                return false;

            for (const auto& w : summary.writes)
            {
                if (not w.idempotence.shape)
                    return false;

                for (const auto& name : w.idempotence.needsGlobals)
                {
                    if (not invariant(name))
                        return false;
                }
            }

            return true;
        }

        const Summary* ProgramEffects::summaryOf(const std::string& function) const
        {
            auto it = summaries.find(function);
            return it == summaries.end() ? nullptr : &it->second;
        }

        std::vector<std::string> ProgramEffects::writtenTexts(const StatementPtr& statement) const
        {
            if (not usable)
                return {locs.text(locs.unknown())};

            const std::vector<LocId> writes = effectsOf(statement).writes;

            // Each as it is written, and as a slot of each object it names
            std::vector<LocId> all = writes;

            for (auto w : writes)
            {
                if (locs.depth(w) == 0)
                    continue;

                const KeyId key = reachesAll(w) ? Locations::AnyKey : locs.keyOf(w);

                for (auto object : objectsOf(w))
                    all.push_back(locs.rootKind(object) == RootKind::Unknown ? object : locs.child(object, key));
            }

            sortUnique(all);
            return textsOf(locs, all);
        }

        std::vector<std::string> ProgramEffects::regionsOf(const std::string& global) const
        {
            auto it = regions.find(global);
            return it == regions.end() ? std::vector<std::string>{} : textsOf(locs, {it->second.begin(), it->second.end()});
        }

        std::string ProgramEffects::fileFacts(size_t f) const
        {
            std::string names;
            std::string told = decided(f) ? "" : "plain;";

            for (size_t i = 0; i < programFiles[f].size(); ++i)
            {
                const StatementPtr& s = programFiles[f][i];
                const std::string name = declaredBy(s);

                if (not name.empty())
                    names += name + ",";

                if (not decided(f))
                    continue;

                if (s->kind() == AstKind::VariableStatement and inferred.count(name))
                    told += "var " + name + ";";

                if (fileJoins[f][i].joined)
                    told += std::to_string(i) + " joins " + declaredBy(programFiles[f][fileJoins[f][i].target]) + ";";
            }

            if (names.empty())
                return "";

            const bool known = f < origins.size() and not origins[f].empty();
            return (known ? origins[f] : names) + ":" + (told.empty() ? "nothing;" : told);
        }

        std::string ProgramEffects::factsOf(const std::set<std::string>& identifiers) const
        {
            if (not usable)
                return "";

            std::vector<std::string> told;

            for (size_t f = 0; f < programFiles.size(); ++f)
            {
                if (decidedText[f].empty())
                    continue;

                const bool held = std::all_of(programFiles[f].begin(), programFiles[f].end(), [&](const StatementPtr& s)
                {
                    const std::string name = declaredBy(s);
                    return name.empty() or identifiers.count(name) > 0;
                });

                if (held)
                    told.push_back(decidedText[f]);
            }

            std::sort(told.begin(), told.end());

            std::string out;

            for (const auto& text : told)
                out += text + "\n";

            return out;
        }

        int ProgramEffects::definitions(const std::string& name) const
        {
            auto it = defined.find(name);
            return it == defined.end() ? 0 : it->second;
        }
    }
}
