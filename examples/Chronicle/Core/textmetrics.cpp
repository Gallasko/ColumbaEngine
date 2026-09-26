#include "textmetrics.h"

#include <unordered_map>

#include "UI/themesystem.h"
#include "UI/ttftext.h"

using namespace pg;

namespace chronicle
{
    float ascenderOf(EntitySystem* ecs, const std::string& style)
    {
        static std::unordered_map<std::string, float> cache;

        const TextStyle& s = ecs->getSystem<ThemeSystem>()->style(style);

        auto it = cache.find(s.fontAlias);
        if (it != cache.end())
            return it->second;

        const float asc = ecs->getSystem<TTFTextSystem>()->measureText(s.fontAlias, "H", 1.0f, 0.0f, 0.0f, s.letterSpacingPx).ascender;

        cache[s.fontAlias] = asc;
        return asc;
    }

    float baselineShift(EntitySystem* ecs, const std::string& fromStyle, const std::string& toStyle)
    {
        return ascenderOf(ecs, fromStyle) - ascenderOf(ecs, toStyle);
    }
}
