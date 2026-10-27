#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace arena
{
    /**
     * A small JSON reader for content files.
     *
     * Every value remembers the line it was read on, so a content error can say
     * "mods/base/weapons/sword.json:14: ..." instead of only "bad file".
     * Comments (// to end of line) are accepted, since content files are hand written.
     */
    struct Json
    {
        enum class Type { Null, Bool, Number, String, Array, Object };

        Type type = Type::Null;
        bool boolean = false;
        double number = 0.0;
        std::string string;
        std::vector<Json> array;
        // Keys in file order: patches and registries apply entries in the order they were written
        std::vector<std::pair<std::string, Json>> object;

        int line = 0;

        bool isNull() const   { return type == Type::Null; }
        bool isBool() const   { return type == Type::Bool; }
        bool isNumber() const { return type == Type::Number; }
        bool isString() const { return type == Type::String; }
        bool isArray() const  { return type == Type::Array; }
        bool isObject() const { return type == Type::Object; }

        /** The member named key, or nullptr when this is not an object or has no such key. */
        const Json* find(const std::string& key) const;
        Json* find(const std::string& key);

        bool has(const std::string& key) const { return find(key) != nullptr; }

        /** Typed reads with a fallback for a missing key. A key of the wrong type is an error (see ContentErrors). */
        double num(const std::string& key, double fallback) const;
        std::string str(const std::string& key, const std::string& fallback) const;
        bool flag(const std::string& key, bool fallback) const;

        /** Set or replace a member (objects only). */
        void set(const std::string& key, const Json& value);

        static Json makeObject() { Json j; j.type = Type::Object; return j; }
        static Json makeNumber(double v) { Json j; j.type = Type::Number; j.number = v; return j; }
        static Json makeString(const std::string& v) { Json j; j.type = Type::String; j.string = v; return j; }
    };

    struct JsonParseResult
    {
        bool ok = false;
        Json value;
        std::string error;  // "line N: message" when not ok
        int errorLine = 0;
    };

    JsonParseResult parseJson(const std::string& text);

    /** Write a value back as text (used by tests and the debug console). */
    std::string toJsonString(const Json& value);

    /**
     * Deep merge of patch into base: objects merge key by key, everything else is replaced.
     * This is how a mod patches content it did not define.
     */
    void mergeJson(Json& base, const Json& patch);
}
