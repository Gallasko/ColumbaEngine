#include "json.h"

#include <cmath>
#include <cstdlib>
#include <sstream>

namespace arena
{
    const Json* Json::find(const std::string& key) const
    {
        if (type != Type::Object)
            return nullptr;

        for (const auto& kv : object)
        {
            if (kv.first == key)
                return &kv.second;
        }

        return nullptr;
    }

    Json* Json::find(const std::string& key)
    {
        return const_cast<Json*>(static_cast<const Json*>(this)->find(key));
    }

    double Json::num(const std::string& key, double fallback) const
    {
        const auto* v = find(key);
        return (v and v->isNumber()) ? v->number : fallback;
    }

    std::string Json::str(const std::string& key, const std::string& fallback) const
    {
        const auto* v = find(key);
        return (v and v->isString()) ? v->string : fallback;
    }

    bool Json::flag(const std::string& key, bool fallback) const
    {
        const auto* v = find(key);
        return (v and v->isBool()) ? v->boolean : fallback;
    }

    void Json::set(const std::string& key, const Json& value)
    {
        if (type != Type::Object)
            return;

        if (auto* v = find(key))
        {
            *v = value;
            return;
        }

        object.emplace_back(key, value);
    }

    namespace
    {
        struct Parser
        {
            const std::string& s;
            size_t pos = 0;
            int line = 1;
            std::string error;
            int errorLine = 0;

            explicit Parser(const std::string& s) : s(s) {}

            bool fail(const std::string& message)
            {
                if (error.empty())
                {
                    error = "line " + std::to_string(line) + ": " + message;
                    errorLine = line;
                }

                return false;
            }

            void skipSpace()
            {
                while (pos < s.size())
                {
                    char c = s[pos];

                    if (c == '\n')
                    {
                        ++line;
                        ++pos;
                    }
                    else if (c == ' ' or c == '\t' or c == '\r')
                    {
                        ++pos;
                    }
                    else if (c == '/' and pos + 1 < s.size() and s[pos + 1] == '/')
                    {
                        while (pos < s.size() and s[pos] != '\n')
                            ++pos;
                    }
                    else
                    {
                        break;
                    }
                }
            }

            bool parseString(std::string& out)
            {
                // Opening quote already checked by the caller
                ++pos;

                while (pos < s.size())
                {
                    char c = s[pos++];

                    if (c == '"')
                        return true;

                    if (c == '\n')
                        return fail("a string runs past the end of its line");

                    if (c == '\\')
                    {
                        if (pos >= s.size())
                            break;

                        char e = s[pos++];

                        switch (e)
                        {
                            case 'n': out += '\n'; break;
                            case 't': out += '\t'; break;
                            case 'r': out += '\r'; break;
                            case '"': out += '"'; break;
                            case '\\': out += '\\'; break;
                            case '/': out += '/'; break;
                            default: return fail(std::string("unknown escape \\") + e);
                        }
                    }
                    else
                    {
                        out += c;
                    }
                }

                return fail("a string is never closed");
            }

            bool parseValue(Json& out, int depth)
            {
                if (depth > 64)
                    return fail("values are nested too deep");

                skipSpace();

                if (pos >= s.size())
                    return fail("a value was expected, the file ended");

                out.line = line;

                char c = s[pos];

                if (c == '{')
                {
                    out.type = Json::Type::Object;
                    ++pos;
                    skipSpace();

                    if (pos < s.size() and s[pos] == '}')
                    {
                        ++pos;
                        return true;
                    }

                    while (true)
                    {
                        skipSpace();

                        if (pos >= s.size() or s[pos] != '"')
                            return fail("a key in quotes was expected");

                        std::string key;
                        if (not parseString(key))
                            return false;

                        for (const auto& kv : out.object)
                        {
                            if (kv.first == key)
                                return fail("the key \"" + key + "\" is written twice");
                        }

                        skipSpace();

                        if (pos >= s.size() or s[pos] != ':')
                            return fail("':' was expected after \"" + key + "\"");

                        ++pos;

                        Json value;
                        if (not parseValue(value, depth + 1))
                            return false;

                        out.object.emplace_back(std::move(key), std::move(value));

                        skipSpace();

                        if (pos < s.size() and s[pos] == ',')
                        {
                            ++pos;
                            skipSpace();

                            // A trailing comma before '}' is forgiven
                            if (pos < s.size() and s[pos] == '}')
                            {
                                ++pos;
                                return true;
                            }

                            continue;
                        }

                        if (pos < s.size() and s[pos] == '}')
                        {
                            ++pos;
                            return true;
                        }

                        return fail("',' or '}' was expected");
                    }
                }

                if (c == '[')
                {
                    out.type = Json::Type::Array;
                    ++pos;
                    skipSpace();

                    if (pos < s.size() and s[pos] == ']')
                    {
                        ++pos;
                        return true;
                    }

                    while (true)
                    {
                        Json value;
                        if (not parseValue(value, depth + 1))
                            return false;

                        out.array.push_back(std::move(value));

                        skipSpace();

                        if (pos < s.size() and s[pos] == ',')
                        {
                            ++pos;
                            skipSpace();

                            if (pos < s.size() and s[pos] == ']')
                            {
                                ++pos;
                                return true;
                            }

                            continue;
                        }

                        if (pos < s.size() and s[pos] == ']')
                        {
                            ++pos;
                            return true;
                        }

                        return fail("',' or ']' was expected");
                    }
                }

                if (c == '"')
                {
                    out.type = Json::Type::String;
                    return parseString(out.string);
                }

                if (c == '-' or (c >= '0' and c <= '9'))
                {
                    size_t start = pos;
                    ++pos;

                    while (pos < s.size())
                    {
                        char d = s[pos];
                        if ((d >= '0' and d <= '9') or d == '.' or d == 'e' or d == 'E' or d == '-' or d == '+')
                            ++pos;
                        else
                            break;
                    }

                    const std::string text = s.substr(start, pos - start);
                    char* end = nullptr;
                    double v = std::strtod(text.c_str(), &end);

                    if (end != text.c_str() + text.size() or not std::isfinite(v))
                        return fail("\"" + text + "\" is not a number");

                    out.type = Json::Type::Number;
                    out.number = v;
                    return true;
                }

                auto word = [&](const char* w) {
                    size_t n = std::char_traits<char>::length(w);
                    if (s.compare(pos, n, w) == 0)
                    {
                        pos += n;
                        return true;
                    }
                    return false;
                };

                if (word("true"))  { out.type = Json::Type::Bool; out.boolean = true;  return true; }
                if (word("false")) { out.type = Json::Type::Bool; out.boolean = false; return true; }
                if (word("null"))  { out.type = Json::Type::Null; return true; }

                return fail(std::string("unexpected character '") + c + "'");
            }
        };

        void writeString(std::ostringstream& out, const std::string& v)
        {
            out << '"';
            for (char c : v)
            {
                switch (c)
                {
                    case '"': out << "\\\""; break;
                    case '\\': out << "\\\\"; break;
                    case '\n': out << "\\n"; break;
                    case '\t': out << "\\t"; break;
                    default: out << c;
                }
            }
            out << '"';
        }

        void write(std::ostringstream& out, const Json& v)
        {
            switch (v.type)
            {
                case Json::Type::Null: out << "null"; break;
                case Json::Type::Bool: out << (v.boolean ? "true" : "false"); break;
                case Json::Type::Number:
                {
                    if (v.number == std::floor(v.number) and std::fabs(v.number) < 1e15)
                        out << static_cast<long long>(v.number);
                    else
                        out << v.number;
                    break;
                }
                case Json::Type::String: writeString(out, v.string); break;
                case Json::Type::Array:
                {
                    out << '[';
                    for (size_t i = 0; i < v.array.size(); ++i)
                    {
                        if (i > 0)
                            out << ", ";
                        write(out, v.array[i]);
                    }
                    out << ']';
                    break;
                }
                case Json::Type::Object:
                {
                    out << '{';
                    for (size_t i = 0; i < v.object.size(); ++i)
                    {
                        if (i > 0)
                            out << ", ";
                        writeString(out, v.object[i].first);
                        out << ": ";
                        write(out, v.object[i].second);
                    }
                    out << '}';
                    break;
                }
            }
        }
    }

    JsonParseResult parseJson(const std::string& text)
    {
        JsonParseResult result;
        Parser p(text);

        if (p.parseValue(result.value, 0))
        {
            p.skipSpace();

            if (p.pos < text.size())
                p.fail("text was found after the end of the value");
        }

        result.ok = p.error.empty();
        result.error = p.error;
        result.errorLine = p.errorLine;

        if (not result.ok)
            result.value = Json{};

        return result;
    }

    std::string toJsonString(const Json& value)
    {
        std::ostringstream out;
        write(out, value);
        return out.str();
    }

    void mergeJson(Json& base, const Json& patch)
    {
        if (base.isObject() and patch.isObject())
        {
            for (const auto& kv : patch.object)
            {
                if (auto* existing = base.find(kv.first))
                    mergeJson(*existing, kv.second);
                else
                    base.object.push_back(kv);
            }

            return;
        }

        base = patch;
    }
}
