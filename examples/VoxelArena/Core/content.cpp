#include "content.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace arena
{
    namespace
    {
        /**
         * What in the content only looks, by kind of definition: left out of the fingerprint, so
         * players may have their own (a texture pack, other sounds, another sword model) and still
         * play together. Paths go down objects; "*" is every member. Everything else is gameplay
         * and must match.
         */
        const std::vector<std::string> TUNING_LOOKS = {
            "camera", "firstPerson", "controls",
            "player.model", "player.modelScale", "player.hand",
            "feel.hitFlashFrames", "feel.shakeDecay", "feel.shakeOffset", "feel.shakeAngle", "feel.shakeFrequency", "feel.landingShake",
            "terrain.crushShake",
        };

        const std::map<std::string, std::vector<std::string>> DEF_LOOKS = {
            {"block", {"color", "colorNoise", "visible"}},
            {"weapon", {"name", "model", "modelScale", "grip", "rest",
                        "moveset.attacks.*.swing", "moveset.attacks.*.shake", "moveset.attacks.*.turn",
                        "moveset.attacks.*.projectile.model", "moveset.attacks.*.projectile.modelScale", "moveset.attacks.*.projectile.shake"}},
            {"enemy", {"name", "model", "modelScale",
                       "moveset.attacks.*.swing", "moveset.attacks.*.shake", "moveset.attacks.*.turn",
                       "moveset.attacks.*.projectile.model", "moveset.attacks.*.projectile.modelScale", "moveset.attacks.*.projectile.shake"}},
            {"ability", {"name", "projectile.model", "projectile.modelScale", "projectile.shake"}},
        };

        void dropPath(Json& j, const std::vector<std::string>& parts, size_t at)
        {
            if (not j.isObject() or at >= parts.size())
                return;

            const std::string& key = parts[at];
            const bool last = at + 1 == parts.size();

            for (auto it = j.object.begin(); it != j.object.end();)
            {
                if (key == "*" or it->first == key)
                {
                    if (last)
                    {
                        it = j.object.erase(it);
                        continue;
                    }

                    dropPath(it->second, parts, at + 1);
                }

                ++it;
            }
        }

        Json withoutLooks(Json j, const std::vector<std::string>& paths)
        {
            for (const auto& path : paths)
            {
                std::vector<std::string> parts;
                size_t from = 0;

                for (size_t dot = path.find('.'); ; dot = path.find('.', from))
                {
                    parts.push_back(path.substr(from, dot == std::string::npos ? std::string::npos : dot - from));

                    if (dot == std::string::npos)
                        break;

                    from = dot + 1;
                }

                dropPath(j, parts, 0);
            }

            return j;
        }
    }

    std::string ContentMessage::str() const
    {
        if (line > 0)
            return file + ":" + std::to_string(line) + ": " + text;

        return file + ": " + text;
    }

    bool elementFromName(const std::string& name, Element& out)
    {
        if (name.empty()) { out = Element::None; return true; }
        if (name == "fire") { out = Element::Fire; return true; }
        if (name == "ice") { out = Element::Ice; return true; }
        if (name == "volt") { out = Element::Volt; return true; }
        return false;
    }

    const char* elementName(Element e)
    {
        switch (e)
        {
            case Element::Fire: return "fire";
            case Element::Ice: return "ice";
            case Element::Volt: return "volt";
            case Element::None: break;
        }

        return "";
    }

    std::string resolveId(const std::string& ref, const std::string& currentMod)
    {
        if (ref.find(':') != std::string::npos)
            return ref;

        return currentMod + ":" + ref;
    }

    std::pair<std::string, std::string> splitId(const std::string& id)
    {
        const auto colon = id.find(':');

        if (colon == std::string::npos)
            return {"", id};

        return {id.substr(0, colon), id.substr(colon + 1)};
    }

    const AttackDef* Moveset::find(const std::string& name) const
    {
        const auto it = attacks.find(name);
        return it == attacks.end() ? nullptr : &it->second;
    }

    BlockId Content::block(const std::string& id) const
    {
        const auto it = blockIds.find(id);
        return it == blockIds.end() ? AIR : it->second;
    }

    const AttackDef* Content::attackOf(const std::string& source, const std::string& attack) const
    {
        if (auto e = enemies.find(source); e != enemies.end())
            return e->second.moveset.find(attack);

        if (auto w = weapons.find(source); w != weapons.end())
            return w->second.moveset.find(attack);

        return nullptr;
    }

    const ModInfo* Content::mod(const std::string& id) const
    {
        for (const auto& m : mods)
        {
            if (m.id == id)
                return &m;
        }

        return nullptr;
    }

    std::string Content::assetPath(const std::string& id, const std::string& folder, const std::string& extension) const
    {
        const auto [ns, name] = splitId(id);
        const ModInfo* m = mod(ns);

        if (not m)
            return "";

        return (fs::path(m->dir) / folder / (name + extension)).string();
    }

    namespace
    {
        bool readText(const std::string& path, std::string& out)
        {
            std::ifstream in(path, std::ios::binary);

            if (not in)
                return false;

            std::ostringstream ss;
            ss << in.rdbuf();
            out = ss.str();
            return true;
        }

        /** Reads and parses a JSON file; a missing or broken file is reported and gives false. */
        bool readJsonFile(const std::string& path, Json& out, ContentReport& report)
        {
            std::string text;

            if (not readText(path, text))
            {
                report.error(path, 0, "cannot be read");
                return false;
            }

            auto parsed = parseJson(text);

            if (not parsed.ok)
            {
                report.error(path, parsed.errorLine, parsed.error.substr(parsed.error.find(':') + 2));
                return false;
            }

            out = std::move(parsed.value);
            return true;
        }

        bool validName(const std::string& s)
        {
            if (s.empty())
                return false;

            for (char c : s)
            {
                if (not ((c >= 'a' and c <= 'z') or (c >= '0' and c <= '9') or c == '_' or c == '.' or c == '/'))
                    return false;
            }

            return true;
        }

        /** Warns about keys that are not in the known list: a typo would otherwise be silently ignored. */
        void checkKeys(const Json& json, std::initializer_list<const char*> known, const std::string& file, ContentReport& report, const std::string& where)
        {
            if (not json.isObject())
                return;

            for (const auto& kv : json.object)
            {
                bool found = false;

                for (const char* k : known)
                {
                    if (kv.first == k)
                    {
                        found = true;
                        break;
                    }
                }

                if (not found)
                {
                    std::string known_list;
                    for (const char* k : known)
                        known_list += std::string(known_list.empty() ? "" : ", ") + k;

                    report.warn(file, kv.second.line, "unknown key \"" + kv.first + "\" in " + where + " (known: " + known_list + ")");
                }
            }
        }

        // Typed field reads: a missing key keeps the default, a key of the wrong type is an error

        void readNum(const Json& json, const char* key, float& out, const std::string& file, ContentReport& report)
        {
            const Json* v = json.find(key);

            if (not v)
                return;

            if (not v->isNumber())
            {
                report.error(file, v->line, std::string("\"") + key + "\" must be a number");
                return;
            }

            out = static_cast<float>(v->number);
        }

        void readInt(const Json& json, const char* key, int& out, const std::string& file, ContentReport& report)
        {
            const Json* v = json.find(key);

            if (not v)
                return;

            if (not v->isNumber() or std::fabs(v->number - static_cast<double>(static_cast<int>(v->number))) > 0.0)
            {
                report.error(file, v->line, std::string("\"") + key + "\" must be a whole number");
                return;
            }

            out = static_cast<int>(v->number);
        }

        void readStr(const Json& json, const char* key, std::string& out, const std::string& file, ContentReport& report)
        {
            const Json* v = json.find(key);

            if (not v)
                return;

            if (not v->isString())
            {
                report.error(file, v->line, std::string("\"") + key + "\" must be a string");
                return;
            }

            out = v->string;
        }

        void readBool(const Json& json, const char* key, bool& out, const std::string& file, ContentReport& report)
        {
            const Json* v = json.find(key);

            if (not v)
                return;

            if (not v->isBool())
            {
                report.error(file, v->line, std::string("\"") + key + "\" must be true or false");
                return;
            }

            out = v->boolean;
        }

        bool toVec(const Json& v, int n, float* out)
        {
            if (not v.isArray() or static_cast<int>(v.array.size()) != n)
                return false;

            for (int i = 0; i < n; ++i)
            {
                if (not v.array[i].isNumber())
                    return false;
            }

            for (int i = 0; i < n; ++i)
                out[i] = static_cast<float>(v.array[i].number);

            return true;
        }

        void readVec3(const Json& json, const char* key, glm::vec3& out, const std::string& file, ContentReport& report)
        {
            const Json* v = json.find(key);

            if (not v)
                return;

            float tmp[3];

            if (not toVec(*v, 3, tmp))
            {
                report.error(file, v->line, std::string("\"") + key + "\" must be a list of 3 numbers");
                return;
            }

            out = glm::vec3(tmp[0], tmp[1], tmp[2]);
        }

        void readIVec3(const Json& json, const char* key, glm::ivec3& out, const std::string& file, ContentReport& report)
        {
            glm::vec3 f(out);
            const size_t before = report.errors.size();
            readVec3(json, key, f, file, report);

            if (report.errors.size() == before)
                out = glm::ivec3(glm::round(f));
        }

        void readStringList(const Json& json, const char* key, std::vector<std::string>& out, const std::string& file, ContentReport& report)
        {
            const Json* v = json.find(key);

            if (not v)
                return;

            if (not v->isArray())
            {
                report.error(file, v->line, std::string("\"") + key + "\" must be a list of strings");
                return;
            }

            for (const auto& e : v->array)
            {
                if (e.isString())
                    out.push_back(e.string);
                else
                    report.error(file, e.line, std::string("\"") + key + "\" must hold strings only");
            }
        }

        void readElement(const Json& json, Element& out, const std::string& file, ContentReport& report)
        {
            const Json* e = json.find("element");

            if (not e)
                return;

            if (not e->isString() or not elementFromName(e->string, out))
                report.error(file, e->line, "\"element\" is \"fire\", \"ice\" or \"volt\"");
        }

        /** "status": "burn", or { "id": "chill", "stacks": 2 } */
        void readStatusHit(const Json& json, StatusHit& out, const std::string& file, ContentReport& report)
        {
            const Json* s = json.find("status");

            if (not s)
                return;

            if (s->isString())
            {
                out.status = s->string;
            }
            else if (s->isObject())
            {
                checkKeys(*s, {"id", "stacks"}, file, report, "a status");
                readStr(*s, "id", out.status, file, report);
                readInt(*s, "stacks", out.stacks, file, report);
                out.stacks = std::clamp(out.stacks, 1, 99);
            }
            else
            {
                report.error(file, s->line, "\"status\" is a status id, or { \"id\": ..., \"stacks\": n }");
            }
        }

        void readProjectile(const Json& pj, ProjectileDef& p, const std::string& file, ContentReport& report, const std::string& where)
        {
            checkKeys(pj, {"speed", "gravity", "radius", "lifetime", "lead", "damage", "poiseDamage", "knockback", "terrainDamage", "hitstop", "shake", "spawn", "model", "modelScale",
                           "element", "status", "burst", "terrainRadius"},
                      file, report, where);

            p.enabled = true;
            readNum(pj, "speed", p.speed, file, report);
            readNum(pj, "gravity", p.gravity, file, report);
            readNum(pj, "radius", p.radius, file, report);
            readNum(pj, "lifetime", p.lifetime, file, report);
            readNum(pj, "lead", p.lead, file, report);
            readNum(pj, "damage", p.damage, file, report);
            readNum(pj, "poiseDamage", p.poiseDamage, file, report);
            readNum(pj, "knockback", p.knockback, file, report);
            readNum(pj, "terrainDamage", p.terrainDamage, file, report);
            readInt(pj, "hitstop", p.hitstop, file, report);
            readNum(pj, "shake", p.shake, file, report);
            readVec3(pj, "spawn", p.spawn, file, report);
            readStr(pj, "model", p.model, file, report);
            readNum(pj, "modelScale", p.modelScale, file, report);
            readElement(pj, p.element, file, report);
            readStatusHit(pj, p.status, file, report);
            readNum(pj, "burst", p.burst, file, report);
            readNum(pj, "terrainRadius", p.terrainRadius, file, report);
            p.speed = std::max(0.1f, p.speed);
        }

        void readSwing(const Json& json, SwingPose& out, const std::string& file, ContentReport& report)
        {
            checkKeys(json, {"yawFrom", "yawTo", "pitchFrom", "pitchTo", "yaw", "pitch", "reach"}, file, report, "a swing");

            // "yaw"/"pitch" set both ends (a still pose)
            if (json.has("yaw"))
            {
                readNum(json, "yaw", out.yawFrom, file, report);
                out.yawTo = out.yawFrom;
            }

            if (json.has("pitch"))
            {
                readNum(json, "pitch", out.pitchFrom, file, report);
                out.pitchTo = out.pitchFrom;
            }

            readNum(json, "yawFrom", out.yawFrom, file, report);
            readNum(json, "yawTo", out.yawTo, file, report);
            readNum(json, "pitchFrom", out.pitchFrom, file, report);
            readNum(json, "pitchTo", out.pitchTo, file, report);
            readNum(json, "reach", out.reach, file, report);
        }

        // ----------------------------------------------------------------------------------------
        // Mod discovery and order
        // ----------------------------------------------------------------------------------------

        std::vector<ModInfo> discoverMods(const std::string& modsDir, ContentReport& report)
        {
            std::vector<ModInfo> found;
            std::error_code ec;

            if (not fs::is_directory(modsDir, ec))
            {
                report.error(modsDir, 0, "the mods folder does not exist");
                return found;
            }

            std::vector<fs::path> dirs;

            for (const auto& entry : fs::directory_iterator(modsDir, ec))
            {
                if (entry.is_directory())
                    dirs.push_back(entry.path());
            }

            std::sort(dirs.begin(), dirs.end());

            std::set<std::string> ids;

            for (const auto& dir : dirs)
            {
                const auto manifest = (dir / "mod.json").string();

                if (not fs::exists(manifest, ec))
                {
                    report.warn(dir.string(), 0, "has no mod.json, skipped");
                    continue;
                }

                Json json;
                if (not readJsonFile(manifest, json, report))
                    continue;

                ModInfo mod;
                mod.dir = dir.string();
                mod.id = dir.filename().string();

                checkKeys(json, {"id", "name", "version", "api", "dependencies", "loadAfter", "description"}, manifest, report, "mod.json");

                readStr(json, "id", mod.id, manifest, report);
                readStr(json, "name", mod.name, manifest, report);
                readStr(json, "version", mod.version, manifest, report);
                readInt(json, "api", mod.api, manifest, report);
                readStringList(json, "dependencies", mod.dependencies, manifest, report);
                readStringList(json, "loadAfter", mod.loadAfter, manifest, report);

                if (not validName(mod.id) or mod.id.find('/') != std::string::npos)
                {
                    report.error(manifest, json.find("id") ? json.find("id")->line : 0, "mod id \"" + mod.id + "\" may only use a-z, 0-9, _ and .");
                    continue;
                }

                if (ids.count(mod.id))
                {
                    report.error(manifest, 0, "a mod with the id \"" + mod.id + "\" is already loaded, skipped");
                    continue;
                }

                if (mod.api != CONTENT_API_VERSION)
                    report.warn(manifest, 0, "written for content API " + std::to_string(mod.api) + ", the game offers " + std::to_string(CONTENT_API_VERSION) + " (the API is unstable: expect breakage)");

                ids.insert(mod.id);
                found.push_back(std::move(mod));
            }

            return found;
        }

        /** Dependencies first; among mods free to load, alphabetical. Mods with a missing dependency or in a cycle are dropped. */
        std::vector<ModInfo> orderMods(std::vector<ModInfo> mods, ContentReport& report)
        {
            // Drop mods whose dependencies are missing, until nothing changes (a drop can orphan another mod)
            bool changed = true;

            while (changed)
            {
                changed = false;

                std::set<std::string> present;
                for (const auto& m : mods)
                    present.insert(m.id);

                for (auto it = mods.begin(); it != mods.end(); ++it)
                {
                    for (const auto& dep : it->dependencies)
                    {
                        if (not present.count(dep))
                        {
                            report.error((fs::path(it->dir) / "mod.json").string(), 0, "needs the mod \"" + dep + "\", which is not loaded: \"" + it->id + "\" is skipped");
                            mods.erase(it);
                            changed = true;
                            break;
                        }
                    }

                    if (changed)
                        break;
                }
            }

            std::set<std::string> present;
            for (const auto& m : mods)
                present.insert(m.id);

            std::vector<ModInfo> ordered;
            std::set<std::string> done;

            while (ordered.size() < mods.size())
            {
                const ModInfo* next = nullptr;

                for (const auto& m : mods)
                {
                    if (done.count(m.id))
                        continue;

                    bool ready = true;

                    for (const auto& dep : m.dependencies)
                        ready = ready and done.count(dep);

                    for (const auto& after : m.loadAfter)
                        ready = ready and (done.count(after) or not present.count(after));

                    if (ready and (not next or m.id < next->id))
                        next = &m;
                }

                if (not next)
                {
                    for (const auto& m : mods)
                    {
                        if (not done.count(m.id))
                            report.error((fs::path(m.dir) / "mod.json").string(), 0, "is part of a dependency cycle: \"" + m.id + "\" is skipped");
                    }

                    break;
                }

                done.insert(next->id);
                ordered.push_back(*next);
            }

            return ordered;
        }

        // ----------------------------------------------------------------------------------------
        // Reading a mod's files
        // ----------------------------------------------------------------------------------------

        void addRaw(Content& content, const std::string& kind, const std::string& id, const ModInfo& mod, const std::string& file, Json data)
        {
            auto& registry = content.raw[kind];
            auto it = registry.find(id);

            if (it != registry.end())
                content.report.info.push_back(mod.id + " replaces " + kind + " " + id + " (from " + it->second.mod + ")");

            registry[id] = RawDef{id, mod.id, file, std::move(data), {}};
        }

        std::vector<fs::path> filesIn(const fs::path& dir, const std::string& extension)
        {
            std::vector<fs::path> out;
            std::error_code ec;

            if (not fs::is_directory(dir, ec))
                return out;

            for (const auto& entry : fs::directory_iterator(dir, ec))
            {
                const auto name = entry.path().filename().string();

                if (entry.is_regular_file() and name.size() > extension.size() and name.compare(name.size() - extension.size(), extension.size(), extension) == 0)
                    out.push_back(entry.path());
            }

            std::sort(out.begin(), out.end());
            return out;
        }

        std::string stem(const fs::path& p, const std::string& extension)
        {
            const auto name = p.filename().string();
            return name.substr(0, name.size() - extension.size());
        }

        struct PendingPatch
        {
            std::string kind;
            std::string target;
            Json set;
            std::string file;
            int line;
            std::string mod;
        };

        void readMod(Content& content, const ModInfo& mod, std::vector<PendingPatch>& patches)
        {
            auto& report = content.report;
            const fs::path dir(mod.dir);
            std::error_code ec;

            // Blocks: one file, one entry per block
            const auto blocksFile = (dir / "blocks.json").string();

            if (fs::exists(blocksFile, ec))
            {
                Json json;

                if (readJsonFile(blocksFile, json, report))
                {
                    if (not json.isObject())
                    {
                        report.error(blocksFile, json.line, "must be an object of block name -> block");
                    }
                    else
                    {
                        for (const auto& kv : json.object)
                            addRaw(content, "block", resolveId(kv.first, mod.id), mod, blocksFile, kv.second);
                    }
                }
            }

            // Statuses: one file, one entry per status (burning, chilled...)
            const auto statusesFile = (dir / "statuses.json").string();

            if (fs::exists(statusesFile, ec))
            {
                Json json;

                if (readJsonFile(statusesFile, json, report))
                {
                    if (not json.isObject())
                        report.error(statusesFile, json.line, "must be an object of status name -> status");
                    else
                        for (const auto& kv : json.object)
                            addRaw(content, "status", resolveId(kv.first, mod.id), mod, statusesFile, kv.second);
                }
            }

            // One definition per file
            for (const auto& [kind, folder] : std::vector<std::pair<std::string, std::string>>{{"weapon", "weapons"}, {"enemy", "enemies"}, {"ability", "abilities"}})
            {
                for (const auto& path : filesIn(dir / folder, ".json"))
                {
                    Json json;

                    if (not readJsonFile(path.string(), json, report))
                        continue;

                    if (not json.isObject())
                    {
                        report.error(path.string(), json.line, "must be an object");
                        continue;
                    }

                    // "id" lets a mod replace a definition of another mod: { "id": "base:sword", ... }
                    std::string id = stem(path, ".json");
                    readStr(json, "id", id, path.string(), report);

                    addRaw(content, kind, resolveId(id, mod.id), mod, path.string(), std::move(json));
                }
            }

            // Tuning: every mod's tuning.json is merged over the previous ones
            const auto tuningFile = (dir / "tuning.json").string();

            if (fs::exists(tuningFile, ec))
            {
                Json json;

                if (readJsonFile(tuningFile, json, report))
                {
                    if (content.tuning.isNull())
                        content.tuning = Json::makeObject();

                    mergeJson(content.tuning, json);

                    if (content.tuningFile.empty())
                        content.tuningFile = tuningFile;
                }
            }

            // Effects and sounds: merged over the previous mods' like the tuning. A sound's "file" is
            // relative to the mod that names it: made absolute here, before the merge forgets the mod
            for (const auto& [name, target] : std::vector<std::pair<std::string, Json*>>{{"effects.json", &content.effects}, {"sounds.json", &content.sounds}})
            {
                const auto path = (dir / name).string();

                if (not fs::exists(path, ec))
                    continue;

                Json json;

                if (not readJsonFile(path, json, report))
                    continue;

                if (target == &content.sounds)
                {
                    for (auto& [soundName, sound] : json.object)
                    {
                        if (Json* f = sound.find("file"); f and f->isString())
                            f->string = (dir / f->string).string();
                    }
                }

                if (target->isNull())
                    *target = Json::makeObject();

                mergeJson(*target, json);
            }

            // Arena: the last mod that has one wins
            const auto arenaFile = (dir / "arena.json").string();

            if (fs::exists(arenaFile, ec))
            {
                Json json;

                if (readJsonFile(arenaFile, json, report))
                    addRaw(content, "arena", "arena", mod, arenaFile, std::move(json));
            }

            // Assets: registered by id, loaded once all mods are read
            for (const auto& path : filesIn(dir / "models", ".vxl.json"))
                addRaw(content, "model", resolveId(stem(path, ".vxl.json"), mod.id), mod, path.string(), Json{});

            for (const auto& path : filesIn(dir / "scripts", ".pg"))
                content.scripts[resolveId(stem(path, ".pg"), mod.id)] = path.string();

            // Patches: { "kind": "enemy", "target": "base:brute", "set": { ... } }, or a list of them
            for (const auto& path : filesIn(dir / "patches", ".json"))
            {
                Json json;

                if (not readJsonFile(path.string(), json, report))
                    continue;

                std::vector<const Json*> entries;

                if (json.isArray())
                    for (const auto& e : json.array)
                        entries.push_back(&e);
                else
                    entries.push_back(&json);

                for (const Json* e : entries)
                {
                    if (not e->isObject())
                    {
                        report.error(path.string(), e->line, "a patch must be an object");
                        continue;
                    }

                    checkKeys(*e, {"kind", "target", "set"}, path.string(), report, "a patch");

                    PendingPatch p;
                    p.file = path.string();
                    p.line = e->line;
                    p.mod = mod.id;
                    readStr(*e, "kind", p.kind, p.file, report);
                    readStr(*e, "target", p.target, p.file, report);

                    if (const Json* set = e->find("set"))
                        p.set = *set;

                    if (p.kind == "tuning")
                        p.target = "tuning";
                    else if (p.kind == "arena")
                        p.target = "arena";
                    else
                        p.target = resolveId(p.target, mod.id);

                    if (p.kind.empty() or not p.set.isObject())
                    {
                        report.error(p.file, p.line, "a patch needs a \"kind\" (block, weapon, enemy, tuning, arena) and a \"set\" object");
                        continue;
                    }

                    patches.push_back(std::move(p));
                }
            }
        }

        void applyPatches(Content& content, const std::vector<PendingPatch>& patches)
        {
            for (const auto& p : patches)
            {
                if (p.kind == "tuning")
                {
                    if (content.tuning.isNull())
                        content.tuning = Json::makeObject();

                    mergeJson(content.tuning, p.set);
                    content.report.info.push_back(p.mod + " patches the tuning");
                    continue;
                }

                auto kindIt = content.raw.find(p.kind);

                if (kindIt == content.raw.end() or not kindIt->second.count(p.target))
                {
                    content.report.error(p.file, p.line, "patches " + p.kind + " \"" + p.target + "\", which no loaded mod defines");
                    continue;
                }

                auto& def = kindIt->second[p.target];
                mergeJson(def.data, p.set);
                def.patchedBy.push_back(p.mod);
                content.report.info.push_back(p.mod + " patches " + p.kind + " " + p.target);
            }
        }

        // ----------------------------------------------------------------------------------------
        // Typing
        // ----------------------------------------------------------------------------------------

        void typeBlocks(Content& content)
        {
            auto& report = content.report;

            BlockDef air;
            air.id = "base:air";
            air.look.solid = false;
            air.look.visible = false;
            content.blocks.push_back(air);
            content.blockIds[air.id] = AIR;

            for (const auto& [id, raw] : content.raw["block"])
            {
                if (id == "base:air")
                    continue;

                const Json& j = raw.data;

                if (not j.isObject())
                {
                    report.error(raw.file, j.line, "block \"" + id + "\" must be an object");
                    continue;
                }

                checkKeys(j, {"color", "solid", "colorNoise", "name", "hardness", "breakable", "flammable", "weakness"}, raw.file, report, "block " + id);

                BlockDef def;
                def.id = id;

                glm::vec3 color(200.0f);
                readVec3(j, "color", color, raw.file, report);
                def.look.color = glm::clamp(color / 255.0f, glm::vec3(0.0f), glm::vec3(1.0f));
                readBool(j, "solid", def.look.solid, raw.file, report);
                readNum(j, "colorNoise", def.look.colorNoise, raw.file, report);
                readNum(j, "hardness", def.look.hardness, raw.file, report);
                readBool(j, "breakable", def.look.breakable, raw.file, report);
                readBool(j, "flammable", def.look.flammable, raw.file, report);

                // "weakness": { "volt": 4 }: terrain damage of that element, scaled
                if (const Json* weak = j.find("weakness"))
                {
                    checkKeys(*weak, {"fire", "ice", "volt"}, raw.file, report, "the weakness of block " + id);

                    for (const char* name : {"fire", "ice", "volt"})
                    {
                        Element e = Element::None;
                        elementFromName(name, e);
                        readNum(*weak, name, def.look.weakness[static_cast<int>(e)], raw.file, report);
                    }
                }

                if (content.blocks.size() >= 65535)
                {
                    report.error(raw.file, j.line, "too many blocks");
                    break;
                }

                def.runtimeId = static_cast<BlockId>(content.blocks.size());
                content.blockIds[id] = def.runtimeId;
                content.blocks.push_back(def);
            }

            content.blockTable.clear();

            for (const auto& b : content.blocks)
                content.blockTable.push_back(b.look);
        }

        void typeStatuses(Content& content)
        {
            auto& report = content.report;

            for (const auto& [id, raw] : content.raw["status"])
            {
                const Json& j = raw.data;

                if (not j.isObject())
                {
                    report.error(raw.file, j.line, "status \"" + id + "\" must be an object");
                    continue;
                }

                checkKeys(j, {"duration", "damagePerSecond", "slow", "maxStacks", "atMax", "stagger", "heavy"}, raw.file, report, "status " + id);

                StatusDef s;
                s.id = id;
                readNum(j, "duration", s.duration, raw.file, report);
                readNum(j, "damagePerSecond", s.damagePerSecond, raw.file, report);
                readNum(j, "slow", s.slow, raw.file, report);
                readInt(j, "maxStacks", s.maxStacks, raw.file, report);
                readStr(j, "atMax", s.atMax, raw.file, report);
                readInt(j, "stagger", s.stagger, raw.file, report);
                readBool(j, "heavy", s.heavy, raw.file, report);
                s.maxStacks = std::clamp(s.maxStacks, 1, 99);
                s.duration = std::max(0.05f, s.duration);
                s.atMax = s.atMax.empty() ? "" : resolveId(s.atMax, raw.mod);
                content.statuses[id] = s;
            }

            for (const auto& [id, s] : content.statuses)
                if (not s.atMax.empty() and not content.statuses.count(s.atMax))
                {
                    const RawDef& raw = content.raw["status"].at(id);
                    report.error(raw.file, raw.data.line, "status \"" + id + "\": \"atMax\" names \"" + s.atMax + "\", which no statuses.json has");
                }
        }

        /** A status named by an attack, projectile or spell: in the mod's namespace, and known. */
        void resolveStatus(Content& content, StatusHit& hit, const RawDef& raw)
        {
            if (hit.status.empty())
                return;

            hit.status = resolveId(hit.status, raw.mod);

            if (not content.statuses.count(hit.status))
                content.report.error(raw.file, raw.data.line, "unknown status \"" + hit.status + "\" (statuses.json)");
        }

        void typeArena(Content& content)
        {
            auto& report = content.report;
            auto& raws = content.raw["arena"];

            if (raws.empty())
            {
                report.error("mods", 0, "no mod defines an arena.json");
                return;
            }

            const RawDef& raw = raws.begin()->second;
            const Json& j = raw.data;
            ArenaDef& a = content.arena;
            a.file = raw.file;
            a.mod = raw.mod;

            checkKeys(j, {"size", "playerSpawn", "shapes", "spawns"}, raw.file, report, "the arena");

            readIVec3(j, "size", a.size, raw.file, report);
            a.size = glm::clamp(a.size, glm::ivec3(4), glm::ivec3(256));
            readVec3(j, "playerSpawn", a.playerSpawn, raw.file, report);

            if (const Json* shapes = j.find("shapes"))
            {
                if (shapes->isArray())
                    a.shapes = *shapes;
                else
                    report.error(raw.file, shapes->line, "\"shapes\" must be a list");
            }

            // Shapes refer to blocks by id: check them now so the error points at the right line
            for (const auto& s : a.shapes.array)
            {
                checkKeys(s, {"block", "from", "to", "hollow"}, raw.file, report, "an arena shape");

                const std::string blockRef = s.str("block", "");
                const std::string id = resolveId(blockRef, raw.mod);

                if (blockRef != "air" and content.block(id) == AIR)
                    report.error(raw.file, s.line, "unknown block \"" + blockRef + "\"");
            }

            if (const Json* spawns = j.find("spawns"))
            {
                for (const auto& s : spawns->array)
                {
                    checkKeys(s, {"enemy", "position", "yaw"}, raw.file, report, "an arena spawn");

                    ArenaSpawn spawn;
                    readStr(s, "enemy", spawn.enemy, raw.file, report);
                    spawn.enemy = resolveId(spawn.enemy, raw.mod);
                    readVec3(s, "position", spawn.position, raw.file, report);
                    readNum(s, "yaw", spawn.yaw, raw.file, report);

                    if (not content.enemies.count(spawn.enemy))
                        report.error(raw.file, s.line, "unknown enemy \"" + spawn.enemy + "\"");
                    else
                        a.spawns.push_back(spawn);
                }
            }
        }

        void requireModel(Content& content, const std::string& id, const std::string& file, int line)
        {
            if (not id.empty() and not content.raw["model"].count(id))
                content.report.error(file, line, "unknown model \"" + id + "\" (expected a file models/" + splitId(id).second + ".vxl.json in mod " + splitId(id).first + ")");
        }

        /** Projectile models are named like any model: resolved in the mod's namespace, and checked. */
        void resolveProjectiles(Content& content, Moveset& m, const RawDef& raw)
        {
            for (auto& [name, attack] : m.attacks)
            {
                resolveStatus(content, attack.status, raw);
                resolveStatus(content, attack.projectile.status, raw);

                if (attack.projectile.model.empty())
                    continue;

                attack.projectile.model = resolveId(attack.projectile.model, raw.mod);
                requireModel(content, attack.projectile.model, raw.file, raw.data.line);
            }
        }

        void typeWeapons(Content& content)
        {
            auto& report = content.report;

            for (const auto& [id, raw] : content.raw["weapon"])
            {
                const Json& j = raw.data;

                checkKeys(j, {"id", "name", "model", "modelScale", "grip", "rest", "moveset"}, raw.file, report, "weapon " + id);

                WeaponDef w;
                w.id = id;
                w.file = raw.file;
                w.name = splitId(id).second;
                readStr(j, "name", w.name, raw.file, report);
                readStr(j, "model", w.model, raw.file, report);
                w.model = w.model.empty() ? "" : resolveId(w.model, raw.mod);
                readNum(j, "modelScale", w.modelScale, raw.file, report);
                readVec3(j, "grip", w.grip, raw.file, report);

                if (const Json* rest = j.find("rest"))
                    readSwing(*rest, w.rest, raw.file, report);

                requireModel(content, w.model, raw.file, j.find("model") ? j.find("model")->line : j.line);

                const Json* moveset = j.find("moveset");

                if (not moveset)
                {
                    report.error(raw.file, j.line, "weapon \"" + id + "\" has no \"moveset\"");
                    continue;
                }

                if (not readMoveset(*moveset, raw.file, w.moveset, report))
                    continue;

                resolveProjectiles(content, w.moveset, raw);

                if (w.moveset.light.empty())
                {
                    report.error(raw.file, moveset->line, "weapon \"" + id + "\" has no \"light\" attack");
                    continue;
                }

                content.weapons[id] = std::move(w);
            }

            // Hot keys follow load order: base's weapons first
            for (const auto& mod : content.mods)
            {
                for (const auto& [id, w] : content.weapons)
                {
                    if (splitId(id).first == mod.id)
                        content.weaponOrder.push_back(id);
                }
            }
        }

        void typeEnemies(Content& content)
        {
            auto& report = content.report;

            for (const auto& [id, raw] : content.raw["enemy"])
            {
                const Json& j = raw.data;

                checkKeys(j, {"id", "name", "model", "modelScale", "size", "health", "poise", "poiseRegenDelay", "staggerFrames",
                              "knockbackScale", "walkSpeed", "turnSpeed", "respawnSeconds", "regenerates", "script", "params", "moveset"},
                          raw.file, report, "enemy " + id);

                EnemyDef e;
                e.id = id;
                e.file = raw.file;
                e.name = splitId(id).second;
                readStr(j, "name", e.name, raw.file, report);
                readStr(j, "model", e.model, raw.file, report);
                e.model = e.model.empty() ? "" : resolveId(e.model, raw.mod);
                readNum(j, "modelScale", e.modelScale, raw.file, report);

                if (const Json* size = j.find("size"))
                {
                    float wh[2];

                    if (toVec(*size, 2, wh))
                    {
                        e.width = wh[0];
                        e.height = wh[1];
                    }
                    else
                    {
                        report.error(raw.file, size->line, "\"size\" must be [width, height]");
                    }
                }

                readNum(j, "health", e.health, raw.file, report);
                readNum(j, "poise", e.poise, raw.file, report);
                readNum(j, "poiseRegenDelay", e.poiseRegenDelay, raw.file, report);
                readInt(j, "staggerFrames", e.staggerFrames, raw.file, report);
                readNum(j, "knockbackScale", e.knockbackScale, raw.file, report);
                readNum(j, "walkSpeed", e.walkSpeed, raw.file, report);
                readNum(j, "turnSpeed", e.turnSpeed, raw.file, report);
                readNum(j, "respawnSeconds", e.respawnSeconds, raw.file, report);
                readBool(j, "regenerates", e.regenerates, raw.file, report);
                readStr(j, "script", e.script, raw.file, report);

                if (not e.script.empty())
                {
                    e.script = resolveId(e.script, raw.mod);

                    if (not content.scripts.count(e.script))
                        report.error(raw.file, j.find("script")->line, "unknown script \"" + e.script + "\" (expected scripts/" + splitId(e.script).second + ".pg in mod " + splitId(e.script).first + ")");
                }

                if (const Json* params = j.find("params"))
                    e.params = *params;

                requireModel(content, e.model, raw.file, j.find("model") ? j.find("model")->line : j.line);

                if (const Json* moveset = j.find("moveset"))
                {
                    if (not readMoveset(*moveset, raw.file, e.moveset, report))
                        continue;

                    resolveProjectiles(content, e.moveset, raw);
                }

                if (e.health <= 0.0f)
                {
                    report.error(raw.file, j.line, "enemy \"" + id + "\" needs a health above 0");
                    continue;
                }

                content.enemies[id] = std::move(e);
            }
        }

        void typeAbilities(Content& content)
        {
            auto& report = content.report;

            for (const auto& [id, raw] : content.raw["ability"])
            {
                const Json& j = raw.data;

                checkKeys(j, {"id", "name", "kind", "block", "staminaCost", "manaCost", "cooldown", "range", "distance", "lifetime", "damage", "rate", "width", "height", "depth",
                              "projectile", "count", "spread", "chains", "chainRange", "poiseDamage", "knockback", "terrainDamage", "element", "status"},
                          raw.file, report, "ability " + id);

                AbilityDef a;
                a.id = id;
                a.file = raw.file;
                a.name = splitId(id).second;
                readStr(j, "name", a.name, raw.file, report);
                readStr(j, "kind", a.kind, raw.file, report);
                readStr(j, "block", a.block, raw.file, report);
                readNum(j, "staminaCost", a.staminaCost, raw.file, report);
                readNum(j, "cooldown", a.cooldown, raw.file, report);
                readNum(j, "range", a.range, raw.file, report);
                readNum(j, "distance", a.distance, raw.file, report);
                readNum(j, "lifetime", a.lifetime, raw.file, report);
                readNum(j, "damage", a.damage, raw.file, report);
                readNum(j, "rate", a.rate, raw.file, report);
                readInt(j, "width", a.width, raw.file, report);
                readInt(j, "height", a.height, raw.file, report);
                readInt(j, "depth", a.depth, raw.file, report);
                readNum(j, "manaCost", a.manaCost, raw.file, report);
                readInt(j, "count", a.count, raw.file, report);
                readNum(j, "spread", a.spread, raw.file, report);
                readInt(j, "chains", a.chains, raw.file, report);
                readNum(j, "chainRange", a.chainRange, raw.file, report);
                readNum(j, "poiseDamage", a.poiseDamage, raw.file, report);
                readNum(j, "knockback", a.knockback, raw.file, report);
                readNum(j, "terrainDamage", a.terrainDamage, raw.file, report);
                readElement(j, a.element, raw.file, report);
                readStatusHit(j, a.status, raw.file, report);
                resolveStatus(content, a.status, raw);

                if (const Json* pj = j.find("projectile"))
                {
                    readProjectile(*pj, a.projectile, raw.file, report, "the projectile of ability " + id);
                    resolveStatus(content, a.projectile.status, raw);

                    if (not a.projectile.model.empty())
                    {
                        a.projectile.model = resolveId(a.projectile.model, raw.mod);
                        requireModel(content, a.projectile.model, raw.file, pj->line);
                    }
                }

                // The projectile carries the spell's element and status unless it names its own
                if (a.projectile.element == Element::None)
                    a.projectile.element = a.element;
                if (a.projectile.status.status.empty())
                    a.projectile.status = a.status;

                a.count = std::clamp(a.count, 1, 12);
                a.chains = std::clamp(a.chains, 0, 12);

                const int kindLine = j.find("kind") ? j.find("kind")->line : j.line;

                if (a.kind != "wall" and a.kind != "pit" and a.kind != "mine" and a.kind != "projectile" and a.kind != "bolt")
                {
                    report.error(raw.file, kindLine, "ability \"" + id + "\" needs a \"kind\": wall, pit, mine, projectile or bolt");
                    continue;
                }

                if (a.kind == "projectile" and not a.projectile.enabled)
                {
                    report.error(raw.file, kindLine, "ability \"" + id + "\": a projectile spell needs a \"projectile\"");
                    continue;
                }

                if (a.kind == "wall")
                {
                    a.block = resolveId(a.block, raw.mod);

                    if (content.block(a.block) == AIR)
                    {
                        report.error(raw.file, j.find("block") ? j.find("block")->line : j.line, "ability \"" + id + "\": unknown block \"" + a.block + "\"");
                        continue;
                    }
                }

                a.width = std::clamp(a.width, 1, 9);
                a.height = std::clamp(a.height, 1, 9);
                a.depth = std::clamp(a.depth, 1, 9);
                a.rate = std::max(0.1f, a.rate);

                content.abilities[id] = std::move(a);
            }
        }

        void loadModels(Content& content)
        {
            for (const auto& [id, raw] : content.raw["model"])
            {
                auto model = std::make_shared<VoxelModel>();
                model->id = id;

                if (loadVoxelModel(raw.file, *model, content.report))
                    content.models[id] = model;
            }
        }
    }

    bool readAttack(const Json& j, const std::string& name, const std::string& file, AttackDef& a, ContentReport& report)
    {
        if (not j.isObject())
        {
            report.error(file, j.line, "attack \"" + name + "\" must be an object");
            return false;
        }

        checkKeys(j, {"startup", "active", "recovery", "damage", "poiseDamage", "staminaCost", "knockback", "knockup", "hitstop", "shake",
                      "hitboxes", "lunge", "lungeFrom", "lungeTo", "turnRate", "moveSpeed", "moveUntil", "armor", "terrainDamage", "dodgeCancelFrom", "chainFrom", "bufferFrom",
                      "next", "swing", "projectile", "element", "status", "gravityScale", "plungeSpeed", "iframes", "leap", "leapAt", "turn"},
                  file, report, "attack " + name);

        const size_t errorsBefore = report.errors.size();

        a.name = name;
        readInt(j, "startup", a.startup, file, report);
        readInt(j, "active", a.active, file, report);
        readInt(j, "recovery", a.recovery, file, report);
        readNum(j, "damage", a.damage, file, report);
        readNum(j, "poiseDamage", a.poiseDamage, file, report);
        readNum(j, "staminaCost", a.staminaCost, file, report);
        readNum(j, "knockback", a.knockback, file, report);
        readNum(j, "knockup", a.knockup, file, report);
        readInt(j, "hitstop", a.hitstop, file, report);
        readNum(j, "shake", a.shake, file, report);
        readNum(j, "lunge", a.lunge, file, report);
        readInt(j, "lungeFrom", a.lungeFrom, file, report);
        readInt(j, "lungeTo", a.lungeTo, file, report);
        readNum(j, "turnRate", a.turnRate, file, report);
        readNum(j, "moveSpeed", a.moveSpeed, file, report);
        readInt(j, "moveUntil", a.moveUntil, file, report);

        if (a.moveSpeed < 0.0f or a.moveSpeed > 1.5f)
            report.error(file, j.find("moveSpeed")->line, "attack \"" + name + "\": moveSpeed is a fraction of the walk speed, 0 to 1.5");
        readNum(j, "armor", a.armor, file, report);
        readNum(j, "terrainDamage", a.terrainDamage, file, report);
        readInt(j, "dodgeCancelFrom", a.dodgeCancelFrom, file, report);
        readInt(j, "chainFrom", a.chainFrom, file, report);
        readInt(j, "bufferFrom", a.bufferFrom, file, report);
        readElement(j, a.element, file, report);
        readStatusHit(j, a.status, file, report);
        readNum(j, "gravityScale", a.gravityScale, file, report);
        readNum(j, "plungeSpeed", a.plungeSpeed, file, report);
        readInt(j, "leapAt", a.leapAt, file, report);

        if (const Json* leap = j.find("leap"))
        {
            if (leap->isArray() and leap->array.size() == 2 and leap->array[0].isNumber() and leap->array[1].isNumber())
                a.leap = glm::vec2(static_cast<float>(leap->array[0].number), static_cast<float>(leap->array[1].number));
            else
                report.error(file, leap->line, "\"leap\" is [along the facing, up] in m/s");
        }

        if (const Json* frames = j.find("iframes"))
        {
            if (frames->isArray() and frames->array.size() == 2 and frames->array[0].isNumber() and frames->array[1].isNumber())
            {
                a.iframeFrom = static_cast<int>(frames->array[0].number);
                a.iframeTo = static_cast<int>(frames->array[1].number);
            }
            else
            {
                report.error(file, frames->line, "\"iframes\" is [first frame, frame after the last]");
            }
        }

        if (const Json* turn = j.find("turn"))
        {
            checkKeys(*turn, {"pitch", "yaw", "from", "to"}, file, report, "the turn of " + name);
            readNum(*turn, "pitch", a.turn.pitch, file, report);
            readNum(*turn, "yaw", a.turn.yaw, file, report);
            readInt(*turn, "from", a.turn.from, file, report);
            readInt(*turn, "to", a.turn.to, file, report);
        }

        if (const Json* next = j.find("next"))
        {
            checkKeys(*next, {"light", "heavy"}, file, report, "\"next\" of " + name);
            readStr(*next, "light", a.nextLight, file, report);
            readStr(*next, "heavy", a.nextHeavy, file, report);
        }

        if (const Json* pj = j.find("projectile"))
            readProjectile(*pj, a.projectile, file, report, "the projectile of " + name);

        if (const Json* swing = j.find("swing"))
            readSwing(*swing, a.swing, file, report);

        if (const Json* boxes = j.find("hitboxes"))
        {
            if (not boxes->isArray())
            {
                report.error(file, boxes->line, "\"hitboxes\" must be a list");
            }
            else
            {
                for (const auto& b : boxes->array)
                {
                    checkKeys(b, {"offset", "size"}, file, report, "a hitbox");

                    HitBox box;
                    readVec3(b, "offset", box.offset, file, report);
                    readVec3(b, "size", box.size, file, report);
                    a.hitboxes.push_back(box);
                }
            }
        }

        if (a.startup < 0 or a.active < 1 or a.recovery < 0)
            report.error(file, j.line, "attack \"" + name + "\" needs startup >= 0, active >= 1, recovery >= 0");

        if (a.chainFrom < 0)
            a.chainFrom = a.startup + a.active;

        return report.errors.size() == errorsBefore;
    }

    bool readMoveset(const Json& j, const std::string& file, Moveset& m, ContentReport& report)
    {
        if (not j.isObject())
        {
            report.error(file, j.line, "a moveset must be an object");
            return false;
        }

        checkKeys(j, {"light", "heavy", "dodgeAttack", "dashAttack", "airLight", "airHeavy", "attacks"}, file, report, "a moveset");

        const size_t errorsBefore = report.errors.size();

        readStr(j, "light", m.light, file, report);
        readStr(j, "heavy", m.heavy, file, report);
        readStr(j, "dodgeAttack", m.dodgeAttack, file, report);
        readStr(j, "dashAttack", m.dashAttack, file, report);
        readStr(j, "airLight", m.airLight, file, report);
        readStr(j, "airHeavy", m.airHeavy, file, report);

        const Json* attacks = j.find("attacks");

        if (not attacks or not attacks->isObject())
        {
            report.error(file, j.line, "a moveset needs an \"attacks\" object");
            return false;
        }

        for (const auto& [name, aj] : attacks->object)
        {
            AttackDef a;

            if (readAttack(aj, name, file, a, report))
                m.attacks[name] = a;
        }

        // Every name the moveset links to must exist, or a combo would stop dead in play
        auto check = [&](const std::string& ref, int line, const std::string& what) {
            if (not ref.empty() and not m.attacks.count(ref))
                report.error(file, line, what + " names the attack \"" + ref + "\", which the moveset does not have");
        };

        check(m.light, j.find("light") ? j.find("light")->line : j.line, "\"light\"");
        check(m.heavy, j.find("heavy") ? j.find("heavy")->line : j.line, "\"heavy\"");
        check(m.dodgeAttack, j.find("dodgeAttack") ? j.find("dodgeAttack")->line : j.line, "\"dodgeAttack\"");
        check(m.dashAttack, j.find("dashAttack") ? j.find("dashAttack")->line : j.line, "\"dashAttack\"");
        check(m.airLight, j.find("airLight") ? j.find("airLight")->line : j.line, "\"airLight\"");
        check(m.airHeavy, j.find("airHeavy") ? j.find("airHeavy")->line : j.line, "\"airHeavy\"");

        for (const auto& [name, a] : m.attacks)
        {
            const int line = attacks->find(name)->line;
            check(a.nextLight, line, "attack \"" + name + "\" next.light");
            check(a.nextHeavy, line, "attack \"" + name + "\" next.heavy");
        }

        return report.errors.size() == errorsBefore;
    }

    bool loadVoxelModel(const std::string& path, VoxelModel& out, ContentReport& report)
    {
        Json j;

        if (not readJsonFile(path, j, report))
            return false;

        out.file = path;

        const Json* canvas = j.find("canvas");
        const Json* palette = j.find("palette");
        const Json* cells = j.find("cells");

        if (not canvas or not palette or not cells or not palette->isArray() or not cells->isArray())
        {
            report.error(path, j.line, "a voxel model needs \"canvas\", \"palette\" and \"cells\" (the VoxelTest editor's .vxl.json format)");
            return false;
        }

        const int w = static_cast<int>(canvas->num("w", 0));
        const int h = static_cast<int>(canvas->num("h", 0));
        const int d = static_cast<int>(canvas->num("d", 0));

        if (w <= 0 or h <= 0 or d <= 0 or w > 256 or h > 256 or d > 256)
        {
            report.error(path, canvas->line, "the canvas size must be between 1 and 256 on each side");
            return false;
        }

        out.grid = VoxelGrid(w, h, d);
        out.grid.outsideSolid = false;

        out.table.clear();
        out.table.emplace_back();
        out.table[0].solid = false;
        out.table[0].visible = false;

        for (const auto& c : palette->array)
        {
            float rgba[4] = {255, 255, 255, 255};

            if (not toVec(c, 4, rgba) and not toVec(c, 3, rgba))
                report.error(path, c.line, "a palette entry must be [r, g, b, a]");

            BlockLook look;
            look.color = glm::vec3(rgba[0], rgba[1], rgba[2]) / 255.0f;
            out.table.push_back(look);
        }

        // Cells of hidden layers are left out: the model is what the artist sees in the editor
        std::vector<bool> layerVisible;
        std::vector<std::string> layerNames;

        if (const Json* layers = j.find("layers"))
        {
            for (const auto& l : layers->array)
            {
                layerVisible.push_back(l.flag("visible", true));
                layerNames.push_back(l.str("name", "layer"));
            }
        }

        // One part per layer, filled below; the empty ones are dropped at the end
        std::vector<VoxelModel::Part> parts(std::max<size_t>(1, layerNames.size()));
        std::vector<glm::ivec3> partLo(parts.size(), glm::ivec3(w, h, d)), partHi(parts.size(), glm::ivec3(-1));

        for (size_t i = 0; i < parts.size(); ++i)
        {
            parts[i].name = i < layerNames.size() ? layerNames[i] : "body";
            parts[i].grid = VoxelGrid(w, h, d);
            parts[i].grid.outsideSolid = false;
        }

        glm::ivec3 lo(w, h, d), hi(-1);

        for (const auto& c : cells->array)
        {
            const int x = static_cast<int>(c.num("x", -1));
            const int y = static_cast<int>(c.num("y", -1));
            const int z = static_cast<int>(c.num("z", -1));
            const int ci = static_cast<int>(c.num("c", 0));
            const int li = static_cast<int>(c.num("l", 0));

            if (li >= 0 and li < static_cast<int>(layerVisible.size()) and not layerVisible[li])
                continue;

            if (not out.grid.inBounds(x, y, z) or ci < 0 or ci + 1 >= static_cast<int>(out.table.size()))
            {
                report.error(path, c.line, "a cell is outside the canvas or uses a colour the palette does not have");
                continue;
            }

            out.grid.set(x, y, z, static_cast<BlockId>(ci + 1));
            lo = glm::min(lo, glm::ivec3(x, y, z));
            hi = glm::max(hi, glm::ivec3(x, y, z));

            const size_t part = (li >= 0 and li < static_cast<int>(parts.size())) ? static_cast<size_t>(li) : 0;
            parts[part].grid.set(x, y, z, static_cast<BlockId>(ci + 1));
            partLo[part] = glm::min(partLo[part], glm::ivec3(x, y, z));
            partHi[part] = glm::max(partHi[part], glm::ivec3(x, y, z));
        }

        if (hi.x < 0)
        {
            report.warn(path, 0, "the model has no visible cells");
            out.pivot = glm::vec3(w * 0.5f, 0.0f, d * 0.5f);
            return true;
        }

        out.pivot = glm::vec3((lo.x + hi.x + 1) * 0.5f, static_cast<float>(lo.y), (lo.z + hi.z + 1) * 0.5f);

        // Parts: a limb turns at its top, a head at its bottom
        for (size_t i = 0; i < parts.size(); ++i)
        {
            if (partHi[i].x < 0)
                continue;

            const glm::vec3 l(partLo[i]), hgh(partHi[i] + glm::ivec3(1));
            const bool head = parts[i].name == "head";
            parts[i].joint = glm::vec3((l.x + hgh.x) * 0.5f, head ? l.y : hgh.y, (l.z + hgh.z) * 0.5f);
            out.parts.push_back(std::move(parts[i]));
        }

        return true;
    }

    Content loadContent(const std::string& modsDir)
    {
        Content content;
        auto& report = content.report;

        content.mods = orderMods(discoverMods(modsDir, report), report);

        std::vector<PendingPatch> patches;

        for (const auto& mod : content.mods)
        {
            report.info.push_back("loading mod " + mod.id + (mod.version.empty() ? "" : " " + mod.version));
            readMod(content, mod, patches);
        }

        applyPatches(content, patches);

        typeBlocks(content);
        typeStatuses(content);
        loadModels(content);
        typeWeapons(content);
        typeEnemies(content);
        typeAbilities(content);
        typeArena(content);

        if (content.tuning.isNull())
        {
            report.error(modsDir, 0, "no mod has a tuning.json");
            content.tuning = Json::makeObject();
        }

        if (content.weapons.empty())
            report.error(modsDir, 0, "no mod defines a usable weapon");

        // Fingerprint: FNV-1a over the content as the game will use it
        {
            uint64_t h = 1469598103934665603ull;
            auto feed = [&](const std::string& s) {
                for (unsigned char c : s)
                {
                    h ^= c;
                    h *= 1099511628211ull;
                }
                h ^= 0xff;
                h *= 1099511628211ull;
            };

            // Gameplay only (see Content::fingerprint): what only looks is taken out first
            feed(toJsonString(withoutLooks(content.tuning, TUNING_LOOKS)));
            feed(toJsonString(content.arena.shapes));

            for (const auto& [kind, defs] : content.raw)
            {
                // Models, and the effects and sounds files, are looks through and through
                if (kind == "model")
                    continue;

                const auto looks = DEF_LOOKS.find(kind);

                for (const auto& [id, raw] : defs)
                {
                    feed(kind + ":" + id);
                    feed(toJsonString(looks == DEF_LOOKS.end() ? raw.data : withoutLooks(raw.data, looks->second)));
                }
            }

            for (const auto& [id, path] : content.scripts)
            {
                std::string text;
                feed(id);
                if (readText(path, text))
                    feed(text);
            }

            content.fingerprint = h;
        }

        return content;
    }
}
