#include "stdafx.h"

#include "effects.h"

#include "logger.h"

#include <algorithm>
#include <cctype>
#include <sstream>
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

            bool isConstantValue(const ExprPtr& e)
            {
                return e and (e->kind() == AstKind::Atom or e->kind() == AstKind::NoneAtom);
            }

            // A value made of constants only (constants, negated ones, lists and tables of them): it
            // reads nothing, so no earlier const
            bool isConstantData(const ExprPtr& e)
            {
                if (isConstantValue(e))
                    return true;

                if (e and e->kind() == AstKind::UnaryExpression)
                    return isConstantValue(std::static_pointer_cast<UnaryExpression>(e)->expr);

                if (e and e->kind() == AstKind::List)
                {
                    for (const auto& entry : listOf(std::static_pointer_cast<List>(e)->entries))
                    {
                        // A key written `key: value` is its name, emitted as a string constant
                        const bool constantKey = not entry.key or entry.explicitKey or isConstantData(entry.key);

                        if (not constantKey or not isConstantData(entry.value))
                            return false;
                    }

                    return true;
                }

                return false;
            }

            std::string varName(const ExprPtr& e)
            {
                if (e and e->kind() == AstKind::Var)
                    return std::static_pointer_cast<Var>(e)->name.text;

                return "";
            }

            // The const a statement fills with a constant entry (`t["key"] = value`, `t.key = value`),
            // "" when it is no such fill
            std::string filledConst(const StatementPtr& s)
            {
                if (not s or s->kind() != AstKind::ExpressionStatement)
                    return "";

                const ExprPtr& e = std::static_pointer_cast<ExpressionStatement>(s)->expr;

                if (not e)
                    return "";

                if (e->kind() == AstKind::IndexSet)
                {
                    auto set = std::static_pointer_cast<IndexSet>(e);

                    if (isConstantValue(set->index) and isConstantValue(set->value))
                        return varName(set->object);
                }
                else if (e->kind() == AstKind::Set)
                {
                    auto set = std::static_pointer_cast<Set>(e);

                    if (isConstantValue(set->value))
                        return varName(set->object);
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

        Summary ProgramEffects::evaluate(const BodyIR& body, size_t& collapsed) const
        {
            auto isFollowed = [this](const std::string& name) { return followed.count(name) > 0; };
            auto isDefined = [this](const std::string& name) { return defined.count(name) > 0; };

            Evaluator evaluator(locs, summaries, isFollowed, isDefined);
            Summary out = evaluator.evaluate(body);
            collapsed += evaluator.collapsed();
            return out;
        }

        Summary ProgramEffects::summaryOfStatement(const StatementPtr& statement) const
        {
            if (not statement)
                return {};

            auto twin = moduleStatements.find(statement.get());
            auto known = topLevel.find(twin == moduleStatements.end() ? statement.get() : twin->second);

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
            effects.edges = summary.edges;
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

        void ProgramEffects::addFieldsOf(LocId object, KeyId key, Objects& out) const
        {
            // Nothing is known of what is inside these
            if (locs.rootKind(object) == RootKind::Unknown or isInput(object))
                out.insert(object);

            auto fields = heap.find(object);

            if (fields == heap.end())
                return;

            if (key == Locations::AnyKey or locs.isParamKey(key))
            {
                for (const auto& [name, held] : fields->second)
                    out.insert(held.begin(), held.end());

                return;
            }

            for (const KeyId k : {key, Locations::AnyKey})
            {
                auto held = fields->second.find(k);

                if (held != fields->second.end())
                    out.insert(held->second.begin(), held->second.end());
            }
        }

        ProgramEffects::Objects ProgramEffects::reachableFrom(Objects objects) const
        {
            std::vector<LocId> todo(objects.begin(), objects.end());

            while (not todo.empty())
            {
                auto fields = heap.find(todo.back());
                todo.pop_back();

                if (fields == heap.end())
                    continue;

                for (const auto& [name, held] : fields->second)
                {
                    for (auto o : held)
                    {
                        if (objects.insert(o).second)
                            todo.push_back(o);
                    }
                }
            }

            return objects;
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
                    out.insert(held->second.begin(), held->second.end());
            }
            else
            {
                for (auto o : objectsAt(locs.parent(l)))
                    addFieldsOf(o, locs.keyOf(l), out);

                out.insert(anywhere.begin(), anywhere.end());

                if (locs.deep(l))
                    out = reachableFrom(std::move(out));
            }

            return objectsMemo[l] = std::move(out);
        }

        ProgramEffects::Objects ProgramEffects::objectsOf(LocId write) const
        {
            if (locs.depth(write) == 0)
                return {write};

            const Objects& parents = objectsAt(locs.parent(write));
            Objects objects = reachesAll(write) ? reachableFrom(parents) : parents;

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
                const Objects objects = objectsOf(w);
                out.insert(objects.begin(), objects.end());
            }

            return out;
        }

        void ProgramEffects::resolveEdges(const std::vector<Edge>& edges)
        {
            findHolders(edges);

            // A location holding a value and all inside it: what a call not followed left, the two go
            // together through every summary. It stands for all that is reachable, a store for one slot
            std::unordered_set<uint64_t> seen;

            for (const auto& e : edges)
                seen.insert((static_cast<uint64_t>(e.at) << 32) | e.value);

            for (const auto& e : edges)
            {
                if (locs.depth(e.at) == 0 or locs.keyOf(e.at) != Locations::AnyKey or locs.rootKind(e.value) == RootKind::Unknown)
                    continue;

                const LocId inside = locs.child(e.value, Locations::AnyKey);

                if (inside != e.value and seen.count((static_cast<uint64_t>(e.at) << 32) | inside))
                    reachingAll.insert(e.at);
            }

            auto addAll = [](Objects& into, const Objects& from)
            {
                const size_t before = into.size();
                into.insert(from.begin(), from.end());
                return into.size() != before;
            };

            // Each edge holds at every object its location may name, itself found through the edges
            bool grew = true;

            while (grew)
            {
                grew = false;
                objectsMemo.clear();

                for (const auto& e : edges)
                {
                    const bool all = locs.depth(e.at) > 0 and reachesAll(e.at);
                    const Objects values = all ? reachableFrom(objectsAt(e.value)) : objectsAt(e.value);

                    if (values.empty())
                        continue;

                    if (locs.rootKind(e.at) == RootKind::Unknown)
                        grew = addAll(anywhere, values) or grew;
                    else if (locs.depth(e.at) == 0)
                        grew = addAll(holds[e.at], values) or grew;
                    else
                    {
                        const Objects& parents = objectsAt(locs.parent(e.at));

                        // A field named by a parameter may be any field
                        const KeyId key = all or locs.isParamKey(locs.keyOf(e.at)) ? Locations::AnyKey : locs.keyOf(e.at);

                        // No object known: it is in the global the path starts from
                        const Objects held = parents.empty() ? Objects{locs.rootOf(e.at)} : all ? reachableFrom(parents) : parents;

                        for (auto o : held)
                        {
                            if (locs.rootKind(o) == RootKind::Unknown)
                                grew = addAll(anywhere, values) or grew;
                            else
                                grew = addAll(heap[o][key], values) or grew;
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
                reached.insert(anywhere.begin(), anywhere.end());
                reached = reachableFrom(std::move(reached));

                for (auto o : reached)
                    reachedBy[o].insert(name);

                regions[name] = std::move(reached);
            }
        }

        void ProgramEffects::settleConsts(const std::vector<Objects>& writers, const std::unordered_map<const Statement*, size_t>& writerOf)
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

            bool lost = true;

            while (lost)
            {
                lost = false;
                inferred.clear();

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

                        if (writtenElsewhere(name))
                            continue;

                        inferred.insert(name);
                        found = true;
                    }
                }

                // What will join which const, by the walk the pass does
                std::unordered_map<size_t, std::set<std::string>> joinedAmong;

                for (const auto& file : programFiles)
                {
                    const auto joins = joinsOf(file);

                    for (size_t i = 0; i < file.size(); ++i)
                    {
                        if (joins[i].target)
                            joinedAmong[writerOf.at(file[i].get())] = joins[i].open;
                    }
                }

                // A const is made once when what writes it, besides its definition, joins its run. Any
                // other runs on every run: what reads it was not made of what is the same, and a
                // statement that took it for so may no longer join
                for (auto it = invariantConsts.begin(); it != invariantConsts.end();)
                {
                    const size_t definition = writerOf.at(definitionOf[*it]);
                    const auto reaching = writersReaching(*it);

                    const bool madeOnce = std::all_of(reaching.begin(), reaching.end(), [&](size_t writer)
                    {
                        auto joined = joinedAmong.find(writer);
                        return writer == definition or (joined != joinedAmong.end() and joined->second.count(*it) > 0);
                    });

                    if (madeOnce)
                        ++it;
                    else
                    {
                        it = invariantConsts.erase(it);
                        lost = true;
                    }
                }
            }
        }

        std::shared_ptr<VariableStatement> ProgramEffects::joinTarget(const StatementPtr& s, const std::vector<std::shared_ptr<VariableStatement>>& open) const
        {
            if (not usable or not s)
                return nullptr;

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
                    return nullptr; // A definition is no part of making another
            }

            const StatementEffects e = effectsOf(s);

            std::set<std::string> openNames;

            for (const auto& v : open)
                openNames.insert(v->name.text);

            if (not onlyMakes(e, openNames))
                return nullptr;

            // It reads the consts whose data it writes (itself or through what it calls): the last
            // of those it reads is where it joins, after all it uses is made
            for (auto it = open.rbegin(); it != open.rend(); ++it)
            {
                if (e.reads.count((*it)->name.text))
                    return *it;
            }

            return nullptr;
        }

        std::vector<Join> ProgramEffects::joinsOf(const std::vector<StatementPtr>& statements) const
        {
            std::vector<Join> out(statements.size());

            // The consts the statements since the last other one declared, in their order: a fill of
            // one joins it
            std::vector<std::shared_ptr<VariableStatement>> open;

            auto openNamed = [&open](const std::string& name) -> std::shared_ptr<VariableStatement>
            {
                // A later declaration of the name wins
                for (auto it = open.rbegin(); it != open.rend(); ++it)
                {
                    if ((*it)->name.text == name)
                        return *it;
                }

                return nullptr;
            };

            for (size_t i = 0; i < statements.size(); ++i)
            {
                const StatementPtr& s = statements[i];

                // A definition runs no code: the fills after it still follow their const
                if (s and s->kind() == AstKind::FunctionStatement)
                    continue;

                if (s and s->kind() == AstKind::VariableStatement)
                {
                    auto v = std::static_pointer_cast<VariableStatement>(s);

                    if ((v->isConst or inferred.count(v->name.text)) and v->expr)
                    {
                        // An initializer that may read the consts before it: their later fills would
                        // be seen by it too early, joined to them
                        if (not isConstantData(v->expr))
                            open.clear();

                        open.push_back(v);
                        continue;
                    }
                }

                Join& join = out[i];
                const std::string target = filledConst(s);

                join.target = target.empty() ? nullptr : openNamed(target);
                join.fill = join.target != nullptr;

                // What makes the open consts (a loop putting their tables in order): with the last of them it uses
                if (not join.target and not open.empty())
                    join.target = joinTarget(s, open);

                if (join.target)
                {
                    for (const auto& v : open)
                        join.open.insert(v->name.text);

                    continue;
                }

                // Anything else may read a const: what comes after it fills no initializer
                open.clear();
            }

            return out;
        }

        bool ProgramEffects::readModule(const std::vector<StatementPtr>& statements)
        {
            moduleStatements.clear();

            std::vector<StatementPtr> own;

            for (const auto& s : statements)
            {
                if (s)
                    own.push_back(s);
            }

            auto sameFile = [&own](const std::vector<StatementPtr>& file)
            {
                return file.size() == own.size() and std::equal(file.begin(), file.end(), own.begin(), [](const StatementPtr& a, const StatementPtr& b)
                {
                    // The passes before this one may turn a statement that declares nothing into another (a
                    // loop into a block with what it hoists): what it does is the same
                    const bool declares = not declaredBy(a).empty() or a->kind() == AstKind::ImportStatement;
                    return declaredBy(a) == declaredBy(b) and (a->kind() == b->kind() or (not declares and b->kind() != AstKind::ImportStatement));
                });
            };

            const std::vector<StatementPtr>* twin = nullptr;

            for (const auto& file : programFiles)
            {
                if (not sameFile(file))
                    continue;

                if (twin)
                    return false;

                twin = &file;
            }

            if (not twin)
                return false;

            for (size_t i = 0; i < own.size(); ++i)
                moduleStatements[own[i].get()] = (*twin)[i].get();

            return true;
        }

        bool ProgramEffects::analyze(const std::vector<std::vector<StatementPtr>>& files)
        {
            *this = ProgramEffects();

            // What the top levels define
            std::map<std::string, int> functions;
            bool hiddenCode = false;

            for (const auto& file : files)
            {
                programFiles.emplace_back();

                for (const auto& s : file)
                {
                    if (not s)
                        continue;

                    programFiles.back().push_back(s);

                    if (s->kind() == AstKind::VariableStatement)
                    {
                        auto v = std::static_pointer_cast<VariableStatement>(s);
                        ++defined[v->name.text];
                        (v->isConst ? consts : vars).insert(v->name.text);
                        definitionOf[v->name.text] = v.get();
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
                        ++defined[std::static_pointer_cast<ClassStatement>(s)->name.text];
                        hiddenCode = true;
                    }
                }
            }

            // Code runs behind a field read or a loop, which the IR does not show
            if (hiddenCode)
            {
                LOG_INFO(DOM, "Not analyzed: a class or a metamethod runs behind what the bodies show");
                return false;
            }

            usable = true;

            auto isNative = [this](const std::string& name) { return defined.count(name) == 0; };
            auto isDefined = [this](const std::string& name) { return defined.count(name) > 0; };

            // Every body and every statement: what they assign decides which functions are followed
            std::vector<std::pair<std::string, BodyIR>> functionBodies;
            std::vector<std::pair<const Statement*, BodyIR>> statementBodies;

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
                            functionBodies.emplace_back(f->name.text, buildFunction(*f, locs, isNative));
                            noteRebinds(functionBodies.back().second);
                        }
                        else if (s->kind() != AstKind::ImportStatement)
                        {
                            statementBodies.emplace_back(s.get(), buildTopLevel(s, locs, isNative));
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

            for (const auto& [name, count] : defined)
                definedRoots.insert(locs.global(name));

            // Defined once, as a function, and never assigned: what a call of it runs is known
            for (const auto& [name, count] : functions)
            {
                if (count == 1 and definitions(name) == 1 and not rebinds.count(name))
                    followed.insert(name);
            }

            std::map<std::string, BodyIR> followedBodies;
            std::vector<BodyIR> otherBodies;

            for (auto& [name, body] : functionBodies)
            {
                if (followed.count(name))
                    followedBodies.emplace(name, std::move(body));
                else
                    otherBodies.push_back(std::move(body));
            }

            summaries = summarize(followedBodies, locs, [this](const std::string& name) { return followed.count(name) > 0; }, isDefined, collapsedValues);

            // Who writes what, each with the edges it adds: the statements, and every function (a host
            // may call one by name, or a statement read as anything may), but for what it does to its
            // parameters, which is only known at a call
            std::vector<std::vector<LocId>> ownWrites;
            std::unordered_map<const Statement*, size_t> writerOf;
            std::vector<Edge> edges;

            auto notAParameter = [this](LocId l) { return locs.rootKind(l) != RootKind::Param; };

            for (const auto& [statement, body] : statementBodies)
            {
                Summary summary = evaluate(body, collapsedValues);

                writerOf[statement] = ownWrites.size();
                ownWrites.emplace_back();

                for (const auto& w : summary.writes)
                    ownWrites.back().push_back(w.at);

                edges.insert(edges.end(), summary.edges.begin(), summary.edges.end());
                topLevel.emplace(statement, std::move(summary));
            }

            const size_t statementsWriting = ownWrites.size();

            auto addFunction = [&](const Summary& summary)
            {
                ownWrites.emplace_back();

                for (const auto& w : summary.writes)
                {
                    if (notAParameter(w.at))
                        ownWrites.back().push_back(w.at);
                }

                for (const auto& e : summary.edges)
                {
                    if (notAParameter(e.at) and notAParameter(e.value))
                        edges.push_back(e);
                }
            };

            for (const auto& [name, summary] : summaries)
                addFunction(summary);

            for (const auto& body : otherBodies)
                addFunction(evaluate(body, collapsedValues));

            resolveEdges(edges);
            findRegions();

            // A slot of a table a call makes is written in that table, no table of the program: stored in
            // one, that is the store's write. Anything deeper may be below what that table holds
            for (size_t i = statementsWriting; i < ownWrites.size(); ++i)
            {
                auto& writes = ownWrites[i];

                writes.erase(std::remove_if(writes.begin(), writes.end(), [this](LocId w)
                {
                    return locs.rootKind(w) == RootKind::Site and locs.depth(w) == 1 and not reachesAll(w);
                }), writes.end());
            }

            std::vector<Objects> writers;

            for (const auto& writes : ownWrites)
                writers.push_back(written(writes));

            settleConsts(writers, writerOf);

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

            for (const auto& name : effects.reads)
            {
                if (not targets.count(name) and not invariant(name))
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

        std::string ProgramEffects::factsOf(const std::string& name) const
        {
            if (not usable)
                return "";

            std::ostringstream out;

            if (inferred.count(name))
                out << 'c';

            if (invariant(name))
                out << 'i';

            auto f = summaries.find(name);

            if (f != summaries.end())
                out << 'f' << f->second.digest(locs);

            // What it reaches, and which other globals reach some of it
            auto reach = regions.find(name);

            if (reach != regions.end())
            {
                std::set<std::string> sharing;

                for (auto object : reach->second)
                {
                    const auto& by = reachedBy.at(object);
                    sharing.insert(by.begin(), by.end());
                }

                sharing.erase(name);

                out << 'r' << reach->second.size() << 's';

                for (const auto& g : sharing)
                    out << g << ',';
            }

            return out.str();
        }

        int ProgramEffects::definitions(const std::string& name) const
        {
            auto it = defined.find(name);
            return it == defined.end() ? 0 : it->second;
        }
    }
}
