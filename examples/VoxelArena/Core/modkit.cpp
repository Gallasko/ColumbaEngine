#include "modkit.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "schema.h"

namespace fs = std::filesystem;

namespace arena::modkit
{
    namespace
    {
        bool readText(const fs::path& path, std::string& out)
        {
            std::ifstream in(path, std::ios::binary);

            if (not in)
                return false;

            std::stringstream text;
            text << in.rdbuf();
            out = text.str();
            return true;
        }

        bool writeText(const fs::path& path, const std::string& text)
        {
            std::error_code ec;
            fs::create_directories(path.parent_path(), ec);
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            out << text;
            return static_cast<bool>(out);
        }

        std::string quoted(const std::string& s)
        {
            return toJsonString(Json::makeString(s));
        }

        // "weapons/*.json" against "weapons/sword.json"; a pattern without * names one file
        bool matches(const std::string& pattern, const std::string& path)
        {
            const auto star = pattern.find('*');

            if (star == std::string::npos)
                return pattern == path;

            const std::string before = pattern.substr(0, star), after = pattern.substr(star + 1);
            return path.size() > before.size() + after.size() and path.compare(0, before.size(), before) == 0 and
                   path.compare(path.size() - after.size(), after.size(), after) == 0 and
                   path.find('/', before.size()) == std::string::npos;
        }

        /**
         * Goes through a line of JSON text: f is called with each string literal (its position, its
         * length with the quotes, its contents) and whether it is a key. Stops at a // comment.
         */
        template <typename F>
        void eachString(const std::string& line, F f)
        {
            for (size_t i = 0; i < line.size(); ++i)
            {
                if (line[i] == '/' and i + 1 < line.size() and line[i + 1] == '/')
                    return;

                if (line[i] != '"')
                    continue;

                size_t end = i + 1;
                while (end < line.size() and line[end] != '"')
                    end += line[end] == '\\' ? 2 : 1;

                if (end >= line.size())
                    return;

                size_t after = end + 1;
                while (after < line.size() and (line[after] == ' ' or line[after] == '\t'))
                    ++after;

                f(i, end + 1 - i, line.substr(i + 1, end - i - 1), after < line.size() and line[after] == ':');
                i = end;
            }
        }

        /** Where the first { of the text is that is not in a comment or a string. */
        size_t openingBrace(const std::string& text)
        {
            bool inString = false;

            for (size_t i = 0; i < text.size(); ++i)
            {
                if (inString)
                {
                    if (text[i] == '\\')
                        ++i;
                    else if (text[i] == '"')
                        inString = false;
                }
                else if (text[i] == '"')
                    inString = true;
                else if (text[i] == '/' and i + 1 < text.size() and text[i + 1] == '/')
                    i = text.find('\n', i) == std::string::npos ? text.size() : text.find('\n', i);
                else if (text[i] == '{')
                    return i;
            }

            return std::string::npos;
        }

        /** "lucky_charm" as a player reads it: "Lucky charm". */
        std::string displayName(const std::string& name)
        {
            std::string out = name;
            std::replace(out.begin(), out.end(), '_', ' ');
            if (not out.empty() and out[0] >= 'a' and out[0] <= 'z')
                out[0] = static_cast<char>(out[0] - 'a' + 'A');
            return out;
        }

        /** The text with the string value of a key on its line replaced: the copy's own "id" and "name". */
        std::string replaceValue(const std::string& text, const Json& old, const std::string& value)
        {
            std::stringstream in(text);
            std::string line, out;

            for (int number = 1; std::getline(in, line); ++number)
            {
                if (number == old.line)
                    if (const auto at = line.find(quoted(old.string)); at != std::string::npos)
                        line.replace(at, quoted(old.string).size(), quoted(value));
                out += line + (in.eof() ? "" : "\n");
            }

            return out;
        }

        std::string relativeTo(const std::string& path, const std::string& dir)
        {
            std::error_code ec;
            const auto rel = fs::relative(path, dir, ec);
            return ec or rel.empty() ? path : rel.generic_string();
        }

        bool under(const std::string& file, const std::string& dir)
        {
            const std::string f = fs::path(file).lexically_normal().generic_string();
            const std::string d = fs::path(dir).lexically_normal().generic_string();
            return f.size() > d.size() and f.compare(0, d.size(), d) == 0 and f[d.size()] == '/';
        }
    }

    const std::vector<Kind>& kinds()
    {
        static const std::vector<Kind> list = {
            {"weapon", "weapon", "weapon", "weapons", true, "base:sword"},
            {"enemy", "enemy", "enemy", "enemies", true, "base:brute"},
            {"ability", "ability", "ability", "abilities", true, "base:whirlwind"},
            {"spell", "spell", "spell", "spells", true, "base:firebolt"},
            {"arena", "arenas", "arena", "arenas", true, "base:pit"},
            {"block", "block", "block", "blocks.json", false, "base:stone"},
            {"status", "status", "status", "statuses.json", false, "base:burn"},
            {"keyword", "keyword", "keyword", "keywords.json", false, "base:bolt"},
            {"upgrade", "upgrade", "upgrade", "upgrades.json", false, "base:tough"},
            {"combo", "combo", "combo", "combos.json", false, "base:melt"},
            {"affix", "affix", "affix", "affixes.json", false, "base:blazing"},
        };

        return list;
    }

    const Kind* findKind(std::string_view name)
    {
        if (name == "item" or name == "items")
            name = "upgrade";

        for (const auto& k : kinds())
        {
            const std::string_view place = k.place;
            const std::string_view plural = place.substr(0, place.find('.'));

            if (name == k.name or name == plural)
                return &k;
        }

        return nullptr;
    }

    const Kind* kindOfRaw(std::string_view raw)
    {
        for (const auto& k : kinds())
            if (raw == k.raw)
                return &k;

        return nullptr;
    }

    bool validName(const std::string& name)
    {
        return not name.empty() and name.front() != '.' and name.back() != '.' and
               std::all_of(name.begin(), name.end(), [](char c) { return (c >= 'a' and c <= 'z') or (c >= '0' and c <= '9') or c == '_' or c == '.'; });
    }

    std::string pretty(const Json& json, int indent)
    {
        const std::string flat = toJsonString(json);

        if ((not json.isArray() and not json.isObject()) or indent + flat.size() <= 100)
            return flat;

        const std::string pad(indent + 4, ' ');
        std::string out(json.isArray() ? "[\n" : "{\n");
        const size_t count = json.isArray() ? json.array.size() : json.object.size();

        for (size_t i = 0; i < count; ++i)
        {
            out += pad;

            if (json.isArray())
                out += pretty(json.array[i], indent + 4);
            else
                out += quoted(json.object[i].first) + ": " + pretty(json.object[i].second, indent + 4);

            out += i + 1 < count ? ",\n" : "\n";
        }

        return out + std::string(indent, ' ') + (json.isArray() ? "]" : "}");
    }

    void qualifyIds(Json& json, std::string_view type, const std::string& from)
    {
        schema::forEachId(json, type, [&](schema::IdAt& at) {
            if (not at.id.empty() and at.id.find(':') == std::string::npos)
                at.id = from + ":" + at.id;
        });
    }

    std::string qualifyIds(const std::string& text, std::string_view type, const std::string& from)
    {
        JsonParseResult parsed = parseJson(text);

        if (not parsed.ok)
            return text;

        // The ids to change, line by line (a line may hold the same one twice), names apart from values
        std::map<std::pair<int, bool>, std::multiset<std::string>> todo;

        schema::forEachId(parsed.value, type, [&](schema::IdAt& at) {
            if (not at.id.empty() and at.id.find(':') == std::string::npos)
                todo[{at.line, at.name}].insert(at.id);
        });

        std::stringstream in(text);
        std::string line, out;

        for (int number = 1; std::getline(in, line); ++number)
        {
            auto values = todo.find({number, false});
            auto names = todo.find({number, true});

            if (values != todo.end() or names != todo.end())
            {
                std::string changed;
                size_t copied = 0;

                eachString(line, [&](size_t at, size_t length, const std::string& contents, bool key) {
                    auto& found = key ? names : values;

                    if (found == todo.end() or not found->second.count(contents))
                        return;

                    auto& left = found->second;

                    left.erase(left.find(contents));
                    changed += line.substr(copied, at - copied) + quoted(from + ":" + contents);
                    copied = at + length;
                });

                line = changed + line.substr(copied);
            }

            out += line;
            if (not in.eof())
                out += "\n";
        }

        return out;
    }

    Found find(const Content& content, const std::string& id, const Kind* only)
    {
        Found found;
        std::vector<std::pair<const Kind*, const RawDef*>> named;

        for (const auto& [raw, defs] : content.raw)
        {
            const Kind* kind = kindOfRaw(raw);

            if (not kind or (only and kind != only))
                continue;

            for (const auto& [defId, def] : defs)
            {
                if (defId == id)
                {
                    found.kind = kind;
                    found.def = &def;
                    return found;
                }

                if (id.find(':') == std::string::npos and splitId(defId).second == id)
                    named.push_back({kind, &def});
            }
        }

        if (named.size() == 1)
        {
            found.kind = named[0].first;
            found.def = named[0].second;
        }
        else
            for (const auto& [kind, def] : named)
                found.others.push_back(def->id + " (" + kind->name + ")");

        return found;
    }

    Done newMod(const std::string& dir, const std::string& id, const std::string& name)
    {
        Done done;
        const fs::path folder = fs::path(dir) / id;
        done.path = folder.string();

        if (not validName(id))
        {
            done.message = "a mod's id may only use a-z, 0-9, _ and . (\"" + id + "\")";
            return done;
        }

        std::error_code ec;
        if (fs::exists(folder, ec))
        {
            done.message = "there is a folder " + folder.string() + " already";
            return done;
        }

        const std::string manifest = "{\n"
                                     "    \"id\": " + quoted(id) + ",\n"
                                     "    \"name\": " + quoted(name.empty() ? id : name) + ",\n"
                                     "    \"version\": \"0.1.0\",\n"
                                     "    \"api\": " + std::to_string(CONTENT_API_VERSION) + ",\n"
                                     "    \"dependencies\": [\"base\"]\n"
                                     "}\n";

        if (not writeText(folder / "mod.json", manifest))
        {
            done.message = "could not write " + (folder / "mod.json").string();
            return done;
        }

        done.ok = true;
        done.message = "made the mod " + id + " in " + folder.string();
        return done;
    }

    Done addDefinition(const Content& content, const ModInfo& mod, const Kind& kind, const std::string& name, const std::string& from)
    {
        Done done;
        const std::string source = from.empty() ? kind.example : from;
        const std::string id = mod.id + ":" + name;

        if (not validName(name))
        {
            done.message = "a name may only use a-z, 0-9, _ and . (\"" + name + "\")";
            return done;
        }

        // Of that kind first: an affix and a shop item may share a name
        Found found = find(content, source, &kind);

        if (not found.def and found.others.empty())
            found = find(content, source);

        if (not found.def)
        {
            done.message = found.others.empty() ? "nothing is called " + source : source + " could be: " + [&] {
                std::string s;
                for (const auto& o : found.others)
                    s += (s.empty() ? "" : ", ") + o;
                return s;
            }();
            return done;
        }

        if (found.kind != &kind)
        {
            done.message = found.def->id + " is " + (std::string("aeiou").find(found.kind->name[0]) != std::string::npos ? "an " : "a ") + found.kind->name +
                           ", not " + (std::string("aeiou").find(kind.name[0]) != std::string::npos ? "an " : "a ") + kind.name;
            return done;
        }

        if (auto defs = content.raw.find(kind.raw); defs != content.raw.end() and defs->second.count(id))
        {
            done.message = id + " exists already (" + defs->second.at(id).file + ")";
            return done;
        }

        const RawDef& def = *found.def;
        const std::string made = "Made from " + def.id + " by VoxelArenaMod" + (def.patchedBy.empty() ? "" : ", without the patches of " + def.patchedBy.front()) + ".";

        if (kind.ownFile)
        {
            const fs::path path = fs::path(mod.dir) / kind.place / (name + ".json");
            done.path = path.string();

            std::error_code ec;
            if (fs::exists(path, ec))
            {
                done.message = path.string() + " exists already";
                return done;
            }

            std::string text;
            if (not readText(def.file, text))
            {
                done.message = "could not read " + def.file;
                return done;
            }

            text = qualifyIds(text, kind.object, def.mod);

            // A copy of a replacement ("id": "base:sword") must not replace it too; and it has a name of its own
            if (const JsonParseResult parsed = parseJson(text); parsed.ok)
            {
                if (const Json* old = parsed.value.find("id"); old and old->isString())
                    text = replaceValue(text, *old, id);

                if (const Json* old = parsed.value.find("name"); old and old->isString())
                    text = replaceValue(text, *old, displayName(name));
            }

            if (not writeText(path, "// " + made + "\n" + text))
            {
                done.message = "could not write " + path.string();
                return done;
            }
        }
        else
        {
            const fs::path path = fs::path(mod.dir) / kind.place;
            done.path = path.string();

            Json data = def.data;
            qualifyIds(data, kind.object, def.mod);

            if (Json* old = data.find("name"); old and old->isString())
                old->string = displayName(name);
            const std::string entry = "    // " + made + "\n    " + quoted(name) + ": " + pretty(data, 4) + ",";

            std::string text;
            std::error_code ec;

            if (not fs::exists(path, ec))
                text = "{\n" + entry + "\n}\n";
            else
            {
                if (not readText(path, text))
                {
                    done.message = "could not read " + path.string();
                    return done;
                }

                const JsonParseResult parsed = parseJson(text);

                if (not parsed.ok or not parsed.value.isObject())
                {
                    done.message = path.string() + " has a mistake to fix first: " + (parsed.ok ? "it is not an object" : parsed.error);
                    return done;
                }

                if (parsed.value.find(name))
                {
                    done.message = path.string() + " has a \"" + name + "\" already";
                    return done;
                }

                // At the top: no comma to add after the last entry, past whatever comments follow it
                const size_t brace = openingBrace(text);
                text.insert(brace + 1, "\n" + entry);
            }

            if (not writeText(path, text))
            {
                done.message = "could not write " + path.string();
                return done;
            }
        }

        done.ok = true;
        done.message = "added " + id + ", a copy of " + def.id + ", to " + done.path;
        return done;
    }

    Done writePatch(const Content& content, const ModInfo& mod, const std::string& id)
    {
        Done done;
        const Found found = find(content, id);

        if (not found.def)
        {
            done.message = found.others.empty() ? "nothing is called " + id : id + " could be: " + [&] {
                std::string s;
                for (const auto& o : found.others)
                    s += (s.empty() ? "" : ", ") + o;
                return s;
            }();
            return done;
        }

        const RawDef& def = *found.def;
        const auto [ns, name] = splitId(def.id);
        fs::path path = fs::path(mod.dir) / "patches" / (name + ".json");
        std::error_code ec;

        if (fs::exists(path, ec))
            path = fs::path(mod.dir) / "patches" / (ns + "_" + name + ".json");

        done.path = path.string();

        if (fs::exists(path, ec))
        {
            done.message = path.string() + " exists already: change it there";
            return done;
        }

        const ModInfo* owner = content.mod(def.mod);
        std::string text = "// Changes to " + def.id + ", " + (std::string("aeiou").find(found.kind->name[0]) != std::string::npos ? "an " : "a ") + found.kind->name +
                           " of the mod " + def.mod + (owner ? " (" + relativeTo(def.file, fs::path(owner->dir).parent_path().string()) + ")" : "") + ".\n"
                           "// Remove the // at the start of a line to change that value. An object (like a moveset) is\n"
                           "// changed key by key: keep only the keys you change in it. Anything else is replaced.\n"
                           "[\n"
                           "    { \"kind\": " + quoted(found.kind->raw) + ", \"target\": " + quoted(def.id) + ", \"set\": {\n";

        for (const auto& [key, value] : def.data.object)
        {
            if (key == "id")
                continue;

            const std::string flat = toJsonString(value);

            if (flat.size() <= 90)
                text += "        // " + quoted(key) + ": " + flat + ",\n";
            else
                text += "        // " + quoted(key) + ": too long to show here (VoxelArenaMod show " + def.id + ")\n";
        }

        text += "    } }\n]\n";

        if (not writeText(path, text))
        {
            done.message = "could not write " + path.string();
            return done;
        }

        done.ok = true;
        done.message = "wrote a patch of " + def.id + " in " + path.string();
        return done;
    }

    Problems check(const Content& content, const ModInfo* mod)
    {
        Problems out;
        std::set<std::pair<std::string, int>> said;

        auto mine = [&](const std::string& file) { return not mod or under(file, mod->dir); };

        for (const auto& e : content.report.errors)
            if (mine(e.file))
            {
                out.errors.push_back(e);
                said.insert({fs::path(e.file).lexically_normal().generic_string(), e.line});
            }

        for (const auto& w : content.report.warnings)
            if (mine(w.file))
            {
                out.warnings.push_back(w);
                said.insert({fs::path(w.file).lexically_normal().generic_string(), w.line});
            }

        // What the game lets by but the table does not: values of the wrong kind, files it never reads
        for (const auto& m : content.mods)
        {
            if (mod and m.id != mod->id)
                continue;

            std::error_code ec;

            for (auto it = fs::recursive_directory_iterator(m.dir, ec); not ec and it != fs::recursive_directory_iterator(); it.increment(ec))
            {
                if (not it->is_regular_file(ec))
                    continue;

                const std::string file = it->path().lexically_normal().generic_string();
                const std::string rel = relativeTo(file, m.dir);
                const std::string ext = it->path().extension().string();
                const schema::File* known = nullptr;

                for (const auto& f : schema::files())
                    if (matches(f.path, rel))
                        known = &f;

                if (not known)
                {
                    if (ext == ".json" or ext == ".png" or ext == ".pg")
                        out.warnings.push_back({file, 0, "the game does not read this file: is it in the right folder, with the right name?"});
                    continue;
                }

                if (std::string(known->type).empty())
                    continue;

                std::string text;
                if (not readText(it->path(), text))
                    continue;

                const JsonParseResult parsed = parseJson(text);
                if (not parsed.ok)
                    continue;   // the game said so already

                std::vector<schema::Problem> problems;
                schema::validate(parsed.value, known->type, "", problems);

                for (const auto& p : problems)
                    if (not said.count({file, p.line}) and p.text.find("unknown key") == std::string::npos)
                        out.warnings.push_back({file, p.line, p.text});
            }
        }

        return out;
    }

    Done setModOn(const std::string& settings, const std::string& id, bool on)
    {
        Done done;
        done.path = settings;

        std::vector<std::string> off;
        std::string text;

        if (readText(settings, text))
        {
            const JsonParseResult parsed = parseJson(text);

            if (not parsed.ok)
            {
                done.message = settings + " has a mistake to fix first: " + parsed.error;
                return done;
            }

            if (const Json* list = parsed.value.find("off"); list and list->isArray())
                for (const auto& e : list->array)
                    if (e.isString())
                        off.push_back(e.string);
        }

        const bool wasOn = std::find(off.begin(), off.end(), id) == off.end();
        off.erase(std::remove(off.begin(), off.end(), id), off.end());

        if (not on)
            off.push_back(id);

        Json list;
        list.type = Json::Type::Array;
        for (const auto& o : off)
            list.array.push_back(Json::makeString(o));

        if (not writeText(settings, "{ \"off\": " + toJsonString(list) + " }\n"))
        {
            done.message = "could not write " + settings;
            return done;
        }

        done.ok = true;
        done.message = "the mod " + id + (wasOn == on ? " was already " : " is now ") + (on ? "on" : "off");
        return done;
    }
}
