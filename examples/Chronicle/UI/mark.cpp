#include "mark.h"

#include <unordered_set>

#include "logger.h"

#include "UI/iconsystem.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Mark";

        // The 27 marks, grouped as the design system lists them. This vector is the order
        // the gallery draws and the only place the set is enumerated.
        const std::vector<std::string> NAMES = {
            // Parts
            "strength", "dexterity", "intelligence", "vitality", "swordsmanship", "magic",
            // Resources
            "gold", "equipment", "relic",
            // Activities
            "adventure", "work", "study", "training", "forge", "quill", "trade",
            // Places
            "town", "gate", "academy", "guild",
            // Meta
            "time", "age", "reputation", "seal", "skull",
            // Status
            "check", "cross",
        };

        const std::unordered_set<std::string> NAME_SET(NAMES.begin(), NAMES.end());

        // ECS instances that have already registered the set; keeps a repeat call a no-op.
        std::unordered_set<EntitySystem*>& registeredIn()
        {
            static std::unordered_set<EntitySystem*> instances;
            return instances;
        }
    }

    MarkSize markSizeFor(const std::string& style)
    {
        if (style == "caption" or style == "tick" or style == "body-sm"
            or style == "gloss" or style == "label")
            return MarkSize::S14;
        if (style == "body")
            return MarkSize::S16;
        if (style == "figure" or style == "heading")
            return MarkSize::S18;
        if (style == "title" or style == "figure-xl" or style == "chapter")
            return MarkSize::S24;
        if (style == "versal")
            return MarkSize::S48;

        static std::unordered_set<std::string> warned;
        if (warned.insert(style).second)
            LOG_WARNING(DOM, "No mark size for style '" << style << "'; using S16");
        return MarkSize::S16;
    }

    const std::vector<std::string>& markNames() { return NAMES; }

    bool isMarkName(const std::string& name) { return NAME_SET.count(name) > 0; }

    bool registerMarks(EntitySystem* ecs, const std::string& iconRoot)
    {
        auto* icons = ecs->getSystem<IconSystem>();
        if (not icons)
        {
            LOG_ERROR(DOM, "registerMarks: no IconSystem in this ECS");
            return false;
        }

        // Idempotent: a second call must not re-rasterise the 135 glyphs.
        if (not registeredIn().insert(ecs).second)
            return true;

        std::vector<std::string> paths;
        paths.reserve(NAMES.size());
        for (const auto& name : NAMES)
            paths.push_back(iconRoot + "/" + name + ".svg");

        icons->registerIconSet("chronicle", paths, {14, 16, 18, 24, 48});
        return true;
    }
}
