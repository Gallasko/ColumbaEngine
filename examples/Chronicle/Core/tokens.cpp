#include "tokens.h"

// Step 1 skeleton: real parsing/alias/contrast logic lands in "Add Chronicle tokens loader".

namespace chronicle
{
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

    Tokens Tokens::load(const std::string&) { return Tokens{}; }
    Tokens Tokens::parse(const std::string&, const std::string&) { return Tokens{}; }

    bool Tokens::ok() const { return errorList.empty(); }
    const std::vector<std::string>& Tokens::errors() const { return errorList; }

    Theme Tokens::theme() const { return currentTheme; }
    void Tokens::setTheme(Theme t) { currentTheme = t; }

    pg::constant::Vector4D Tokens::colour(const std::string& name) const { return colour(name, currentTheme); }
    pg::constant::Vector4D Tokens::colour(const std::string&, Theme) const { return {255.0f, 0.0f, 255.0f, 255.0f}; }
    bool Tokens::hasColour(const std::string& name) const { return colours.find(name) != colours.end(); }
    std::vector<std::string> Tokens::colourNames() const { return colourOrder; }

    float Tokens::space(int) const { return 0.0f; }
    float Tokens::spacing(const std::string&) const { return 0.0f; }
    float Tokens::border(const std::string&) const { return 0.0f; }
    float Tokens::radius(const std::string&) const { return 0.0f; }
    float Tokens::opacity(const std::string&) const { return 0.0f; }

    int Tokens::version() const { return fileVersion; }

    const nlohmann::json& Tokens::typeSection() const { return type; }
}
