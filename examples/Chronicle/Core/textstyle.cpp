#include "textstyle.h"

#include <cmath>

#include "ECS/entitysystem.h"
#include "logger.h"

#include "fontfiles.h"

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.TextStyles";

        const TextStyle fallbackStyle{};

        bool parsePxInt(const std::string& s, int& out)
        {
            if (s.size() > 2 and s.compare(s.size() - 2, 2, "px") == 0)
            {
                try { out = std::stoi(s.substr(0, s.size() - 2)); return true; }
                catch (const std::exception&) { return false; }
            }
            return false;
        }

        bool parseEm(const std::string& s, float& out)
        {
            if (s.size() > 2 and s.compare(s.size() - 2, 2, "em") == 0)
            {
                try { out = std::stof(s.substr(0, s.size() - 2)); return true; }
                catch (const std::exception&) { return false; }
            }
            return false;
        }
    }

    TextStyles TextStyles::fromTokens(const Tokens& tokens)
    {
        TextStyles result;

        const nlohmann::json& type = tokens.typeSection();
        if (not type.contains("groups") or not type["groups"].is_array())
        {
            result.errorList.push_back("type section has no groups");
            return result;
        }

        for (const auto& group : type["groups"])
        {
            const std::string family = group.value("family", std::string());

            if (not group.contains("styles"))
                continue;

            for (const auto& st : group["styles"])
            {
                TextStyle style;
                style.name = st.value("name", std::string());
                style.family = family;

                int size = 0;
                if (st.contains("fontSize") and st["fontSize"].is_string() and parsePxInt(st["fontSize"].get<std::string>(), size))
                    style.sizePx = size;
                else
                    result.errorList.push_back("style '" + style.name + "': bad fontSize");

                int lineHeight = 0;
                if (st.contains("lineHeight") and st["lineHeight"].is_string() and parsePxInt(st["lineHeight"].get<std::string>(), lineHeight))
                    style.lineHeightPx = lineHeight;
                else
                    result.errorList.push_back("style '" + style.name + "': bad lineHeight");

                if (st.contains("fontWeight") and st["fontWeight"].is_number_integer())
                    style.weight = st["fontWeight"].get<int>();

                style.italic = st.contains("fontStyle") and st["fontStyle"].is_string() and st["fontStyle"].get<std::string>() == "italic";

                float em = 0.0f;
                if (st.contains("letterSpacing") and st["letterSpacing"].is_string())
                    parseEm(st["letterSpacing"].get<std::string>(), em);
                style.letterSpacingPx = std::round(em * style.sizePx / 0.25f) * 0.25f;

                style.fontAlias = "chr-" + style.name;

                auto it = fontFiles().find({family, style.weight, style.italic});
                if (it == fontFiles().end())
                    result.errorList.push_back("style '" + style.name + "': no font file for family '" + family
                        + "' weight " + std::to_string(style.weight) + (style.italic ? " italic" : ""));
                else
                    style.fontFile = it->second;

                result.styles.push_back(style);
            }
        }

        return result;
    }

    bool TextStyles::ok() const { return errorList.empty(); }
    const std::vector<std::string>& TextStyles::errors() const { return errorList; }

    const TextStyle& TextStyles::get(const std::string& name) const
    {
        for (const auto& s : styles)
            if (s.name == name)
                return s;

        if (reportedUnknown.insert(name).second)
            LOG_ERROR(DOM, "Unknown text style '" << name << "'");

        for (const auto& s : styles)
            if (s.name == "body")
                return s;

        return fallbackStyle;
    }

    bool TextStyles::has(const std::string& name) const
    {
        for (const auto& s : styles)
            if (s.name == name)
                return true;
        return false;
    }

    const std::vector<TextStyle>& TextStyles::all() const { return styles; }

    size_t TextStyles::registerAll(pg::TTFTextSystem* ttf, const std::string& fontRoot)
    {
        size_t count = 0;

        for (auto& style : styles)
        {
            if (style.fontFile.empty())
                continue;

            ttf->registerFont(fontRoot + "/" + style.fontFile, style.fontAlias, style.sizePx);

            // registerFont skips a missing file (and logs); detect it and move on.
            if (ttf->fonts.find(style.fontAlias) == ttf->fonts.end())
                continue;

            const float atlasLineHeight = ttf->measureText(style.fontAlias, "Hg").lineHeight;
            style.lineSpacingPx = static_cast<float>(style.lineHeightPx) - atlasLineHeight;

            ++count;
        }

        return count;
    }

    pg::CompList<pg::PositionComponent, pg::UiAnchor, pg::ViewportComponent, pg::TTFText>
    TextStyles::makeText(pg::EntitySystem* ecs, const std::string& style, const std::string& text,
                         const pg::constant::Vector4D& colour, float x, float y, float z) const
    {
        const TextStyle& s = get(style);

        auto comp = pg::makeTTFText(ecs, x, y, z, s.fontAlias, text, 1.0f, colour);

        auto ttf = comp.get<pg::TTFText>();
        ttf->setLetterSpacing(s.letterSpacingPx);
        ttf->setSpacing(s.lineSpacingPx);

        return comp;
    }
}
