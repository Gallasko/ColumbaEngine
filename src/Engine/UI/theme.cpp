#include "stdafx.h"

#include "theme.h"

#include <cmath>
#include <map>
#include <sstream>

#include "Files/filemanager.h"
#include "logger.h"

#include "UI/ttftext.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Theme";

        const constant::Vector4D MAGENTA{255.0f, 0.0f, 255.0f, 255.0f};

        const TextStyle fallbackStyle{};
        const std::vector<TextStyle> noStyles{};

        constexpr size_t MaxAliasDepth = 8;
        constexpr size_t MaxExtendsDepth = 8;

        int hexValue(char c)
        {
            if (c >= '0' and c <= '9')
                return c - '0';

            if (c >= 'a' and c <= 'f')
                return c - 'a' + 10;

            if (c >= 'A' and c <= 'F')
                return c - 'A' + 10;

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

        bool parseEm(const std::string& s, float& out)
        {
            if (s.size() > 2 and s.compare(s.size() - 2, 2, "em") == 0)
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

        // A number or a "Npx" string -> pixels.
        bool parsePxValue(const nlohmann::json& value, float& out)
        {
            if (value.is_number())
            {
                out = value.get<float>();
                return true;
            }

            if (value.is_string())
                return parsePx(value.get<std::string>(), out);

            return false;
        }

        ElementType toElementType(const nlohmann::json& value)
        {
            if (value.is_string())
                return ElementType(value.get<std::string>());

            if (value.is_boolean())
                return ElementType(value.get<bool>());

            if (value.is_number_integer())
                return ElementType(static_cast<float>(value.get<int>()));

            if (value.is_number())
                return ElementType(value.get<float>());

            return ElementType();
        }

        // A font is identified by the family id, a numeric weight and the italic flag.
        struct FontKey
        {
            std::string family;
            int weight;
            bool italic;

            bool operator<(const FontKey& other) const
            {
                if (family != other.family)
                    return family < other.family;

                if (weight != other.weight)
                    return weight < other.weight;

                return italic < other.italic;
            }
        };
    }

    Theme Theme::load(const std::string& path)
    {
        TextFile file = UniversalFileAccessor::openTextFile(path);

        if (file.data.empty())
        {
            Theme theme;
            theme.errorList.push_back(path + ": could not open or empty file");
            theme.ids.push_back("default");
            theme.idIndex["default"] = 0;
            theme.styleTable.resize(1);

            return theme;
        }

        return parse(file.data, path);
    }

    Theme Theme::parse(const std::string& jsonText, const std::string& sourceName)
    {
        Theme theme;

        nlohmann::json root = nlohmann::json::parse(jsonText, nullptr, false);

        if (root.is_discarded())
        {
            theme.errorList.push_back(sourceName + ": not valid JSON");
            theme.ids.push_back("default");
            theme.idIndex["default"] = 0;
            theme.styleTable.resize(1);

            return theme;
        }

        if (root.contains("name") and root["name"].is_string())
            theme.themeName = root["name"].get<std::string>();

        if (root.contains("version") and root["version"].is_number_integer())
            theme.fileVersion = root["version"].get<int>();

        theme.parseThemeIds(root, sourceName);
        theme.parseColors(root, sourceName);
        theme.parseScales(root, sourceName);
        theme.parseStyles(root, sourceName);
        theme.parseElements(root, sourceName);

        return theme;
    }

    void Theme::parseThemeIds(const nlohmann::json& root, const std::string& source)
    {
        const nlohmann::json* list = nullptr;

        if (root.contains("themes") and root["themes"].is_array())
            list = &root["themes"];
        else if (root.contains("color") and root["color"].contains("themes") and root["color"]["themes"].is_array())
            list = &root["color"]["themes"];

        if (list)
        {
            for (const auto& entry : *list)
            {
                std::string id;

                if (entry.is_string())
                    id = entry.get<std::string>();
                else if (entry.is_object() and entry.contains("id") and entry["id"].is_string())
                    id = entry["id"].get<std::string>();

                if (id.empty())
                {
                    errorList.push_back(source + ": a theme entry has no id");
                    continue;
                }

                if (idIndex.count(id))
                {
                    errorList.push_back(source + ": theme id '" + id + "' listed twice");
                    continue;
                }

                idIndex[id] = ids.size();
                ids.push_back(id);
            }
        }

        if (ids.empty())
        {
            ids.push_back("default");
            idIndex["default"] = 0;
        }

        styleTable.resize(ids.size());
    }

    const nlohmann::json* Theme::pickValue(const nlohmann::json& value, size_t themeIdx) const
    {
        if (not value.is_object())
            return &value;

        const std::string& id = ids[themeIdx];

        if (value.contains(id))
            return &value[id];

        if (value.contains("default"))
            return &value["default"];

        return nullptr;
    }

    void Theme::parseColors(const nlohmann::json& root, const std::string& source)
    {
        std::unordered_map<std::string, RawColor> raw;

        if (root.contains("color") and root["color"].contains("tokens") and root["color"]["tokens"].is_array())
        {
            for (const auto& tok : root["color"]["tokens"])
            {
                if (not tok.contains("name") or not tok["name"].is_string())
                {
                    errorList.push_back(source + ": a color token has no name");
                    continue;
                }

                const std::string name = tok["name"].get<std::string>();

                if (not tok.contains("value"))
                {
                    errorList.push_back(source + ": color '" + name + "' has no value");
                    continue;
                }

                RawColor color;
                bool valid = true;

                for (size_t t = 0; t < ids.size(); ++t)
                {
                    auto value = pickValue(tok["value"], t);

                    if (not value or not value->is_string())
                    {
                        errorList.push_back(source + ": color '" + name + "' has no value for theme '" + ids[t] + "'");
                        valid = false;
                        break;
                    }

                    color.byTheme.push_back(value->get<std::string>());
                }

                if (not valid)
                    continue;

                raw[name] = color;
                colorOrder.push_back(name);
            }
        }

        for (const std::string& name : colorOrder)
        {
            std::vector<constant::Vector4D> resolved;

            for (size_t t = 0; t < ids.size(); ++t)
            {
                std::set<std::string> visited;
                resolved.push_back(resolveColor(raw, name, t, visited, source));
            }

            colors[name] = resolved;
        }
    }

    constant::Vector4D Theme::resolveColor(const std::unordered_map<std::string, RawColor>& raw, const std::string& token, size_t themeIdx, std::set<std::string>& visited, const std::string& source)
    {
        if (visited.count(token))
        {
            errorList.push_back(source + ": color '" + token + "' (" + ids[themeIdx] + "): alias cycle");
            return MAGENTA;
        }

        if (visited.size() >= MaxAliasDepth)
        {
            errorList.push_back(source + ": color '" + token + "' (" + ids[themeIdx] + "): alias chain too deep");
            return MAGENTA;
        }

        auto it = raw.find(token);

        if (it == raw.end())
        {
            errorList.push_back(source + ": alias target '" + token + "' not found");
            return MAGENTA;
        }

        visited.insert(token);

        const std::string& value = it->second.byTheme[themeIdx];

        if (isAlias(value))
            return resolveColor(raw, aliasTarget(value), themeIdx, visited, source);

        constant::Vector4D out;

        if (parseColorLiteral(value, out))
            return out;

        errorList.push_back(source + ": color '" + token + "' (" + ids[themeIdx] + "): cannot parse '" + value + "'");

        return MAGENTA;
    }

    void Theme::parseScales(const nlohmann::json& root, const std::string& source)
    {
        auto loadSection = [&](const char* section, std::unordered_map<std::string, std::vector<float>>& into, bool isOpacity) {
            if (not root.contains(section) or not root[section].contains("tokens") or not root[section]["tokens"].is_array())
                return;

            for (const auto& tok : root[section]["tokens"])
            {
                if (not tok.contains("name") or not tok["name"].is_string() or not tok.contains("value"))
                {
                    errorList.push_back(source + std::string(": a ") + section + " token is malformed");
                    continue;
                }

                const std::string name = tok["name"].get<std::string>();
                std::vector<float> values;
                bool valid = true;

                for (size_t t = 0; t < ids.size(); ++t)
                {
                    auto value = pickValue(tok["value"], t);
                    float parsed = 0.0f;
                    bool okValue = false;

                    if (value and isOpacity)
                    {
                        try
                        {
                            parsed = value->is_number() ? value->get<float>() : std::stof(value->get<std::string>());
                            okValue = parsed >= 0.0f and parsed <= 1.0f;
                        }
                        catch (const std::exception&)
                        {
                            okValue = false;
                        }
                    }
                    else if (value)
                    {
                        okValue = parsePxValue(*value, parsed);
                    }

                    if (not okValue)
                    {
                        errorList.push_back(source + ": " + section + " '" + name + "' (" + ids[t] + "): " + (isOpacity ? "must be a number in [0,1]" : "cannot parse the value"));
                        valid = false;
                        break;
                    }

                    values.push_back(parsed);
                }

                if (valid)
                    into[name] = values;
            }
        };

        loadSection("spacing", spacings, false);
        loadSection("border", borders, false);
        loadSection("radius", radii, false);
        loadSection("opacity", opacities, true);
    }

    void Theme::parseStyles(const nlohmann::json& root, const std::string& source)
    {
        if (not root.contains("type") or not root["type"].is_object())
            return;

        const nlohmann::json& type = root["type"];

        std::map<FontKey, std::string> fontFiles;

        if (type.contains("fonts") and type["fonts"].is_array())
        {
            for (const auto& font : type["fonts"])
            {
                if (not font.is_object() or not font.contains("family") or not font.contains("file"))
                {
                    errorList.push_back(source + ": a type.fonts entry needs family and file");
                    continue;
                }

                FontKey key{font["family"].get<std::string>(), font.value("weight", 400), font.value("italic", false)};
                fontFiles[key] = font["file"].get<std::string>();
            }
        }

        if (not type.contains("groups") or not type["groups"].is_array())
            return;

        for (const auto& group : type["groups"])
        {
            const std::string family = group.value("family", std::string());

            if (not group.contains("styles") or not group["styles"].is_array())
                continue;

            for (const auto& st : group["styles"])
            {
                const std::string name = st.value("name", std::string());

                if (name.empty())
                {
                    errorList.push_back(source + ": a text style has no name");
                    continue;
                }

                std::vector<TextStyle> perTheme;

                for (size_t t = 0; t < ids.size(); ++t)
                {
                    TextStyle style;
                    style.name = name;
                    style.family = family;
                    style.sample = st.value("sample", std::string());

                    auto pxField = [&](const char* field, int& out) {
                        if (not st.contains(field))
                            return false;

                        auto value = pickValue(st[field], t);
                        float px = 0.0f;

                        if (value and parsePxValue(*value, px))
                        {
                            out = static_cast<int>(std::lround(px));
                            return true;
                        }

                        return false;
                    };

                    if (not pxField("fontSize", style.sizePx))
                        errorList.push_back(source + ": style '" + name + "' (" + ids[t] + "): bad fontSize");

                    if (not pxField("lineHeight", style.lineHeightPx))
                        errorList.push_back(source + ": style '" + name + "' (" + ids[t] + "): bad lineHeight");

                    if (st.contains("fontWeight"))
                    {
                        auto value = pickValue(st["fontWeight"], t);

                        if (value and value->is_number_integer())
                            style.weight = value->get<int>();
                    }

                    if (st.contains("fontStyle"))
                    {
                        auto value = pickValue(st["fontStyle"], t);
                        style.italic = value and value->is_string() and value->get<std::string>() == "italic";
                    }

                    float em = 0.0f;

                    if (st.contains("letterSpacing"))
                    {
                        auto value = pickValue(st["letterSpacing"], t);

                        if (value and value->is_string())
                            parseEm(value->get<std::string>(), em);
                        else if (value and value->is_number())
                            em = value->get<float>();
                    }

                    style.letterSpacingPx = std::round(em * style.sizePx / 0.25f) * 0.25f;

                    auto it = fontFiles.find({family, style.weight, style.italic});

                    if (it == fontFiles.end())
                        errorList.push_back(source + ": style '" + name + "' (" + ids[t] + "): no font file for family '" + family + "' weight " + std::to_string(style.weight) + (style.italic ? " italic" : ""));
                    else
                        style.fontFile = it->second;

                    perTheme.push_back(style);
                }

                // One atlas when every theme rasterises the same face at the same size, one per theme otherwise.
                bool identical = true;

                for (size_t t = 1; t < perTheme.size(); ++t)
                {
                    if (perTheme[t].fontFile != perTheme[0].fontFile or perTheme[t].sizePx != perTheme[0].sizePx)
                        identical = false;
                }

                for (size_t t = 0; t < perTheme.size(); ++t)
                {
                    perTheme[t].fontAlias = identical ? name : name + "@" + ids[t];
                    styleTable[t].push_back(perTheme[t]);
                }
            }
        }
    }

    void Theme::parseElements(const nlohmann::json& root, const std::string& source)
    {
        if (not root.contains("elements") or not root["elements"].is_object())
            return;

        for (auto it = root["elements"].begin(); it != root["elements"].end(); ++it)
        {
            const std::string& key = it.key();
            const nlohmann::json& def = it.value();

            if (not def.is_object())
            {
                errorList.push_back(source + ": element '" + key + "' must be an object");
                continue;
            }

            ThemeElement element;
            element.byTheme.resize(ids.size());

            for (auto field = def.begin(); field != def.end(); ++field)
            {
                if (field.key() == "extends")
                {
                    if (field.value().is_string())
                        element.extends = field.value().get<std::string>();
                    else
                        errorList.push_back(source + ": element '" + key + "': extends must be a string");

                    continue;
                }

                for (size_t t = 0; t < ids.size(); ++t)
                {
                    auto value = pickValue(field.value(), t);

                    if (not value)
                    {
                        errorList.push_back(source + ": element '" + key + "' entry '" + field.key() + "' has no value for theme '" + ids[t] + "'");
                        continue;
                    }

                    if (not value->is_string() and not value->is_number() and not value->is_boolean())
                    {
                        errorList.push_back(source + ": element '" + key + "' entry '" + field.key() + "' must be a string or a number");
                        continue;
                    }

                    element.byTheme[t][field.key()] = toElementType(*value);
                }
            }

            elementOrder.push_back(key);
            elements[key] = element;
        }

        for (const auto& key : elementOrder)
        {
            const std::string& base = elements[key].extends;

            if (not base.empty() and elements.find(base) == elements.end())
                errorList.push_back(source + ": element '" + key + "' extends unknown element '" + base + "'");
        }
    }

    bool Theme::parseColorLiteral(const std::string& s, constant::Vector4D& out)
    {
        if (s.empty())
            return false;

        if (s[0] == '#')
        {
            const std::string hex = s.substr(1);

            if (hex.size() == 3)
            {
                out.w = 255.0f;
                return parseHexPair(hex[0], hex[0], out.x) and parseHexPair(hex[1], hex[1], out.y) and parseHexPair(hex[2], hex[2], out.z);
            }

            if (hex.size() == 6)
            {
                out.w = 255.0f;
                return parseHexPair(hex[0], hex[1], out.x) and parseHexPair(hex[2], hex[3], out.y) and parseHexPair(hex[4], hex[5], out.z);
            }

            if (hex.size() == 8)
            {
                return parseHexPair(hex[0], hex[1], out.x) and parseHexPair(hex[2], hex[3], out.y) and parseHexPair(hex[4], hex[5], out.z) and parseHexPair(hex[6], hex[7], out.w);
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
                out.w = std::round(std::stof(parts[3]) * 255.0f);
            }
            catch (const std::exception&)
            {
                return false;
            }

            return true;
        }

        return false;
    }

    bool Theme::ok() const
    {
        return errorList.empty();
    }

    const std::vector<std::string>& Theme::errors() const
    {
        return errorList;
    }

    const std::string& Theme::name() const
    {
        return themeName;
    }

    int Theme::version() const
    {
        return fileVersion;
    }

    const std::vector<std::string>& Theme::themeIds() const
    {
        return ids;
    }

    bool Theme::hasThemeId(const std::string& id) const
    {
        return idIndex.find(id) != idIndex.end();
    }

    int Theme::themeIndex(const std::string& id) const
    {
        auto it = idIndex.find(id);

        if (it == idIndex.end())
            return -1;

        return static_cast<int>(it->second);
    }

    size_t Theme::themeIndexOrDefault(const std::string& themeId) const
    {
        auto it = idIndex.find(themeId);

        if (it == idIndex.end())
        {
            if (reportedUnknown.insert("theme:" + themeId).second)
                LOG_ERROR(DOM, "Unknown theme id '" << themeId << "', using '" << ids[0] << "'");

            return 0;
        }

        return it->second;
    }

    constant::Vector4D Theme::color(const std::string& token, const std::string& themeId) const
    {
        auto it = colors.find(token);

        if (it == colors.end())
        {
            if (reportedUnknown.insert("color:" + token).second)
                LOG_ERROR(DOM, "Unknown color token '" << token << "'");

            return MAGENTA;
        }

        return it->second[themeIndexOrDefault(themeId)];
    }

    bool Theme::hasColor(const std::string& token) const
    {
        return colors.find(token) != colors.end();
    }

    const std::vector<std::string>& Theme::colorNames() const
    {
        return colorOrder;
    }

    float Theme::lookupScale(const std::unordered_map<std::string, std::vector<float>>& table, const std::string& token, const std::string& themeId) const
    {
        auto it = table.find(token);

        if (it == table.end())
        {
            if (reportedUnknown.insert("scale:" + token).second)
                LOG_ERROR(DOM, "Unknown scale token '" << token << "'");

            return 0.0f;
        }

        return it->second[themeIndexOrDefault(themeId)];
    }

    float Theme::spacing(const std::string& token, const std::string& themeId) const
    {
        return lookupScale(spacings, token, themeId);
    }

    float Theme::space(int step, const std::string& themeId) const
    {
        if (step < 1)
            step = 1;

        if (step > 7)
            step = 7;

        return spacing("space-" + std::to_string(step), themeId);
    }

    float Theme::border(const std::string& token, const std::string& themeId) const
    {
        return lookupScale(borders, token, themeId);
    }

    float Theme::radius(const std::string& token, const std::string& themeId) const
    {
        return lookupScale(radii, token, themeId);
    }

    float Theme::opacity(const std::string& token, const std::string& themeId) const
    {
        return lookupScale(opacities, token, themeId);
    }

    bool Theme::hasScale(const std::string& token) const
    {
        return spacings.count(token) or borders.count(token) or radii.count(token) or opacities.count(token);
    }

    bool Theme::hasStyle(const std::string& styleName) const
    {
        if (styleTable.empty())
            return false;

        for (const auto& s : styleTable[0])
        {
            if (s.name == styleName)
                return true;
        }

        return false;
    }

    const TextStyle& Theme::style(const std::string& styleName, const std::string& themeId) const
    {
        const auto& table = styles(themeId);

        for (const auto& s : table)
        {
            if (s.name == styleName)
                return s;
        }

        if (reportedUnknown.insert("style:" + styleName).second)
            LOG_ERROR(DOM, "Unknown text style '" << styleName << "'");

        for (const auto& s : table)
        {
            if (s.name == "body")
                return s;
        }

        return fallbackStyle;
    }

    const std::vector<TextStyle>& Theme::styles(const std::string& themeId) const
    {
        if (styleTable.empty())
            return noStyles;

        return styleTable[themeIndexOrDefault(themeId)];
    }

    size_t Theme::registerFonts(TTFTextSystem* ttf, const std::string& fontRoot)
    {
        if (not ttf)
            return 0;

        size_t count = 0;
        std::set<std::string> registered;

        for (auto& table : styleTable)
        {
            for (auto& style : table)
            {
                if (style.fontFile.empty())
                    continue;

                if (registered.insert(style.fontAlias).second)
                    ttf->registerFont(fontRoot + "/" + style.fontFile, style.fontAlias, style.sizePx);

                // registerFont skips a missing file (and logs); detect it and move on.
                if (ttf->fonts.find(style.fontAlias) == ttf->fonts.end())
                    continue;

                const float atlasLineHeight = ttf->measureText(style.fontAlias, "Hg").lineHeight;
                style.lineSpacingPx = static_cast<float>(style.lineHeightPx) - atlasLineHeight;

                ++count;
            }
        }

        return count;
    }

    bool Theme::hasElement(const std::string& key) const
    {
        return elements.find(key) != elements.end();
    }

    const std::vector<std::string>& Theme::elementNames() const
    {
        return elementOrder;
    }

    void Theme::mergeElement(const std::string& key, size_t themeIdx, ElementMap& into, std::set<std::string>& visited, bool& found) const
    {
        auto it = elements.find(key);

        if (it == elements.end())
            return;

        if (visited.count(key) or visited.size() >= MaxExtendsDepth)
        {
            if (reportedUnknown.insert("extends:" + key).second)
                LOG_ERROR(DOM, "Element '" << key << "': extends chain is cyclic or too deep");

            return;
        }

        visited.insert(key);
        found = true;

        if (not it->second.extends.empty())
        {
            bool baseFound = false;
            std::set<std::string> baseVisited = visited;
            ElementMap base;

            // The base is resolved in full: its own prefix chain and its own extends.
            std::string prefix;
            std::stringstream stream(it->second.extends);
            std::string part;

            while (std::getline(stream, part, '.'))
            {
                prefix = prefix.empty() ? part : prefix + "." + part;
                mergeElement(prefix, themeIdx, base, baseVisited, baseFound);
            }

            for (const auto& entry : base)
                into[entry.first] = entry.second;
        }

        for (const auto& entry : it->second.byTheme[themeIdx])
            into[entry.first] = entry.second;
    }

    ElementMap Theme::resolveElement(const std::string& key, const std::string& themeId, bool& found) const
    {
        ElementMap out;
        found = false;

        const size_t themeIdx = themeIndexOrDefault(themeId);

        std::string prefix;
        std::stringstream stream(key);
        std::string part;

        while (std::getline(stream, part, '.'))
        {
            prefix = prefix.empty() ? part : prefix + "." + part;

            std::set<std::string> visited;

            if (elements.count(prefix))
            {
                mergeElement(prefix, themeIdx, out, visited, found);
            }
            else if (colors.count(part))
            {
                // An undefined segment that names a color token paints with it: "label.body.ink-muted"
                out["color"] = ElementType(part);
                found = true;
            }
            else if (part != prefix and elements.count(part))
            {
                // An undefined segment that names a root element mixes it in: "button.seal.ground.disabled"
                mergeElement(part, themeIdx, out, visited, found);
            }
        }

        return out;
    }
}
