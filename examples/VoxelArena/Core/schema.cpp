#include "schema.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace arena::schema
{
    const Field* Object::field(std::string_view key) const
    {
        for (const auto& f : fields)
            if (key == f.name)
                return &f;

        return nullptr;
    }

    const Object* find(std::string_view name)
    {
        static const std::map<std::string, const Object*, std::less<>> byName = [] {
            std::map<std::string, const Object*, std::less<>> m;
            for (const auto& o : objects())
                m[o.name] = &o;
            return m;
        }();

        const auto it = byName.find(name);
        return it == byName.end() ? nullptr : it->second;
    }

    const Object& get(std::string_view name)
    {
        if (const Object* o = find(name))
            return *o;

        throw std::logic_error("the schema has no object \"" + std::string(name) + "\"");
    }

    // ---- The notation ---------------------------------------------------------------------------

    namespace
    {
        const std::map<std::string, Type::Kind, std::less<>> BASIC = {
            {"number", Type::Kind::Number}, {"integer", Type::Kind::Integer}, {"bool", Type::Kind::Bool}, {"text", Type::Kind::Text},
            {"any", Type::Kind::Any},       {"vec2", Type::Kind::Vec2},       {"vec3", Type::Kind::Vec3}, {"colour", Type::Kind::Colour},
        };

        const char* const ID_KINDS[] = {"block", "status", "element", "keyword", "model", "script", "enemy", "weapon",
                                        "ability", "spell", "arena", "affix", "upgrade", "combo", "texture"};

        struct Parser
        {
            std::string_view text;
            size_t at = 0;
            std::string error;

            bool done() const { return at >= text.size(); }

            bool eat(std::string_view word)
            {
                if (text.substr(at, word.size()) != word)
                    return false;

                at += word.size();
                return true;
            }

            /** Letters, digits and _ up to the next mark of the notation. */
            std::string word()
            {
                const size_t from = at;
                while (not done() and text[at] != '|' and text[at] != ')' and text[at] != '(' and text[at] != ':' and text[at] != ',')
                    ++at;
                return std::string(text.substr(from, at - from));
            }

            bool either(Type& out)
            {
                Type first;
                if (not single(first))
                    return false;

                if (done() or text[at] != '|')
                {
                    out = std::move(first);
                    return true;
                }

                out = Type{};
                out.kind = Type::Kind::Either;
                out.of.push_back(std::move(first));

                while (eat("|"))
                {
                    Type next;
                    if (not single(next))
                        return false;
                    out.of.push_back(std::move(next));
                }

                return true;
            }

            bool single(Type& out)
            {
                out = Type{};

                if (eat("("))
                {
                    if (not either(out))
                        return false;

                    if (not eat(")"))
                    {
                        error = "a \"(\" is not closed";
                        return false;
                    }

                    return true;
                }

                const bool list = eat("list:");

                if (list or eat("map:"))
                {
                    out.kind = list ? Type::Kind::List : Type::Kind::Map;
                    out.of.emplace_back();
                    return single(out.of.back());
                }

                if (eat("id:"))
                {
                    out.kind = Type::Kind::Id;
                    out.name = word();

                    for (const char* k : ID_KINDS)
                        if (out.name == k)
                            return true;

                    error = "\"" + out.name + "\" is not a kind of id";
                    return false;
                }

                if (eat("enum:"))
                {
                    out.kind = Type::Kind::Enum;

                    do
                        out.words.push_back(word());
                    while (eat(","));

                    for (const auto& w : out.words)
                    {
                        if (w.empty())
                        {
                            error = "an enum has an empty word";
                            return false;
                        }
                    }

                    return true;
                }

                const std::string name = word();

                if (const auto basic = BASIC.find(name); basic != BASIC.end())
                {
                    out.kind = basic->second;
                    return true;
                }

                if (find(name))
                {
                    out.kind = Type::Kind::Object;
                    out.name = name;
                    return true;
                }

                error = name.empty() ? "a type is missing" : "\"" + name + "\" is not a type nor an object of the table";
                return false;
            }
        };

        // ---- Checking values ----------------------------------------------------------------------

        bool isWhole(double v) { return std::floor(v) == v; }

        bool numbers(const Json& j, size_t from, size_t to)
        {
            if (not j.isArray() or j.array.size() < from or j.array.size() > to)
                return false;

            for (const auto& e : j.array)
                if (not e.isNumber())
                    return false;

            return true;
        }

        /** Whether a value is of the type's kind at its top, ignoring what is inside. */
        bool shapeFits(const Json& j, const Type& t)
        {
            switch (t.kind)
            {
                case Type::Kind::Number:
                case Type::Kind::Integer: return j.isNumber();
                case Type::Kind::Bool: return j.isBool();
                case Type::Kind::Text:
                case Type::Kind::Id:
                case Type::Kind::Enum: return j.isString();
                case Type::Kind::Vec2:
                case Type::Kind::Vec3:
                case Type::Kind::Colour:
                case Type::Kind::List: return j.isArray();
                case Type::Kind::Object:
                case Type::Kind::Map: return j.isObject();
                case Type::Kind::Any: return true;
                case Type::Kind::Either:
                    for (const auto& c : t.of)
                        if (shapeFits(j, c))
                            return true;
                    return false;
            }

            return false;
        }

        void check(const Json& j, const Type& t, const std::string& where, std::vector<Problem>& out)
        {
            auto wrong = [&] { out.push_back({j.line, where + " should be " + describe(t)}); };

            switch (t.kind)
            {
                case Type::Kind::Any: return;
                case Type::Kind::Number: if (not j.isNumber()) wrong(); return;
                case Type::Kind::Integer: if (not j.isNumber() or not isWhole(j.number)) wrong(); return;
                case Type::Kind::Bool: if (not j.isBool()) wrong(); return;
                case Type::Kind::Text:
                case Type::Kind::Id: if (not j.isString()) wrong(); return;
                case Type::Kind::Vec2: if (not numbers(j, 2, 2)) wrong(); return;
                case Type::Kind::Vec3: if (not numbers(j, 3, 3)) wrong(); return;
                case Type::Kind::Colour: if (not numbers(j, 3, 4)) wrong(); return;

                case Type::Kind::Enum:
                    if (not j.isString())
                        wrong();
                    else if (std::find(t.words.begin(), t.words.end(), j.string) == t.words.end())
                        out.push_back({j.line, where + " is \"" + j.string + "\", it should be " + describe(t)});
                    return;

                case Type::Kind::List:
                    if (not j.isArray())
                        return wrong();
                    for (size_t i = 0; i < j.array.size(); ++i)
                        check(j.array[i], t.of[0], where + "[" + std::to_string(i) + "]", out);
                    return;

                case Type::Kind::Map:
                    if (not j.isObject())
                        return wrong();
                    for (const auto& [key, value] : j.object)
                        check(value, t.of[0], where + "." + key, out);
                    return;

                case Type::Kind::Object:
                {
                    if (not j.isObject())
                        return wrong();

                    const Object& o = get(t.name);

                    for (const auto& [key, value] : j.object)
                    {
                        const Field* f = o.field(key);

                        if (not f)
                        {
                            out.push_back({value.line, where + ": unknown key \"" + key + "\" in " + o.title});
                            continue;
                        }

                        Type ft;
                        std::string error;

                        if (parseType(f->type, ft, error))
                            check(value, ft, where + "." + key, out);
                    }

                    return;
                }

                case Type::Kind::Either:
                {
                    // The first choice that fits it all; else the problems of the only one of its shape
                    const Type* only = nullptr;
                    int shaped = 0;

                    for (const auto& c : t.of)
                    {
                        std::vector<Problem> tried;
                        check(j, c, where, tried);

                        if (tried.empty())
                            return;

                        if (shapeFits(j, c))
                        {
                            only = &c;
                            ++shaped;
                        }
                    }

                    if (shaped == 1)
                        check(j, *only, where, out);
                    else
                        wrong();

                    return;
                }
            }
        }

        std::string article(const std::string& word)
        {
            return (std::string("aeiou").find(word.empty() ? 'x' : word[0]) != std::string::npos ? "an " : "a ") + word;
        }
    }

    bool parseType(std::string_view text, Type& out, std::string& error)
    {
        Parser p{text, 0, {}};

        if (not p.either(out))
        {
            error = p.error;
            return false;
        }

        if (not p.done())
        {
            error = "\"" + std::string(text.substr(p.at)) + "\" is left over";
            return false;
        }

        return true;
    }

    std::string describe(const Type& t)
    {
        switch (t.kind)
        {
            case Type::Kind::Number: return "a number";
            case Type::Kind::Integer: return "a whole number";
            case Type::Kind::Bool: return "true or false";
            case Type::Kind::Text: return "text";
            case Type::Kind::Any: return "anything";
            case Type::Kind::Vec2: return "two numbers [a, b]";
            case Type::Kind::Vec3: return "three numbers [x, y, z]";
            case Type::Kind::Colour: return "a colour [r, g, b] (0-255)";
            case Type::Kind::Id: return "the id of " + article(t.name);
            case Type::Kind::Object: return get(t.name).title;
            case Type::Kind::List: return "a list of: " + describe(t.of[0]);
            case Type::Kind::Map: return "names, each with " + describe(t.of[0]);

            case Type::Kind::Enum:
            {
                std::string s = t.words.size() == 1 ? "\"" + t.words[0] + "\"" : "one of ";
                for (size_t i = 0; t.words.size() > 1 and i < t.words.size(); ++i)
                    s += (i ? ", \"" : "\"") + t.words[i] + "\"";
                return s;
            }

            case Type::Kind::Either:
            {
                std::string s;
                for (size_t i = 0; i < t.of.size(); ++i)
                    s += (i == 0 ? "" : i + 1 == t.of.size() ? " or " : ", ") + describe(t.of[i]);
                return s;
            }
        }

        return "";
    }

    void validate(const Json& json, std::string_view type, const std::string& where, std::vector<Problem>& out)
    {
        Type t;
        std::string error;

        if (not parseType(type, t, error))
        {
            out.push_back({json.line, where + ": the schema's type \"" + std::string(type) + "\" is wrong: " + error});
            return;
        }

        check(json, t, where, out);
    }
}
