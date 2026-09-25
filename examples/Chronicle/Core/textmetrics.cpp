#include "textmetrics.h"

#include <unordered_map>

#include "UI/ttftext.h"

using namespace pg;

namespace chronicle
{
    float ascenderOf(EntitySystem* ecs, const TextStyles& styles, const std::string& style)
    {
        static std::unordered_map<std::string, float> cache;

        auto it = cache.find(style);
        if (it != cache.end())
            return it->second;

        const TextStyle& s = styles.get(style);
        const float asc = ecs->getSystem<TTFTextSystem>()->measureText(s.fontAlias, "H", 1.0f, 0.0f, 0.0f, s.letterSpacingPx).ascender;

        cache[style] = asc;
        return asc;
    }

    float baselineShift(EntitySystem* ecs, const TextStyles& styles, const std::string& fromStyle, const std::string& toStyle)
    {
        return ascenderOf(ecs, styles, fromStyle) - ascenderOf(ecs, styles, toStyle);
    }
}
