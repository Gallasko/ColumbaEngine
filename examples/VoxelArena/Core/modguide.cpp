#include "modkit.h"

#include <cctype>
#include <cstdio>
#include <set>

#include "paths.h"
#include "schema.h"
#include "tuning.h"

// The modding guide: one web page, written from the schema and the loaded content, so it always
// says what this build of the game reads.

namespace arena::modkit
{
    namespace
    {
        std::string esc(const std::string& s)
        {
            std::string out;

            for (char c : s)
            {
                switch (c)
                {
                    case '&': out += "&amp;"; break;
                    case '<': out += "&lt;"; break;
                    case '>': out += "&gt;"; break;
                    case '"': out += "&quot;"; break;
                    default: out += c;
                }
            }

            return out;
        }

        std::string code(const std::string& s) { return "<code>" + esc(s) + "</code>"; }

        std::string anchor(const std::string& object) { return "o-" + object; }

        /** A type in words, its objects linked to their tables. */
        std::string typeHtml(const schema::Type& t)
        {
            using K = schema::Type::Kind;

            switch (t.kind)
            {
                case K::Object: return "<a href=\"#" + anchor(t.name) + "\">" + esc(schema::get(t.name).title) + "</a>";
                case K::Id: return "the id of " + std::string(std::string("aeiou").find(t.name[0]) != std::string::npos ? "an " : "a ") + esc(t.name);
                case K::List: return "a list of: " + typeHtml(t.of[0]);
                case K::Map: return (t.of.size() > 1 ? "names (" + typeHtml(t.of[1]) + ")" : std::string("names")) + ", each with " + typeHtml(t.of[0]);

                case K::Enum:
                {
                    std::string s = t.words.size() == 1 ? "" : "one of ";
                    for (size_t i = 0; i < t.words.size(); ++i)
                        s += (i ? ", " : "") + code(t.words[i]);
                    return s;
                }

                case K::Either:
                {
                    std::string s;
                    for (size_t i = 0; i < t.of.size(); ++i)
                        s += (i == 0 ? "" : i + 1 == t.of.size() ? " or " : ", ") + typeHtml(t.of[i]);
                    return s;
                }

                default: return esc(schema::describe(t));
            }
        }

        std::string typeHtml(const char* notation)
        {
            schema::Type t;
            std::string error;
            return schema::parseType(notation, t, error) ? typeHtml(t) : esc(notation);
        }

        const char* STYLE = R"(
:root { --bg: #fbfaf7; --fg: #1d1d1f; --muted: #5f6368; --line: #dedbd2; --code: #efece4; --link: #1f5fbf; --head: #f3f1ea; }
@media (prefers-color-scheme: dark) {
  :root { --bg: #17181b; --fg: #e8e6e1; --muted: #a3a29d; --line: #34363b; --code: #26282d; --link: #8ab4f8; --head: #202226; }
}
* { box-sizing: border-box; }
body { margin: 0; background: var(--bg); color: var(--fg); font: 16px/1.55 system-ui, -apple-system, "Segoe UI", sans-serif; }
main { max-width: 980px; margin: 0 auto; padding: 32px 16px 80px; }
h1 { font-size: 2rem; margin: 0 0 4px; }
h2 { margin-top: 48px; padding-top: 8px; border-top: 2px solid var(--line); }
h3 { margin: 32px 0 4px; }
p.sub, p.note { color: var(--muted); }
a { color: var(--link); }
code { background: var(--code); padding: 1px 5px; border-radius: 4px; font: 0.9em ui-monospace, Consolas, monospace; }
pre { background: var(--code); padding: 12px 14px; border-radius: 6px; overflow-x: auto; font: 0.9em/1.45 ui-monospace, Consolas, monospace; }
table { border-collapse: collapse; width: 100%; margin: 8px 0 16px; font-size: 0.95rem; }
th, td { text-align: left; vertical-align: top; padding: 6px 10px; border-bottom: 1px solid var(--line); }
th { background: var(--head); font-weight: 600; }
td:first-child { white-space: nowrap; }
.toc { columns: 3 200px; padding-left: 18px; }
.ids { columns: 3 220px; font-size: 0.92rem; }
.ids div { break-inside: avoid; }
)";
    }

    std::string guide(const Content& content)
    {
        std::string h;

        h += "<!doctype html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
        h += "<title>Voxel Arena modding guide</title>\n<style>" + std::string(STYLE) + "</style>\n</head>\n<body>\n<main>\n";
        h += "<h1>Voxel Arena modding guide</h1>\n";
        h += "<p class=\"sub\">Written by <code>VoxelArenaMod docs</code> for content API " + std::to_string(CONTENT_API_VERSION) +
             ", with the mods loaded when it was made:";
        for (size_t i = 0; i < content.mods.size(); ++i)
            h += (i ? ", " : " ") + code(content.mods[i].id);
        h += ".</p>\n";

        h += "<ul class=\"toc\">\n<li><a href=\"#start\">Making a mod</a></li>\n<li><a href=\"#ids\">Ids</a></li>\n<li><a href=\"#change\">Changing the game's content</a></li>\n"
             "<li><a href=\"#mistakes\">Mistakes</a></li>\n<li><a href=\"#share\">Sharing a mod</a></li>\n<li><a href=\"#files\">The files of a mod</a></li>\n"
             "<li><a href=\"#keys\">Every key</a></li>\n<li><a href=\"#there\">What there is</a></li>\n<li><a href=\"#tuning\">Tuning numbers</a></li>\n</ul>\n";

        // ---- How mods work ------------------------------------------------------------------------

        h += "<h2 id=\"start\">Making a mod</h2>\n";
        h += "<p>A mod is a folder with a " + code("mod.json") + " file in it, in your mods folder:</p>\n<ul>\n<li>Linux: " +
             code("~/.local/share/voxelarena/mods") + "</li>\n<li>Windows: " + code("%APPDATA%\\VoxelArena\\mods") + "</li>\n</ul>\n";
        h += "<p>On this computer it is " + code(userModsDir()) + ". The game loads your mods when it starts, and again when you press F5 in it.</p>\n";
        h += "<p>The quickest way to start is the modding tool, " + code("VoxelArenaMod") + ", next to the game:</p>\n";
        h += "<pre>VoxelArenaMod new mymod \"My mod\"\nVoxelArenaMod add mymod weapon halberd base:sword\nVoxelArenaMod check mymod</pre>\n";
        h += "<p>This makes the mod, puts a copy of the sword in it as " + code("weapons/halberd.json") +
             ", then checks it. Change the copy as you like and press F5 in the game: the halberd is in the weapon choice of a run.</p>\n";
        h += "<table>\n<tr><th>Command</th><th>What it does</th></tr>\n";
        for (const auto& [command, text] : std::vector<std::pair<std::string, std::string>>{
                 {"new <mod> [\"Name\"]", "Makes a new mod in your mods folder."},
                 {"add <mod> <kind> <name> [id]", "Puts a copy of something in your mod, to start from: a weapon, an enemy, an item... Without an id, it copies one of the base game's."},
                 {"patch <mod> <id>", "Writes a patch of something of another mod, with every value in comments: remove the // in front of the ones to change."},
                 {"check [mod]", "Finds mistakes in the mods."},
                 {"show <id>", "Shows something as the game reads it, after every mod."},
                 {"list [kind]", "Lists what there is."},
                 {"mods, on <mod>, off <mod>", "Lists the mods, and turns one on or off."},
                 {"docs [file]", "Writes this guide."},
                 {"pack <mod> [file]", "Checks the mod and makes a .zip of it to share."}})
            h += "<tr><td>" + code(command) + "</td><td>" + esc(text) + "</td></tr>\n";
        h += "</table>\n";
        h += "<p>Every file is JSON, and may have comments: from " + code("//") + " to the end of the line. A comma after the last value is fine.</p>\n";

        h += "<h2 id=\"ids\">Ids</h2>\n";
        h += "<p>Everything a mod adds has an id: the mod's id, a colon, and its name. A weapon in " + code("weapons/halberd.json") + " of the mod " +
             code("mymod") + " is " + code("mymod:halberd") + ". A block named " + code("glow") + " in " + code("blocks.json") + " is " + code("mymod:glow") + ".</p>\n";
        h += "<p>Inside your mod, a name alone means your own: " + code("\"model\": \"halberd\"") + " is " + code("mymod:halberd") +
             ". To use something of another mod, write its whole id: " + code("\"model\": \"base:sword\"") + ". Ids use a-z, 0-9, _ and . only.</p>\n";

        h += "<h2 id=\"change\">Changing the game's content</h2>\n";
        h += "<p><b>Replacing</b> something: give your definition its id. A file " + code("weapons/sword.json") + " in your mod with " +
             code("\"id\": \"base:sword\"") + " replaces the base game's sword.</p>\n";
        h += "<p><b>Patching</b> something changes only some of it: a file in your mod's " + code("patches/") +
             " folder, with one patch or a list of them (see <a href=\"#" + anchor("patch") + "\">a patch</a>). Objects are changed key by key; anything else is replaced.</p>\n";
        h += "<pre>[\n    { \"kind\": \"enemy\", \"target\": \"base:brute\", \"set\": { \"health\": 300, \"walkSpeed\": 3.5 } },\n"
             "    { \"kind\": \"tuning\", \"set\": { \"player\": { \"walkSpeed\": 7 } } }\n]</pre>\n";
        h += "<p>Mods load after the mods they need (" + code("dependencies") + " in " + code("mod.json") +
             "), then in the order of their ids. What a later mod replaces or patches wins.</p>\n";
        h += "<p><b>Turning a mod off</b>: " + code("VoxelArenaMod off <mod>") + ". The game skips it, and the mods that need it.</p>\n";

        h += "<h2 id=\"mistakes\">Mistakes</h2>\n";
        h += "<p>Nothing a mod gets wrong crashes the game. A broken file is reported with its name and line, and what is broken is left out. "
             "The first mistake shows at the top of the screen; " + code("errors") + " and " + code("warnings") + " in the game's console (the " +
             code("`") + " key) list them all. " + code("VoxelArenaMod check") + " finds the same mistakes without starting the game, "
             "and a few more: values of the wrong kind, and files the game never reads (a file in " + code("weapon/") + " instead of " + code("weapons/") + ").</p>\n";

        h += "<h2 id=\"share\">Sharing a mod</h2>\n";
        h += "<p>" + code("VoxelArenaMod pack mymod") + " checks the mod and makes " + code("mymod.zip") + ". To install it, unzip it into the mods folder: "
             "the folder " + code("mymod") + " (with " + code("mod.json") + " in it) must be right inside " + code("mods") + ", not in another folder.</p>\n";
        h += "<p>Playing together, you do not need to share anything: the host sends its mods to everyone who joins, for that game.</p>\n";

        // ---- The files and their keys -------------------------------------------------------------

        h += "<h2 id=\"files\">The files of a mod</h2>\n<table>\n<tr><th>File</th><th>What is in it</th><th>Holds</th></tr>\n";
        for (const auto& f : schema::files())
            h += "<tr><td>" + code(f.path) + "</td><td>" + esc(f.text) + "</td><td>" +
                 (std::string(f.type).empty() ? std::string("not JSON") : std::string(f.type) == "any" ? std::string("see the base game's") : typeHtml(f.type)) + "</td></tr>\n";
        h += "</table>\n";

        h += "<h2 id=\"keys\">Every key</h2>\n";
        h += "<p>Every key is optional unless its text says so. The base game's files are good examples: they are in " + code("mods/base") +
             " next to the game, and " + code("VoxelArenaMod show <id>") + " shows any of them.</p>\n<p>";
        for (size_t i = 0; i < schema::objects().size(); ++i)
        {
            const auto& o = schema::objects()[i];
            h += std::string(i ? " · " : "") + "<a href=\"#" + anchor(o.name) + "\">" + esc(o.title) + "</a>";
        }
        h += "</p>\n";

        for (const auto& o : schema::objects())
        {
            std::string title = o.title;
            if (not title.empty())
                title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(title[0])));

            h += "<h3 id=\"" + anchor(o.name) + "\">" + esc(title) + "</h3>\n<p>" + esc(o.text) + "</p>\n";
            h += "<table>\n<tr><th>Key</th><th>Holds</th><th>What it does</th></tr>\n";
            for (const auto& f : o.fields)
                h += "<tr><td>" + code(f.name) + "</td><td>" + typeHtml(f.type) + "</td><td>" + esc(f.text) + "</td></tr>\n";
            h += "</table>\n";
        }

        // ---- What is loaded -----------------------------------------------------------------------

        h += "<h2 id=\"there\">What there is</h2>\n<p class=\"note\">The ids of the mods loaded when this guide was made.</p>\n";

        auto idList = [&](const std::string& title, const std::vector<std::string>& ids) {
            if (ids.empty())
                return;
            h += "<h3>" + esc(title) + " (" + std::to_string(ids.size()) + ")</h3>\n<div class=\"ids\">";
            for (const auto& id : ids)
                h += "<div>" + code(id) + "</div>";
            h += "</div>\n";
        };

        auto rawIds = [&](const std::string& raw) {
            std::vector<std::string> ids;
            if (const auto defs = content.raw.find(raw); defs != content.raw.end())
                for (const auto& [id, def] : defs->second)
                    ids.push_back(id);
            return ids;
        };

        for (const auto& k : kinds())
            idList(std::string(k.name) == "upgrade" ? "Shop items" : std::string(k.name) == "ability" ? "Abilities" : std::string(k.name) == "status" ? "Statuses"
                   : std::string(k.name) == "enemy" ? "Enemies" : std::string(k.name) == "affix" ? "Affixes" : std::string(1, static_cast<char>(std::toupper(k.name[0]))) + (k.name + 1) + "s",
                   rawIds(k.raw));

        idList("Elements", {"fire", "ice", "volt"});
        idList("Models", rawIds("model"));

        std::vector<std::string> textures, scripts;
        for (const auto& [id, file] : content.textureFiles)
            textures.push_back(id);
        for (const auto& [id, file] : content.scripts)
            scripts.push_back(id);
        idList("Textures", textures);
        idList("Enemy scripts", scripts);

        h += "<h2 id=\"tuning\">Tuning numbers</h2>\n";
        h += "<p>Gameplay numbers, from " + code("tuning.json") + ". A mod's " + code("tuning.json") +
             " is merged over the ones loaded before, or a patch of kind " + code("tuning") + " changes a few. In the game, the console's " +
             code("tune <name> <value>") + " tries one until the next F5.</p>\n";
        h += "<table>\n<tr><th>Name</th><th>Now</th></tr>\n";
        {
            ContentReport ignored;
            Tuning tuning = readTuning(content.tuning, content.tuningFile, ignored);
            tuning.forEachNumber([&](const std::string& name, float& value) {
                char v[32];
                std::snprintf(v, sizeof v, "%g", static_cast<double>(value));
                h += "<tr><td>" + code(name) + "</td><td>" + esc(v) + "</td></tr>\n";
            });
        }
        h += "</table>\n";

        h += "</main>\n</body>\n</html>\n";
        return h;
    }
}
