#pragma once

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "pgconstant.h"

#include "Memory/elementtype.h"
#include "Helpers/json.hpp"

namespace pg
{
    struct TTFTextSystem;

    // One text style of a theme, resolved for one theme id (styles may override fields per theme).
    struct TextStyle
    {
        std::string name;          // "body", "figure-xl", ...
        std::string family;        // family id of the type section, "display" | "text"
        int sizePx = 16;           // exact rasterisation size
        int lineHeightPx = 24;     // the stacking rhythm of the style
        int weight = 400;
        bool italic = false;
        float letterSpacingPx = 0.0f;   // letterSpacing (em) x sizePx, rounded to 0.25 px
        std::string fontAlias;     // the TTFTextSystem font name: the style name, or "<name>@<theme>" when the style differs per theme
        std::string fontFile;      // from the type.fonts entries, relative to the font root
        float lineSpacingPx = 0.0f;     // filled by registerFonts: lineHeightPx - atlas line height
        std::string sample;        // the specimen text of the style, "" when the file gives none
    };

    // An element of the theme: the role values (color, font, alpha, radius, border...) an entity painted with that key receives.
    struct ThemeElement
    {
        std::string extends;                // another element merged under this one
        std::vector<ElementMap> byTheme;    // one map per theme id, in themeIds() order
    };

    /**
     * @brief The design tokens of a theme file: colours, scales, text styles and elements, resolved for every theme id at load time.
     *
     * A token value is either a scalar (the same for every theme) or an object keyed by theme id with an optional "default" entry.
     * Colours may alias another colour with "{name}". Everything is resolved when the file is parsed so ok() is meaningful and
     * no lookup fails at runtime: an unknown colour is magenta and logged once.
     */
    class Theme
    {
    public:
        static Theme load(const std::string& path);
        static Theme parse(const std::string& jsonText, const std::string& sourceName = "<memory>");

        bool ok() const;
        const std::vector<std::string>& errors() const;

        const std::string& name() const;
        int version() const;

        const std::vector<std::string>& themeIds() const;
        bool hasThemeId(const std::string& id) const;
        int themeIndex(const std::string& id) const;                  // -1 when unknown

        // Colours: 0-255 RGBA in the engine's convention.
        constant::Vector4D color(const std::string& token, const std::string& themeId) const;
        bool hasColor(const std::string& token) const;
        const std::vector<std::string>& colorNames() const;          // in file order

        float spacing(const std::string& token, const std::string& themeId) const;   // "space-3" -> 12
        float space(int step, const std::string& themeId) const;                     // space-1 .. space-7
        float border(const std::string& token, const std::string& themeId) const;    // "border-rule" -> 2
        float radius(const std::string& token, const std::string& themeId) const;    // "radius-none" -> 0
        float opacity(const std::string& token, const std::string& themeId) const;   // "opacity-hatch" -> 0.4
        bool hasScale(const std::string& token) const;

        bool hasStyle(const std::string& styleName) const;
        const TextStyle& style(const std::string& styleName, const std::string& themeId) const;   // unknown -> logs once, returns "body" or a default
        const std::vector<TextStyle>& styles(const std::string& themeId) const;                    // file order

        // Registers every distinct font alias as its own atlas: ttf->registerFont(root + "/" + fontFile, alias, sizePx),
        // then fills lineSpacingPx. Returns the number registered; a style whose file is missing is logged and skipped.
        size_t registerFonts(TTFTextSystem* ttf, const std::string& fontRoot);

        bool hasElement(const std::string& key) const;
        const std::vector<std::string>& elementNames() const;

        // Merges the element definitions along the dotted prefix chain of key ("a", "a.b", "a.b.c"), each one after its
        // "extends" chain, deeper prefixes overriding. An undefined prefix whose last segment names a color token sets
        // "color" to it; one whose last segment names a root element mixes that element in. found is false when
        // nothing matched.
        ElementMap resolveElement(const std::string& key, const std::string& themeId, bool& found) const;

        // "#rgb", "#rrggbb", "#rrggbbaa" or "rgba(r,g,b,a)" -> 0-255 RGBA.
        static bool parseColorLiteral(const std::string& text, constant::Vector4D& out);

    private:
        struct RawColor { std::vector<std::string> byTheme; };

        void parseThemeIds(const nlohmann::json& root, const std::string& source);
        void parseColors(const nlohmann::json& root, const std::string& source);
        void parseScales(const nlohmann::json& root, const std::string& source);
        void parseStyles(const nlohmann::json& root, const std::string& source);
        void parseElements(const nlohmann::json& root, const std::string& source);

        // Picks the value for one theme out of a scalar or a per-theme object; nullptr when the theme has no value.
        const nlohmann::json* pickValue(const nlohmann::json& value, size_t themeIdx) const;

        constant::Vector4D resolveColor(const std::unordered_map<std::string, RawColor>& raw, const std::string& token, size_t themeIdx, std::set<std::string>& visited, const std::string& source);

        void mergeElement(const std::string& key, size_t themeIdx, ElementMap& into, std::set<std::string>& visited, bool& found) const;

        float lookupScale(const std::unordered_map<std::string, std::vector<float>>& table, const std::string& token, const std::string& themeId) const;

        size_t themeIndexOrDefault(const std::string& themeId) const;

        std::string themeName;
        int fileVersion = 0;
        std::vector<std::string> errorList;

        std::vector<std::string> ids;
        std::unordered_map<std::string, size_t> idIndex;

        std::vector<std::string> colorOrder;
        std::unordered_map<std::string, std::vector<constant::Vector4D>> colors;

        std::unordered_map<std::string, std::vector<float>> spacings;
        std::unordered_map<std::string, std::vector<float>> borders;
        std::unordered_map<std::string, std::vector<float>> radii;
        std::unordered_map<std::string, std::vector<float>> opacities;

        std::vector<std::vector<TextStyle>> styleTable;   // [themeIdx][file order]

        std::vector<std::string> elementOrder;
        std::unordered_map<std::string, ThemeElement> elements;

        // Distinct unknown names already reported, so a hot loop logs once each.
        mutable std::set<std::string> reportedUnknown;
    };
}
