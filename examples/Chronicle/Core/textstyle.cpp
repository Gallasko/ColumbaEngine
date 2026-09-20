#include "textstyle.h"

#include "ECS/entitysystem.h"

// Step 1 skeleton: real style parsing and font registration lands in
// "Add Chronicle text styles and font registration".

namespace chronicle
{
    namespace
    {
        const TextStyle defaultStyle{};
    }

    TextStyles TextStyles::fromTokens(const Tokens&) { return TextStyles{}; }

    bool TextStyles::ok() const { return errorList.empty(); }
    const std::vector<std::string>& TextStyles::errors() const { return errorList; }

    const TextStyle& TextStyles::get(const std::string&) const { return defaultStyle; }
    bool TextStyles::has(const std::string&) const { return false; }
    const std::vector<TextStyle>& TextStyles::all() const { return styles; }

    size_t TextStyles::registerAll(pg::TTFTextSystem*, const std::string&) { return 0; }

    pg::CompList<pg::PositionComponent, pg::UiAnchor, pg::ViewportComponent, pg::TTFText>
    TextStyles::makeText(pg::EntitySystem* ecs, const std::string& style, const std::string& text,
                         const pg::constant::Vector4D& colour, float x, float y, float z) const
    {
        const TextStyle& s = get(style);
        return pg::makeTTFText(ecs, x, y, z, s.fontAlias, text, 1.0f, colour);
    }
}
