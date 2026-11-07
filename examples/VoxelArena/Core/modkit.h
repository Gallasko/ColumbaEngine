#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "content.h"

/**
 * What the modding tool (VoxelArenaMod) does, without its command line: making a mod, copying a
 * definition into one, writing a patch, checking a mod. Kept here so the tests run it too.
 */
namespace arena::modkit
{
    /** A kind of definition a mod can add, and where it goes in the mod's folder. */
    struct Kind
    {
        const char* name;      // as the tool takes it: "weapon"
        const char* raw;       // as content.raw and patches name it: "arenas" for an arena
        const char* object;    // its object in the schema
        const char* place;     // a folder (one definition a file) or a file (one entry each)
        bool ownFile;          // place is a folder
        const char* example;   // the definition copied when none is named
    };

    const std::vector<Kind>& kinds();

    /** By the tool's name ("item" is an upgrade too); nullptr when there is none. */
    const Kind* findKind(std::string_view name);

    /** By content.raw's name. */
    const Kind* kindOfRaw(std::string_view raw);

    /** a-z, 0-9, _ and ., as ids are. */
    bool validName(const std::string& name);

    /** JSON as text: what fits on a line stays on one, the rest is spread out, 4 spaces a level. */
    std::string pretty(const Json& json, int indent = 0);

    /**
     * Every id without a namespace given from's, where the schema type says an id goes: a definition
     * copied out of mod from still points at what it pointed at ("sword" -> "base:sword"). The text
     * version keeps the comments and the layout.
     */
    void qualifyIds(Json& json, std::string_view type, const std::string& from);
    std::string qualifyIds(const std::string& text, std::string_view type, const std::string& from);

    /** A definition by id; "sword" finds "base:sword" when no other mod has a "sword" (of that kind, when one is given). */
    struct Found
    {
        const Kind* kind = nullptr;
        const RawDef* def = nullptr;
        std::vector<std::string> others;   // "sword" matching several: their ids, and nothing found
    };

    Found find(const Content& content, const std::string& id, const Kind* only = nullptr);

    struct Done
    {
        bool ok = false;
        std::string path;      // what was written
        std::string message;   // what happened, or why not
    };

    /** A new mod in dir: dir/<id>/mod.json, needing the base game. */
    Done newMod(const std::string& dir, const std::string& id, const std::string& name);

    /** A copy of the definition from (an id) in the mod, named name. */
    Done addDefinition(const Content& content, const ModInfo& mod, const Kind& kind, const std::string& name, const std::string& from);

    /** patches/<name>.json in the mod, with every value of the definition id in comments, ready to change. */
    Done writePatch(const Content& content, const ModInfo& mod, const std::string& id);

    /** What is wrong in a mod's files (all mods: nullptr): what the game says loading it, then values of the wrong kind. */
    struct Problems
    {
        std::vector<ContentMessage> errors;
        std::vector<ContentMessage> warnings;
    };

    Problems check(const Content& content, const ModInfo* mod);

    /** Turns a mod on or off in the settings file ({ "off": [...] }). */
    Done setModOn(const std::string& settings, const std::string& id, bool on);
}
