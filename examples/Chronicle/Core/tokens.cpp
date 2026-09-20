#include "tokens.h"

#include <cassert>
#include <cmath>
#include <sstream>

#include "Files/filemanager.h"
#include "logger.h"

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Tokens";

        const pg::constant::Vector4D MAGENTA{255.0f, 0.0f, 255.0f, 255.0f};

        int hexValue(char c)
        {
            if (c >= '0' and c <= '9') return c - '0';
            if (c >= 'a' and c <= 'f') return c - 'a' + 10;
            if (c >= 'A' and c <= 'F') return c - 'A' + 10;
            return -1;
        }

        bool parseHexPair(char hi, char lo, float& out)
        {
            const int h = hexValue(hi);
            const int l = hexValue(lo);
            if (h < 0 or l < 0)
                return false;
            out = static_cast<float>(h * 16 + l);
            return true;
        }

        // Parses "#rgb", "#rrggbb", "#rrggbbaa" or "rgba(r,g,b,a)" into 0-255 RGBA.
        // Aliases ("{token}") are resolved before this is called.
        bool parseColourString(const std::string& s, pg::constant::Vector4D& out)
        {
            if (s.empty())
                return false;

            if (s[0] == '#')
            {
                const std::string hex = s.substr(1);

                if (hex.size() == 3)
                {
                    return parseHexPair(hex[0], hex[0], out.x)
                       and parseHexPair(hex[1], hex[1], out.y)
                       and parseHexPair(hex[2], hex[2], out.z)
                       and (out.w = 255.0f, true);
                }
                if (hex.size() == 6)
                {
                    return parseHexPair(hex[0], hex[1], out.x)
                       and parseHexPair(hex[2], hex[3], out.y)
                       and parseHexPair(hex[4], hex[5], out.z)
                       and (out.w = 255.0f, true);
                }
                if (hex.size() == 8)
                {
                    return parseHexPair(hex[0], hex[1], out.x)
                       and parseHexPair(hex[2], hex[3], out.y)
                       and parseHexPair(hex[4], hex[5], out.z)
                       and parseHexPair(hex[6], hex[7], out.w);
                }
                return false;
            }

            if (s.rfind("rgba(", 0) == 0 and s.back() == ')')
            {
                const std::string inner = s.substr(5, s.size() - 6);
                std::stringstream stream(inner);
                std::string part;
                std::vector<std::string> parts;
                while (std::getline(stream, part, ','))
                    parts.push_back(part);

                if (parts.size() != 4)
                    return false;

                try
                {
                    out.x = static_cast<float>(std::stoi(parts[0]));
                    out.y = static_cast<float>(std::stoi(parts[1]));
                    out.z = static_cast<float>(std::stoi(parts[2]));
                    const float alpha = std::stof(parts[3]);
                    out.w = std::round(alpha * 255.0f);
                }
                catch (const std::exception&)
                {
                    return false;
                }

                return true;
            }

            return false;
        }

        bool isAlias(const std::string& s)
        {
            return s.size() >= 2 and s.front() == '{' and s.back() == '}';
        }

        std::string aliasTarget(const std::string& s)
        {
            return s.substr(1, s.size() - 2);
        }

        // "Npx" or "0" -> pixels.
        bool parsePx(const std::string& s, float& out)
        {
            if (s == "0")
            {
                out = 0.0f;
                return true;
            }
            if (s.size() > 2 and s.compare(s.size() - 2, 2, "px") == 0)
            {
                try
                {
                    out = std::stof(s.substr(0, s.size() - 2));
                    return true;
                }
                catch (const std::exception&)
                {
                    return false;
                }
            }
            return false;
        }
    }

    const char* themeId(Theme t)
    {
        return t == Theme::Candle ? "candle" : "day";
    }

    bool themeFromId(const std::string& id, Theme& out)
    {
        if (id == "day")    { out = Theme::Day;    return true; }
        if (id == "candle") { out = Theme::Candle; return true; }
        return false;
    }

    Tokens Tokens::load(const std::string& path)
    {
        pg::TextFile file = pg::UniversalFileAccessor::openTextFile(path);

        if (file.data.empty())
        {
            Tokens tokens;
            tokens.errorList.push_back(path + ": could not open or empty file");
            return tokens;
        }

        return parse(file.data, path);
    }

    Tokens Tokens::parse(const std::string& jsonText, const std::string& sourceName)
    {
        Tokens tokens;

        nlohmann::json root = nlohmann::json::parse(jsonText, nullptr, false);
        if (root.is_discarded())
        {
            tokens.errorList.push_back(sourceName + ": not valid JSON");
            return tokens;
        }

        if (root.contains("version") and root["version"].is_number_integer())
            tokens.fileVersion = root["version"].get<int>();

        if (root.contains("type"))
            tokens.type = root["type"];

        // ── Colours: gather raw per-theme strings, then resolve aliases eagerly. ──
        std::unordered_map<std::string, std::array<std::string, 2>> raw;

        if (root.contains("color") and root["color"].contains("tokens"))
        {
            for (const auto& tok : root["color"]["tokens"])
            {
                if (not tok.contains("name") or not tok["name"].is_string())
                {
                    tokens.errorList.push_back(sourceName + ": a colour token has no name");
                    continue;
                }

                const std::string name = tok["name"].get<std::string>();

                const bool validValue = tok.contains("value") and tok["value"].is_object()
                    and tok["value"].contains("day") and tok["value"]["day"].is_string()
                    and tok["value"].contains("candle") and tok["value"]["candle"].is_string();

                if (not validValue)
                {
                    tokens.errorList.push_back(sourceName + ": colour '" + name + "': value must be an object with both day and candle");
                    continue;
                }

                raw[name] = { tok["value"]["day"].get<std::string>(), tok["value"]["candle"].get<std::string>() };
                tokens.colourOrder.push_back(name);
            }
        }

        for (const std::string& name : tokens.colourOrder)
        {
            Colour resolved;
            for (int t = 0; t < 2; ++t)
            {
                std::set<std::string> visited;
                resolved.byTheme[t] = resolveColour(raw, name, t, visited, tokens.errorList, sourceName);
            }
            tokens.colours[name] = resolved;
        }

        // ── Scales: spacing, border, radius (pixels), opacity (0..1). ──
        auto loadPxSection = [&](const char* section, std::unordered_map<std::string, float>& into)
        {
            if (not root.contains(section) or not root[section].contains("tokens"))
                return;

            for (const auto& tok : root[section]["tokens"])
            {
                if (not tok.contains("name") or not tok["name"].is_string() or not tok.contains("value") or not tok["value"].is_string())
                {
                    tokens.errorList.push_back(sourceName + std::string(": a ") + section + " token is malformed");
                    continue;
                }

                const std::string name = tok["name"].get<std::string>();
                float value = 0.0f;
                if (parsePx(tok["value"].get<std::string>(), value))
                    into[name] = value;
                else
                    tokens.errorList.push_back(sourceName + ": " + section + " '" + name + "': cannot parse '" + tok["value"].get<std::string>() + "'");
            }
        };

        loadPxSection("spacing", tokens.spacings);
        loadPxSection("border", tokens.borders);
        loadPxSection("radius", tokens.radii);

        if (root.contains("opacity") and root["opacity"].contains("tokens"))
        {
            for (const auto& tok : root["opacity"]["tokens"])
            {
                if (not tok.contains("name") or not tok["name"].is_string() or not tok.contains("value") or not tok["value"].is_string())
                {
                    tokens.errorList.push_back(sourceName + ": an opacity token is malformed");
                    continue;
                }

                const std::string name = tok["name"].get<std::string>();
                float value = 0.0f;
                bool okValue = false;
                try
                {
                    value = std::stof(tok["value"].get<std::string>());
                    okValue = value >= 0.0f and value <= 1.0f;
                }
                catch (const std::exception&)
                {
                    okValue = false;
                }

                if (okValue)
                    tokens.opacities[name] = value;
                else
                    tokens.errorList.push_back(sourceName + ": opacity '" + name + "': must be a number in [0,1]");
            }
        }

        return tokens;
    }

    pg::constant::Vector4D Tokens::resolveColour(const std::unordered_map<std::string, std::array<std::string, 2>>& raw,
                                                 const std::string& name, int themeIdx, std::set<std::string>& visited,
                                                 std::vector<std::string>& errors, const std::string& source)
    {
        if (visited.count(name))
        {
            errors.push_back(source + ": colour '" + name + "' (" + (themeIdx == 0 ? "day" : "candle") + "): alias cycle");
            return MAGENTA;
        }
        if (visited.size() >= 8)
        {
            errors.push_back(source + ": colour '" + name + "' (" + (themeIdx == 0 ? "day" : "candle") + "): alias chain too deep");
            return MAGENTA;
        }

        auto it = raw.find(name);
        if (it == raw.end())
        {
            errors.push_back(source + ": alias target '" + name + "' not found");
            return MAGENTA;
        }

        visited.insert(name);

        const std::string& value = it->second[themeIdx];

        if (isAlias(value))
            return resolveColour(raw, aliasTarget(value), themeIdx, visited, errors, source);

        pg::constant::Vector4D out;
        if (parseColourString(value, out))
            return out;

        errors.push_back(source + ": colour '" + name + "' (" + (themeIdx == 0 ? "day" : "candle") + "): cannot parse '" + value + "'");
        return MAGENTA;
    }

    bool Tokens::ok() const { return errorList.empty(); }
    const std::vector<std::string>& Tokens::errors() const { return errorList; }

    Theme Tokens::theme() const { return currentTheme; }
    void Tokens::setTheme(Theme t) { currentTheme = t; }

    pg::constant::Vector4D Tokens::colour(const std::string& name) const
    {
        return colour(name, currentTheme);
    }

    pg::constant::Vector4D Tokens::colour(const std::string& name, Theme t) const
    {
        auto it = colours.find(name);
        if (it == colours.end())
        {
            if (reportedUnknown.insert(name).second)
                LOG_ERROR(DOM, "Unknown colour token '" << name << "'");
            return MAGENTA;
        }

        return it->second.byTheme[static_cast<int>(t)];
    }

    bool Tokens::hasColour(const std::string& name) const
    {
        return colours.find(name) != colours.end();
    }

    std::vector<std::string> Tokens::colourNames() const
    {
        return colourOrder;
    }

    float Tokens::space(int step) const
    {
        assert(step >= 1 and step <= 7);
        if (step < 1) step = 1;
        if (step > 7) step = 7;
        return spacing("space-" + std::to_string(step));
    }

    float Tokens::lookupScale(const std::unordered_map<std::string, float>& table, const std::string& name) const
    {
        auto it = table.find(name);
        if (it == table.end())
        {
            if (reportedUnknown.insert(name).second)
                LOG_ERROR(DOM, "Unknown token '" << name << "'");
            return 0.0f;
        }
        return it->second;
    }

    float Tokens::spacing(const std::string& name) const { return lookupScale(spacings, name); }
    float Tokens::border(const std::string& name) const { return lookupScale(borders, name); }
    float Tokens::radius(const std::string& name) const { return lookupScale(radii, name); }
    float Tokens::opacity(const std::string& name) const { return lookupScale(opacities, name); }

    int Tokens::version() const { return fileVersion; }

    const nlohmann::json& Tokens::typeSection() const { return type; }
}
