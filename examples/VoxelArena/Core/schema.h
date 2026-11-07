#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "json.h"

/**
 * Every key a mod's files may use, with what it holds and what it does: one table the loader checks
 * keys against, and the modding tools read to write the guide, templates and editor schemas.
 *
 * A field's type is written in a short notation:
 *   number, integer, bool, text, any
 *   vec2, vec3            [a, b] and [x, y, z]
 *   colour                [r, g, b], 0-255
 *   id:<kind>             the id of a block, status, element, keyword, model, script, enemy, weapon,
 *                         ability, spell, arena, affix, upgrade, combo or texture
 *   enum:a,b,c            one of these words
 *   <object>              an object of the table ("hitbox")
 *   list:<type>           a list of them
 *   map:<type>            an object of name -> them
 *   map(<key>):<type>     the same, its names of that type: map(enum:any|id:status):number
 *   a|b                   either; parentheses group: list:(id:status|statusRef)
 */
namespace arena::schema
{
    struct Field
    {
        const char* name;
        const char* type;
        const char* text;   // what it does, in plain words
    };

    struct Object
    {
        const char* name;    // how types refer to it: "attack"
        const char* title;   // in a sentence: "an attack"
        const char* text;    // what it is and where it is used
        std::vector<Field> fields;

        const Field* field(std::string_view key) const;
    };

    /** A file a mod may hold, and what is in it. */
    struct File
    {
        const char* path;   // in the mod's folder: "blocks.json", "weapons/*.json"
        const char* type;   // its contents, in the notation ("map:block"); "" for a file that is not JSON
        const char* text;
    };

    const std::vector<Object>& objects();
    const std::vector<File>& files();

    /** The object of the table with this name, nullptr when there is none. */
    const Object* find(std::string_view name);

    /** The object with this name; a name missing from the table is the game's own mistake (std::logic_error). */
    const Object& get(std::string_view name);

    /** A type of the notation, parsed. */
    struct Type
    {
        enum class Kind { Number, Integer, Bool, Text, Any, Vec2, Vec3, Colour, Id, Enum, Object, List, Map, Either };

        Kind kind = Kind::Any;
        std::string name;                 // Id: the kind of id; Object: the object's name
        std::vector<std::string> words;   // Enum
        std::vector<Type> of;             // List and Map: what they hold (a Map's second: its names); Either: the choices
    };

    /** Reads the notation; false, and why in error, when it is wrong (an unknown word or object, a stray bracket). */
    bool parseType(std::string_view text, Type& out, std::string& error);

    /** The type in words: "a number", "a list of status ids", "a hitbox". */
    std::string describe(const Type& type);

    struct Problem
    {
        int line = 0;
        std::string text;
    };

    /**
     * Checks a value against a type, all the way down: values of the wrong kind and unknown keys, each
     * with its line. Ids are not looked up (the loader does that, knowing what is loaded). where names
     * the value in the messages ("weapon base:sword").
     */
    void validate(const Json& json, std::string_view type, const std::string& where, std::vector<Problem>& out);

    /**
     * Calls f with every id where the type says one goes, and the kind of id ("model", "status"...):
     * text values, and the names of a map(id:...). Where a choice is open (an id or an object), the
     * choice that fits the value is followed. A value that does not fit its type is skipped.
     */
    struct IdAt
    {
        std::string& id;   // changing it changes the value, or the name
        int line;
        bool name;         // a map's name, not a value
        const std::string& kind;
    };

    void forEachId(Json& json, std::string_view type, const std::function<void(IdAt& at)>& f);
}
