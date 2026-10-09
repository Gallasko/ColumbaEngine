#include "rulevm.h"

#include <cstdlib>
#include <sstream>

#include "logger.h"

#include "Compiler/vm.h"
#include "Compiler/vmreaders.h"
#include "ECS/entitysystem.h"
#include "ECS/scriptregistry.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Rules";

        std::vector<std::string> splitPath(const std::string& path)
        {
            std::vector<std::string> parts;
            std::stringstream stream(path);
            std::string part;

            while (std::getline(stream, part, '.'))
                parts.push_back(part);

            return parts;
        }

        bool isIndex(const std::string& s)
        {
            return not s.empty() and s.find_first_not_of("0123456789") == std::string::npos;
        }

        std::string text(const ElementMap& map, const std::string& key)
        {
            auto it = map.find(key);

            if (it == map.end() or it->second.type != UnionType::STRING)
                return "";

            return it->second.get<std::string>();
        }

        // The same keys with the same values of the same types: an answer kept for one stands for the other
        bool sameMap(const ElementMap& a, const ElementMap& b)
        {
            if (a.size() != b.size())
                return false;

            for (const auto& [key, value] : a)
            {
                auto it = b.find(key);

                if (it == b.end() or it->second.type != value.type)
                    return false;

                switch (value.type)
                {
                case UnionType::INT:
                    if (value.get<int>() != it->second.get<int>())
                        return false;

                    break;

                case UnionType::FLOAT:
                    if (value.get<float>() != it->second.get<float>())
                        return false;

                    break;

                case UnionType::BOOL:
                    if (value.get<bool>() != it->second.get<bool>())
                        return false;

                    break;

                default:
                    if (value.toString() != it->second.toString())
                        return false;

                    break;
                }
            }

            return true;
        }

        float number(const ElementMap& map, const std::string& key, float fallback)
        {
            auto it = map.find(key);

            if (it == map.end())
                return fallback;

            if (it->second.type == UnionType::INT)
                return static_cast<float>(it->second.get<int>());

            if (it->second.type == UnionType::FLOAT)
                return it->second.get<float>();

            return fallback;
        }
    }

    // What an input holds until it is (re)defined as a global before a run.
    struct RuleScript::Input
    {
        enum class Kind { Scalar, Table, Tables, Scalars } kind = Kind::Scalar;

        ElementType scalar;
        ElementMap table;
        RecordList tables;
        std::vector<ElementType> scalars;
    };

    RuleScript::RuleScript() = default;

    RuleScript::~RuleScript()
    {
        if (vm and fn)
            vm->cleanupFunction(fn);
    }

    void RuleScript::error(const std::string& message)
    {
        const std::string line = scriptPath + ": " + message;

        LOG_ERROR(DOM, line);
        errorList.push_back(line);
    }

    bool RuleScript::load(EntitySystem* ecs, const std::string& path)
    {
        scriptPath = path;
        errorList.clear();

        if (vm and fn)
            vm->cleanupFunction(fn);

        fn = nullptr;

        if (not ecs)
        {
            error("no EntitySystem to compile it with");
            return false;
        }

        handle = ecs->scripts().load(path);

        if (not handle)
        {
            error("not a .pg or .pgc file");
            return false;
        }

        bound = handle->bytecode();

        if (not bound or bound->empty())
        {
            error("does not compile (the VM's message is in the log above)");
            return false;
        }

        vm = std::make_unique<VM>();
        ecs->setupVm(*vm);

        fn = vm->prepareCachedFunction(*bound, path);

        if (not fn)
        {
            error("the compiled bytecode could not be prepared");
            return false;
        }

        return true;
    }

    void RuleScript::set(const std::string& name, const ElementType& value)
    {
        auto input = std::make_shared<Input>();
        input->kind = Input::Kind::Scalar;
        input->scalar = value;
        inputs[name] = input;
    }

    void RuleScript::set(const std::string& name, const ElementMap& table)
    {
        auto input = std::make_shared<Input>();
        input->kind = Input::Kind::Table;
        input->table = table;
        inputs[name] = input;
    }

    void RuleScript::set(const std::string& name, const RecordList& tables)
    {
        auto input = std::make_shared<Input>();
        input->kind = Input::Kind::Tables;
        input->tables = tables;
        inputs[name] = input;
    }

    void RuleScript::set(const std::string& name, const std::vector<ElementType>& values)
    {
        auto input = std::make_shared<Input>();
        input->kind = Input::Kind::Scalars;
        input->scalars = values;
        inputs[name] = input;
    }

    uint64_t RuleScript::makeValue(const Input& input)
    {
        auto makeTable = [this](const ElementMap& map) -> Value {
            Value table = vm->createTable();
            ObjInstance* instance = vm->asInstance(table);

            for (const auto& [key, value] : map)
                instance->setField(key, vm->retainValue(vm->elementToValue(value)));

            return table;
        };

        switch (input.kind)
        {
        case Input::Kind::Table:
            return makeTable(input.table);

        case Input::Kind::Tables:
        {
            Value list = vm->createVector();
            ObjVector* vector = vm->asVector(list);

            for (const auto& map : input.tables)
                vector->fields.push_back(vm->retainValue(makeTable(map)));

            return list;
        }

        case Input::Kind::Scalars:
        {
            Value list = vm->createVector();
            ObjVector* vector = vm->asVector(list);

            for (const auto& value : input.scalars)
                vector->fields.push_back(vm->retainValue(vm->elementToValue(value)));

            return list;
        }

        case Input::Kind::Scalar:
        default:
            return vm->elementToValue(input.scalar);
        }
    }

    bool RuleScript::run()
    {
        if (not fn)
        {
            error("not loaded");
            return false;
        }

        // Inputs are defined after the prepared function's one-time constant freeze, as a
        // system hook's per-run globals are: they are released and replaced every run.
        for (const auto& [name, input] : inputs)
            vm->defineGlobal(name, makeValue(*input));

        const InterpretResult result = vm->runPreparedFunction(fn, 0, scriptPath);

        if (result != InterpretResult::OK)
        {
            error("runtime error (the VM's message is in the log above)");
            return false;
        }

        // A script reports what it refuses in an `errors` list rather than throwing
        const Value errs = vm->findGlobal("errors");

        if (IS_VECTOR(errs))
        {
            bool failed = false;

            for (Value e : vm->asVector(errs)->fields)
            {
                error(IS_STRING(e) ? vm->asString(e) : std::string("an error that is not a string"));
                failed = true;
            }

            if (failed)
                return false;
        }

        return true;
    }

    bool RuleScript::resolve(const std::string& path, uint64_t& out)
    {
        if (not vm)
        {
            error("not loaded");
            return false;
        }

        const std::vector<std::string> parts = splitPath(path);

        if (parts.empty())
        {
            error("an empty output path");
            return false;
        }

        Value v = vm->findGlobal(parts[0]);

        if (IS_UNDEFINED(v))
        {
            error("no output global `" + parts[0] + "`");
            return false;
        }

        for (size_t i = 1; i < parts.size(); ++i)
        {
            const std::string& part = parts[i];

            if (IS_INSTANCE(v))
            {
                ObjInstance* instance = vm->asInstance(v);
                bool found = false;

                for (size_t f = 0; f < instance->fieldNames.size(); ++f)
                {
                    if (instance->fieldNames[f] == part)
                    {
                        v = instance->fieldValues[f];
                        found = true;
                        break;
                    }
                }

                if (not found)
                {
                    error("`" + path + "`: no field `" + part + "`");
                    return false;
                }
            }
            else if (IS_VECTOR(v) and isIndex(part))
            {
                const auto& fields = vm->asVector(v)->fields;
                const size_t index = static_cast<size_t>(std::strtoul(part.c_str(), nullptr, 10));

                if (index >= fields.size())
                {
                    error("`" + path + "`: index " + part + " is past the end (" + std::to_string(fields.size()) + ")");
                    return false;
                }

                v = fields[index];
            }
            else
            {
                error("`" + path + "`: `" + part + "` is neither a field of a table nor an index of a list");
                return false;
            }
        }

        out = v;
        return true;
    }

    bool RuleScript::get(const std::string& path, ElementType& out)
    {
        Value v;

        if (not resolve(path, v))
            return false;

        if (not vmread::isScalar(v))
        {
            error("`" + path + "` is not a scalar");
            return false;
        }

        out = vmread::scalar(*vm, v);
        return true;
    }

    bool RuleScript::get(const std::string& path, ElementMap& out)
    {
        Value v;

        if (not resolve(path, v))
            return false;

        if (not IS_INSTANCE(v))
        {
            error("`" + path + "` is not a table");
            return false;
        }

        // The table's scalars; what it nests (lists, tables) is read by its own path.
        ObjInstance* instance = vm->asInstance(v);

        for (size_t i = 0; i < instance->fieldValues.size(); ++i)
        {
            const std::string& key = instance->fieldNames[i];

            if (key.empty() or key.rfind("__", 0) == 0 or not vmread::isScalar(instance->fieldValues[i]))
                continue;

            out[key] = vmread::scalar(*vm, instance->fieldValues[i]);
        }

        return true;
    }

    bool RuleScript::get(const std::string& path, RecordList& out)
    {
        Value v;

        if (not resolve(path, v))
            return false;

        std::vector<std::string> problems;
        const bool ok = vmread::records(*vm, v, out, &problems, path);

        for (const auto& problem : problems)
            error(problem);

        return ok;
    }

    bool RuleScript::get(const std::string& path, std::vector<ElementType>& out)
    {
        Value v;

        if (not resolve(path, v))
            return false;

        std::vector<std::string> problems;
        const bool ok = vmread::scalars(*vm, v, out, &problems, path);

        for (const auto& problem : problems)
            error(problem);

        return ok;
    }

    bool RuleScript::get(const std::string& path, const std::vector<std::string>& lists, std::vector<Record>& out)
    {
        Value v;

        out.clear();

        if (not resolve(path, v))
            return false;

        if (not IS_VECTOR(v))
        {
            error("`" + path + "` is not a list");
            return false;
        }

        const auto& items = vm->asVector(v)->fields;

        out.reserve(items.size());

        for (size_t i = 0; i < items.size(); ++i)
        {
            const std::string at = path + "." + std::to_string(i);

            if (not IS_INSTANCE(items[i]))
            {
                error("`" + at + "` is not a table");
                return false;
            }

            ObjInstance* instance = vm->asInstance(items[i]);

            Record record;
            record.lists.resize(lists.size());

            std::vector<bool> found(lists.size(), false);

            for (size_t f = 0; f < instance->fieldValues.size(); ++f)
            {
                const std::string& key = instance->fieldNames[f];

                if (key.empty() or key.rfind("__", 0) == 0)
                    continue;

                if (vmread::isScalar(instance->fieldValues[f]))
                {
                    record.fields[key] = vmread::scalar(*vm, instance->fieldValues[f]);
                    continue;
                }

                // What it nests is read when asked for, left when not
                for (size_t l = 0; l < lists.size(); ++l)
                {
                    if (lists[l] != key)
                        continue;

                    std::vector<std::string> problems;
                    const bool ok = vmread::records(*vm, instance->fieldValues[f], record.lists[l], &problems, at + "." + key);

                    for (const auto& problem : problems)
                        error(problem);

                    if (not ok)
                        return false;

                    found[l] = true;
                }
            }

            for (size_t l = 0; l < lists.size(); ++l)
            {
                if (not found[l])
                {
                    error("`" + at + "." + lists[l] + "`: no field `" + lists[l] + "`");
                    return false;
                }
            }

            out.push_back(std::move(record));
        }

        return true;
    }

    bool RuleScript::size(const std::string& path, size_t& out)
    {
        Value v;

        if (not resolve(path, v))
            return false;

        if (not IS_VECTOR(v))
        {
            error("`" + path + "` is not a list");
            return false;
        }

        out = vm->asVector(v)->fields.size();
        return true;
    }

    // ---- Rules --------------------------------------------------------------------------------

    bool Rules::fail(RuleScript& script)
    {
        errors = script.errors();
        return false;
    }

    void Rules::forget()
    {
        shapedFor.known = false;
        foreseenFor.known = false;
        foreseen.clear();
        milestonesKnown = false;
        monthKnown = false;
        deedsKnown = false;
    }

    bool Rules::stands(const Asked& asked, float age, const ElementMap& character) const
    {
        return asked.known and asked.age == age and asked.world == world and asked.lives == lives and asked.running == running and sameMap(asked.character, character) and sameMap(asked.done, done);
    }

    void Rules::keep(Asked& asked, float age, const ElementMap& character) const
    {
        asked.known = true;
        asked.age = age;
        asked.character = character;
        asked.done = done;
        asked.world = world;
        asked.lives = lives;
        asked.running = running;
    }

    bool Rules::load(EntitySystem* ecs, const std::string& root)
    {
        errors.clear();
        forget();
        nbRuns = 0;

        bool ok = true;

        for (auto [script, file] : {std::pair<RuleScript*, const char*>{&milestonesScript, "milestones.pg"},
                                    {&windowsScript, "windows.pg"},
                                    {&forecastScript, "forecast.pg"},
                                    {&resourcesScript, "resources.pg"},
                                    {&achievementsScript, "achievements.pg"},
                                    {&epitaphScript, "epitaph.pg"}})
        {
            if (not script->load(ecs, root + "/" + file))
            {
                errors.insert(errors.end(), script->errors().begin(), script->errors().end());
                ok = false;
            }
        }

        return ok;
    }

    bool Rules::shape(float age, const ElementMap& character)
    {
        if (stands(shapedFor, age, character))
            return true;

        RuleScript& s = windowsScript;
        s.clearErrors();

        shapedFor.known = false;

        s.set("age", ElementType{age});
        s.set("character", character);
        s.set("done", done);
        s.set("world", ElementType{world});
        s.set("lives", ElementType{lives});
        s.set("running", ElementType{running});
        s.set("activityId", ElementType{std::string()});

        ++nbRuns;

        std::vector<RuleScript::Record> records;

        // A list read is added to what its list holds
        shapedWindows.clear();

        if (not s.run() or not s.get("activities", {"gains", "costs", "requires"}, records) or not s.get("windows", shapedWindows))
            return fail(s);

        shapedActivities.clear();
        shapedActivities.reserve(records.size());

        for (auto& record : records)
        {
            RuleActivity activity;

            activity.fields = std::move(record.fields);
            activity.gains = std::move(record.lists[0]);
            activity.costs = std::move(record.lists[1]);
            activity.requires = std::move(record.lists[2]);

            shapedActivities.push_back(std::move(activity));
        }

        keep(shapedFor, age, character);

        return true;
    }

    bool Rules::activities(float age, const ElementMap& character, std::vector<RuleActivity>& out)
    {
        out.clear();

        if (not shape(age, character))
            return false;

        out = shapedActivities;

        return true;
    }

    bool Rules::milestones(float age, std::vector<RuleMilestone>& out, ElementMap& next, ElementMap* headline)
    {
        out.clear();
        next.clear();

        if (headline)
            headline->clear();

        if (not milestonesKnown or milestonesAge != age or milestonesWorld != world or milestonesLives != lives)
        {
            RuleScript& s = milestonesScript;
            s.clearErrors();

            milestonesKnown = false;

            s.set("age", ElementType{age});
            s.set("world", ElementType{world});
            s.set("lives", ElementType{lives});

            ++nbRuns;

            std::vector<RuleScript::Record> records;

            keptNext.clear();
            keptHeadline.clear();

            if (not s.run() or not s.get("milestones", {"asks"}, records) or not s.get("next", keptNext) or not s.get("headline", keptHeadline))
                return fail(s);

            keptMilestones.clear();

            for (auto& record : records)
            {
                RuleMilestone milestone;

                milestone.fields = std::move(record.fields);
                milestone.asks = std::move(record.lists[0]);

                keptMilestones.push_back(std::move(milestone));
            }

            milestonesKnown = true;
            milestonesAge = age;
            milestonesWorld = world;
            milestonesLives = lives;
        }

        out = keptMilestones;
        next = keptNext;

        if (headline)
            *headline = keptHeadline;

        return true;
    }

    bool Rules::windows(float age, const ElementMap& character, RecordList& out)
    {
        out.clear();

        if (not shape(age, character))
            return false;

        out = shapedWindows;

        return true;
    }

    bool Rules::forecast(float age, const ElementMap& character, const std::string& activityId, int monthsIn, RuleForecast& out)
    {
        out = RuleForecast{};

        if (not stands(foreseenFor, age, character))
        {
            foreseen.clear();
            keep(foreseenFor, age, character);
        }

        const auto asked = std::make_pair(activityId, monthsIn);

        if (auto it = foreseen.find(asked); it != foreseen.end())
        {
            out = it->second;
            return true;
        }

        RuleScript& s = forecastScript;
        s.clearErrors();

        s.set("age", ElementType{age});
        s.set("character", character);
        s.set("done", done);
        s.set("world", ElementType{world});
        s.set("lives", ElementType{lives});
        s.set("running", ElementType{running});
        s.set("activityId", ElementType{activityId});
        s.set("monthsIn", ElementType{monthsIn});

        ++nbRuns;

        ElementMap head;

        if (not s.run() or not s.get("forecast", head))
            return fail(s);

        out.error = text(head, "error");

        if (not out.error.empty())
        {
            s.error(out.error + " (`" + activityId + "`)");
            return fail(s);
        }

        out.percent = number(head, "percent", 0.0f);
        out.toward = number(head, "toward", 0.0f);
        out.months = static_cast<int>(number(head, "months", 0.0f));
        out.caption = text(head, "caption");

        if (not s.get("forecast.atStart", out.atStart) or not s.get("forecast.atTerm", out.atTerm) or not s.get("forecast.gaps", out.gaps) or not s.get("forecast.entries", out.entries))
            return fail(s);

        foreseen[asked] = out;

        return true;
    }

    bool Rules::month(float age, const ElementMap& character, bool board, RuleMonth& out)
    {
        out = RuleMonth{};

        if (monthKnown and monthAge == age and monthBoard == board and sameMap(monthCharacter, character))
        {
            out = keptMonth;
            return true;
        }

        RuleScript& s = resourcesScript;
        s.clearErrors();

        monthKnown = false;

        s.set("age", ElementType{age});
        s.set("character", character);
        s.set("board", ElementType{board});

        ++nbRuns;

        if (not s.run() or not s.get("month.after", out.after) or not s.get("month.entries", out.entries) or not s.get("rows", out.rows))
            return fail(s);

        ElementType death;
        ElementType warning;
        ElementType advice;
        std::vector<ElementType> hurt;
        std::vector<RuleScript::Record> glosses;

        if (not s.get("death", death) or not s.get("warning", warning) or not s.get("advice", advice) or not s.get("month.hurt", hurt) or not s.get("caps", out.caps) or not s.get("glosses", {"rows"}, glosses))
            return fail(s);

        out.death = death.toString();
        out.warning = warning.toString();
        out.advice = advice.toString();

        for (const auto& stat : hurt)
            out.hurt.push_back(stat.toString());

        for (auto& record : glosses)
        {
            RuleGloss gloss;

            gloss.fields = std::move(record.fields);
            gloss.rows = std::move(record.lists[0]);

            out.glosses.push_back(std::move(gloss));
        }

        monthKnown = true;
        monthAge = age;
        monthBoard = board;
        monthCharacter = character;
        keptMonth = out;

        return true;
    }

    bool Rules::achievements(std::vector<RuleAchievement>& out)
    {
        out.clear();

        // The deeds are the same whoever lives: read once
        if (not deedsKnown)
        {
            RuleScript& s = achievementsScript;
            s.clearErrors();

            ++nbRuns;

            std::vector<RuleScript::Record> records;

            if (not s.run() or not s.get("achievements", {"asks", "gives", "until"}, records))
                return fail(s);

            keptDeeds.clear();

            for (auto& record : records)
            {
                RuleAchievement achievement;

                achievement.fields = std::move(record.fields);
                achievement.asks = std::move(record.lists[0]);
                achievement.gives = std::move(record.lists[1]);
                achievement.until = std::move(record.lists[2]);

                const std::string kind = text(achievement.fields, "kind");

                achievement.kind = kind == "guide" ? RuleKind::Guide : kind == "lore" ? RuleKind::Lore : RuleKind::Deed;
                achievement.order = static_cast<int>(number(achievement.fields, "order", 0.0f));

                keptDeeds.push_back(std::move(achievement));
            }

            deedsKnown = true;
        }

        out = keptDeeds;

        return true;
    }
    bool Rules::epitaph(float age, const ElementMap& character, const std::vector<std::string>& deeds, RuleEpitaph& out)
    {
        RuleScript& s = epitaphScript;
        s.clearErrors();
        out = RuleEpitaph{};

        std::vector<ElementType> names;

        for (const auto& deed : deeds)
            names.push_back(ElementType{deed});

        s.set("age", ElementType{age});
        s.set("character", character);
        s.set("done", done);
        s.set("world", ElementType{world});
        s.set("lives", ElementType{lives});
        s.set("running", ElementType{running});
        s.set("activityId", ElementType{std::string()});
        s.set("deeds", names);

        ++nbRuns;

        ElementMap head;
        std::vector<ElementType> story;

        if (not s.run() or not s.get("epitaph", head) or not s.get("epitaph.story", story))
            return fail(s);

        out.cause = text(head, "cause");
        out.text = text(head, "text");
        out.tally = text(head, "tally");

        for (const auto& line : story)
            out.story.push_back(line.toString());

        return true;
    }
}
