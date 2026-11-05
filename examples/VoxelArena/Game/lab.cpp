#include "stdafx.h"

#include "lab.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <mutex>

#include <SDL2/SDL_scancode.h>

#include "Compiler/vm.h"
#include "Helpers/algorithmmodule.h"
#include "Helpers/mathmodule.h"
#include "Helpers/randommodule.h"
#include "Helpers/stringmodule.h"
#include "logger.h"

namespace fs = std::filesystem;

using namespace pg;

namespace arena
{
    using namespace ui;

    namespace
    {
        constexpr const char* DOM = "Lab";

        // The VM tells its errors through the logger only: while the lab's script runs, they are kept here
        thread_local std::vector<std::string>* captured = nullptr;

        class CaptureSink : public Logger::LogSink
        {
        public:
            void processLog(const Logger::Info& log) override
            {
                if (captured and log.level >= Logger::InfoLevel::error)
                    captured->push_back(log.message);
            }
        };

        struct Capture
        {
            std::vector<std::string> lines;
            std::vector<std::string>* previous;

            Capture() : previous(captured)
            {
                static std::once_flag once;
                std::call_once(once, [] { Logger::registerSink<CaptureSink>(); });
                captured = &lines;
            }
            ~Capture() { captured = previous; }

            std::string text() const
            {
                std::string out;
                for (const auto& l : lines)
                    out += (out.empty() ? "" : " | ") + l;
                return out;
            }
        };

        Value toValue(VM& vm, const Json& j)
        {
            switch (j.type)
            {
                case Json::Type::Bool: return makeBoolValue(j.boolean);
                // Whole numbers as integers: a level reads "level 3" in a caption, not "level 3.000000"
                case Json::Type::Number:
                    return std::abs(j.number - std::round(j.number)) < 1e-9 and std::abs(j.number) < 1e9 ? makeIntValue(static_cast<int>(std::lround(j.number)))
                                                                                                          : makeDoubleValue(j.number);
                case Json::Type::String: return vm.createString(j.string);
                case Json::Type::Array:
                {
                    Value list = vm.createVector();
                    for (const auto& e : j.array)
                        vm.asVector(list)->fields.push_back(toValue(vm, e));
                    return list;
                }
                case Json::Type::Object:
                {
                    Value table = vm.createTable();
                    for (const auto& [k, v] : j.object)
                        vm.asInstance(table)->setField(k, toValue(vm, v));
                    return table;
                }
                case Json::Type::Null:
                default:
                    return makeNoneValue();
            }
        }

        /** A script value as a command word: whole numbers without their decimals (wait(30) is "wait 30"). */
        std::string word(VM& vm, Value v)
        {
            if (IS_STRING(v))
                return vm.asString(v);
            if (IS_BOOL(v))
                return AS_BOOL(v) ? "on" : "off";

            double d = 0.0;
            if (IS_INT(v))
                return std::to_string(AS_INT(v));
            if (IS_DOUBLE(v))
                d = AS_DOUBLE(v);
            else
                return "none";

            if (std::abs(d - std::round(d)) < 1e-6)
                return std::to_string(static_cast<long long>(std::llround(d)));

            std::string s = std::to_string(d);
            while (s.back() == '0')
                s.pop_back();
            return s;
        }

        Json str(const std::string& s) { return Json::makeString(s); }
        Json num(double v) { return Json::makeNumber(v); }
        Json flag(bool b)
        {
            Json j;
            j.type = Json::Type::Bool;
            j.boolean = b;
            return j;
        }
        Json list()
        {
            Json j;
            j.type = Json::Type::Array;
            return j;
        }

        std::string localId(const std::string& id)
        {
            const size_t colon = id.find(':');
            return colon == std::string::npos ? id : id.substr(colon + 1);
        }

        /** Levels to pick from: the written ones, the branch's, and two of mastery past it. */
        std::vector<std::string> levelLabels(int written, int branchLevel)
        {
            const int branchAt = std::max(branchLevel, 1 + written + 1);
            std::vector<std::string> labels;
            for (int l = 1; l <= branchAt + 2; ++l)
                labels.push_back(std::to_string(l));
            return labels;
        }

        /** The input action of the ability slot holding `id` ("ability4"), "" if no slot does. */
        std::string actionOf(const Tuning& tuning, const std::string& id)
        {
            for (size_t i = 0; i < tuning.abilities.size(); ++i)
                for (const auto& a : tuning.abilities[i])
                    if (a == id)
                        return "ability" + std::to_string(i + 1);
            return "";
        }

        const char* kindNames[Lab::KINDS] = {"Items", "Weapons", "Skills", "Spells", "Enemies"};
        const char* showFunctions[Lab::KINDS] = {"showItem", "showWeapon", "showTechnique", "showSpell", "showEnemy"};

        constexpr float PX = 10.0f, PY = 10.0f, PW = 340.0f, PH = 700.0f;
        constexpr float LIST_Y = 100.0f, ROW_H = 23.0f;
        constexpr int ROWS = 14;
    }

    struct Lab::Script
    {
        std::unique_ptr<VM> vm;
        ObjFunction* fn = nullptr;

        ~Script()
        {
            if (vm and fn)
                vm->cleanupFunction(fn);
        }
    };

    Lab::Lab() = default;
    Lab::~Lab() = default;

    // --------------------------------------------------------------------------------------------
    // The lists
    // --------------------------------------------------------------------------------------------

    void Lab::build(const Content& content, const Tuning& tuning)
    {
        // The pick, to find again in the new lists
        std::string picked[KINDS];
        for (int k = 0; k < KINDS; ++k)
            if (index[k] >= 0 and index[k] < static_cast<int>(lists[k].size()))
                picked[k] = lists[k][static_cast<size_t>(index[k])].id;

        for (auto& l : lists)
            l.clear();

        const int branchLevel = content.run.branchLevel;
        auto byName = [](const Entry& a, const Entry& b) { return a.name < b.name; };

        // Items: the shop's, worn or circling the player
        {
            std::vector<std::string> rarityNames;
            for (const auto& r : content.run.rarities)
                rarityNames.push_back(r.name);
            if (rarityNames.empty())
                rarityNames.push_back("Common");

            for (const auto& [id, u] : content.upgrades)
            {
                Entry e;
                e.id = id;
                e.name = u.name.empty() ? localId(id) : u.name;
                e.text = u.text;
                e.levelLabel = "Rarity";

                Json rarities = list();
                for (size_t r = 0; r < rarityNames.size(); ++r)
                    if (u.comesAt(rarityNames[r]))
                    {
                        e.levels.push_back(rarityNames[r]);
                        rarities.array.push_back(num(static_cast<double>(r)));
                    }

                Json pieces = list();
                for (const auto& p : u.look.pieces)
                {
                    Json piece = Json::makeObject();
                    piece.set("model", str(p.model));
                    piece.set("part", str(p.part));
                    pieces.array.push_back(piece);
                }

                if (u.look.orbit)
                {
                    e.tag = "gem";
                    e.colour = Colour(u.look.colour * 255.0f, 255.0f);
                }
                else if (not u.look.pieces.empty())
                {
                    e.tag = u.look.pieces.front().part;
                    e.colour = TEXT_DIM;
                }
                else
                {
                    e.tag = "no look";
                    e.colour = FAINT;
                    e.dim = true;
                }

                e.info = Json::makeObject();
                e.info.set("kind", str("item"));
                e.info.set("id", str(id));
                e.info.set("name", str(e.name));
                e.info.set("text", str(u.text));
                e.info.set("orbit", flag(u.look.orbit));
                e.info.set("pieces", pieces);
                e.info.set("tag", str(u.tag));
                e.info.set("rarities", rarities);
                lists[static_cast<int>(Kind::Items)].push_back(std::move(e));
            }
        }

        // The weapons' techniques (and the terrain abilities), with the slot they are used from
        auto techniquesOf = [&](const std::string& type) {
            Json skills = list();
            for (const auto& [id, a] : content.abilities)
            {
                if (a.kind != "attack" or (not a.weapons.empty() and std::find(a.weapons.begin(), a.weapons.end(), type) == a.weapons.end()))
                    continue;
                Json s = Json::makeObject();
                s.set("id", str(id));
                s.set("name", str(a.name));
                s.set("action", str(actionOf(tuning, id)));
                s.set("charge", num(a.attack.charge.frames));
                skills.array.push_back(s);
            }
            return skills;
        };

        // Weapons, in their hot key order
        for (const auto& id : content.weaponOrder)
        {
            const auto it = content.weapons.find(id);
            if (it == content.weapons.end())
                continue;
            const WeaponDef& w = it->second;

            Entry e;
            e.id = id;
            e.name = w.name.empty() ? localId(id) : w.name;
            e.text = w.text;
            e.tag = w.role.empty() ? w.type : w.role;
            e.colour = KIND_WEAPON;
            e.levels = levelLabels(static_cast<int>(w.levels.size()), branchLevel);
            for (const auto& b : w.branches)
                e.branches.push_back(b.name);

            const Moveset& m = w.moveset;
            auto charge = [&](const std::string& attack) {
                const AttackDef* a = m.find(attack);
                return a ? a->charge.frames : 0;
            };

            e.info = Json::makeObject();
            e.info.set("kind", str("weapon"));
            e.info.set("id", str(id));
            e.info.set("name", str(e.name));
            e.info.set("text", str(w.text));
            e.info.set("type", str(w.type));
            e.info.set("role", str(w.role));
            e.info.set("light", str(m.light));
            e.info.set("heavy", str(m.heavy));
            e.info.set("dashAttack", str(m.dashAttack));
            e.info.set("dodgeAttack", str(m.dodgeAttack));
            e.info.set("airLight", str(m.airLight));
            e.info.set("airHeavy", str(m.airHeavy));
            e.info.set("lightCharge", num(charge(m.light)));
            e.info.set("heavyCharge", num(charge(m.heavy)));
            e.info.set("skills", techniquesOf(w.type));
            lists[static_cast<int>(Kind::Weapons)].push_back(std::move(e));
        }

        // Techniques: the weapon skills, then the terrain abilities
        for (const auto& [id, a] : content.abilities)
        {
            Entry e;
            e.id = id;
            e.name = a.name.empty() ? localId(id) : a.name;
            e.text = a.text;

            std::string weapon, role;
            for (const auto& wid : content.weaponOrder)
                if (const auto w = content.weapons.find(wid); w != content.weapons.end())
                    if (a.weapons.empty() or std::find(a.weapons.begin(), a.weapons.end(), w->second.type) != a.weapons.end())
                    {
                        weapon = wid;
                        role = w->second.role;
                        break;
                    }

            if (a.kind == "attack")
            {
                e.tag = a.weapons.empty() ? "any weapon" : a.weapons.front();
                e.colour = KIND_SKILL;
                e.levels = levelLabels(static_cast<int>(a.levels.size()), branchLevel);
                for (const auto& b : a.branches)
                    e.branches.push_back(b.name);
            }
            else
            {
                e.tag = "terrain";
                e.colour = MUTED;
            }

            e.info = Json::makeObject();
            e.info.set("kind", str("technique"));
            e.info.set("id", str(id));
            e.info.set("name", str(e.name));
            e.info.set("text", str(a.text));
            e.info.set("abilityKind", str(a.kind));
            e.info.set("action", str(actionOf(tuning, id)));
            e.info.set("weapon", str(weapon));
            e.info.set("role", str(role));
            e.info.set("charge", num(a.attack.charge.frames));
            e.info.set("range", num(a.range));
            lists[static_cast<int>(Kind::Techniques)].push_back(std::move(e));
        }
        // Weapon skills first: they are what grows
        std::stable_sort(lists[static_cast<int>(Kind::Techniques)].begin(), lists[static_cast<int>(Kind::Techniques)].end(),
                         [](const Entry& a, const Entry& b) { return (a.tag == "terrain") < (b.tag == "terrain"); });

        // Spells: the ready-made ones, cast from key 1
        const std::string spellKey = actionOf(tuning, "spell:1");
        for (const auto& [id, s] : content.spells)
        {
            Entry e;
            e.id = id;
            e.name = s.name.empty() ? localId(id) : s.name;
            e.text = s.text;
            e.tag = s.recipe.element;
            e.colour = elementText(s.recipe.element);
            e.levels = levelLabels(static_cast<int>(s.levels.size()), branchLevel);
            for (const auto& b : s.branches)
                e.branches.push_back(b.name);

            Json keywords = list();
            for (const auto& k : s.recipe.keywords)
                keywords.array.push_back(str(k));

            e.info = Json::makeObject();
            e.info.set("kind", str("spell"));
            e.info.set("id", str(id));
            e.info.set("name", str(e.name));
            e.info.set("text", str(s.text));
            e.info.set("element", str(s.recipe.element));
            e.info.set("keywords", keywords);
            e.info.set("action", str(spellKey.empty() ? "ability5" : spellKey));
            lists[static_cast<int>(Kind::Spells)].push_back(std::move(e));
        }

        // Enemies, plain or as an elite with each affix
        {
            std::vector<std::string> affixIds;
            std::vector<std::string> affixNames;
            for (const auto& [id, a] : content.affixes)
            {
                affixIds.push_back(id);
                affixNames.push_back(a.name.empty() ? localId(id) : a.name);
            }

            for (const auto& [id, d] : content.enemies)
            {
                Entry e;
                e.id = id;
                e.name = d.name.empty() ? localId(id) : d.name;
                e.tag = d.boss ? "boss" : (d.script.empty() ? "still" : "");
                e.colour = d.boss ? BOSS : MUTED;
                e.branches = affixNames;
                e.branchLabel = "Elite";
                e.noBranch = "Plain";

                Json attacks = list();
                for (const auto& [name, a] : d.moveset.attacks)
                    attacks.array.push_back(str(name));
                Json affixes = list();
                for (const auto& a : affixIds)
                    affixes.array.push_back(str(a));

                e.info = Json::makeObject();
                e.info.set("kind", str("enemy"));
                e.info.set("id", str(id));
                e.info.set("name", str(e.name));
                e.info.set("boss", flag(d.boss));
                e.info.set("brain", flag(not d.script.empty()));
                e.info.set("height", num(d.height));
                e.info.set("attacks", attacks);
                e.info.set("affixes", affixes);
                lists[static_cast<int>(Kind::Enemies)].push_back(std::move(e));
            }
        }

        for (int k : {static_cast<int>(Kind::Items), static_cast<int>(Kind::Spells), static_cast<int>(Kind::Enemies)})
            std::stable_sort(lists[k].begin(), lists[k].end(), byName);

        // The pick again (by id), else the first
        for (int k = 0; k < KINDS; ++k)
        {
            index[k] = 0;
            for (size_t i = 0; i < lists[k].size(); ++i)
                if (lists[k][i].id == picked[k])
                    index[k] = static_cast<int>(i);
        }

        if (const Entry* e = current())
        {
            level = std::clamp(level, 0, std::max(0, static_cast<int>(e->levels.size()) - 1));
            branch = std::clamp(branch, -1, static_cast<int>(e->branches.size()) - 1);
        }
        clampScroll();
    }

    // --------------------------------------------------------------------------------------------
    // The script
    // --------------------------------------------------------------------------------------------

    void Lab::load(const std::string& path, const Content& content, const Tuning& tuning)
    {
        scriptPath = path;
        build(content, tuning);

        std::error_code ec;
        scriptTime = fs::last_write_time(path, ec);
        script.reset();
        scriptOk = false;

        if (ec)
        {
            status = "no script at " + path;
            LOG_ERROR(DOM, status);
            return;
        }

        auto s = std::make_unique<Script>();

        // What a script answers with: console commands. run() takes the words of any command; the others
        // are the commands a scene uses most, by name (wait(30) queues "wait 30")
        auto setup = [this](VM& vm) {
            vm.sandboxedImports = true;
            vm.addNativeModule("math", MathModule{});
            vm.addNativeModule("algorithm", AlgorithmModule{});
            vm.addNativeModule("string", StringModule{});
            vm.addNativeModule("random", RandomModule{});

            auto joined = [](VM* vm, int argCount, Value* args) {
                std::string line;
                for (int i = 0; i < argCount; ++i)
                    line += (i ? " " : "") + word(*vm, args[i]);
                return line;
            };

            vm.registerNative("run", [this, joined](VM* vm, int argCount, Value* args) -> Value {
                out.push_back(joined(vm, argCount, args));
                return makeNoneValue();
            });

            for (const char* command : {"wait", "sleep", "press", "hold", "caption", "cam", "orbit", "face", "put", "view", "teleport", "weapon", "item",
                                        "grow", "spell", "god", "ai", "elite", "fill", "place", "slowmo", "clear", "spawn", "tune", "map"})
            {
                const std::string name = command;
                vm.registerNative(name, [this, joined, name](VM* vm, int argCount, Value* args) -> Value {
                    const std::string rest = joined(vm, argCount, args);
                    out.push_back(rest.empty() ? name : name + " " + rest);
                    return makeNoneValue();
                });
            }

            vm.registerNative("print", [this, joined](VM* vm, int argCount, Value* args) -> Value {
                const std::string line = joined(vm, argCount, args);
                LOG_INFO("lab.pg", line);
                printed.push_back(line);
                if (printed.size() > 4)
                    printed.erase(printed.begin());
                return makeNoneValue();
            });
        };

        // Compiled to bytecode by a throwaway VM, then run on the one that stays (as the enemies' scripts are)
        std::vector<char> bytes;
        {
            Capture capture;
            auto compiler = std::make_unique<VM>();
            setup(*compiler);

            const fs::path cacheDir = fs::temp_directory_path() / "voxelarena-scripts";
            fs::create_directories(cacheDir, ec);
            const std::string compiled = (cacheDir / "lab.pgc").string();

            if (compiler->interpretFromFile(path, true, compiled) != InterpretResult::OK)
            {
                status = "lab.pg does not compile: " + (capture.text().empty() ? std::string("see the log") : capture.text());
                LOG_ERROR(DOM, status);
                return;
            }

            std::ifstream in(compiled, std::ios::binary);
            bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            in.close();
            fs::remove(compiled, ec);
        }

        Capture capture;
        s->vm = std::make_unique<VM>();
        setup(*s->vm);
        s->fn = s->vm->prepareCachedFunction(bytes, path);

        if (not s->fn)
        {
            status = "lab.pg: its bytecode could not be prepared";
            LOG_ERROR(DOM, status);
            return;
        }

        out.clear();
        if (s->vm->runPreparedFunction(s->fn, 0, path) != InterpretResult::OK)
        {
            s->vm->frameCount = 0;
            status = "lab.pg failed while starting: " + capture.text();
            LOG_ERROR(DOM, status);
            return;
        }
        out.clear();   // the top level only defines

        script = std::move(s);
        scriptOk = true;
        status = "lab.pg ready";
        LOG_INFO(DOM, "Script " << path << " ready");
    }

    bool Lab::scriptChanged(float frameSeconds)
    {
        checkTimer -= frameSeconds;
        if (checkTimer > 0.0f or scriptPath.empty())
            return false;
        checkTimer = 0.5f;

        std::error_code ec;
        const auto t = fs::last_write_time(scriptPath, ec);
        return not ec and t != scriptTime;
    }

    std::vector<std::string> Lab::call(const std::string& function, const Json* arg)
    {
        out.clear();

        if (not script)
            return {};

        VM& vm = *script->vm;
        const Value fn = vm.findGlobal(function);

        if (not IS_CLOSURE(fn))
        {
            status = "lab.pg has no fun " + function + (arg ? "(pick)" : "()");
            return {};
        }

        const int arity = vm.asClosure(fn)->function->arity;
        if (arity != (arg ? 1 : 0))
        {
            status = "lab.pg: " + function + " takes " + (arg ? "one argument (the pick)" : "no argument");
            return {};
        }

        Capture capture;
        InterpretResult result = InterpretResult::RUNTIME_ERROR;

        try
        {
            vm.push(vm.retainValue(fn));
            if (arg)
                vm.push(toValue(vm, *arg));

            if (vm.call(vm.asClosure(fn), arg ? 1 : 0))
                result = vm.run();
        }
        catch (const std::exception& e)
        {
            capture.lines.push_back(e.what());
        }

        if (result != InterpretResult::OK)
        {
            vm.resetStack();
            vm.frameCount = 0;
            status = "lab.pg " + function + ": " + (capture.text().empty() ? std::string("runtime error") : capture.text());
            LOG_ERROR(DOM, status);
            out.clear();
            return {};
        }

        status = "lab.pg ready";
        return std::move(out);
    }

    std::vector<std::string> Lab::stage()
    {
        return call("stage", nullptr);
    }

    std::vector<std::string> Lab::show()
    {
        const Entry* e = current();
        if (not e)
            return {};

        // The pick with its level, branch or rarity
        Json pick = e->info;
        const std::string kindName = e->info.str("kind", "");

        if (kindName == "item")
        {
            const Json* rarities = e->info.find("rarities");
            const int r = rarities and level < static_cast<int>(rarities->array.size()) ? static_cast<int>(rarities->array[static_cast<size_t>(level)].number) : 0;
            pick.set("rarity", num(r));
            pick.set("rarityName", str(level < static_cast<int>(e->levels.size()) ? e->levels[static_cast<size_t>(level)] : ""));
        }
        else
        {
            pick.set("level", num(level + 1));
            pick.set("branch", num(branch));
            pick.set("branchName", str(branch >= 0 and branch < static_cast<int>(e->branches.size()) ? e->branches[static_cast<size_t>(branch)] : ""));
        }

        if (kindName == "enemy")
        {
            const Json* affixes = e->info.find("affixes");
            pick.set("affix", str(branch >= 0 and affixes and branch < static_cast<int>(affixes->array.size()) ? affixes->array[static_cast<size_t>(branch)].string : ""));
        }

        return call(showFunctions[kind], &pick);
    }

    // --------------------------------------------------------------------------------------------
    // Picking
    // --------------------------------------------------------------------------------------------

    const Lab::Entry* Lab::current() const
    {
        const auto& l = lists[kind];
        const int i = index[kind];
        return i >= 0 and i < static_cast<int>(l.size()) ? &l[static_cast<size_t>(i)] : nullptr;
    }

    void Lab::select(int i)
    {
        const int n = static_cast<int>(lists[kind].size());
        if (n == 0)
            return;

        index[kind] = (i % n + n) % n;
        level = 0;
        branch = -1;
        clampScroll();
    }

    void Lab::clampScroll()
    {
        for (int k = 0; k < KINDS; ++k)
        {
            const int n = static_cast<int>(lists[k].size());
            int& top = scrollTop[k];

            if (index[k] < top)
                top = index[k];
            if (index[k] >= top + ROWS)
                top = index[k] - ROWS + 1;
            top = std::clamp(top, 0, std::max(0, n - ROWS));
        }
    }

    void Lab::scroll(float steps)
    {
        const int n = static_cast<int>(lists[kind].size());
        scrollTop[kind] = std::clamp(scrollTop[kind] - static_cast<int>(std::round(steps * 3.0f)), 0, std::max(0, n - ROWS));
    }

    bool Lab::pick(const std::string& id, int newLevel, int newBranch)
    {
        for (int k = 0; k < KINDS; ++k)
            for (size_t i = 0; i < lists[k].size(); ++i)
            {
                const Entry& e = lists[k][i];
                if (e.id != id and localId(e.id) != id)
                    continue;

                kind = k;
                index[k] = static_cast<int>(i);
                level = std::clamp(newLevel - 1, 0, std::max(0, static_cast<int>(e.levels.size()) - 1));
                branch = std::clamp(newBranch, -1, static_cast<int>(e.branches.size()) - 1);
                clampScroll();
                return true;
            }
        return false;
    }

    bool Lab::key(int scancode, bool& replay)
    {
        const Entry* e = current();
        const int levels = e ? static_cast<int>(e->levels.size()) : 0;
        const int branches = e ? static_cast<int>(e->branches.size()) : 0;

        switch (scancode)
        {
            case SDL_SCANCODE_UP: select(index[kind] - 1); replay = true; return true;
            case SDL_SCANCODE_DOWN: select(index[kind] + 1); replay = true; return true;
            case SDL_SCANCODE_PAGEUP:
            case SDL_SCANCODE_PAGEDOWN:
                kind = (kind + (scancode == SDL_SCANCODE_PAGEUP ? KINDS - 1 : 1)) % KINDS;
                level = 0;
                branch = -1;
                replay = true;
                return true;
            case SDL_SCANCODE_LEFT:
            case SDL_SCANCODE_RIGHT:
            {
                const int next = std::clamp(level + (scancode == SDL_SCANCODE_LEFT ? -1 : 1), 0, std::max(0, levels - 1));
                replay = next != level;
                level = next;
                return true;
            }
            case SDL_SCANCODE_B:
                if (branches > 0)
                {
                    branch = branch + 1 >= branches ? -1 : branch + 1;
                    replay = true;
                }
                return true;
            case SDL_SCANCODE_RETURN:
            case SDL_SCANCODE_KP_ENTER:
                replay = true;
                return true;
            case SDL_SCANCODE_L:
                loop = not loop;
                return true;
            default:
                return false;
        }
    }

    bool Lab::click(int action, bool& replay, std::string& command)
    {
        if (action >= ACTION_KIND and action < ACTION_KIND + KINDS)
        {
            kind = action - ACTION_KIND;
            level = 0;
            branch = -1;
            replay = true;
            return true;
        }

        if (action >= ACTION_ENTRY and action < ACTION_ENTRY + ROWS)
        {
            select(scrollTop[kind] + action - ACTION_ENTRY);
            replay = true;
            return true;
        }

        if (action >= ACTION_LEVEL and action < ACTION_LEVEL + 100)
        {
            level = action - ACTION_LEVEL;
            replay = true;
            return true;
        }

        if (action >= ACTION_BRANCH and action < ACTION_BRANCH + 100)
        {
            branch = action - ACTION_BRANCH - 1;
            replay = true;
            return true;
        }

        switch (action)
        {
            case ACTION_REPLAY: replay = true; return true;
            case ACTION_STAGE: replay = true; return true;
            case ACTION_LOOP: loop = not loop; return true;
            case ACTION_SLOW: command = "slowmo"; return true;
            case ACTION_BOXES: command = "hitboxes"; return true;
            case ACTION_REACH: command = "reach"; return true;
            case ACTION_VIEW: command = "view"; return true;
            case ACTION_ORBIT: command = "orbit"; return true;
            case ACTION_CLOSE: command = "lab off"; return true;
            default: return false;
        }
    }

    // The panel is drawn from the window's top left corner: base units are window pixels over the UI's scale
    // (not Ui::X, which goes by the anchor drawn with last)
    float Lab::coverRight(const Ui& ui) const
    {
        return (PX + PW) * ui.scale();
    }

    bool Lab::covers(const Ui& ui, float px, float py) const
    {
        const float x = px / ui.scale(), y = py / ui.scale();
        return x >= PX and x <= PX + PW and y >= PY and y <= PY + PH;
    }

    // --------------------------------------------------------------------------------------------
    // The panel
    // --------------------------------------------------------------------------------------------

    void Lab::draw(Ui& ui, const View& v)
    {
        ui.section("lab");
        ui.anchor(Anchor::TopLeft);

        ui.raised(PX, PY, PW, PH, PANEL, 4.0f);
        ui.hit(PX, PY, PW, PH, -1);   // the panel's blank space: no click through to the arena

        const float x0 = PX + 16.0f, inner = PW - 32.0f;

        // The title, and how the script is doing
        ui.text(x0, PY + 12, "LAB", Font::Display, 28, GOLD, Align::Left, 1.0f);
        const bool fine = scriptOk and status == "lab.pg ready";
        ui.text(x0 + 62, PY + 22, fine ? (v.busy ? "playing..." : "lab.pg ready") : "script error", Font::Text600, 11, fine ? (v.busy ? GOLD : GOOD) : WARNING);
        {
            const float bx = PX + PW - 16.0f - 58.0f, by = PY + 14.0f;
            const bool over = ui.hover(bx, by, 58, 24);
            ui.box(bx, by, 58, 24, CARD, 2.0f, over ? GOLD : LINE);
            ui.text(bx + 29, by + 5, "Hide", Font::Text700, 11, over ? GOLD : TEXT_DIM, Align::Centre);
            ui.hit(bx, by, 58, 24, ACTION_CLOSE);
        }

        // The kinds
        {
            const float gap = 4.0f, tw = (inner - gap * (KINDS - 1)) / KINDS, ty = PY + 54.0f;
            for (int k = 0; k < KINDS; ++k)
            {
                const auto s = ui.scope(static_cast<uint64_t>(k));
                const float tx = x0 + static_cast<float>(k) * (tw + gap);
                const bool on = k == kind, over = ui.hover(tx, ty, tw, 28);
                ui.box(tx, ty, tw, 28, on ? hex(0x2A2414) : CARD, 2.0f, on ? GOLD : (over ? TEXT_DIM : LINE));
                ui.text(tx + tw * 0.5f, ty + 7, kindNames[k], Font::Text700, 11, on ? GOLD : TEXT_DIM, Align::Centre);
                ui.hit(tx, ty, tw, 28, ACTION_KIND + k);
            }
        }

        // The list
        const auto& l = lists[kind];
        ui.box(x0, LIST_Y, inner, ROW_H * ROWS + 8.0f, WELL, 2.0f, LINE);
        for (int r = 0; r < ROWS; ++r)
        {
            const int i = scrollTop[kind] + r;
            if (i >= static_cast<int>(l.size()))
                break;

            const auto s = ui.scope(static_cast<uint64_t>(r));
            const Entry& e = l[static_cast<size_t>(i)];
            const float ry = LIST_Y + 4.0f + static_cast<float>(r) * ROW_H;
            const bool on = i == index[kind], over = ui.hover(x0 + 2, ry, inner - 4, ROW_H);

            if (on)
                ui.rect(x0 + 2, ry, inner - 4, ROW_H, hex(0x2A2414));
            else if (over)
                ui.rect(x0 + 2, ry, inner - 4, ROW_H, CARD);

            const float tagW = e.tag.empty() ? 0.0f : ui.measure(e.tag, Font::Text500, 11) + 8.0f;
            ui.text(x0 + 10, ry + 4, ui.fit(e.name, Font::Text600, 13, inner - 24 - tagW), Font::Text600, 13, on ? GOLD : (e.dim ? FAINT : TEXT));
            if (not e.tag.empty())
                ui.text(x0 + inner - 10, ry + 6, e.tag, Font::Text500, 11, e.colour, Align::Right);
            ui.hit(x0 + 2, ry, inner - 4, ROW_H, ACTION_ENTRY + r);
        }

        if (l.size() > static_cast<size_t>(ROWS))
        {
            // Where the list is scrolled to
            const float trackH = ROW_H * ROWS, barH = std::max(16.0f, trackH * ROWS / static_cast<float>(l.size()));
            const float barY = LIST_Y + 4.0f + (trackH - barH) * static_cast<float>(scrollTop[kind]) / static_cast<float>(l.size() - ROWS);
            ui.rect(x0 + inner - 4, barY, 2, barH, LINE);
        }

        float y = LIST_Y + ROW_H * ROWS + 20.0f;
        const Entry* e = current();

        if (e)
        {
            ui.text(x0, y, ui.fit(e->name, Font::Display, 24, inner - 80), Font::Display, 24, TEXT);
            ui.text(x0 + inner, y + 9, std::to_string(index[kind] + 1) + " / " + std::to_string(l.size()), Font::Text600, 11, FAINT, Align::Right);
            y += 32.0f;
            ui.text(x0, y, ui.fit(e->id, Font::Mono, 11, inner), Font::Mono, 11, FAINT);
            y += 18.0f;

            if (not e->text.empty())
                y += ui.paragraph(x0, y, inner, e->text, Font::Text500, 12, TEXT_DIM, 16.0f, 3) + 6.0f;

            // A row of chips that wraps: the levels, the branches
            auto chips = [&](const std::string& label, const std::vector<std::string>& names, int picked, int action, int first) {
                ui.text(x0, y + 5, label, Font::Text700, 11, MUTED);
                float cx = x0 + 58.0f;
                for (size_t c = 0; c < names.size(); ++c)
                {
                    const auto s = ui.scope(Ui::idOf(label) + c);
                    const float cw = std::max(24.0f, ui.measure(names[c], Font::Text700, 11) + 14.0f);
                    if (cx + cw > x0 + inner and cx > x0 + 58.0f)
                    {
                        cx = x0 + 58.0f;
                        y += 28.0f;
                    }
                    const bool on = static_cast<int>(c) + first == picked, over = ui.hover(cx, y, cw, 24);
                    ui.box(cx, y, cw, 24, on ? hex(0x2A2414) : CARD, 2.0f, on ? GOLD : (over ? TEXT_DIM : LINE));
                    ui.text(cx + cw * 0.5f, y + 5, names[c], Font::Text700, 11, on ? GOLD : TEXT_DIM, Align::Centre);
                    ui.hit(cx, y, cw, 24, action + static_cast<int>(c));
                    cx += cw + 4.0f;
                }
                y += 32.0f;
            };

            if (e->levels.size() > 1)
                chips(e->levelLabel, e->levels, level, ACTION_LEVEL, 0);

            if (not e->branches.empty())
            {
                std::vector<std::string> names = {e->noBranch};
                names.insert(names.end(), e->branches.begin(), e->branches.end());
                chips(e->branchLabel, names, branch + 1, ACTION_BRANCH, 0);
            }
        }

        // What went wrong in the script (it shows over the details, until it is fixed)
        if (not scriptOk or status != "lab.pg ready")
        {
            ui.box(x0, y, inner, 64, hex(0x2A1414), 2.0f, WARNING);
            ui.paragraph(x0 + 8, y + 6, inner - 16, status, Font::Text500, 11, WARNING, 14.0f, 4);
            y += 72.0f;
        }
        else if (not printed.empty())
        {
            ui.text(x0, y, ui.fit("print: " + printed.back(), Font::Mono, 11, inner), Font::Mono, 11, TEXT_DIM);
        }

        // The buttons: replay, and the toggles
        {
            const float by = PY + PH - 76.0f;
            struct Button
            {
                const char* label;
                int action;
                bool on;
            };
            const Button buttons[] = {{"Replay", ACTION_REPLAY, false}, {"Loop", ACTION_LOOP, v.loop},   {"Slow", ACTION_SLOW, v.slow},
                                      {"Boxes", ACTION_BOXES, v.boxes},  {"Reach", ACTION_REACH, v.reach}, {"Orbit", ACTION_ORBIT, v.orbit},
                                      {v.firstPerson ? "1st" : "3rd", ACTION_VIEW, false}};
            constexpr int COUNT = 7;
            const float gap = 4.0f, bw = (inner - gap * (COUNT - 1)) / COUNT;
            for (int b = 0; b < COUNT; ++b)
            {
                const auto s = ui.scope(static_cast<uint64_t>(b));
                const float bx = x0 + static_cast<float>(b) * (bw + gap);
                const bool over = ui.hover(bx, by, bw, 30), replay = buttons[b].action == ACTION_REPLAY;
                ui.box(bx, by, bw, 30, replay ? GOLD : (buttons[b].on ? hex(0x2A2414) : CARD), 2.0f, over ? TEXT : (buttons[b].on ? GOLD : LINE));
                ui.text(bx + bw * 0.5f, by + 8, buttons[b].label, Font::Text700, 11, replay ? GOLD_INK : (buttons[b].on ? GOLD : TEXT_DIM), Align::Centre);
                ui.hit(bx, by, bw, 30, buttons[b].action);
            }
        }

        ui.text(x0, PY + PH - 36.0f, "Up/Down pick   Left/Right level   B branch   PgUp/PgDn tab", Font::Text500, 11, FAINT);
        ui.text(x0, PY + PH - 21.0f, "Enter replay   L loop   drag: turn the camera   F3 hide", Font::Text500, 11, FAINT);

        // What the scene shows now, over the arena
        if (not v.caption.empty())
        {
            ui.anchor(Anchor::Top);
            const float w = ui.measure(v.caption, Font::Display, 26) + 40.0f;
            const float cx = 640.0f + (PX + PW) * 0.5f;   // the middle of what the panel leaves
            ui.raised(cx - w * 0.5f, 20, w, 44, PANEL, 3.0f);
            ui.text(cx, 28, v.caption, Font::Display, 26, GOLD, Align::Centre);
        }
    }
}
