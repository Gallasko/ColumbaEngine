#pragma once

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "pgconstant.h"

#include "Helpers/json.hpp"

namespace chronicle
{
    enum class Theme : uint8_t { Day = 0, Candle = 1 };

    const char* themeId(Theme t);                        // "day" / "candle"
    bool themeFromId(const std::string& id, Theme& out);

    // Sent by whoever calls setTheme (the scene), not by Tokens itself.
    struct ThemeChangedEvent { Theme theme; };

    // Loads the Chronicle design tokens (colours with per-theme values and aliases,
    // spacing/border/radius/opacity scales). Colours are resolved for both themes at
    // load time, so ok() is meaningful and colour() never fails at runtime.
    class Tokens
    {
    public:
        static Tokens load(const std::string& path);
        static Tokens parse(const std::string& jsonText, const std::string& sourceName = "<memory>");

        bool ok() const;
        const std::vector<std::string>& errors() const;

        Theme theme() const;
        void setTheme(Theme t);

        // Colours: 0-255 RGBA in the engine's convention, resolved for the current theme.
        pg::constant::Vector4D colour(const std::string& name) const;
        pg::constant::Vector4D colour(const std::string& name, Theme t) const;
        bool hasColour(const std::string& name) const;
        std::vector<std::string> colourNames() const;    // in file order

        float space(int step) const;                     // space-1 .. space-7 -> 4 .. 48
        float spacing(const std::string& name) const;    // "space-3" -> 12
        float border(const std::string& name) const;     // "border-rule" -> 2
        float radius(const std::string& name) const;     // "radius-none" -> 0
        float opacity(const std::string& name) const;    // "opacity-hatch" -> 0.4

        int version() const;

        // The raw "type" section, read by TextStyles.
        const nlohmann::json& typeSection() const;

    private:
        // Colour, resolved to a concrete RGBA per theme (index 0 = Day, 1 = Candle).
        struct Colour { std::array<pg::constant::Vector4D, 2> byTheme; };

        Theme currentTheme = Theme::Day;
        int fileVersion = 0;
        std::vector<std::string> errorList;

        std::vector<std::string> colourOrder;
        std::unordered_map<std::string, Colour> colours;

        std::unordered_map<std::string, float> spacings;
        std::unordered_map<std::string, float> borders;
        std::unordered_map<std::string, float> radii;
        std::unordered_map<std::string, float> opacities;

        nlohmann::json type;

        // Distinct unknown names already reported, so a hot loop logs once each.
        mutable std::set<std::string> reportedUnknown;
    };
}
