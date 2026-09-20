#include "ornament.h"

#include <unordered_set>
#include <vector>

#include "logger.h"

#include "UI/iconsystem.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Ornament";

        // The seven ornament drawings, authored on square viewBoxes.
        const std::vector<std::string> ORNAMENT_NAMES = {
            "knot", "flourish",
            "corner-tl", "corner-tr", "corner-bl", "corner-br",
            "versal-curls",
        };

        std::unordered_set<EntitySystem*>& registeredIn()
        {
            static std::unordered_set<EntitySystem*> instances;
            return instances;
        }
    }

    bool registerOrnaments(EntitySystem* ecs, const std::string& iconRoot)
    {
        auto* icons = ecs->getSystem<IconSystem>();
        if (not icons)
        {
            LOG_ERROR(DOM, "registerOrnaments: no IconSystem in this ECS");
            return false;
        }

        if (not registeredIn().insert(ecs).second)
            return true;

        std::vector<std::string> paths;
        paths.reserve(ORNAMENT_NAMES.size());
        for (const auto& name : ORNAMENT_NAMES)
            paths.push_back(iconRoot + "/" + name + ".svg");

        icons->registerIconSet("chronicle-ornaments", paths, {22, 28, 72, 120});
        return true;
    }
}
